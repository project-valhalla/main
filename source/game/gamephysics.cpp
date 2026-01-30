#include "game.h"

namespace physics
{
    using namespace game;

    const float STAIRHEIGHT = 4.1f;
    const float FLOORZ = 0.867f;
    const float SLOPEZ = 0.5f;
    const float WALLZ = 0.2f;
    const float RAMPZ_MIN = 0.5f;
    const float RAMPZ_MAX = 0.98f;

    bool canmove(gameent* d)
    {
        if (d->type != ENT_PLAYER || d->state == CS_SPECTATOR)
        {
            return true;
        }
        return !mainmenu && !intermission && !(d->state == CS_DEAD && d->deathstate == Death_Gib);
    }

    void recalculatedirection(gameent* d, const vec& oldvel, vec& dir)
    {
        float speed = oldvel.magnitude();
        if (speed > 1e-6f)
        {
            float step = dir.magnitude();
            dir = d->vel;
            dir.add(d->falling);
            dir.mul(step / speed);
        }
    }

    void slideagainst(gameent* d, vec& dir, const vec& obstacle, bool foundfloor, bool slidecollide)
    {
        vec wall(obstacle);
        if (foundfloor ? wall.z > 0 : slidecollide)
        {
            wall.z = 0;
            if (!wall.iszero()) wall.normalize();
        }
        vec oldvel(d->vel);
        oldvel.add(d->falling);
        d->vel.project(wall);
        d->falling.project(wall);
        recalculatedirection(d, oldvel, dir);
    }

    void switchfloor(gameent* d, vec& dir, const vec& floor)
    {
        if (floor.z >= FLOORZ) d->falling = vec(0, 0, 0);

        vec oldvel(d->vel);
        oldvel.add(d->falling);
        if (dir.dot(floor) >= 0)
        {
            if (d->physstate < PHYS_SLIDE || fabs(dir.dot(d->floor)) > 0.01f * dir.magnitude()) return;
            d->vel.projectxy(floor, 0.0f);
        }
        else d->vel.projectxy(floor);
        d->falling.project(floor);
        recalculatedirection(d, oldvel, dir);
    }

    bool trystepup(gameent* d, vec& dir, const vec& obstacle, float maxstep, const vec& floor)
    {
        vec old(d->o), stairdir = (obstacle.z >= 0 && obstacle.z < SLOPEZ ? vec(-obstacle.x, -obstacle.y, 0) : vec(dir.x, dir.y, 0)).rescale(1);
        bool cansmooth = true;

        if (d->physstate != PHYS_STEP_UP)
        {
            // Check if there is space atop the stair to move to.
            vec checkdir = stairdir;
            checkdir.mul(0.1f);
            checkdir.z += maxstep + 0.1f;
            d->o.add(checkdir);
            if (collide(d))
            {
                d->o = old;
                if (!collide(d, vec(0, 0, -1), SLOPEZ)) return false;
                cansmooth = false;
            }
        }

        if (cansmooth)
        {
            vec checkdir = stairdir;
            checkdir.z += 1;
            checkdir.mul(maxstep);
            d->o = old;
            d->o.add(checkdir);
            int scale = 2;
            if (collide(d, checkdir))
            {
                if (!collide(d, vec(0, 0, -1), SLOPEZ))
                {
                    d->o = old;
                    return false;
                }
                d->o.add(checkdir);
                if (collide(d, vec(0, 0, -1), SLOPEZ)) scale = 1;
            }
            if (scale != 1)
            {
                d->o = old;
                d->o.sub(checkdir.mul(vec(2, 2, 1)));
                if (!collide(d, vec(0, 0, -1), SLOPEZ)) scale = 1;
            }

            d->o = old;
            vec smoothdir(dir.x, dir.y, 0);
            float magxy = smoothdir.magnitude();
            if (magxy > 1e-9f)
            {
                if (magxy > scale * dir.z)
                {
                    smoothdir.mul(1 / magxy);
                    smoothdir.z = 1.0f / scale;
                    smoothdir.mul(dir.magnitude() / smoothdir.magnitude());
                }
                else smoothdir.z = dir.z;
                d->o.add(smoothdir);
                d->o.z += maxstep + 0.1f;
                if (!collide(d, smoothdir))
                {
                    d->o.z -= maxstep + 0.1f;
                    if (d->physstate == PHYS_FALL || d->floor != floor)
                    {
                        d->timeinair = 0;
                        d->floor = floor;
                        switchfloor(d, dir, d->floor);
                    }
                    d->physstate = PHYS_STEP_UP;
                    return true;
                }
            }
        }

        // Try stepping up.
        d->o = old;
        d->o.z += dir.magnitude();
        if (!collide(d, vec(0, 0, 1)))
        {
            if (d->physstate == PHYS_FALL || d->floor != floor)
            {
                d->timeinair = 0;
                d->floor = floor;
                switchfloor(d, dir, d->floor);
            }
            if (cansmooth) d->physstate = PHYS_STEP_UP;
            return true;
        }
        d->o = old;
        return false;
    }

    static bool retryToStepDown(gameent* player, vec& dir, const float step, const float horizontalScale, const float verticalScale, const bool isStepping = false)
    {
        vec stepDirection(dir.x, dir.y, 0);
        stepDirection.z = -stepDirection.magnitude2() * verticalScale / horizontalScale;
        if (stepDirection.z == 0)
        {
            return false;
        }
        stepDirection.normalize();
        const vec previousPosition = vec(player->o);
        player->o.add(vec(stepDirection).mul(STAIRHEIGHT / fabs(stepDirection.z))).z -= STAIRHEIGHT;
        player->zmargin = -STAIRHEIGHT;
        if (collide(player, vec(0, 0, -1), SLOPEZ))
        {
            player->o = previousPosition;
            player->o.add(vec(stepDirection).mul(step));
            player->zmargin = 0;
            if (!collide(player, vec(0, 0, -1)))
            {
                vec stepFloor(stepDirection);
                stepFloor.mul(-stepFloor.z).z += 1;
                stepFloor.normalize();
                if (player->physstate >= PHYS_SLOPE && player->floor != stepFloor)
                {
                    // Prevent alternating step-down/step-up states if player would keep bumping into the same floor.
                    const vec stepPosition(player->o);
                    player->o.z -= 0.5f;
                    player->zmargin = -0.5f;
                    if (collide(player, stepDirection) && collidewall == player->floor)
                    {
                        player->o = previousPosition;
                        if (!isStepping)
                        {
                            player->o.x += dir.x;
                            player->o.y += dir.y;
                            if (dir.z <= 0 || collide(player, dir))
                            {
                                player->o.z += dir.z;
                            }
                        }
                        player->zmargin = 0;
                        player->physstate = PHYS_STEP_DOWN;
                        return true;
                    }
                    player->o = isStepping ? previousPosition : stepPosition;
                    player->zmargin = 0;
                }
                else if (isStepping)
                {
                    player->o = previousPosition;
                }
                switchfloor(player, dir, stepFloor);
                player->floor = stepFloor;
                if (isStepping)
                {
                    player->physstate = PHYS_STEP_DOWN;
                    player->timeinair = 0;
                }
                return true;
            }
        }
        player->o = previousPosition;
        player->zmargin = 0;
        return false;
    }

