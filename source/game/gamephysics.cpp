#include "game.h"

namespace physics
{
    using namespace game;

    const float STAIRHEIGHT = 4.1f;
    const float FLOORZ = 0.867f;
    const float SLOPEZ = 0.5f;
    const float WALLZ = 0.2f;

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

    bool retrystepdown(gameent* d, vec& dir, float step, float xy, float z, bool init = false)
    {
        vec stepdir(dir.x, dir.y, 0);
        stepdir.z = -stepdir.magnitude2() * z / xy;
        if (!stepdir.z) return false;
        stepdir.normalize();

        vec old(d->o);
        d->o.add(vec(stepdir).mul(STAIRHEIGHT / fabs(stepdir.z))).z -= STAIRHEIGHT;
        d->zmargin = -STAIRHEIGHT;
        if (collide(d, vec(0, 0, -1), SLOPEZ))
        {
            d->o = old;
            d->o.add(vec(stepdir).mul(step));
            d->zmargin = 0;
            if (!collide(d, vec(0, 0, -1)))
            {
                vec stepfloor(stepdir);
                stepfloor.mul(-stepfloor.z).z += 1;
                stepfloor.normalize();
                if (d->physstate >= PHYS_SLOPE && d->floor != stepfloor)
                {
                    // Prevent alternating step-down/step-up states if player would keep bumping into the same floor.
                    vec stepped(d->o);
                    d->o.z -= 0.5f;
                    d->zmargin = -0.5f;
                    if (collide(d, stepdir) && collidewall == d->floor)
                    {
                        d->o = old;
                        if (!init) { d->o.x += dir.x; d->o.y += dir.y; if (dir.z <= 0 || collide(d, dir)) d->o.z += dir.z; }
                        d->zmargin = 0;
                        d->physstate = PHYS_STEP_DOWN;
                        d->timeinair = 0;
                        return true;
                    }
                    d->o = init ? old : stepped;
                    d->zmargin = 0;
                }
                else if (init) d->o = old;
                switchfloor(d, dir, stepfloor);
                d->floor = stepfloor;
                d->physstate = PHYS_STEP_DOWN;
                d->timeinair = 0;
                return true;
            }
        }
        d->o = old;
        d->zmargin = 0;
        return false;
    }

    bool canmove(gameent* d)
    {
        if (d->type != ENT_PLAYER || d->state == CS_SPECTATOR) return true;
        return !intermission && !(gore && d->state == CS_DEAD && d->deathstate == Death_Gib);
    }

