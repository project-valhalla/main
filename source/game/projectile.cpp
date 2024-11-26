/*
 * =====================================================================
 * projectile.cpp
 * Manages the creation and behavior of projectiles in the game.
 * =====================================================================
 *
 * Projectiles include:
 * - Rockets and grenades fired by weapons to attack players.
 * - Objects affected by physics, such as debris or giblets.
 *
 * Each projectile type can have unique properties and behaviours:
 * - Trajectory/movement patterns.
 * - Specific interaction with physics, like bouncing, sticking to surfaces, etc.
 * - Specific effects on impact or over time (e.g. explosions/particle effects).
 * - Health and countdown values that determine durability and lifespan.
 * - Potential for players to pick them up.
 *
 * Projectiles are implemented using a specialized structure.
 * This is based on the "dynent" entity, which itself is based on "physent" (from which all dynamic entities are derived).
 * This allows for precise control over their dynamics and interactions in the game world,
 * considering that physics do not impact every entity in the same way.
 *
 * =====================================================================
 */

#include "game.h"

namespace game
{
    vector<projectile *> projectiles;

    void setprojectilemodel(projectile& proj)
    {
        if (!projs[proj.projtype].directory)
        {
            return;
        }

        proj.setvariant();
        if (proj.variant)
        {
            defformatstring(variantname, "%s/%02d", projs[proj.projtype].directory, proj.variant);
            copystring(proj.model, variantname);
        }
        else
        {
            copystring(proj.model, projs[proj.projtype].directory);
        }
    }

    void makeprojectile(gameent *owner, const vec &from, const vec &to, const bool islocal, const int id, const int atk, const int type, const int lifetime, const int speed, const float gravity, const float elasticity)
    {
        projectile &proj = *projectiles.add(new projectile);
        proj.owner = owner;
        proj.projtype = type;
        proj.setflags();
        proj.o = from;
        proj.from = from;
        proj.to = to;
        proj.setradius();
        proj.islocal = islocal;
        proj.id = islocal ? lastmillis : id;
        proj.atk = atk;
        proj.lifetime = lifetime;
        proj.speed = speed;
        proj.gravity = gravity;
        proj.elasticity = elasticity;

        setprojectilemodel(proj);

        vec dir(to);
        dir.sub(from).safenormalize();
        proj.vel = dir;
        if(proj.flags & ProjFlag_Bounce) proj.vel.mul(speed);

        avoidcollision(&proj, dir, owner, 0.1f);

        if (proj.flags & ProjFlag_Weapon)
        {
            proj.offset = hudgunorigin(attacks[proj.atk].gun, from, to, owner);
        }
        if(proj.flags & ProjFlag_Bounce)
        {
            if (proj.flags & ProjFlag_Weapon)
            {
                if (owner == hudplayer() && !isthirdperson())
                {
                    proj.offset.sub(owner->o).rescale(16).add(owner->o);
                }
            }
            else proj.offset = from;
        }

        const vec o = proj.flags & ProjFlag_Bounce ? proj.o : from;
        proj.offset.sub(o);

        proj.offsetmillis = OFFSET_MILLIS;

        if(proj.flags & ProjFlag_Bounce) proj.resetinterp();

        proj.lastposition = owner->o;

        proj.checkliquid();
        proj.setsounds();
    }

    extern int blood;

    void applybounceeffects(projectile* proj, const vec& surface)
    {
        if (proj->inwater) return;

        if (proj->vel.magnitude() > 5.0f)
        {
            if (validsound(proj->bouncesound))
            {
                playsound(proj->bouncesound, NULL, &proj->o, NULL, 0, 0, 0, -1);
            }
        }
        switch (proj->projtype)
        {
            case Projectile_Rocket2:
            {
                particle_splash(PART_SPARK2, 20, 150, proj->o, 0xFFC864, 0.3f, 250, 1);
                break;
            }
            
            case Projectile_Gib:
            {
                if (blood)
                {
                    addstain(STAIN_BLOOD, vec(proj->o).sub(vec(surface).mul(proj->radius)), surface, 2.96f / proj->bounces, getbloodcolor(proj->owner), rnd(4));
                }
                break;
            }

            default: break;
        }
        
    }