    static bool tryToStepDown(gameent* player, vec& dir, const bool isStepping = false)
    {
        const vec previousPosition(player->o);
        player->o.z -= 0.1f;
        if (collideinside || collide(player, vec(0, 0, -1)))
        {
            player->o = previousPosition;
            return false;
        }
        player->o.z -= STAIRHEIGHT;
        player->zmargin = -STAIRHEIGHT;
        if (!collide(player, vec(0, 0, -1), SLOPEZ))
        {
            player->o = previousPosition;
            player->zmargin = 0;
            return false;
        }
        player->o = previousPosition;
        player->zmargin = 0;

        // Stronger check to move down slopes smoothly, even if moving fast.
        const float step = dir.magnitude();
        if (retryToStepDown(player, dir, step, 2, 1, isStepping))
        {
            return true;
        }
        if (retryToStepDown(player, dir, step, 1, 1, isStepping))
        {
            return true;
        }
        if (retryToStepDown(player, dir, step, 1, 2, isStepping))
        {
            return true;
        }

        return false;
    }

    void fall(gameent* d, vec& dir, const vec& floor)
    {
        if (d->climbing)
        {
            d->timeinair = 0;
            d->physstate = PHYS_FLOOR;
        }
        if (floor.z > 0.0f && floor.z < SLOPEZ)
        {
            if (floor.z >= WALLZ) switchfloor(d, dir, floor);
            d->timeinair = 0;
            d->physstate = PHYS_SLIDE;
            d->floor = floor;
        }
        else if (d->physstate < PHYS_SLOPE || dir.dot(d->floor) > 0.01f * dir.magnitude() || (floor.z != 0.0f && floor.z != 1.0f) || !tryToStepDown(d, dir, true))
        {
            d->physstate = PHYS_FALL;
        }
    }

    void land(gameent* d, vec& dir, const vec& floor, bool collided)
    {
#if 0
        if (d->physstate == PHYS_FALL)
        {
            d->timeinair = 0;
            if (dir.z < 0.0f) dir.z = d->vel.z = 0.0f;
        }
#endif
        switchfloor(d, dir, floor);
        d->timeinair = 0;
        if ((d->physstate != PHYS_STEP_UP && d->physstate != PHYS_STEP_DOWN) || !collided)
        {
            if (floor.z >= FLOORZ)
            {
                if (floor.z > RAMPZ_MIN && floor.z <= RAMPZ_MAX)
                {
                    d->physstate = PHYS_RAMP;
                }
                else
                {
                    d->physstate = PHYS_FLOOR;
                }
            }
            else
            {
                d->physstate = PHYS_SLOPE;
            }
            
        }
        d->floor = floor;
    }