    bool trystepdown(gameent* d, vec& dir, bool init = false)
    {
        if ((!d->move && !d->strafe) || !canmove(d)) return false;
        vec old(d->o);
        d->o.z -= STAIRHEIGHT;
        d->zmargin = -STAIRHEIGHT;
        if (!collide(d, vec(0, 0, -1), SLOPEZ))
        {
            d->o = old;
            d->zmargin = 0;
            return false;
        }
        d->o = old;
        d->zmargin = 0;
        float step = dir.magnitude();
#if 1
        // Weaker check, just enough to avoid hopping up slopes.
        if (retrystepdown(d, dir, step, 4, 1, init)) return true;
#else
        if (retrystepdown(d, dir, step, 2, 1, init)) return true;
        if (retrystepdown(d, dir, step, 1, 1, init)) return true;
        if (retrystepdown(d, dir, step, 1, 2, init)) return true;
#endif
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
        else if (d->physstate < PHYS_SLOPE || dir.dot(d->floor) > 0.01f * dir.magnitude() || (floor.z != 0.0f && floor.z != 1.0f) || !trystepdown(d, dir, true))
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
            d->physstate = floor.z >= FLOORZ ? PHYS_FLOOR : PHYS_SLOPE;
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

    const int CROUCH_TIME = 180;

    void crouchplayer(gameent* d, int moveres, bool local)
    {
        if (!curtime) return;
        float minheight = d->maxheight * CROUCH_HEIGHT, speed = (d->maxheight - minheight) * curtime / float(CROUCH_TIME);
        if (d->crouching < 0)
        {
            if (d->eyeheight > minheight)
            {
                float diff = min(d->eyeheight - minheight, speed);
                d->eyeheight -= diff;
                if (d->physstate >= PHYS_FALL)
                {
                    d->o.z -= diff;
                    d->newpos.z -= diff;
                }
            }
        }
        else if (d->eyeheight < d->maxheight)
        {
            float diff = min(d->maxheight - d->eyeheight, speed), step = diff / moveres;
            d->eyeheight += diff;
            if (d->physstate >= PHYS_FALL)
            {
                d->o.z += diff;
                d->newpos.z += diff;
            }
            d->crouching = 0;
            loopi(moveres)
            {
                if (!collide(d, vec(0, 0, d->physstate < PHYS_FALL ? -1 : 1), 0, true)) break;
                d->crouching = 1;
                d->eyeheight -= step;
                if (d->physstate >= PHYS_FALL)
                {
                    d->o.z -= step;
                    d->newpos.z -= step;
                }
            }
        }
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
            if (d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR || (collide(d, vec(0, 0, -1), SLOPEZ) && (d->physstate == PHYS_STEP_UP || d->physstate == PHYS_STEP_DOWN || collidewall.z >= FLOORZ)))
            {
                d->o = old;
                d->zmargin = 0;
                if (trystepup(d, dir, obstacle, STAIRHEIGHT, d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR ? d->floor : vec(collidewall))) return true;
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
            if (trystepdown(d, dir)) return true;
            d->o = moved;
        }
        vec floor(0, 0, 0);
        bool slide = collided,
            found = findfloor(d, dir, collided, obstacle, slide, floor);
        if (slide || (!collided && floor.z > 0 && floor.z < WALLZ))
        {
            slideagainst(d, dir, slide ? obstacle : floor, found, slidecollide);
            if(!d->climbing) d->blocked = true;
        }
        if (found) land(d, dir, floor, collided);
        else fall(d, dir, floor);
        return !collided;
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
        else if (d->role == ROLE_ZOMBIE || d->role == ROLE_BERSERKER)
        {
            speed += 10.0f; // Speed bonus.
        }
        return speed;
    }

    inline bool canjump(gameent* d)
    {
        return d->physstate >= PHYS_SLOPE || ((d->haspowerup(PU_AGILITY) || d->role == ROLE_ZOMBIE || d->role == ROLE_BERSERKER) && !d->doublejumping);
    }

    const float VELOCITY_JUMP = 135.0f;
    const float VELOCITY_CROUCH = 0.4f;
    const float VELOCITY_LADDER = 0.7f;
    const float VELOCITY_WATER_DAMP = 8.0f;

    VAR(floatspeed, 1, 100, 10000);

    int materialcheck(gameent* d)
    {
        return lookupmaterial(vec(d->o.x, d->o.y, d->o.z + (3 * d->aboveeye - d->eyeheight) / 4));
    }

    bool isFloating(gameent* d)
    {
        return (d->type == ENT_PLAYER && (d->state == CS_EDITING || d->state == CS_SPECTATOR));
    }

    bool hascamerapitchmovement(gameent* d)
    {
        return isFloating(d) || isliquidmaterial(materialcheck(d) & MATF_VOLUME) || d->climbing || d->type == ENT_CAMERA;
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

                triggerphysicsevent(d, PHYSEVENT_JUMP, d->inwater);
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
            vecfromyawpitch(d->yaw, hascamerapitchmovement(d) ? d->pitch : 0, d->move, d->strafe, m);

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
        bool isonfloor = d->physstate >= PHYS_SLOPE || d->climbing;
        if (d->type == ENT_PLAYER)
        {
            if (isfloating)
            {
                if (d == self) dir.mul(floatspeed / 100.0f);
            }
            else if (d->crouching && isonfloor)
            {
                dir.mul(VELOCITY_CROUCH);
            }
            else if (d->climbing)
            {
                dir.mul(VELOCITY_LADDER);
            }
            else if (!isinwater)
            {
                dir.mul((d->move && !d->strafe ? 1.3f : 1.0f) * (!isonfloor ? 1.3f : 1.0f));
            }
        }
        // Calculate and apply friction.
        float friction = 25.0f;
        if (isinwater && !d->climbing && !isfloating)
        {
            friction = 20.0f;
        }
        else if (isonfloor || isfloating)
        {
            friction = 4.0f;
        }
        d->vel.lerp(dir, d->vel, pow(1 - 1 / friction, curtime / 20.0f));
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
            triggerphysicsevent(d, PHYSEVENT_LIQUID_IN, material & MATF_VOLUME);
        }
        else if (transition == LiquidTransition_Out)
        {
            triggerphysicsevent(d, PHYSEVENT_LIQUID_OUT, d->inwater);
        }
    }