    void bounce(physent* d, const vec& surface)
    {
        if (d->type != ENT_PROJECTILE) return;

        projectile* proj = (projectile*)d;
        proj->bounces++;

        const int maxbounces = projs[proj->projtype].maxbounces;
        if ((maxbounces && proj->bounces > maxbounces) || lastmillis - proj->lastbounce < 100) return;

        applybounceeffects(proj, surface);
        proj->lastbounce = lastmillis;
    }

    void collidewithentity(physent* bouncer, physent* collideentity)
    {
        /*
         * When a projectile collides with another alive entity.
         * 
         * Nothing yet.
         */
    }

    float projectiledistance(dynent* o, vec& dir, const vec& v, const vec& vel, int flags)
    {
        vec middle = o->o;
        middle.z += (o->aboveeye - o->eyeheight) / 2;

        if (flags & ProjFlag_Linear)
        {
            dir = vec(middle).sub(v).add(vec(vel).mul(5)).safenormalize();
            float low = min(o->o.z - o->eyeheight + o->radius, middle.z);
            float high = max(o->o.z + o->aboveeye - o->radius, middle.z);
            vec closest(o->o.x, o->o.y, clamp(v.z, low, high));

            return max(closest.dist(v) - o->radius, 0.0f);
        }

        float distance = middle.dist(v, dir);
        dir.div(distance);
        if (distance < 0) distance = 0;

        return distance;   
    }

    void stain(vec dir, const vec& pos, const int atk)
    {
        vec negdir = vec(dir).neg();
        float radius = attacks[atk].exprad;
        addstain(STAIN_SCORCH, pos, negdir, radius * 0.75f);
        if (lookupmaterial(pos) & MAT_WATER)
        {
            // Glow in water is unnecessary.
            return;
        }
        const int gun = attacks[atk].gun;
        if (gun != GUN_ROCKET)
        {
            int color = gun == GUN_PULSE ? 0xEE88EE : (gun == GUN_GRENADE ? 0x74BCF9 : 0x00FFFF);
            addstain(STAIN_GLOW2, pos, negdir, radius * 0.5f, color);
        }
    }

    bool candealdamage(dynent* o, projectile& proj, const vec& v)
    {
        if (betweenrounds || o->state != CS_ALIVE) return false;
        if (!isintersecting(o, proj.o, v, attacks[proj.atk].margin)) return false;
        if (isweaponprojectile(proj.projtype))
        {
            vec dir;
            projectiledistance(o, dir, v, proj.vel, proj.flags);
            registerhit(o, proj.owner, o->o, dir, attacks[proj.atk].damage, proj.atk, 0, 1);
        }
        return true;
    }

    VARP(maxdebris, 10, 60, 1000);