    bool findfloor(gameent* d, const vec& dir, bool collided, const vec& obstacle, bool& slide, vec& floor)
    {
        bool found = false;
        vec moved(d->o);
        d->o.z -= 0.1f;
        if (collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE || d->physstate == PHYS_STEP_DOWN ? SLOPEZ : FLOORZ))
        {
            if (d->physstate == PHYS_STEP_UP && d->floor != collidewall)
            {
                vec old(d->o), checkfloor(collidewall), checkdir = vec(dir).projectxydir(checkfloor).rescale(dir.magnitude());
                d->o.add(checkdir);
                if (!collide(d, checkdir))
                {
                    floor = checkfloor;
                    found = true;
                    goto foundfloor;
                }
                d->o = old;
            }
            else
            {
                floor = collidewall;
                found = true;
                goto foundfloor;
            }
        }
        if (collided && obstacle.z >= SLOPEZ)
        {
            floor = obstacle;
            found = true;
            slide = false;
        }
        else if (d->physstate == PHYS_STEP_UP || d->physstate == PHYS_SLIDE)
        {
            if (collide(d, vec(0, 0, -1)) && collidewall.z > 0.0f)
            {
                floor = collidewall;
                if (floor.z >= SLOPEZ) found = true;
            }
        }
        else if (d->physstate >= PHYS_SLOPE && d->floor.z < 1.0f)
        {
            if (collide(d, vec(d->floor).neg(), 0.95f) || collide(d, vec(0, 0, -1)))
            {
                floor = collidewall;
                if (floor.z >= SLOPEZ && floor.z < 1.0f) found = true;
            }
        }
        foundfloor:
        if (collided && (!found || obstacle.z > floor.z))
        {
            floor = obstacle;
            slide = !found && (floor.z < WALLZ || floor.z >= SLOPEZ);
        }
        d->o = moved;
        return found;
    }

    FVAR(crouchsmooth, 0, 0.1f, 1);
    FVAR(crouchsmoothfall, 0, 0.5f, 1);

    void crouch(gameent* player, const int steps)
    {
        if (curtime == 0)
        {
            return;
        }

        // Different smoothing for falling players, so we know we're crouch-jumping.
        const float smoothing = player->physstate == PHYS_FALL ? crouchsmoothfall : crouchsmooth;

        if (player->crouching < 0 || player->slide.isSliding()) // Crouching in.
        {
            const float minimumHeight = player->maxheight * CROUCH_HEIGHT;
            if (player->eyeheight > minimumHeight)
            {
                const float smoothDifference = (player->eyeheight - minimumHeight) * smoothing;
                player->eyeheight -= smoothDifference;

                // Update our position while airborne only if we are crouching in (crouch-jumps).
                if (player->physstate >= PHYS_FALL)
                {
                    player->o.z -= smoothDifference;
                    player->newpos.z -= smoothDifference;
                }
            }
        }
        else if (player->eyeheight < player->maxheight) // Standing up.
        {
            const float smoothDifference = (player->maxheight - player->eyeheight) * smoothing;
            player->eyeheight += smoothDifference;

            /*
                Avoid jerks while airborne (we are already moving vertically),
                We are updating our vertical position once we land.
            */
            if (player->physstate > PHYS_FALL)
            {
                player->o.z += smoothDifference;
                player->newpos.z += smoothDifference;
            }

            player->crouching = 0;
            const float smoothSteps = smoothDifference / steps;
            loopi(steps)
            {
                const bool isColliding = collide(player, vec(0, 0, player->physstate <= PHYS_FALL ? -1 : 1), 0, true);
                if (!isColliding)
                {
                    // We are under an obstacle, can't stand up.
                    break;
                }
                player->crouching = 1;
                player->eyeheight -= smoothSteps;
                if (player->physstate > PHYS_FALL)
                {
                    player->o.z -= smoothSteps;
                    player->newpos.z -= smoothSteps;
                }
            }
        }
    }

    namespace physics
    {
        struct SlideInfo
        {
            enum State
            {
                Idle = 0,
                Queued,
                Sliding
            };

            static constexpr int duration = 450;
            static constexpr int cooldown = 230;
            static constexpr int reductionWindow = 1200;
            static constexpr float minimumVelocity = 30.0f;
            static constexpr float stopVelocity = 80.0f;

            State state = Idle;
            int startTime = 0;
            int endTime = 0;
            float reduction = 1.0f;
            float progress = 0;

            void start(gameent* player, const int time);
            void stop(gameent* player, const int time);

            void reset() noexcept
            {
                state = Idle;
                startTime = 0;
                endTime = 0;
                progress = 0;
                resetReduction();
            }

            void queue(gameent* player, const int time) noexcept
            {
                if (!canQueue(player, time))
                {
                    return;
                }
                state = Queued;
                startTime = 0;
            }

            void dequeue() noexcept
            {
                state = Idle;
                startTime = 0;
            }

            void resetReduction()
            {
                reduction = 1.0f;
            }

            // Apply reduction for consecutive slides.
            void applyReduction(const int time) noexcept
            {
                reduction = clamp(reduction - 0.25f, 0.0f, 1.0f);
                conoutf(CON_GAMEINFO, "reduction: %.2f", reduction);
            }

            void checkReduction(const int time) noexcept
            {
                if (hasDelay(time, reductionWindow))
                {
                    return;
                }
                resetReduction();
            }

            bool evaluate(gameent* player, const int time);
            bool canQueue(gameent* player, const int time);
            bool canStart(gameent* player, const int time);

            bool isIdle() const noexcept
            {
                return state == Idle;
            }

            bool isQueued() const noexcept
            {
                return state == Queued;
            }

            bool isSliding() const noexcept
            {
                return state == Sliding;
            }

            bool hasDelay(const int time, const int delay) const noexcept
            {
                return endTime > 0 && time - endTime <= delay;
            }
        };
    }

    bool SlideInfo::canQueue(gameent* player, const int time)
    {
        if (hasDelay(time, cooldown) || state == Sliding || state == Queued || reduction <= 0)
        {
            return false;
        }

        // Check what the player is doing before queueing.
        if (player->zooming || player->blocked || player->inwater || player->climbing)
        {
            return false;
        }

        // Check the minimum velocity to start sliding.
        if (player->move <= 0 || player->vel.magnitude() <= minimumVelocity)
        {
            return false;
        }

        return true;
    }

    bool SlideInfo::canStart(gameent* player, const int time)
    {
        if (hasDelay(time, cooldown) || state == Sliding || !player->onfloor())
        {
            return false;
        }
        return true;
    }

    void SlideInfo::start(gameent* player, const int time)
    {
        if (hasDelay(time, cooldown))
        {
            return;
        }
        state = Sliding;
        startTime = time;

        // Player crouches when starting a slide.
        player->crouching = -1;

        const float slideVelocity = 100.0f * reduction;
        if (slideVelocity <= 0)
        {
            stop(player, time);
            return;
        }

        // Apply velocity boost.
        float velocity = player->vel.magnitude() + slideVelocity;
        if (velocity <= slideVelocity)
        {
            velocity *= 1.5f;
        }
        vec direction(player->vel.x, player->vel.y, 0);
        vec floorNormal = player->floor;
        direction.sub(floorNormal.mul(direction.dot(floorNormal)));
        if (direction.magnitude() > 0)
        {
            direction.normalize();
            player->vel = direction.mul(velocity);
        }

        // Physics event.
        triggerPlayerPhysicsEvent(player, PhysEvent_CrouchSlide);
    }

    void SlideInfo::stop(gameent* player, const int time)
    {
        state = Idle;
        endTime = time;
        startTime = 0;

        // Player stands up after a slide.
        player->crouching = abs(player->crouching);

        // Velocity reduction for consecutive slides.
        applyReduction(time);

        // Physics event.
        triggerPlayerPhysicsEvent(player, PhysEvent_CrouchSlideStop);
    }

    // Called each physics frame to update sliding state.
    bool SlideInfo::evaluate(gameent* player, const int time)
    {
        if (state == Queued && canStart(player, time))
        {
            // If slide is queued and ready to start.
            start(player, time);
            return true;
        }
        else if (state == Sliding) // Stop sliding if player no longer meets certain conditions.
        {
            if (!player->onfloor() || player->inwater || player->blocked)
            {
                stop(player, time);
                return false;
            }

            // Stop if too slow!
            if (player->vel.magnitude() <= stopVelocity)
            {
                stop(player, time);
                return false;
            }

            // If the velocity is enough, keep sliding on ramps, otherwise stop when the slide is usually finished.
            const bool isEncouraged = player->physstate == PHYS_RAMP || player->physstate == PHYS_STEP_UP || player->physstate == PHYS_STEP_DOWN;
            const bool isStarted = startTime > 0;
            const bool shouldKeepSliding = isEncouraged && isStarted;
            if (!shouldKeepSliding && time - startTime > duration)
            {
                stop(player, time);
                return false;
            }

            // Continue sliding.
            return true;
        }
        checkReduction(time);
        return false;
    }

    bool ismoving(gameent* d, vec& dir)
    {
        vec old(d->o);
        bool collided = false, slidecollide = false;
        vec obstacle(0, 0, 0);
        d->o.add(dir);
        if (collide(d, dir) || (d->type == ENT_AI && collide(d, vec(0, 0, 0), 0, false)))
        {
            obstacle = collidewall;
            // Check to see if there is an obstacle that would prevent this one from being used as a floor (or ceiling bump).
            if (d->type == ENT_PLAYER && ((collidewall.z >= SLOPEZ && dir.z < 0) || (collidewall.z <= -SLOPEZ && dir.z > 0)) && (dir.x || dir.y) && collide(d, vec(dir.x, dir.y, 0)))
            {
                if (collidewall.dot(dir) >= 0) slidecollide = true;
                obstacle = collidewall;
            }
            d->o = old;
            d->o.z -= STAIRHEIGHT;
            d->zmargin = -STAIRHEIGHT;
            const bool hasFloor = d->physstate == PHYS_SLOPE || d->physstate == PHYS_RAMP || d->physstate == PHYS_FLOOR;
            if (hasFloor || (collide(d, vec(0, 0, -1), SLOPEZ) && (d->physstate == PHYS_STEP_UP || d->physstate == PHYS_STEP_DOWN || collidewall.z >= FLOORZ)))
            {
                d->o = old;
                d->zmargin = 0;
                if (trystepup(d, dir, obstacle, STAIRHEIGHT, hasFloor ? d->floor : vec(collidewall))) return true;
            }
            else
            {
                d->o = old;
                d->zmargin = 0;
            }
            // Can't step over the obstacle, so just slide against it.
            collided = true;
        }
        else if (d->physstate == PHYS_STEP_UP)
        {
            if (collide(d, vec(0, 0, -1), SLOPEZ))
            {
                d->o = old;
                if (trystepup(d, dir, vec(0, 0, 1), STAIRHEIGHT, vec(collidewall))) return true;
                d->o.add(dir);
            }
        }
        else if (d->physstate == PHYS_STEP_DOWN && dir.dot(d->floor) <= 1e-6f)
        {
            vec moved(d->o);
            d->o = old;
            if (tryToStepDown(d, dir))
            {
                return true;
            }
            d->o = moved;
        }
        vec floor(0, 0, 0);
        bool slide = collided,
            found = findfloor(d, dir, collided, obstacle, slide, floor);
        if (slide || (!collided && floor.z > 0 && floor.z < WALLZ))
        {
            slideagainst(d, dir, slide ? obstacle : floor, found, slidecollide);
            if (!d->climbing)
            {
                d->blocked = true;
            }
        }
        if (found) land(d, dir, floor, collided);
        else fall(d, dir, floor);
        return !collided;
    }

    // TO-DO: Move to sound file?
    static void updateSounds()
    {
        loopv(players)
        {
            gameent* player = players[i];
            if (player->slide.isSliding())
            {
                player->playchannelsound(Chan_Slide, S_SLIDE_LOOP, 200, true);
            }
            else if (validsound(player->chan[Chan_Slide]))
            {
                player->stopchannelsound(Chan_Slide, 280);
            }
        }
    }