    VARP(rollstrafemax, 0, 1, 20);
    FVAR(rollstrafe, 0, 0.018f, 90);
    FVAR(rollfade, 0, 0.9f, 1);
    FVAR(rollonland, 0, 5, 20);

    void addroll(gameent* d, float amount)
    {
        if (!d || (d->lastroll && lastmillis - d->lastroll < 500))
        {
            return;
        }

        float strafingRollAmount = rollstrafemax ? amount / 2 : amount;
        d->roll += d->roll > 0 ? -strafingRollAmount : (d->roll < 0 ? strafingRollAmount : (rnd(2) ? amount : -amount));
        d->lastroll = lastmillis;
    }

    const int SHORT_JUMP_THRESHOLD = 350;
    const int LONG_JUMP_THRESHOLD = 800;

    bool isplayermoving(gameent* d, int moveres, bool local, int curtime)
    {
        if (!canmove(d)) return false;
        int material = materialcheck(d);
        bool isinwater = isliquidmaterial(material & MATF_VOLUME);
        bool isfloating = isFloating(d);
        float secs = curtime / 1000.f;


        if (!isfloating && !d->climbing) modifygravity(d, isinwater, curtime); // Apply gravity.
        modifyvelocity(d, local, isinwater, isfloating, curtime); // Apply any player generated changes in velocity.

        vec dir(d->vel);
        if (!isfloating && isinwater) dir.mul(0.5f);
        dir.add(d->falling);
        dir.mul(secs);

        d->blocked = false;

        if (isfloating)
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
                d->doublejumping = false; // Now that we landed, we can double jump again.
                if (timeinair > SHORT_JUMP_THRESHOLD && timeinair < LONG_JUMP_THRESHOLD)
                {
                    triggerphysicsevent(d, PHYSEVENT_LAND_LIGHT, material); // Short jump.
                }
                else if (timeinair >= LONG_JUMP_THRESHOLD) // If we land after a long time, it must have been a high jump.
                {
                    triggerphysicsevent(d, PHYSEVENT_LAND_HEAVY, material); // Make a heavy landing sound.
                }
                triggerphysicsevent(d, PHYSEVENT_FOOTSTEP, material);
            }
        }

        // Automatically apply smooth roll when strafing.
        if (d->strafe && rollstrafemax && !isfloating) d->roll = clamp(d->roll - pow(clamp(1.0f + d->strafe * d->roll / rollstrafemax, 0.0f, 1.0f), 0.33f) * d->strafe * curtime * rollstrafe, -rollstrafemax, rollstrafemax);
        else d->roll *= curtime == PHYSFRAMETIME ? rollfade : pow(rollfade, curtime / float(PHYSFRAMETIME));

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
                if (lookupmaterial(d->feetpos()) & MAT_DAMAGE || lookupmaterial(d->feetpos()) & MAT_LAVA)
                {
                    game::hurt(d); // Harm the player if their feet or body are inside harmful materials.
                }
                if (lookupmaterial(d->feetpos()) & MAT_CLIMB)
                {
                    // Enable ladder-like movement.
                    if (!d->climbing)
                    {
                        d->climbing = true;
                        d->falling = d->vel = vec(0, 0, 0);
                    }
                }
                else if (d->climbing) d->climbing = false;
            }
        }
        else if (d->climbing) d->climbing = false;

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

    VARP(footstepssounds, 0, 1, 1);
    VARP(footstepdelay, 1, 44000, 50000);

    void playfootstepsounds(gameent* d, int sound, bool hascrouchfootsteps = true)
    {
        bool isonfloor = d->physstate >= PHYS_SLOPE || d->climbing;
        if (!footstepssounds || !isonfloor || (hascrouchfootsteps && d->crouching) || d->blocked)
        {
            return;
        }
        if (d->move || d->strafe)
        {
            if (lastmillis - d->lastfootstep < (footstepdelay / fmax(d->vel.magnitude(), 1))) return;
            playsound(sound, d);
        }
        d->lastfootstep = lastmillis;
    }

    struct footstepinfo
    {
        int sound;
        bool hascrouchfootsteps;
    };

    footstepinfo footstepsound(gameent* d)
    {
        footstepinfo foot;
        if ((lookupmaterial(d->feetpos(-1)) & MATF_VOLUME) == MAT_GLASS)
        {
            foot.sound = S_FOOTSTEP_GLASS;
            foot.hascrouchfootsteps = false;
        }
        else if (lookupmaterial(d->feetpos()) & MAT_WATER)
        {
            foot.sound = S_FOOTSTEP_WATER;
            foot.hascrouchfootsteps = true;
        }
        else
        {
            int texture = lookuptextureeffect(d->feetpos(-1));
            foot.sound = textureeffects[texture].footstepsound;
            foot.hascrouchfootsteps = textureeffects[texture].hascrouchfootsteps;
        }
        return foot;
    }

    void triggerfootsteps(gameent* d, bool islanding)
    {
        footstepinfo foot = footstepsound(d);
        if (islanding)
        {
            // just send the landing sound effect (single footstep)
            msgsound(foot.sound, d);
        }
        else
        {
            // manage additional conditions and timing for walking sounds
            playfootstepsounds(d, foot.sound, foot.hascrouchfootsteps);
        }
    }

    void applyliquideffects(gameent* d)
    {
        particle_splash(PART_WATER, 200, 250, d->o, 0xFFFFFF, 0.09f, 800, 1);
        particle_splash(PART_SPLASH, 10, 100, d->o, 0xFFFFFF, 10.0f, 500, -1);
    }

    void triggerphysicsevent(physent* pl, int event, int material, vec origin)
    {
        gameent* d = (gameent*)pl;
        if (d->state > CS_DEAD) return;
        switch (event)
        {
            case PHYSEVENT_JUMP:
            {
                if (material & MAT_WATER || !(d == self || d->type != ENT_PLAYER || d->ai))
                {
                    break;
                }
                if (!d->timeinair) msgsound(S_JUMP1, d);
                else msgsound(S_JUMP2, d);
                break;
            }

            case PHYSEVENT_LAND_LIGHT:
            case PHYSEVENT_FOOTSTEP:
            {
                if (!(d == self || d->type != ENT_PLAYER || d->ai)) break;
                if (event == PHYSEVENT_LAND_LIGHT)
                {
                    sway.addevent(d, SwayEvent_Land, 350, -3);
                    triggerfootsteps(d, true);
                }
                else
                {
                    triggerfootsteps(d, false);
                }
                d->lastfootleft = d->lastfootright = vec(-1, -1, -1);
                break;
            }

            case PHYSEVENT_LAND_HEAVY:
            {
                if (!(d == self || d->type != ENT_PLAYER || d->ai)) break;
                msgsound(material & MAT_WATER ? S_LAND_WATER : S_LAND, d);
                sway.addevent(d, SwayEvent_Land, 380, -8);
                addroll(d, rollonland);
                d->lastfootleft = d->lastfootright = vec(-1, -1, -1);
                break;
            }

            case PHYSEVENT_RAGDOLL_COLLIDE:
            {
                playsound(S_CORPSE, NULL, origin.iszero() ? (d == self ? NULL : &d->o) : &origin);
                break;
            }

            case PHYSEVENT_LIQUID_IN:
            {
                playsound(material == MAT_LAVA ? S_LAVA_IN : S_WATER_IN, NULL, origin.iszero() ? (d == self ? NULL : &d->o) : &origin);
                applyliquideffects(d);
                break;
            }

            case PHYSEVENT_LIQUID_OUT:
            {
                if (material == MAT_LAVA) break;
                playsound(S_WATER_OUT, NULL, origin.iszero() ? (d == self ? NULL : &d->o) : &origin);
                applyliquideffects(d);
                break;
            }

            default: break;
        }
    }

    void collidewithdynamicentity(physent* d, physent* o, const vec& dir)
    {
        if (d->type == ENT_AI)
        {
            if (dir.z > 0) stackmonster((monster*)d, o);
        }
    }

