#include "game.h"

namespace physics
{
    using namespace game;

    const float STAIRHEIGHT = 4.1f;
    const float FLOORZ = 0.867f;
    const float SLOPEZ = 0.5f;
    const float WALLZ = 0.2f;

    void recalcdir(physent* d, const vec& oldvel, vec& dir)
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

    void slideagainst(physent* d, vec& dir, const vec& obstacle, bool foundfloor, bool slidecollide)
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
        recalcdir(d, oldvel, dir);
    }

    void switchfloor(physent* d, vec& dir, const vec& floor)
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
        recalcdir(d, oldvel, dir);
    }

    bool trystepup(physent* d, vec& dir, const vec& obstacle, float maxstep, const vec& floor)
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

    bool trystepdown(physent* d, vec& dir, float step, float xy, float z, bool init = false)
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

    bool trystepdown(physent* d, vec& dir, bool init = false)
    {
        if ((!d->move && !d->strafe) || !game::allowmove(d)) return false;
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
        if (trystepdown(d, dir, step, 4, 1, init)) return true;
#else
        if (trystepdown(d, dir, step, 2, 1, init)) return true;
        if (trystepdown(d, dir, step, 1, 1, init)) return true;
        if (trystepdown(d, dir, step, 1, 2, init)) return true;
#endif
        return false;
    }

    void falling(physent* d, vec& dir, const vec& floor)
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

    void landing(physent* d, vec& dir, const vec& floor, bool collided)
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

    bool findfloor(physent* d, const vec& dir, bool collided, const vec& obstacle, bool& slide, vec& floor)
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

    const int CROUCH_TIME = 200;
    const float CROUCH_SPEED = 0.4f;

    void crouchplayer(physent* pl, int moveres, bool local)
    {
        if (!curtime) return;
        float minheight = pl->maxheight * CROUCH_HEIGHT, speed = (pl->maxheight - minheight) * curtime / float(CROUCH_TIME);
        if (pl->crouching < 0)
        {
            if (pl->eyeheight > minheight)
            {
                float diff = min(pl->eyeheight - minheight, speed);
                pl->eyeheight -= diff;
                if (pl->physstate >= PHYS_FALL)
                {
                    pl->o.z -= diff;
                    pl->newpos.z -= diff;
                }
            }
        }
        else if (pl->eyeheight < pl->maxheight)
        {
            float diff = min(pl->maxheight - pl->eyeheight, speed), step = diff / moveres;
            pl->eyeheight += diff;
            if (pl->physstate >= PHYS_FALL)
            {
                pl->o.z += diff;
                pl->newpos.z += diff;
            }
            pl->crouching = 0;
            loopi(moveres)
            {
                if (!collide(pl, vec(0, 0, pl->physstate < PHYS_FALL ? -1 : 1), 0, true)) break;
                pl->crouching = 1;
                pl->eyeheight -= step;
                if (pl->physstate >= PHYS_FALL)
                {
                    pl->o.z -= step;
                    pl->newpos.z -= step;
                }
            }
        }
    }

    bool ismoving(physent* d, vec& dir)
    {
        vec old(d->o);
        bool collided = false, slidecollide = false;
        vec obstacle;
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
            d->blocked = true;
        }
        if (found) landing(d, dir, floor, collided);
        else falling(d, dir, floor);
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

    void interppos(physent* pl)
    {
        pl->o = pl->newpos;

        int diff = lastphysframe - lastmillis;
        if (diff <= 0 || !physinterp) return;

        vec deltapos(pl->deltapos);
        deltapos.mul(min(diff, physframetime) / float(physframetime));
        pl->o.add(deltapos);
    }

    void updatephysstate(physent* d)
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

    float calcspeed(gameent* d)
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

    const float JUMP_VEL = 135.0f;
    const float CROUCH_VEL = 0.4f;
    const float LADDER_VEL = 0.7f;

    VAR(floatspeed, 1, 100, 10000);

    void modifyvelocity(physent* pl, bool local, bool isinwater, bool floating, int curtime)
    {
        gameent* d = (gameent*)pl;
        if (floating)
        {
            if (pl->jumping)
            {
                pl->vel.z = max(pl->vel.z, JUMP_VEL);
            }
            if (pl->crouching < 0)
            {
                pl->vel.z = min(pl->vel.z, -JUMP_VEL);
            }
        }
        else if (canjump(d) || isinwater)
        {
            if (isinwater && !pl->inwater) pl->vel.div(8);
            if (pl->jumping)
            {
                pl->jumping = false;
                if (pl->timeinair)
                {
                    pl->doublejumping = true;
                    pl->lastfootright = pl->rfoot;
                    pl->lastfootleft = pl->lfoot;
                    pl->falling.z = 1;
                }
                pl->vel.z = max(pl->vel.z, JUMP_VEL); // Physics impulse upwards.
                if (isinwater)
                {
                    // Dampen velocity change even harder, gives a decent water feel.
                    pl->vel.x /= 8.0f;
                    pl->vel.y /= 8.0f;
                }

                triggerphysicsevent(pl, PHYSEVENT_JUMP, pl->inwater);
            }
        }
        if (!floating && !pl->climbing && pl->physstate == PHYS_FALL) pl->timeinair += curtime;

        vec m(0.0f, 0.0f, 0.0f);
        if ((pl->move || pl->strafe))
        {
            bool movepitch = floating || isinwater || pl->climbing || pl->type == ENT_CAMERA;
            vecfromyawpitch(pl->yaw, movepitch ? pl->pitch : 0, pl->move, pl->strafe, m);

            if (!floating && pl->physstate >= PHYS_SLOPE)
            {
                /* Move up or down slopes in air,
                 * but only move up slopes in water.
                 */
                float dz = -(m.x * pl->floor.x + m.y * pl->floor.y) / pl->floor.z;
                m.z = isinwater ? max(m.z, dz) : dz;
            }
            if (!m.iszero())
            {
                if (pl->climbing && !pl->crouching) m.addz(m.z >= 0 ? 1 : -1).normalize();
                else m.normalize();
            }
        }

        vec dir(m);
        dir.mul(calcspeed(d));
        bool isonfloor = pl->physstate >= PHYS_SLOPE || pl->climbing;
        if (pl->type == ENT_PLAYER)
        {
            if (floating)
            {
                if (pl == self) dir.mul(floatspeed / 100.0f);
            }
            else if (pl->crouching)
            {
                dir.mul(CROUCH_VEL);
            }
            else if (pl->climbing)
            {
                dir.mul(LADDER_VEL);
            }
            else if (!isinwater)
            {
                dir.mul((pl->move && !pl->strafe ? 1.3f : 1.0f) * (!isonfloor ? 1.3f : 1.0f));
            }
        }
        // Calculate and apply friction.
        float friction = 25.0f;
        if (isinwater && !pl->climbing && !floating)
        {
            friction = 20.0f;
        }
        else if (isonfloor || floating)
        {
            friction = 4.0f;
        }
        pl->vel.lerp(dir, pl->vel, pow(1 - 1 / friction, curtime / 20.0f));
    }

    FVARR(mapgravity, 0, 195.0f, 250.0f);

    void modifygravity(physent* pl, bool isinwater, int curtime)
    {
        float secs = curtime / 1000.0f;
        vec g(0, 0, 0);
        if (pl->physstate == PHYS_FALL) g.z -= mapgravity * secs;
        else if (pl->floor.z > 0 && pl->floor.z < FLOORZ)
        {
            g.z = -1;
            g.project(pl->floor);
            g.normalize();
            g.mul(mapgravity * secs);
        }
        if (!isinwater || (!pl->move && !pl->strafe)) pl->falling.add(g);

        if (isinwater || pl->physstate >= PHYS_SLOPE)
        {
            float fric = isinwater ? 2.0f : 6.0f;
            float c = isinwater ? 1.0f : clamp((pl->floor.z - SLOPEZ) / (FLOORZ - SLOPEZ), 0.0f, 1.0f);
            pl->falling.mul(pow(1 - c / fric, curtime / 20.0f));
        }
    }

    /* Main physics routine, moves a player/monster for a "curtime" step.
     * "moveres" indicated the physics precision (which is lower for monsters and multiplayer prediction).
     * "local" is false for multiplayer prediction.
     */

    void detectcollisions(physent* pl, int moveres, vec& d)
    {
        const float f = 1.0f / moveres;
        int collisions = 0;
        d.mul(f);
        loopi(moveres)
        {
            if (!ismoving(pl, d) && ++collisions < 5)
            {
                i--; // Discrete steps collision detection and sliding.
            }
        }
    }

    VARP(maxroll, 0, 1, 20);
    FVAR(straferoll, 0, 0.018f, 90);
    FVAR(faderoll, 0, 0.9f, 1);

    bool isplayermoving(physent* pl, int moveres, bool local, int curtime)
    {
        if (!game::allowmove(pl)) return false;
        int material = lookupmaterial(vec(pl->o.x, pl->o.y, pl->o.z + (3 * pl->aboveeye - pl->eyeheight) / 4));
        bool isinwater = isliquid(material & MATF_VOLUME);
        bool floating = pl->type == ENT_PLAYER && (pl->state == CS_EDITING || pl->state == CS_SPECTATOR);
        float secs = curtime / 1000.f;

        
        if (!floating && !pl->climbing) modifygravity(pl, isinwater, curtime); // Apply gravity.
        physics::modifyvelocity(pl, local, isinwater, floating, curtime); // Apply any player generated changes in velocity.

        vec d(pl->vel);
        if (!floating && isinwater) d.mul(0.5f);
        d.add(pl->falling);
        d.mul(secs);

        pl->blocked = false;

        gameent* e = (gameent*)pl;

        if (floating)
        {
            if (e->ghost)
            {
                detectcollisions(pl, moveres, d);
            }
            if (pl->physstate != PHYS_FLOAT)
            {
                pl->physstate = PHYS_FLOAT;
                pl->timeinair = 0;
                pl->falling = vec(0, 0, 0);
            }
            pl->o.add(d);
        }
        else
        {
            const int timeinair = pl->timeinair; // Use a constant that represents the maximum amount of time the player has spent in air.
            detectcollisions(pl, moveres, d);
            if (!pl->timeinair && !isinwater) // Player is currently not in air nor swimming.
            {
                pl->doublejumping = false; // Now that we landed, we can double jump again.
                if (timeinair > 350 && timeinair < 800)
                {
                    triggerphysicsevent(pl, PHYSEVENT_LAND_SHORT, material); // Short jump.
                }
                else if (timeinair >= 800) // If we land after a long time, it must have been a high jump.
                {
                    triggerphysicsevent(pl, PHYSEVENT_LAND_MEDIUM, material); // Make a heavy landing sound.
                }
                triggerphysicsevent(pl, PHYSEVENT_FOOTSTEP, material);
            }
        }

        // Automatically apply smooth roll when strafing.
        if (pl->strafe && maxroll && !floating) pl->roll = clamp(pl->roll - pow(clamp(1.0f + pl->strafe * pl->roll / maxroll, 0.0f, 1.0f), 0.33f) * pl->strafe * curtime * straferoll, -maxroll, maxroll);
        else pl->roll *= curtime == PHYSFRAMETIME ? faderoll : pow(faderoll, curtime / float(PHYSFRAMETIME));

        if (pl->state == CS_ALIVE) updatedynentcache(pl);

        // Play sounds on water transitions.
        if (pl->inwater && !isinwater)
        {
            material = lookupmaterial(vec(pl->o.x, pl->o.y, pl->o.z + (pl->aboveeye - pl->eyeheight) / 2));
            isinwater = isliquid(material & MATF_VOLUME);
        }
        if (!pl->inwater && isinwater) triggerphysicsevent(pl, PHYSEVENT_LIQUID_IN, material & MATF_VOLUME);
        else if (pl->inwater && !isinwater) triggerphysicsevent(pl, PHYSEVENT_LIQUID_OUT, pl->inwater);
        pl->inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;

        if (pl->state == CS_ALIVE)
        {
            if (pl->o.z < 0 || material & MAT_DEATH)
            {
                game::suicide(pl); // Kill the player if inside death material or outside of the map (below origin).
            }
            else
            {
                if (lookupmaterial(pl->feetpos()) & MAT_DAMAGE || lookupmaterial(pl->feetpos()) & MAT_LAVA)
                {
                    game::hurt(pl); // Harm the player if their feet or body are inside harmful materials.
                }
                if (lookupmaterial(pl->feetpos()) & MAT_CLIMB)
                {
                    // Enable ladder-like movement.
                    if (!pl->climbing)
                    {
                        pl->climbing = true;
                        pl->falling = pl->vel = vec(0, 0, 0);
                        triggerphysicsevent(pl, PHYSEVENT_LAND_SHORT, material);
                    }
                }
                else if (pl->climbing) pl->climbing = false;
            }
        }
        else if (pl->climbing) pl->climbing = false;

        return true;
    }

    void moveplayer(physent* pl, int moveres, bool local)
    {
        if (physsteps <= 0)
        {
            if (local) interppos(pl);
            return;
        }

        if (local) pl->o = pl->newpos;
        loopi(physsteps - 1) isplayermoving(pl, moveres, local, physframetime);
        if (local) pl->deltapos = pl->o;
        isplayermoving(pl, moveres, local, physframetime);
        if (local)
        {
            pl->newpos = pl->o;
            pl->deltapos.sub(pl->newpos);
            interppos(pl);
        }
    }

    bool hasbounced(physent* d, float secs, float elasticity, float waterfric, float grav)
    {
        // Collision checks.
        if (d->physstate != PHYS_BOUNCE && collide(d, vec(0, 0, 0), 0, false)) return true;
        int mat = lookupmaterial(vec(d->o.x, d->o.y, d->o.z + (d->aboveeye - d->eyeheight) / 2));
        bool isinwater = isliquid(mat);
        if (isinwater)
        {
            d->vel.z -= grav * mapgravity / 16 * secs;
            d->vel.mul(max(1.0f - secs / waterfric, 0.0f));
        }
        else d->vel.z -= grav * mapgravity * secs;
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
            else if (collideplayer) break;
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

    bool isbouncing(physent* d, float elasticity, float waterfric, float grav)
    {
        if (physsteps <= 0)
        {
            interppos(d);
            return false;
        }

        d->o = d->newpos;
        bool hitplayer = false;
        loopi(physsteps - 1)
        {
            if (hasbounced(d, physframetime / 1000.0f, elasticity, waterfric, grav)) hitplayer = true;
        }
        d->deltapos = d->o;
        if (hasbounced(d, physframetime / 1000.0f, elasticity, waterfric, grav)) hitplayer = true;
        d->newpos = d->o;
        d->deltapos.sub(d->newpos);
        interppos(d);
        return !hitplayer;
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
        if (lookupmaterial(d->feetpos(-1)) & MAT_GLASS)
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

    void triggerphysicsevent(physent* pl, int event, int material, vec origin)
    {
        if (pl->state > CS_DEAD) return;
        gameent* e = (gameent*)pl;
        switch (event)
        {
            case PHYSEVENT_JUMP:
            {
                if (material & MAT_WATER || !(pl == self || pl->type != ENT_PLAYER || ((gameent*)pl)->ai))
                {
                    break;
                }
                if (!pl->timeinair) msgsound(S_JUMP1, pl);
                else msgsound(S_JUMP2, pl);
                break;
            }

            case PHYSEVENT_LAND_SHORT:
            case PHYSEVENT_FOOTSTEP:
            {
                if (!(pl == self || pl->type != ENT_PLAYER || ((gameent*)pl)->ai)) break;
                triggerfootsteps(e, event != PHYSEVENT_FOOTSTEP);
                e->lastfootleft = e->lastfootright = vec(-1, -1, -1);
                break;
            }

            case PHYSEVENT_LAND_MEDIUM:
            {
                if (!(pl == self || pl->type != ENT_PLAYER || ((gameent*)pl)->ai)) break;
                msgsound(material & MAT_WATER ? S_LAND_WATER : S_LAND, pl);
                e->lastland = lastmillis;
                e->lastfootleft = e->lastfootright = vec(-1, -1, -1);
                break;
            }

            case PHYSEVENT_RAGDOLL_COLLIDE:
            {
                playsound(S_CORPSE, NULL, origin.iszero() ? (pl == self ? NULL : &pl->o) : &origin);
                break;
            }

            case PHYSEVENT_LIQUID_IN:
            {
                playsound(material == MAT_LAVA ? S_LAVA_IN : S_WATER_IN, NULL, origin.iszero() ? (pl == self ? NULL : &pl->o) : &origin);
                break;
            }

            case PHYSEVENT_LIQUID_OUT:
            {
                if (material == MAT_LAVA) break;
                playsound(S_WATER_OUT, NULL, origin.iszero() ? (pl == self ? NULL : &pl->o) : &origin);
                break;
            }

            default: break;
        }
    }

    void dynentcollide(physent* d, physent* o, const vec& dir)
    {
        if (d->type == ENT_AI)
        {
            if (dir.z > 0) stackmonster((monster*)d, o);
        }
    }

#define dir(name,v,d,s,os) ICOMMAND(name, "D", (int *down), { self->s = *down != 0; self->v = self->s ? d : (self->os ? -(d) : 0); });

    dir(backward, move, -1, k_down, k_up);
    dir(forward, move, 1, k_up, k_down);
    dir(left, strafe, 1, k_left, k_right);
    dir(right, strafe, -1, k_right, k_left);

    void dojump(int down)
    {
        if (!down || game::canjump())
        {
            if (self->physstate > PHYS_FLOAT && self->crouched()) self->crouching = 0;
            else self->jumping = down != 0;
        }
    }
    ICOMMAND(jump, "D", (int* down),
    {
        dojump(*down);
    });

    void docrouch(int down)
    {
        if (!down) self->crouching = abs(self->crouching);
        else if (game::cancrouch()) self->crouching = -1;
    }
    ICOMMAND(crouch, "D", (int* down),
    {
        docrouch(*down);
    });
}