#define PHYSFRAMETIME 8

    int physsteps = 0, physframetime = PHYSFRAMETIME, lastphysframe = 0;

    void physicsframe() // Optimally schedule physics frames inside the graphics frames.
    {
        int diff = lastmillis - lastphysframe;
        if (diff <= 0) physsteps = 0;
        else
        {
            physframetime = clamp(game::scaletime(PHYSFRAMETIME) / 100, 1, PHYSFRAMETIME);
            physsteps = (diff + physframetime - 1) / physframetime;
            lastphysframe += physsteps * physframetime;
        }
        cleardynentcache();
        updateSounds();
    }

    VAR(physinterp, 0, 1, 1);

    void interpolateposition(physent* d)
    {
        d->o = d->newpos;

        int diff = lastphysframe - lastmillis;
        if (diff <= 0 || !physinterp) return;

        vec deltapos(d->deltapos);
        deltapos.mul(min(diff, physframetime) / float(physframetime));
        d->o.add(deltapos);
    }

    void updatephysstate(gameent* d)
    {
        if (d->physstate == PHYS_FALL && !d->climbing) return;
        d->timeinair = 0;
        vec old(d->o);

        /* Attempt to reconstruct the floor state.
         * May be inaccurate since movement collisions are not considered.
         * If good floor is not found, just keep the old floor and hope it's correct enough.
         */
        switch (d->physstate)
        {
            case PHYS_SLOPE:
            case PHYS_FLOOR:
            case PHYS_RAMP:
            case PHYS_STEP_DOWN:
            {
                d->o.z -= 0.15f;
                if (collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE || d->physstate == PHYS_STEP_DOWN ? SLOPEZ : FLOORZ))
                {
                    d->floor = collidewall;
                }
                break;
            }

            case PHYS_STEP_UP:
            {
                d->o.z -= STAIRHEIGHT + 0.15f;
                if (collide(d, vec(0, 0, -1), SLOPEZ))
                {
                    d->floor = collidewall;
                }
                break;
            }

            case PHYS_SLIDE:
            {
                d->o.z -= 0.15f;
                if (collide(d, vec(0, 0, -1)) && collidewall.z < SLOPEZ)
                {
                    d->floor = collidewall;
                }
                break;
            }
        }
        if (d->physstate > PHYS_FALL && d->floor.z <= 0) d->floor = vec(0, 0, 1);
        d->o = old;
    }

    float calculatespeed(gameent* d)
    {
        float speed = d->speed;

        if (d->haspowerup(PU_AGILITY))
        {
            speed += ((float)d->powerupmillis) / 1000.0f;
        }
        else if (d->role == ROLE_BERSERKER)
        {
            speed += 10.0f; // Speed bonus.
        }
        return speed;
    }

    inline bool canjump(gameent* d)
    {
        return d->onfloor() ||
               ((d->haspowerup(PU_AGILITY) || d->role == ROLE_ZOMBIE || d->role == ROLE_BERSERKER) && !d->doublejumping);
    }

    namespace
    {
        constexpr float VELOCITY_JUMP = 135.0f;
        constexpr float VELOCITY_CROUCH = 0.4f;
        constexpr float VELOCITY_ZOOM = 0.5f;
        constexpr float VELOCITY_CLIMB = 0.7f;
        constexpr float VELOCITY_SLIDE = 100.0f;
        constexpr float VELOCITY_WATER_DAMP = 8.0f;
        constexpr float VELOCITY_SPRINT = 1.2f;
        constexpr float VELOCITY_WALK = 0.9f;
        constexpr float VELOCITY_JUMP_STRAFE = 1.1f;
    }

    VAR(floatspeed, 1, 100, 10000);

    static int checkMaterial(gameent* player)
    {
        const vec playerPosition = vec(player->o.x, player->o.y, player->o.z + (3 * player->aboveeye - player->eyeheight) / 4);
        return lookupmaterial(playerPosition);
    }

    bool allowVerticalMovement(gameent* player)
    {
        return player->floating() || isliquidmaterial(checkMaterial(player) & MATF_VOLUME) || player->climbing || player->type == ENT_CAMERA;
    }

    void modifyvelocity(gameent* d, bool local, bool isinwater, bool isfloating, int curtime)
    {
        if (isfloating)
        {
            if (d->jumping)
            {
                d->vel.z = max(d->vel.z, static_cast<float>(floatspeed));
            }
            if (d->crouching < 0)
            {
                d->vel.z = min(d->vel.z, static_cast<float>(-floatspeed));
            }
        }
        else if (canjump(d) || isinwater)
        {
            if (isinwater && !d->inwater)
            {
                d->vel.div(VELOCITY_WATER_DAMP);
            }
            if (d->jumping)
            {
                d->jumping = false;
                if (d->timeinair)
                {
                    d->doublejumping = true;
                    d->lastfootright = d->rfoot;
                    d->lastfootleft = d->lfoot;
                    d->falling.z = 1;
                }
                d->vel.z = max(d->vel.z, VELOCITY_JUMP); // Physics impulse upwards.
                if (isinwater)
                {
                    // Dampen velocity change even harder, gives a decent water feel.
                    d->vel.x /= VELOCITY_WATER_DAMP;
                    d->vel.y /= VELOCITY_WATER_DAMP;
                }

                triggerPlayerPhysicsEvent(d, PhysEvent_Jump, true, d->inwater);
            }
            else if (d->crouching < 0 && isinwater)
            {
                d->vel.z = min(d->vel.z, -VELOCITY_JUMP / VELOCITY_WATER_DAMP);
            }
        }
        if (!isfloating && !d->climbing && d->physstate == PHYS_FALL) d->timeinair += curtime;

        vec m(0.0f, 0.0f, 0.0f);
        if ((d->move || d->strafe))
        {
            vecfromyawpitch(d->yaw, allowVerticalMovement(d) ? d->pitch : 0, d->move, d->strafe, m);

            if (!isfloating && d->physstate >= PHYS_SLOPE)
            {
                /* Move up or down slopes in air,
                 * but only move up slopes in water.
                 */
                float dz = -(m.x * d->floor.x + m.y * d->floor.y) / d->floor.z;
                m.z = isinwater ? max(m.z, dz) : dz;
            }
            if (!m.iszero())
            {
                if (d->climbing && !d->crouching)
                {
                    m.addz(m.z >= 0 ? 1 : -1).normalize();
                }
                else
                {
                    m.normalize();
                }
            }
        }

        vec dir(m);
        dir.mul(calculatespeed(d));
        if (d->type == ENT_PLAYER)
        {
            if (isfloating)
            {
                if (d == self) dir.mul(floatspeed / 100.0f);
            }
            else if (d->crouching && d->onfloor())
            {
                dir.mul(VELOCITY_CROUCH);
            }
            else if (d->zooming)
            {
                dir.mul(VELOCITY_ZOOM);
            }
            else if (d->climbing)
            {
                dir.mul(VELOCITY_CLIMB);
            }
            else if (!isinwater)
            {
                const float airVelocity = d->strafe == 0 ? VELOCITY_SPRINT : VELOCITY_JUMP_STRAFE;
                const float multiplier = d->onfloor() ? VELOCITY_WALK : airVelocity;
                const bool isSprinting = d->move > 0;
                dir.mul((isSprinting ? VELOCITY_SPRINT : VELOCITY_WALK) * multiplier);
            }
        }
        // Calculate and apply friction.
        float friction = 25.0f;
        if (isinwater && !d->climbing && !isfloating)
        {
            friction = 20.0f;
        }
        else if (d->onfloor() || isfloating)
        {
            friction = d->slide.isSliding() ? 23.0f : 4.0f;
        }
        if (!(d->physstate == PHYS_RAMP && d->slide.isSliding()))
        {
            d->vel.lerp(dir, d->vel, pow(1 - 1 / friction, curtime / 20.0f));
        }
    }

    FVARR(mapgravity, 0, 195.0f, 250.0f);

    void modifygravity(gameent* d, bool isinwater, int curtime)
    {
        float secs = curtime / 1000.0f;
        vec g(0, 0, 0);
        if (d->physstate == PHYS_FALL) g.z -= mapgravity * secs;
        else if (d->floor.z > 0 && d->floor.z < FLOORZ)
        {
            g.z = -1;
            g.project(d->floor);
            g.normalize();
            g.mul(mapgravity * secs);
        }
        if (!isinwater || (!d->move && !d->strafe)) d->falling.add(g);

        if (isinwater || d->physstate >= PHYS_SLOPE)
        {
            float fric = isinwater ? 2.0f : 6.0f;
            float c = isinwater ? 1.0f : clamp((d->floor.z - SLOPEZ) / (FLOORZ - SLOPEZ), 0.0f, 1.0f);
            d->falling.mul(pow(1 - c / fric, curtime / 20.0f));
        }
    }

    /* Main physics routine, moves a player/monster for a "curtime" step.
     * "moveres" indicated the physics precision (which is lower for monsters and multiplayer prediction).
     * "local" is false for multiplayer prediction.
     */

    void detectcollisions(gameent* d, int moveres, vec& dir)
    {
        const float f = 1.0f / moveres;
        int collisions = 0;
        dir.mul(f);
        loopi(moveres)
        {
            if (!ismoving(d, dir) && ++collisions < 5)
            {
                i--; // Discrete steps collision detection and sliding.
            }
        }
    }

    int liquidtransition(physent* d, int material, bool isinwater)
    {
        int transition = LiquidTransition_None;
        if (!d->inwater && isinwater)
        {
            transition = LiquidTransition_In;
        }
        else if (d->inwater && !isinwater)
        {
            transition = LiquidTransition_Out;
        }
        d->inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;
        return transition;
    }

    void handleliquidtransitions(gameent* d, int& material, bool& isinwater)
    {
        if (d->inwater && !isinwater)
        {
            material = lookupmaterial(vec(d->o.x, d->o.y, d->o.z + (d->aboveeye - d->eyeheight) / 2));
            isinwater = isliquidmaterial(material & MATF_VOLUME);
        }
        int transition = liquidtransition(d, material, isinwater);
        if (transition == LiquidTransition_In)
        {
            triggerPlayerPhysicsEvent(d, PhysEvent_LiquidIn, true, material & MATF_VOLUME);
        }
        else if (transition == LiquidTransition_Out)
        {
            triggerPlayerPhysicsEvent(d, PhysEvent_LiquidOut, true, d->inwater);
        }
    }

    static const float ROLL_MAX = 20.0f;

    VARP(rolleffect, 0, 1, 1);
    FVAR(rollstrafemax, 0, 1, ROLL_MAX);
    FVAR(rollstrafe, 0, 0.018f, 90);
    FVAR(rollfade, 0, 0.9f, 1);
    FVAR(rollonland, -ROLL_MAX, 2.5f, ROLL_MAX);
    FVARP(rollslide, -10, 2, 10);
    VARP(rollslidefade, 1, 400, 1000);

    void addroll(gameent* player, float amount)
    {
        if (!rolleffect || !player)
        {
            return;
        }
        const int rollDelay = 500;
        if (player->lastroll && lastmillis - player->lastroll < rollDelay)
        {
            return;
        }
        float strafingRollAmount = clamp(player->roll + amount, -ROLL_MAX, ROLL_MAX);
        const float zoomFactor = clamp(2.0f * camera::camera.zoomstate.progress, 1.0f, 2.0f);
        strafingRollAmount *= zoomFactor;
        player->roll += player->roll > 0 ? -strafingRollAmount : (player->roll < 0 ? strafingRollAmount : (rnd(2) ? amount : -amount));
        player->lastroll = lastmillis;
    }

    // Automatically apply smooth roll when strafing or sliding.
    void updateRoll(gameent* player)
    {
        if (rolleffect == 0)
        {
            if (player->roll > 0)
            {
                player->roll = 0;
            }
            return;
        }
        if ((player->strafe && rollstrafemax || player->slide.isSliding()) && !player->floating())
        {
            const float zoomFactor = 2.0f * camera::camera.zoomstate.progress;
            float baseRoll = rollstrafemax * clamp(zoomFactor, 1.0f, 2.0f);
            if (rollslide > 0)
            {
                float& slideProgress = player->slide.progress;
                if (player->slide.isSliding())
                {
                    slideProgress = min(slideProgress + float(curtime) / rollslidefade, 1.0f);
                }
                else
                {
                    slideProgress = max(slideProgress - float(curtime) / rollslidefade, 0.0f);
                }
                baseRoll += rollslide * slideProgress;
            }
            baseRoll = clamp(baseRoll, -ROLL_MAX, ROLL_MAX);
            const float time = curtime * rollstrafe;
            const float finalRoll = player->roll - pow(clamp(1.0f + player->strafe * player->roll / baseRoll, 0.0f, 1.0f), 0.33f) * (player->strafe ? player->strafe : 1) * time;
            player->roll = clamp(finalRoll, -baseRoll, baseRoll);

            // Done.
            return;
        }
        player->roll *= curtime == PHYSFRAMETIME ? rollfade : pow(rollfade, curtime / float(PHYSFRAMETIME));
    }

    const int SHORT_JUMP_THRESHOLD = 350;
    const int LONG_JUMP_THRESHOLD = 800;

    bool isplayermoving(gameent* d, int moveres, bool local, int curtime)
    {
        if (!canmove(d))
        {
            return false;
        }
        int material = checkMaterial(d);
        bool isinwater = isliquidmaterial(material & MATF_VOLUME);
        float secs = curtime / 1000.f;
        if (!d->floating() && !d->climbing)
        {
            modifygravity(d, isinwater, curtime); // Apply gravity.
        }
        modifyvelocity(d, local, isinwater, d->floating(), curtime); // Apply any player generated changes in velocity.

        vec dir(d->vel);
        if (!d->floating() && isinwater)
        {
            dir.mul(0.5f);
        }
        dir.add(d->falling);
        dir.mul(secs);

        d->blocked = false;

        if (d->floating())
        {
            if (d->ghost)
            {
                detectcollisions(d, moveres, dir);
            }
            if (d->physstate != PHYS_FLOAT)
            {
                d->physstate = PHYS_FLOAT;
                d->timeinair = 0;
                d->falling = vec(0, 0, 0);
            }
            d->o.add(dir);
        }
        else
        {
            const int timeinair = d->timeinair; // Use a constant that represents the maximum amount of time the player has spent in air.
            detectcollisions(d, moveres, dir);
            if (!d->timeinair && !isinwater) // Player is currently not in air nor swimming.
            {
                d->slide.evaluate(d, lastmillis);

                // Now that we landed, we can double jump again.
                d->doublejumping = false;

                // Make a landing sound.
                if (!d->zooming)
                {
                    // Short jump.
                    if (timeinair > SHORT_JUMP_THRESHOLD && timeinair < LONG_JUMP_THRESHOLD)
                    {
                        triggerPlayerPhysicsEvent(d, PhysEvent_LandLight, true, material & MATF_VOLUME);
                    }

                    // Must be a big jump, heavy landing.
                    else if (timeinair >= LONG_JUMP_THRESHOLD)
                    {
                        triggerPlayerPhysicsEvent(d, PhysEvent_LandHeavy, true, material & MATF_VOLUME);
                    }
                }
                else if (timeinair > SHORT_JUMP_THRESHOLD)
                {
                    // Scoped landing is always heavy.
                    triggerPlayerPhysicsEvent(d, PhysEvent_LandHeavy, true, material & MATF_VOLUME);
                }

                triggerPlayerPhysicsEvent(d, PhysEvent_Footstep, true, material & MATF_VOLUME);
            }
        }

        updateRoll(d);

        if (d->state == CS_ALIVE) updatedynentcache(d);

        // Handle transitions for entering and exiting liquid materials.
        handleliquidtransitions(d, material, isinwater);

        if (d->state == CS_ALIVE)
        {
            if (d->o.z < 0 || material & MAT_DEATH)
            {
                game::suicide(d); // Kill the player if inside death material or outside of the map (below origin).
            }
            else
            {
                const int checkFeetMaterial = lookupmaterial(d->feetpos());
                if (material & MAT_DAMAGE || checkFeetMaterial & MAT_DAMAGE || (checkFeetMaterial & MATF_VOLUME) == MAT_LAVA)
                {
                    game::hurt(d); // Harm the player if their feet or body are inside harmful materials.
                }
                if (checkFeetMaterial & MAT_CLIMB)
                {
                    // Enable ladder-like movement.
                    if (!d->climbing)
                    {
                        d->climbing = true;
                        d->falling = d->vel = vec(0, 0, 0);
                    }
                }
                else if (d->climbing)
                {
                    d->climbing = false;
                }
            }
        }
        else if (d->climbing)
        {
            d->climbing = false;
        }

        return true;
    }

    void moveplayer(gameent *d, int moveres, bool local)
    {
        if (physsteps <= 0)
        {
            if (local) interpolateposition(d);
            return;
        }

        if (local) d->o = d->newpos;
        loopi(physsteps - 1) isplayermoving(d, moveres, local, physframetime);
        if (local) d->deltapos = d->o;
        isplayermoving(d, moveres, local, physframetime);
        if (local)
        {
            d->newpos = d->o;
            d->deltapos.sub(d->newpos);
            interpolateposition(d);
        }
    }

    static void triggerLiquidEffects(const gameent* player, const int material)
    {
        if (player == nullptr || material == MAT_LAVA)
        {
            return;
        }
        particle_splash(PART_WATER, 200, 250, player->o, 0xFFFFFF, 0.09f, 800, 1);
        particle_splash(PART_SPLASH, 10, 100, player->o, 0xFFFFFF, 10.0f, 500, -1);
    }

    VARP(footstepsounds, 0, 1, 1);
    VARP(footstepdelay, 2500, 2700, 5000);

    static void playFootstepSounds(gameent* player, const int sound, const bool shouldPlayCrouchFootsteps = true)
    {
        // Conditions to reduce unnecessary checks.
        if (!footstepsounds || player == nullptr || !player->onfloor() || player->slide.isSliding() || (player == self && player->blocked))
        {
            return;
        }
        const bool isCrouching = player->crouching && player->crouched();
        const bool isMoving = player->move || player->strafe;
        if (!isMoving || (!shouldPlayCrouchFootsteps && isCrouching))
        {
            return;
        }

        const float lowest = min(player->lfoot.z, player->rfoot.z);
        const float velocity = max(player->vel.magnitude(), 1.0f);
        const float delay = (footstepdelay / velocity) * (1.0f + fabs(lowest - player->o.z));
        if (lastmillis - player->lastfootstep < delay)
        {
            return;
        }
        playsound(sound, player);
        player->lastfootstep = lastmillis;
    }

    struct FootstepInfo
    {
        int sound = S_FOOTSTEP;
        bool hasCrouchFootsteps = false;
    };

    static FootstepInfo getFootstepSound(const gameent* player)
    {
        const FootstepInfo default = { S_FOOTSTEP, false };
        if (player == nullptr)
        {
            return default;
        }
        vec footPosition = player->feetpos();
        int material = lookupmaterial(footPosition);
        FootstepInfo foot;
        if ((material & MATF_VOLUME) == MAT_WATER)
        {
            foot.sound = S_FOOTSTEP_WATER;
            foot.hasCrouchFootsteps = true;
        }
        else
        {
            // Add offset to check for texture/material properties below feet level.
            footPosition = player->feetpos(-1);
            material = lookupmaterial(footPosition);
            if ((material & MATF_VOLUME) == MAT_GLASS)
            {
                foot.sound = S_FOOTSTEP_GLASS;
                foot.hasCrouchFootsteps = false;
            }
            else
            {
                const int texture = lookuptextureeffect(footPosition);
                foot.sound = textureeffects[texture].footstepsound;
                foot.hasCrouchFootsteps = textureeffects[texture].hascrouchfootsteps;
            }
        }
        return foot;
    }

    static void triggerFootsteps(gameent* player, bool islanding)
    {
        const FootstepInfo foot = getFootstepSound(player);
        if (islanding)
        {
            // Just send the landing sound effect (single footstep).
            playsound(foot.sound, player);
        }
        else
        {
            // Manage additional conditions and timing for walking sounds.
            playFootstepSounds(player, foot.sound, foot.hasCrouchFootsteps);
        }
    }

    void triggerPlayerPhysicsEvent(physent* pl, const int event, const bool isLocal, const int material, const vec& origin)
    {
        if (pl == nullptr || pl->type != ENT_PLAYER || pl->state > CS_DEAD)
        {
            return;
        }
        gameent* player = (gameent*)pl;
        if (player != self && !player->ai && isLocal)
        {
            return;
        }
        gameent* hudPlayer = followingplayer(self);
        switch (event)
        {
            case PhysEvent_Jump:
                if (material == MAT_WATER)
                {
                    // Return so we do not send this physics event to other clients.
                    return;
                }
                playsound(S_JUMP1, player);
                if (player == hudPlayer)
                {
                    sway.addevent(player, SwayEvent_Jump, 380, -1.2f);
                }
                break;

            case PhysEvent_LandLight:
            case PhysEvent_Footstep:
                if (event == PhysEvent_LandLight)
                {
                    triggerFootsteps(player, true);
                    if (player == hudPlayer)
                    {
                        sway.addevent(player, SwayEvent_Land, 360, -2.1f);
                        camera::camera.addevent(player, camera::CameraEvent_Land, 120, -1.0f);
                    }
                }
                else
                {
                    triggerFootsteps(player, false);
                }
                player->lastfootleft = player->lastfootright = vec(-1, -1, -1);
                break;

            case PhysEvent_LandHeavy:
                playsound(material == MAT_WATER ? S_LAND_WATER : S_LAND, player);
                if (player == hudPlayer)
                {
                    sway.addevent(player, SwayEvent_LandHeavy, 380, -2 * (player->slide.isQueued() ? 0.5f : 1.0f));
                    camera::camera.addevent(player, camera::CameraEvent_Land, 100, -1.5f);
                    addroll(player, rollonland);
                }
                player->lastfootleft = player->lastfootright = vec(-1, -1, -1);
                break;

            case PhysEvent_CrouchIn:
            case PhysEvent_CrouchOut:
            {
                if (player->slide.isSliding())
                {
                    break;
                }
                static int lastCrouch = 0;
                const int delay = 200;
                if (lastmillis - lastCrouch < delay)
                {
                    // Return so we do not send this physics event to other clients.
                    return;
                }
                if (event == PhysEvent_CrouchIn)
                {
                    sway.addevent(player, SwayEvent_Crouch, 350, -2);
                    playsound(S_CROUCH, player);
                }
                else
                {
                    sway.addevent(player, SwayEvent_Crouch, 380, -3);
                }
                lastCrouch = lastmillis;
                break;
            }

            case PhysEvent_CrouchSlide:
                sway.addevent(player, SwayEvent_Crouch, player->slide.duration, -1.75f);
                playsound(S_SLIDE, player);
                if (!isLocal)
                {
                    player->slide.start(player, lastmillis);
                }
                break;

            case PhysEvent_CrouchSlideStop:
            {
                if (!isLocal)
                {
                    player->slide.reset();
                }
                break;
            }

            case PhysEvent_RagdollCollide:
            {
                playsound(S_CORPSE, NULL, origin.iszero() ? (player == self ? NULL : &player->o) : &origin);

                /*
                    Return so we do not send this physics event to other clients.
                    This to prevent inaccurate ragdoll collision sounds as the ragdoll physics may not be fully deterministic.
                */
                return;
            }

            case PhysEvent_LiquidIn:
                playsound(material == MAT_LAVA ? S_LAVA_IN : S_WATER_IN, NULL, origin.iszero() ? (player == self ? NULL : &player->o) : &origin);
                triggerLiquidEffects(player, material);
                break;

            case PhysEvent_LiquidOut:
                if (material == MAT_LAVA)
                {
                    // Return so we do not send this physics event to other clients.
                    return;
                }
                playsound(S_WATER_OUT, NULL, origin.iszero() ? (player == self ? NULL : &player->o) : &origin);
                triggerLiquidEffects(player, material);
                break;

            default:
                break;
        }

        // Send physics event to other clients.
        if (isLocal)
        {
            addmsg(N_PHYSICSEVENT, "rcii", player, event, material);
        }
    }

    void collidewithdynamicentity(physent* d, physent* o, const vec& dir)
    {
        if (d->type == ENT_AI)
        {
            if (dir.z > 0) stackmonster((monster*)d, o);
        }
    }

    static void moveBackward(int down)
    {
        self->k_down = down != 0;
        self->move = self->k_down ? -1 : (self->k_up ? 1 : 0);
    }
    ICOMMAND(backward, "D", (int* down),
    {
        moveBackward(*down);
    });

    static void moveForward(int down)
    {
        self->k_up = down != 0;
        self->move = self->k_up ? 1 : (self->k_down ? -1 : 0);
    }
    ICOMMAND(forward, "D", (int* down),
    {
        moveForward(*down);
    });

    static void moveLeft(int down)
    {
        self->k_left = down != 0;
        self->strafe = self->k_left ? 1 : (self->k_right ? -1 : 0);
    }
    ICOMMAND(left, "D", (int* down),
    {
        moveLeft(*down);
    });

    static void moveRight(int down)
    {
        self->k_right = down != 0;
        self->strafe = self->k_right ? -1 : (self->k_left ? 1 : 0);
    }
    ICOMMAND(right, "D", (int* down),
    {
        moveRight(*down);
    });

    static bool canJump()
    {
        if (!connected || mainmenu || intermission)
        {
            return false;
        }
        respawn();
        return self->state != CS_DEAD;
    }

    static void doJump(int down)
    {
        if (!down || canJump())
        {
            if (self->physstate > PHYS_FLOAT && self->crouched() && !self->slide.isSliding())
            {
                self->crouching = 0;
            }
            else
            {
                self->jumping = down != 0;
            }
        }
    }
    ICOMMAND(jump, "D", (int* down),
    {
        doJump(*down);
    });

    static bool canCrouch()
    {
        if (!connected || mainmenu || intermission)
        {
            return false;
        }
        return self->state != CS_DEAD;
    }

    static bool canSlide()
    {
        return !self->zooming && !validact(self->attacking);
    }

    VARP(crouchtoggle, 0, 0, 1);

    static void doCrouch(const bool isPressed)
    {
        static int lastPress = 0;
        if (!isPressed)
        {
            // Reset the crouching state when the button is released.
            if (!crouchtoggle && !self->slide.isQueued())
            {
                self->crouching = abs(self->crouching);
            }
        }
        else if (canCrouch())
        {
            const int sinceLastPress = lastmillis - lastPress;

            // Check for double-taps.
            const bool isDoubleTap = sinceLastPress < 250;
            if (isDoubleTap && canSlide())
            {
                // Mark slide as queued.
                self->slide.queue(self, lastmillis);
            }
            else
            {
                // Reset slide queue if not a double-tap.
                self->slide.dequeue();
            }
            lastPress = lastmillis;

            if (!isDoubleTap)
            {
                if (crouchtoggle)
                {
                    self->crouching = (self->crouching < 0) ? abs(self->crouching) : -1;
                }
                else
                {
                    self->crouching = -1;
                }
            }
        }
    }
    ICOMMAND(crouch, "D", (int* down),
    {
        doCrouch(*down != 0);
    });

    /*
     * Simulates projectile physics, including the effects of gravity and friction.
     */
    void modifyprojectilevelocity(ProjEnt* proj, const float secs, float waterFriction, const float gravity)
    {
        const int material = lookupmaterial(vec(proj->o.x, proj->o.y, proj->o.z + (proj->aboveeye - proj->eyeheight) / 2));
        const bool isInWater = isliquidmaterial(material & MATF_VOLUME);
        if (isInWater)
        {
            proj->vel.z -= gravity * mapgravity / 16 * secs;
            proj->vel.mul(max(1.0f - secs / waterFriction, 0.0f));
        }
        else
        {
            proj->vel.z -= gravity * mapgravity * secs;
        }
    }

    /*
     * Manages the collision detection and bouncing physics of a projectile.
     * Returns true when a collision occurs, which typically triggers detonation
     * or other projectile-specific actions.
     */
    bool hasbounced(ProjEnt* proj, const float secs, const float elasticity, float waterFriction, const float gravity)
    {
        if (proj->physstate != PHYS_BOUNCE && collide(proj, vec(0, 0, 0), 0, false))
        {
            return true;
        }
        modifyprojectilevelocity(proj, secs, waterFriction, gravity);
        const vec old(proj->o);
        const bool isDetonatingOnImpact = proj->flags & ProjFlag_Impact;
        loopi(2)
        {
            vec dir(proj->vel);
            dir.mul(secs);
            proj->o.add(dir);
            if (!collide(proj, dir, 0, true, true))
            {
                if (collideinside)
                {
                    // Projectile is rejected in case it enters solid geometry.
                    proj->o = old;
                    proj->vel.mul(-elasticity);
                    if (isDetonatingOnImpact)
                    {
                        // Detonates on impact.
                        return true;
                    }
                }
                break;
            }
            else if (collideplayer)
            {
                /* Projectile collided with a living entity,
                 * may it be a player, NPC or another projectile.
                 */
                projectiles::collidewithentity(proj, collideplayer);
                break;
            }
            if (isDetonatingOnImpact)
            {
                // Detonate on impact without bouncing.
                return true;
            }
            else
            {
                // Projectile bounces off geometry or entities.
                proj->o = old;
                projectiles::bounce(proj, collidewall);
                const float c = collidewall.dot(proj->vel);
                const float k = 1.0f + (1.0f - elasticity) * c / proj->vel.magnitude();
                proj->vel.mul(k);
                proj->vel.sub(vec(collidewall).mul(elasticity * 2.0f * c));
            }
        }
        if (proj->physstate != PHYS_BOUNCE)
        {
            // Make sure bouncing projectiles don't spawn inside geometry!
            if (proj->o == old)
            {
                return !collideplayer;
            }
            proj->physstate = PHYS_BOUNCE;
        }
        return collideplayer != NULL && collideplayer != proj;
    }

    /*
     * Updates the position of a bouncing projectile and checks for collision confirmations
     * to determine the lifespan of a projectile.
     * If a collision is confirmed, the function decides whether the projectile should detonate
     * or continue bouncing (or even stick), depending on specific projectile behaviours.
     * If the projectile impacts geometry or a living entity and should detonate, it returns false.
     * Otherwise, it returns true, meaning the projectile will continue.
     */
    bool isbouncing(ProjEnt* proj, const float elasticity, const float waterFriction, const float gravity)
    {
        if (physsteps <= 0)
        {
            interpolateposition(proj);
            return true;
        }
        proj->o = proj->newpos;
        bool hitplayer = false;
        loopi(physsteps - 1)
        {

            if (hasbounced(proj, physframetime / 1000.0f, elasticity, waterFriction, gravity))
            {
                hitplayer = true;
            }
        }
        proj->deltapos = proj->o;
        if (hasbounced(proj, physframetime / 1000.0f, elasticity, waterFriction, gravity))
        {
            hitplayer = true;
        }
        proj->newpos = proj->o;
        proj->deltapos.sub(proj->newpos);
        interpolateposition(proj);
        return !hitplayer;
    }

    void updateragdoll(dynent* pl, vec center, float radius, bool& water)
    {
        int material = lookupmaterial(vec(center.x, center.y, center.z + radius / 2));
        water = isliquidmaterial(material & MATF_VOLUME);
        if (!pl->inwater && water)
        {
            triggerPlayerPhysicsEvent(pl, PhysEvent_LiquidIn, true, material & MATF_VOLUME, center);
        }
        else if (pl->inwater && !water)
        {
            material = lookupmaterial(center);
            water = isliquidmaterial(material & MATF_VOLUME);
            if (!water)
            {
                triggerPlayerPhysicsEvent(pl, PhysEvent_LiquidOut, true, pl->inwater, center);
            }
        }
        pl->inwater = water ? material & MATF_VOLUME : MAT_AIR;

        gameent* d = (gameent*)pl;
        if (d->deathstate == Death_Shock && lastmillis - d->lastpain <= 2000)
        {
            const float scale = 1.0f + rndscale(12.0f);
            particle_flare(center, center, 1, PART_ELECTRICITY, 0xEE88EE, scale);
            addgamelight(center, vec(238.0f, 136.0f, 238.0f), scale * 2);
        }
    }

    FVAR(ragdollgravity, 0, 198.0f, 200);
    VAR(ragdolltwitch, 1, 10, 15);

    void updateragdollvertex(dynent* pl, vec pos, vec& dpos, float ts)
    {
        gameent* d = (gameent*)pl;
        float gravity = ragdollgravity ? ragdollgravity : mapgravity;
        if (d->deathstate == Death_Disrupt)
        {
            particle_flare(pos, pos, 1, PART_RING, 0x00FFFF, 2.0f * d->transparency);
        }
        else
        {
            dpos.z -= gravity * ts * ts;
            if (d->deathstate == Death_Shock && lastmillis - d->lastpain <= 2500)
            {
                dpos.add(vec(rnd(201) - 100, rnd(201) - 100, rnd(201) - 100).normalize().mul(ragdolltwitch * ts));
            }
        }
    }

    FVAR(ragdolleyesmooth, 0, 0, 1);
    VAR(ragdolleyesmoothmillis, 1, 1, 10000);

    void updateragdolleye(dynent* pl, vec eye, const vec offset)
    {
        gameent* d = (gameent*)pl;
        const bool isFirstPerson = camera::isfirstpersondeath();
        if (!isFirstPerson && (d->deathstate == Death_Fall || d->deathstate == Death_Gib))
        {
            return;
        }
        if (d == self && isFirstPerson)
        {
            camera1->o = eye;
            return;
        }
        eye.add(offset);
        const float k = pow(ragdolleyesmooth, float(curtime) / ragdolleyesmoothmillis);
        d->o.lerp(eye, 1 - k);
    }

    VARP(ragdollpush, 0, 1, 1);

    void pushRagdoll(dynent *d, const vec &position, const int damage)
    {
        if (!ragdollpush || !d->ragdoll)
        {
            return;
        }
        gameent* ragdoll = (gameent*)d;
        if (ragdoll->deathstate == Death_Gib)
        {
            return;
        }
        if (damage)
        {
            if (ragdoll->health <= 0)
            {
                /* Reset ragdoll's health with zero consequences,
                 * just to make sure it explodes with heavy damage only.
                 */
                ragdoll->health = ragdoll->maxhealth;
            }
            ragdoll->health -= damage;
        }
        if (ragdoll->shouldgib())
        {
            ragdoll->deathstate = Death_Gib;
            gibeffect(damage, ragdoll->vel, ragdoll);
        }
        else
        {
            pushragdoll(ragdoll, position);
        }
        pushragdoll(d, position);
    }
}