#define DIR(name, v, d, s, os) ICOMMAND(name, "D", (int *down), \
{ \
    self->s = *down != 0; \
    self->v = self->s ? d : (self->os ? -(d) : 0); \
}); \

    DIR(backward, move, -1, k_down, k_up);
    DIR(forward, move, 1, k_up, k_down);
    DIR(left, strafe, 1, k_left, k_right);
    DIR(right, strafe, -1, k_right, k_left);

#undef DIR

    bool canjump()
    {
        if (!connected || intermission) return false;
        respawn();
        return self->state != CS_DEAD;
    }

    void dojump(int down)
    {
        if (!down || canjump())
        {
            if (self->physstate > PHYS_FLOAT && self->crouched()) self->crouching = 0;
            else self->jumping = down != 0;
        }
    }
    ICOMMAND(jump, "D", (int* down),
    {
        dojump(*down);
    });

    bool cancrouch()
    {
        if (!connected || intermission) return false;
        return self->state != CS_DEAD;
    }

    void docrouch(int down)
    {
        if (!down) self->crouching = abs(self->crouching);
        else if (cancrouch()) self->crouching = -1;
    }
    ICOMMAND(crouch, "D", (int* down),
    {
        docrouch(*down);
    });

    bool hasbounced(physent* d, float secs, float elasticity, float waterfric, float gravity)
    {
        // Collision checks.
        if (d->physstate != PHYS_BOUNCE && collide(d, vec(0, 0, 0), 0, false)) return true;
        int mat = lookupmaterial(vec(d->o.x, d->o.y, d->o.z + (d->aboveeye - d->eyeheight) / 2));
        bool isinwater = isliquidmaterial(mat);
        if (isinwater)
        {
            d->vel.z -= gravity * mapgravity / 16 * secs;
            d->vel.mul(max(1.0f - secs / waterfric, 0.0f));
        }
        else d->vel.z -= gravity * mapgravity * secs;
        vec old(d->o);
        loopi(2)
        {
            vec dir(d->vel);
            dir.mul(secs);
            d->o.add(dir);
            if (!collide(d, dir, 0, true, true))
            {
                if (collideinside)
                {
                    d->o = old;
                    d->vel.mul(-elasticity);
                }
                break;
            }
            else if (collideplayer)
            {
                break;
            }
            d->o = old;
            game::bounced(d, collidewall);
            float c = collidewall.dot(d->vel),
                k = 1.0f + (1.0f - elasticity) * c / d->vel.magnitude();
            d->vel.mul(k);
            d->vel.sub(vec(collidewall).mul(elasticity * 2.0f * c));
        }
        if (d->physstate != PHYS_BOUNCE)
        {
            // Make sure bouncers don't start inside geometry!
            if (d->o == old) return !collideplayer;
            d->physstate = PHYS_BOUNCE;
        }
        return collideplayer != NULL;
    }

    bool isbouncing(physent* d, float elasticity, float waterfric, float gravity)
    {
        if (physsteps <= 0)
        {
            interpolateposition(d);
            return true;
        }

        d->o = d->newpos;
        bool hitplayer = false;
        loopi(physsteps - 1)
        {
            if (hasbounced(d, physframetime / 1000.0f, elasticity, waterfric, gravity))
            {
                hitplayer = true;
            }
        }
        d->deltapos = d->o;
        if (hasbounced(d, physframetime / 1000.0f, elasticity, waterfric, gravity))
        {
            hitplayer = true;
        }
        d->newpos = d->o;
        d->deltapos.sub(d->newpos);
        interpolateposition(d);
        return !hitplayer;
    }

    void updateragdoll(dynent* pl, vec center, float radius, bool& water)
    {
        int material = lookupmaterial(vec(center.x, center.y, center.z + radius / 2));
        water = isliquidmaterial(material & MATF_VOLUME);
        if (!pl->inwater && water)
        {
            triggerphysicsevent(pl, PHYSEVENT_LIQUID_IN, material & MATF_VOLUME, center);
        }
        else if (pl->inwater && !water)
        {
            material = lookupmaterial(center);
            water = isliquidmaterial(material & MATF_VOLUME);
            if (!water)
            {
                triggerphysicsevent(pl, PHYSEVENT_LIQUID_OUT, pl->inwater, center);
            }
        }
        pl->inwater = water ? material & MATF_VOLUME : MAT_AIR;

        gameent* d = (gameent*)pl;
        if (d->deathstate == Death_Shock && lastmillis - d->lastpain <= 2000)
        {
            float scale = 1.0f + rndscale(12.0f);
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
        bool isfirstperson = isfirstpersondeath();
        if (!isfirstperson && (d->deathstate == Death_Fall || d->deathstate == Death_Gib))
        {
            return;
        }

        if (d == self && isfirstperson)
        {
            camera1->o = eye;
            return;
        }

        eye.add(offset);
        float k = pow(ragdolleyesmooth, float(curtime) / ragdolleyesmoothmillis);
        d->o.lerp(eye, 1 - k);
    }
}