    void addexplosioneffects(gameent* owner, const int atk, vec v)
    {
        int explosioncolor = 0x50CFE5, explosiontype = PART_EXPLOSION1, fade = 400;
        float minsize = 0.10f, maxsize = attacks[atk].exprad * 1.15f;
        vec explosionlightcolor = vec(1.0f, 3.0f, 4.0f);
        bool iswater = (lookupmaterial(v) & MATF_VOLUME) == MAT_WATER;
        switch (atk)
        {
            case ATK_ROCKET1:
            case ATK_ROCKET2:
            {
                explosioncolor = 0xD47F00;
                explosionlightcolor = vec(0.831f, 0.498f, 0.0f);
                if (iswater) break;
                explosiontype = PART_EXPLOSION3;
                particle_flare(v, v, 280, PART_EXPLODE1 + rnd(3), 0xFFC864, 56.0f);
                particle_splash(PART_SPARK2, 100, 250, v, explosioncolor, 0.10f + rndscale(0.50f), 600, 1);
                particle_splash(PART_SMOKE, 100, 280, v, 0x222222, 10.0f, 250, 200);
                break;
            }

            case ATK_PULSE1:
            {
                explosioncolor = 0xEE88EE;
                explosionlightcolor = vec(0.933f, 0.533f, 0.933f);
                if (iswater)
                {
                    particle_flare(v, v, 300, PART_ELECTRICITY, explosioncolor, 12.0f);
                    return;
                }
                explosiontype = PART_EXPLOSION2;
                particle_splash(PART_SPARK2, 5 + rnd(20), 200, v, explosioncolor, 0.08f + rndscale(0.35f), 400, 2);
                particle_flare(v, v, 250, PART_EXPLODE1 + rnd(2), 0xF1B4F1, 15.0f);
                particle_splash(PART_SMOKE, 60, 180, v, 0x222222, 2.5f + rndscale(3.8f), 180, 60);
                break;
            }

            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                explosioncolor = 0x74BCF9;
                explosionlightcolor = vec(0.455f, 0.737f, 0.976f);
                explosiontype = PART_EXPLOSION2;
                fade = 200;
                particle_flare(v, v, 280, PART_ELECTRICITY, 0x49A7F7, 30.0f);
                break;
            }

            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                explosioncolor = 0x00FFFF;
                explosionlightcolor = vec(0.0f, 1.0f, 1.0f);
                if (atk == ATK_PISTOL2 && iswater)
                {
                    particle_flare(v, v, 300, PART_ELECTRICITY, explosioncolor, 12.0f);
                    return;
                }
                minsize = attacks[atk].exprad * 1.15f;
                maxsize = 0;
                particle_fireball(v, 1.0f, PART_EXPLOSION2, atk == ATK_PISTOL2 ? 200 : 500, 0x00FFFF, attacks[atk].exprad);
                particle_splash(PART_SPARK2, 50, 180, v, 0x00FFFF, 0.18f, 500);
                if (atk == ATK_PISTOL_COMBO)
                {
                    particle_flare(v, v, 600, PART_EXPLODE1, explosioncolor, 50.0f);
                }
                break;
            }
            default: break;
        }
        particle_fireball(v, maxsize, explosiontype, fade, explosioncolor, minsize);
        adddynlight(v, attacks[atk].exprad * 3, explosionlightcolor, fade, 40);
        playsound(attacks[atk].impactsound, NULL, &v);
        if (!iswater) // Debris in water are unnecessary.
        {
            const int numdebris = rnd(maxdebris - 5) + 5;
            const vec debrisvel = vec(owner->o).sub(v).safenormalize();
            vec debrisorigin(v);
            if (atk == ATK_ROCKET1)
            {
                debrisorigin.add(vec(debrisvel).mul(8));
            }
            if (numdebris && (attacks[atk].gun == GUN_ROCKET || attacks[atk].gun == GUN_SCATTER))
            {
                loopi(numdebris)
                {
                    spawnbouncer(debrisorigin, owner, Projectile_Debris);
                }
            }
        }
    }

    void applyradialeffect(dynent* o, const vec& v, const vec& vel, gameent* at, const int atk, const int flags)
    {
        if (o->state != CS_ALIVE) return;
        vec dir;
        float distance = projectiledistance(o, dir, v, vel, flags);
        if (distance < attacks[atk].exprad)
        {
            int damage = static_cast<int>(attacks[atk].damage * (1 - distance / EXP_DISTSCALE / attacks[atk].exprad));
            registerhit(o, at, o->o, dir, damage, atk, distance);
        }
    }

    void explodeprojectile(projectile& proj, const vec& v, dynent* safe, const bool islocal)
    {
        stain(proj.vel, proj.flags & ProjFlag_Linear ? v : proj.offsetposition(), proj.atk);
        vec pos = proj.flags & ProjFlag_Linear ? v : proj.o;
        vec offset = proj.flags & ProjFlag_Linear ? v : proj.offsetposition();
        addexplosioneffects(proj.owner, proj.atk, pos);

        if (betweenrounds || !islocal) return;

        const int numdyn = numdynents();
        loopi(numdyn)
        {
            dynent* o = iterdynents(i);
            if (o->o.reject(pos, o->radius + attacks[proj.atk].exprad) || o == safe) continue;
            applyradialeffect(o, pos, proj.vel, proj.owner, proj.atk, proj.flags);
        }
    }

    void explodeeffects(const int atk, gameent* d, const bool islocal, const int id)
    {
        if (islocal) return;
        switch (atk)
        {
            case ATK_ROCKET2:
            case ATK_GRENADE1:
            case ATK_GRENADE2:
            case ATK_PULSE1:
            case ATK_ROCKET1:
            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                loopv(projectiles)
                {
                    projectile& proj = *projectiles[i];
                    if (proj.owner == d && proj.id == id && !proj.islocal)
                    {
                        if (atk == ATK_PISTOL_COMBO) proj.atk = atk;
                        else if (proj.atk != atk) continue;
                        vec pos = proj.flags & ProjFlag_Bounce ? proj.offsetposition() : vec(proj.offset).mul(proj.offsetmillis / float(OFFSET_MILLIS)).add(proj.o);
                        explodeprojectile(proj, pos, &proj, proj.islocal);
                        delete projectiles.remove(i);
                        break;
                    }
                }
                break;
            }
            default: break;
        }
    }

    void handleliquidtransitions(projectile& proj)
    {
        const bool isinwater = ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_WATER);
        if (isinwater && projs[proj.projtype].flags & ProjFlag_Quench)
        {
            proj.isdestroyed = true;
        }
        const int transition = physics::liquidtransition(&proj, lookupmaterial(proj.o), isinwater);
        if (transition > 0)
        {
            particle_splash(PART_WATER, 200, 250, proj.o, 0xFFFFFF, 0.09f, 500, 1);
            particle_splash(PART_SPLASH, 10, 80, proj.o, 0xFFFFFF, 7.0f, 250, -1);
            if (transition == LiquidTransition_In)
            {
                playsound(S_IMPACT_WATER_PROJ, NULL, &proj.o);
            }
            proj.lastposition = proj.o;
        }
    }

    void checkloopsound(projectile* proj)
    {
        if (!validsound(proj->loopsound)) return;
        if (!proj->isdestroyed)
        {
            proj->loopchan = playsound(proj->loopsound, NULL, &proj->o, NULL, 0, -1, 100, proj->loopchan);
        }
        else
        {
            stopsound(proj->loopsound, proj->loopchan);
        }
    }

    void checklifetime(projectile& proj, int time)
    {
        if (isweaponprojectile(proj.projtype))
        {
            if ((proj.lifetime -= time) < 0)
            {
                proj.isdestroyed = true;
            }
        }
        else if(proj.flags & ProjFlag_Junk)
        {
            // Cheaper variable rate physics for debris, gibs and other "junk" projectiles.
            for (int rtime = time; rtime > 0;)
            {
                int qtime = min(80, rtime);
                rtime -= qtime;
                if ((proj.lifetime -= qtime) < 0 || (proj.flags & ProjFlag_Bounce && physics::hasbounced(&proj, qtime / 1000.0f, 0.5f, 0.4f, 0.7f)))
                {
                    proj.isdestroyed = true;
                }
            }
        }
    }

    void addprojectileeffects(projectile& proj, const vec& oldposition)
    {
        handleliquidtransitions(proj);
        const vec position = proj.flags & ProjFlag_Bounce ? oldposition : vec(proj.offset).mul(proj.offsetmillis / float(OFFSET_MILLIS)).add(oldposition);
        int tailColor = 0xFFFFFF;
        float tailSize = 2.0f, tailMinLength = 90.0f;
        const bool hasenoughvelocity = proj.vel.magnitude() > 30.0f;
        if (proj.inwater)
        {
            if (hasenoughvelocity || proj.flags & ProjFlag_Linear)
            {
                regular_particle_splash(PART_BUBBLE, 1, 200, position, 0xFFFFFF, 1.0f, 8, 50, 1);
            }
            return;
        }
        else switch (proj.projtype)
        {
            case Projectile_Grenade:
            case Projectile_Grenade2:
            {
                if (hasenoughvelocity) regular_particle_splash(PART_RING, 1, 200, position, 0x74BCF9, 1.0f, 1, 500);
                if (proj.projtype == Projectile_Grenade2)
                {
                    if (proj.lifetime < attacks[proj.atk].lifetime - 100) particle_flare(proj.lastposition, position, 500, PART_TRAIL_STRAIGHT, 0x74BCF9, 0.4f);
                }
                proj.lastposition = position;
                break;
            }

            case Projectile_Rocket:
            {
                tailColor = 0xFFC864;
                tailSize = 1.5f;
                if (proj.lifetime <= attacks[proj.atk].lifetime / 2)
                {
                    tailSize *= 2;
                }
                else regular_particle_splash(PART_SMOKE, 3, 300, position, 0x303030, 2.4f, 50, -20);
                particle_flare(position, position, 1, PART_MUZZLE_FLASH3, tailColor, 1.0f + rndscale(tailSize * 2));
                break;
            }

            case Projectile_Rocket2:
            {
                if (hasenoughvelocity) regular_particle_splash(PART_SMOKE, 5, 200, position, 0x555555, 1.60f, 10, 500);
                if (proj.lifetime < attacks[proj.atk].lifetime - 100)
                {
                    particle_flare(proj.lastposition, position, 500, PART_TRAIL_STRAIGHT, 0xFFC864, 0.4f);
                }
                proj.lastposition = position;
                break;
            }

            case Projectile_Pulse:
            {
                tailColor = 0xDD88DD;
                particle_flare(position, position, 1, PART_ORB, tailColor, 1.0f + rndscale(tailSize));
                break;
            }

            case Projectile_Plasma:
            {
                tailColor = 0x00FFFF;
                tailSize = 4.5f;
                tailMinLength = 20.0f;
                particle_flare(position, position, 1, PART_ORB, tailColor, tailSize);
                break;
            }

            case Projectile_Gib:
            {
                if (blood && hasenoughvelocity)
                {
                    regular_particle_splash(PART_BLOOD, 0 + rnd(4), 400, position, getbloodcolor(proj.owner), 0.80f, 25);
                }
                break;
            }

            case Projectile_Debris:
            {
                if (!hasenoughvelocity) break;
                regular_particle_splash(PART_SMOKE, 5, 100, position, 0x222222, 1.80f, 30, 500);
                regular_particle_splash(PART_SPARK, 1, 40, position, 0x903020, 1.20f, 10, 500);
                particle_flare(proj.o, proj.o, 1, PART_EDIT, 0xF69D19, 0.5 + rndscale(2.0f));
                break;
            }

            case Projectile_Bullet:
            {
                int gun = attacks[proj.atk].gun;
                tailColor = gun == GUN_PISTOL ? 0x00FFFF : (gun == GUN_RAIL ? 0x77DD77 : 0xFFC864);
                tailSize = gun == GUN_RAIL ? 1.0f : 0.75f;
                tailMinLength = 75.0f;
                break;
            }

            default: break;
        }
        if (proj.flags & ProjFlag_Linear)
        {
            float length = min(tailMinLength, vec(proj.offset).add(proj.from).dist(position));
            vec dir = vec(proj.dv).normalize(), tail = vec(dir).mul(-length).add(position), head = vec(dir).mul(2.4f).add(position);
            particle_flare(tail, head, 1, PART_TRAIL_PROJECTILE, tailColor, tailSize);
        }
    }

    void updateprojectiles(int time)
    {
        if (projectiles.empty()) return;
        loopv(projectiles)
        {
            projectile& proj = *projectiles[i];

            vec pos = proj.updateposition(time);
            vec old(proj.o);

            if (proj.flags & ProjFlag_Linear)
            {
                hits.setsize(0);
                if (proj.islocal)
                {
                    vec halfdv = vec(proj.dv).mul(0.5f), bo = vec(proj.o).add(halfdv);
                    float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[proj.atk].margin;
                    if (!betweenrounds) loopj(numdynents())
                    {
                        dynent* o = iterdynents(j);
                        if (proj.owner == o || o->o.reject(bo, o->radius + br)) continue;
                        if (candealdamage(o, proj, pos))
                        {
                            proj.isdestroyed = true;
                            break;
                        }
                    }
                }
            }
            if (!proj.isdestroyed)
            {
                checklifetime(proj, time);
                if ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_LAVA)
                {
                    proj.isdestroyed = true;
                }
                else
                {
                    if (proj.flags & ProjFlag_Linear)
                    {
                        if (proj.flags & ProjFlag_Impact && proj.dist < 4)
                        {
                            if (proj.o != proj.to) // If original target was moving, re-evaluate endpoint.
                            {
                                if (raycubepos(proj.o, proj.vel, proj.to, 0, RAY_CLIPMAT | RAY_ALPHAPOLY) >= 4) continue;
                            }
                            proj.isdestroyed = true;
                        }
                    }
                    if (isweaponprojectile(proj.projtype))
                    {
                        if (proj.flags & ProjFlag_Bounce)
                        {
                            bool isbouncing = physics::isbouncing(&proj, proj.elasticity, 0.5f, proj.gravity);
                            bool hasbounced = projs[proj.projtype].maxbounces && proj.bounces >= projs[proj.projtype].maxbounces;
                            if (!isbouncing || hasbounced)
                            {
                                proj.isdestroyed = true;
                            }
                        }
                    }
                }         
                addprojectileeffects(proj, pos);
            }
            checkloopsound(&proj);
            if (proj.isdestroyed)
            {
                if (isweaponprojectile(proj.projtype))
                {
                    explodeprojectile(proj, pos, &proj, proj.islocal);
                    if (proj.islocal)
                    {
                        addmsg(N_EXPLODE, "rci3iv", proj.owner, lastmillis - maptime, proj.atk, proj.id - maptime, hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
                    }
                }
                delete projectiles.remove(i--);
            }
            else
            {
                if (proj.flags & ProjFlag_Bounce)
                {
                    proj.roll += old.sub(proj.o).magnitude() / (4 * RAD);
                    proj.offsetmillis = max(proj.offsetmillis - time, 0);
                    proj.limitoffset();
                }
                else
                {
                    proj.o = pos;
                }
            }
        }
    }

    void updateprojectilelights()
    {
        loopv(projectiles)
        {
            projectile& proj = *projectiles[i];
            if (proj.flags & ProjFlag_Junk) continue;
            vec pos(proj.o);
            pos.add(vec(proj.offset).mul(proj.offsetmillis / float(OFFSET_MILLIS)));
            vec lightColor = vec(0, 0, 0);
            switch (proj.projtype)
            {
                case Projectile_Pulse:
                {
                    lightColor = vec(2.0f, 1.5f, 2.0);
                    break;
                }

                case Projectile_Rocket:
                case Projectile_Rocket2:
                {
                    lightColor = vec(1, 0.75f, 0.5f);
                    break;
                }

                case Projectile_Grenade:
                case Projectile_Grenade2:
                {
                    lightColor = vec(0.25f, 0.25f, 1);
                    break;
                }

                case Projectile_Plasma:
                {
                    lightColor = vec(0, 1.50f, 1.50f);
                    break;
                }

                default:
                {
                    lightColor = vec(0.5f, 0.375f, 0.25f);
                    break;
                }
            }
            adddynlight(pos, 35, lightColor);
        }
    }

    void spawnbouncer(const vec& from, gameent* d, int type)
    {
        vec to(rnd(100) - 50, rnd(100) - 50, rnd(100) - 50);
        float elasticity = 0.6f;
        if (type == Projectile_Eject)
        {
            to = vec(-50, 1, rnd(30) - 15);
            to.rotate_around_z(d->yaw * RAD);
            elasticity = 0.4f;
        }
        if (to.iszero()) to.z += 1;
        to.normalize();
        to.add(from);
        makeprojectile(d, from, to, true, 0, -1, type, type == Projectile_Debris ? 400 : rnd(1000) + 1000, rnd(100) + 20, 0.3f + rndscale(0.8f), elasticity);
    }

    void preloadprojectiles()
    {
        loopi(Projectile_Max)
        {
            const char* file = projs[i].directory;
            if (!file) continue;
            if (projs[i].variants > 0)
            {
                for(int j = 0; j < projs[i].variants; j++)
                {
                    string variantname;
                    formatstring(variantname, "%s/%02d", file, j);
                    preloadmodel(variantname);
                }
            }
            else
            {
                preloadmodel(file);
            }
        }
    }

    vec manipulatemodel(projectile& proj, float& yaw, float& pitch)
    {
        if (proj.flags & ProjFlag_Bounce)
        {
            vec pos = proj.offsetposition();
            vec vel(proj.vel);
            if (vel.magnitude() <= 25.0f)
            {
                yaw = proj.lastyaw;
            }
            else
            {
                vectoyawpitch(vel, yaw, pitch);
                yaw += 90;
                proj.lastyaw = yaw;
            }
            return pos;
        }
        else
        {
            const float dist = min(proj.o.dist(proj.to) / 32.0f, 1.0f);
            vec pos = vec(proj.o).add(vec(proj.offset).mul(dist * proj.offsetmillis / float(OFFSET_MILLIS)));
            vec v = dist < 1e-6f ? proj.vel : vec(proj.to).sub(pos).normalize();
            vectoyawpitch(v, yaw, pitch); // The amount of distance in front of the smoke trail needs to change if the model does.
            v.mul(3);
            v.add(pos);
            return v;
        }
    }

    void renderprojectiles()
    {
        float yaw, pitch;
        loopv(projectiles)
        {
            projectile& proj = *projectiles[i];
            if (!proj.model[0])
            {
                continue;
            }
            vec pos = manipulatemodel(proj, yaw, pitch);
            int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED;
            float fade = 1;
            if (proj.flags & ProjFlag_Junk)
            {
                if (proj.lifetime < 400)
                {
                    fade = proj.lifetime / 400.0f;
                }
            }
            rendermodel(proj.model, ANIM_MAPMODEL | ANIM_LOOP, pos, yaw, pitch, proj.roll, cull, NULL, NULL, 0, 0, fade);
        }
    }

    void removeprojectiles(gameent* owner)
    {
        if (!owner)
        {
            projectiles.deletecontents();
        }
        else
        {
            loopv(projectiles) if (projectiles[i]->owner == owner)
            {
                delete projectiles[i];
                projectiles.remove(i--);
            }
        }
    }

    void avoidprojectiles(ai::avoidset &obstacles, const float radius)
    {
        loopv(projectiles)
        {
            projectile &proj = *projectiles[i];
            obstacles.avoidnear(NULL, proj.o.z + attacks[proj.atk].exprad + 1, proj.o, radius + attacks[proj.atk].exprad);
        }
    }
};
