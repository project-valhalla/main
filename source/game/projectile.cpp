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
    namespace projectile
    {
        vector<ProjEnt*> projectiles, attackProjectiles, junkProjectiles;

        ProjEnt* getprojectile(const int id, gameent* owner)
        {
            loopv(projectiles)
            {
                ProjEnt* proj = projectiles[i];
                if (proj->id == id && (!owner || proj->owner == owner))
                {
                    return proj;
                }
            }
            return NULL;
        }

        void setprojectilemodel(ProjEnt& proj)
        {
            if (!projs[proj.projectile].directory)
            {
                return;
            }

            proj.setVariant();
            if (proj.variant)
            {
                defformatstring(variantname, "%s/%02d", projs[proj.projectile].directory, proj.variant);
                copystring(proj.model, variantname);
            }
            else
            {
                copystring(proj.model, projs[proj.projectile].directory);
            }
        }

        void makeprojectile(gameent* owner, const vec& from, const vec& to, const bool isLocal, const int id, const int attack, const int type, const int lifetime, const int speed, const float gravity, const float elasticity)
        {
            ProjEnt& proj = *projectiles.add(new ProjEnt);
            proj.set(type);
            addprojectile(proj);

            proj.owner = owner;
            proj.o = from;
            proj.from = from;
            proj.to = to;
            proj.setradius();
            proj.isLocal = isLocal;
            proj.id = id;
            proj.attack = attack;
            proj.lifetime = lifetime;
            proj.speed = speed;
            proj.gravity = gravity;
            proj.elasticity = elasticity;

            setprojectilemodel(proj);

            vec dir(to);
            dir.sub(from).safenormalize();
            proj.vel = dir;
            if (proj.flags & ProjFlag_Bounce)
            {
                proj.vel.mul(speed);
            }

            avoidcollision(&proj, dir, owner, 0.1f);

            if (proj.flags & ProjFlag_Weapon)
            {
                proj.offset = weapon::hudgunorigin(attacks[proj.attack].gun, from, to, owner);
            }
            if (proj.flags & ProjFlag_Bounce)
            {
                if (proj.flags & ProjFlag_Weapon)
                {
                    if (owner == hudplayer() && !camera::isthirdperson())
                    {
                        proj.offset.sub(owner->o).rescale(16).add(owner->o);
                    }
                }
                else proj.offset = from;
            }

            const vec o = proj.flags & ProjFlag_Bounce ? proj.o : from;
            proj.offset.sub(o);

            proj.offsetMillis = OFFSET_MILLIS;

            if (proj.flags & ProjFlag_Bounce)
            {
                proj.resetinterp();
            }

            proj.lastPosition = owner->o;

            proj.checkliquid();
            proj.setsounds();
            proj.millis = lastmillis;
        }

        void applybounceeffects(ProjEnt* proj, const vec& surface)
        {
            if (proj->inwater)
            {
                return;
            }
            if (proj->vel.magnitude() > 5.0f)
            {
                if (validsound(proj->bounceSound))
                {
                    playsound(proj->bounceSound, NULL, &proj->o, NULL, 0, 0, 0, -1);
                }
            }
            switch (proj->projectile)
            {
                case Projectile_Rocket2:
                {
                    particle_splash(PART_SPARK2, 20, 150, proj->o, 0xFFC864, 0.3f, 250, 1);
                    break;
                }
                case Projectile_Gib:
                {
                    if (weapon::blood)
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
            if (d->type != ENT_PROJECTILE)
            {
                return;
            }
            ProjEnt* proj = (ProjEnt*)d;
            proj->bounces++;
            const int maxBounces = projs[proj->projectile].maxbounces;
            if ((maxBounces && proj->bounces > maxBounces) || lastmillis - proj->lastBounce < 100)
            {
                return;
            }
            applybounceeffects(proj, surface);
            proj->lastBounce = lastmillis;
        }

        void collidewithentity(physent* bouncer, physent* collideEntity)
        {
            /*
             * When a projectile collides with another alive entity.
             * Nothing yet.
             */
            if (bouncer->state != CS_ALIVE || collideEntity->state != CS_ALIVE)
            {
                return;
            }
            ProjEnt* proj = (ProjEnt*)bouncer;
            if (collideEntity->type == ENT_PROJECTILE)
            {
                ProjEnt* collideProj = (ProjEnt*)collideEntity;
                const int attack = proj->attack;
                kill(proj, collideProj->attack);
                if (collideProj->isLocal)
                {
                    kill(collideProj, attack);
                }
            }
        }

        float projectiledistance(dynent* o, vec& dir, const vec& v, const vec& velocity, const int flags)
        {
            vec middle = o->o;
            middle.z += (o->aboveeye - o->eyeheight) / 2;
            if (flags & ProjFlag_Linear)
            {
                dir = vec(middle).sub(v).add(vec(velocity).mul(5)).safenormalize();
                const float low = min(o->o.z - o->eyeheight + o->radius, middle.z);
                const float high = max(o->o.z + o->aboveeye - o->radius, middle.z);
                vec closest(o->o.x, o->o.y, clamp(v.z, low, high));

                return max(closest.dist(v) - o->radius, 0.0f);
            }
            float distance = middle.dist(v, dir);
            dir.div(distance);
            if (distance < 0)
            {
                distance = 0;
            }
            return distance;
        }

        void stain(vec dir, const vec& pos, const int attack)
        {
            const vec negdir = vec(dir).neg();
            float radius = attacks[attack].exprad;
            addstain(STAIN_SCORCH, pos, negdir, radius * 0.75f);
            if (lookupmaterial(pos) & MAT_WATER)
            {
                // Glow in water is unnecessary.
                return;
            }
            const int gun = attacks[attack].gun;
            if (gun != GUN_ROCKET)
            {
                const int color = gun == GUN_PULSE ? 0xEE88EE : (gun == GUN_GRENADE ? 0x74BCF9 : 0x00FFFF);
                addstain(STAIN_GLOW2, pos, negdir, radius * 0.5f, color);
            }
        }

        bool candealdamage(dynent* o, ProjEnt& proj, const vec& v)
        {
            if (betweenrounds || o->state != CS_ALIVE)
            {
                return false;
            }
            if (isattackprojectile(proj.projectile) && weapon::isintersecting(o, proj.o, v, attacks[proj.attack].margin))
            {
                vec dir;
                projectiledistance(o, dir, v, proj.vel, proj.flags);
                weapon::registerhit(o, proj.owner, o->o, dir, attacks[proj.attack].damage, proj.attack, 0, 1, Hit_Direct);
                return true;
            }
            return false;
        }

        VARP(maxdebris, 10, 60, 1000);

        void addexplosioneffects(gameent* owner, const int attack, const vec& v)
        {
            // Explosion particles.
            int explosioncolor = 0x50CFE5, explosiontype = PART_EXPLOSION1, fade = 400;
            float minsize = 0.10f, maxsize = attacks[attack].exprad * 1.15f;
            vec explosionlightcolor = vec(1.0f, 3.0f, 4.0f);
            const bool isInWater = (lookupmaterial(v) & MATF_VOLUME) == MAT_WATER;
            switch (attack)
            {
                case ATK_ROCKET1:
                case ATK_ROCKET2:
                case ATK_ROCKET3:
                {
                    explosioncolor = 0xD47F00;
                    explosionlightcolor = vec(0.831f, 0.498f, 0.0f);
                    if (isInWater)
                    {
                        return;
                    }
                    explosiontype = PART_EXPLOSION3;
                    particle_flare(v, v, 280, PART_EXPLODE1 + rnd(3), 0xFFC864, 35.0f, NULL, 56.0f);
                    particle_splash(PART_SPARK2, 100, 250, v, explosioncolor, 0.10f + rndscale(0.50f), 600, 1);
                    particle_splash(PART_SMOKE, 100, 280, v, 0x222222, 10.0f, 250, 200);
                    break;
                }

                case ATK_PULSE1:
                {
                    explosioncolor = 0xEE88EE;
                    explosionlightcolor = vec(0.933f, 0.533f, 0.933f);
                    fade = 250;
                    if (isInWater)
                    {
                        particle_flare(v, v, 300, PART_ELECTRICITY, explosioncolor, 5.0f, NULL, 12.0f);
                        return;
                    }
                    explosiontype = PART_EXPLOSION2;
                    particle_splash(PART_SPARK2, 5 + rnd(20), 200, v, explosioncolor, 0.08f + rndscale(0.35f), 400, 2);
                    particle_flare(v, v, 250, PART_EXPLODE1 + rnd(2), 0xF1B4F1, 0.5f, NULL, 15.0f);
                    particle_splash(PART_SMOKE, 60, 180, v, 0x222222, 2.5f + rndscale(3.8f), 180, 60);
                    break;
                }

                case ATK_GRENADE1:
                case ATK_GRENADE2:
                case ATK_GRENADE3:
                {
                    explosioncolor = 0x74BCF9;
                    explosionlightcolor = vec(0.455f, 0.737f, 0.976f);
                    explosiontype = PART_EXPLOSION2;
                    particle_flare(v, v, 280, PART_ELECTRICITY, 0x49A7F7, 30.0f);
                    if (attack == ATK_GRENADE3)
                    {
                        fade = 400;
                        particle_splash(PART_SMOKE, 60, 500, v, 0x02448F, 30.0f, 180, 60, 0.1f);
                    }
                    else
                    {
                        fade = 200;
                    }
                    break;
                }

                case ATK_PISTOL2:
                case ATK_PISTOL3:
                {
                    explosioncolor = 0x00FFFF;
                    explosionlightcolor = vec(0.0f, 1.0f, 1.0f);
                    minsize = attacks[attack].exprad * 1.15f;
                    maxsize = 0;
                    if (attack == ATK_PISTOL2 && isInWater)
                    {
                        particle_flare(v, v, 300, PART_ELECTRICITY, explosioncolor, 5.0f, NULL, 12.0f);
                        return;
                    }
                    particle_fireball(v, 1.0f, PART_EXPLOSION2, attack == ATK_PISTOL2 ? 200 : 500, 0x00FFFF, attacks[attack].exprad);
                    particle_splash(PART_SPARK2, 50, 180, v, 0x00FFFF, 0.18f, 500);
                    if (attack == ATK_PISTOL3)
                    {
                        particle_flare(v, v, 600, PART_EXPLODE1, explosioncolor, 50.0f);
                    }
                    break;
                }
                default: break;
            }
            particle_fireball(v, maxsize, explosiontype, fade, explosioncolor, minsize);
            adddynlight(v, attacks[attack].exprad * 3, explosionlightcolor, fade, 40);
            playsound(attacks[attack].impactsound, NULL, &v);
            // Spawn debris projectiles.
            if (!isInWater) // Debris in water are unnecessary.
            {
                const int numdebris = rnd(maxdebris - 5) + 5;
                const vec debrisvel = vec(owner->o).sub(v).safenormalize();
                vec debrisorigin(v);
                if (attack == ATK_ROCKET1)
                {
                    debrisorigin.add(vec(debrisvel).mul(8));
                }
                if (numdebris && attacks[attack].gun == GUN_ROCKET)
                {
                    loopi(numdebris)
                    {
                        spawnbouncer(debrisorigin, owner, Projectile_Debris);
                    }
                }
            }
            // Shake the screen if close enough to the explosion.
            vec ray = vec(0, 0, 0);
            if (raycubelos(camera1->o, v, ray) && camera1->o.dist(v) <= attacks[attack].exprad * 3)
            {
                camera::camera.addevent(self, camera::CameraEvent_Shake, attacks[attack].damage);
            }
        }

        void calculatesplashdamage(dynent* o, const vec& position, const vec& velocity, gameent* at, const int attack, const int flags)
        {
            if (o->state != CS_ALIVE && o->state != CS_DEAD)
            {
                return;
            }
            vec dir;
            const float distance = projectiledistance(o, dir, position, velocity, flags);
            const int damage = static_cast<int>(attacks[attack].damage * (1 - distance / EXP_DISTSCALE / attacks[attack].exprad));
            if (distance < attacks[attack].exprad)
            {
                if (o->state == CS_ALIVE)
                {
                    weapon::registerhit(o, at, o->o, dir, damage, attack, distance, 1);
                }
                else
                {
                    physics::pushRagdoll(o, dir);
                }
            }
        }

        void applyradialeffect(const vec& position, const vec& velocity, gameent* owner, dynent* safe, const int attack, const int flags)
        {
            const int entityFlags = DYN_PLAYER | DYN_AI | DYN_PROJECTILE;
            const int numdyn = numdynents(entityFlags);
            loopi(numdyn)
            {
                dynent* o = iterdynents(i, entityFlags);
                if (o->o.reject(position, o->radius + attacks[attack].exprad) || (safe && o == safe))
                {
                    continue;
                }
                calculatesplashdamage(o, position, velocity, owner, attack, flags);
            }
            physics::pushragdolls(position, attacks[attack].exprad);
        }

        void explodeprojectile(ProjEnt& proj, const vec& v, const bool isLocal)
        {
            stain(proj.vel, proj.flags & ProjFlag_Linear ? v : proj.offsetposition(), proj.attack);
            const vec pos = proj.flags & ProjFlag_Linear ? v : proj.o;
            addexplosioneffects(proj.owner, proj.attack, pos);
            if (betweenrounds || !isLocal)
            {
                return;
            }
            applyradialeffect(pos, proj.vel, proj.owner, &proj, proj.attack, proj.flags);
        }

        void explode(gameent* owner, const int attack, const vec& position, const vec& velocity)
        {
            addexplosioneffects(owner, attack, position);
            applyradialeffect(position, velocity, owner, NULL, attack, 0);
        }

        void explodeeffects(gameent* d, const bool isLocal, const int id)
        {
            if (isLocal)
            {
                return;
            }
            if (projectiles.length())
            {
                loopv(projectiles)
                {
                    ProjEnt& proj = *projectiles[i];
                    if (!validatk(proj.attack) || !proj.owner || !d || proj.isLocal)
                    {
                        continue;
                    }
                    if (proj.owner == d && proj.id == id)
                    {
                        const vec pos = proj.flags & ProjFlag_Bounce ?
                                        proj.offsetposition() :
                                        vec(proj.offset).mul(proj.offsetMillis / float(OFFSET_MILLIS)).add(proj.o);
                        explodeprojectile(proj, pos, proj.isLocal);
                        removeprojectile(proj);
                        delete projectiles.remove(i);
                        break;
                    }
                }
            }
        }

        void hit(const int damage, ProjEnt* proj, const int attack, const vec& velocity)
        {
            if (!proj || !isvalidprojectile(proj->projectile) || proj->flags & ProjFlag_Invincible)
            {
                return;
            }
            if (proj->isLoyal(attack))
            {
                return;
            }
            if (!velocity.iszero())
            {
                proj->hitpush(damage, attack, velocity);
            }
            proj->dodamage(damage);
        }

        void kill(ProjEnt* proj, const int attack)
        {
            if (!proj || proj->flags & ProjFlag_Invincible || proj->state != CS_ALIVE)
            {
                return;
            }
            if (proj->isLoyal(attack))
            {
                return;
            }
            proj->kill();
        }

        void registerhit(dynent* target, gameent* actor, const vec& velocity, const int damage, const int attack, const float dist, const int rays, const int flags)
        {
            ProjEnt* proj = (ProjEnt*)target;
            if (!proj)
            {
                return;
            }
            if (!m_mp(gamemode))
            {
                // Damage the projectile locally.
                hit(damage, proj, attack, velocity);
            }
            else
            {
                hitmsg& hit = weapon::hits.add();
                hit.target = proj->owner->clientnum;
                hit.lifesequence = -1;
                hit.dist = int(dist * DMF);
                hit.rays = rays;
                hit.flags = flags;
                hit.id = proj->id;
                hit.dir = ivec(vec(velocity).mul(DNF));
            }
        }

        void handleliquidtransitions(ProjEnt& proj)
        {
            const bool isInWater = ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_WATER);
            const bool hasWaterTransitions = proj.projectile != Projectile_Debris;
            if (isInWater && projs[proj.projectile].flags & ProjFlag_Quench)
            {
                proj.state = CS_DEAD;
                if (!hasWaterTransitions)
                {
                    return;
                }
            }
            const int transition = physics::liquidtransition(&proj, lookupmaterial(proj.o), isInWater);
            if (transition > 0)
            {
                int impactsound = -1;
                if (proj.radius >= 1.0f)
                {
                    particle_splash(PART_WATER, 200, 250, proj.o, 0xFFFFFF, 0.09f, 500, 1);
                    particle_splash(PART_SPLASH, 10, 80, proj.o, 0xFFFFFF, 7.0f, 250, -1);
                    if (transition == LiquidTransition_In)
                    {
                        impactsound = S_IMPACT_WATER_PROJ;
                    }
                }
                else
                {
                    particle_splash(PART_WATER, 30, 250, proj.o, 0xFFFFFF, 0.05f, 500, 1);
                    particle_splash(PART_SPLASH, 10, 80, proj.o, 0xFFFFFF, 1.0f, 150, -1);
                    if (transition == LiquidTransition_In)
                    {
                        impactsound = S_IMPACT_WATER;
                    }
                }
                if (validsound(impactsound))
                {
                    playsound(impactsound, NULL, &proj.o);
                }
                proj.lastPosition = proj.o;
            }
        }

        void checkloopsound(ProjEnt* proj)
        {
            if (!validsound(proj->loopSound))
            {
                return;
            }
            if (!proj->state != CS_DEAD)
            {
                proj->loopChannel = playsound(proj->loopSound, NULL, &proj->o, NULL, 0, -1, 100, proj->loopChannel);
            }
            else
            {
                stopsound(proj->loopSound, proj->loopChannel);
            }
        }

        void checklifetime(ProjEnt& proj, const int time)
        {
            if (isattackprojectile(proj.projectile))
            {
                if ((proj.lifetime -= time) < 0)
                {
                    proj.state = CS_DEAD;
                }
            }
            else if (proj.flags & ProjFlag_Junk)
            {
                // Cheaper variable rate physics for debris, gibs and other "junk" projectiles.
                for (int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if ((proj.lifetime -= qtime) < 0 || (proj.flags & ProjFlag_Bounce && physics::hasbounced(&proj, qtime / 1000.0f, 0.5f, 0.4f, 0.7f)))
                    {
                        proj.state = CS_DEAD;
                    }
                }
            }
        }

        void addprojectileeffects(ProjEnt& proj, const vec& oldPosition)
        {
            handleliquidtransitions(proj);
            const vec position = proj.flags & ProjFlag_Bounce ? oldPosition : vec(proj.offset).mul(proj.offsetMillis / float(OFFSET_MILLIS)).add(oldPosition);
            int tailColor = 0xFFFFFF;
            float tailSize = 2.0f, tailMinLength = 25.0f;
            const bool hasEnoughVelocity = proj.vel.magnitude() > 30.0f;
            if (proj.inwater)
            {
                if (hasEnoughVelocity || proj.flags & ProjFlag_Linear)
                {
                    regular_particle_splash(PART_BUBBLE, 1, 200, position, 0xFFFFFF, proj.radius, 8, 50, 1);
                }
                return;
            }
            else switch (proj.projectile)
            {
                case Projectile_Grenade:
                case Projectile_Grenade2:
                {
                    if (hasEnoughVelocity)
                    {
                        regular_particle_splash(PART_RING, 1, 200, position, 0x74BCF9, 1.0f, 1, 500);
                    }
                    if (proj.projectile == Projectile_Grenade2 && proj.lifetime < attacks[proj.attack].lifetime - 100)
                    {
                        particle_flare(proj.lastPosition, position, 500, PART_TRAIL_STRAIGHT, 0x74BCF9, 0.4f);
                    }
                    proj.lastPosition = position;
                    break;
                }
                case Projectile_Rocket:
                {
                    tailColor = 0xFFC864;
                    tailMinLength = 90.0f;
                    if (proj.lifetime <= attacks[proj.attack].lifetime / 2)
                    {
                        tailSize *= 2;
                    }
                    else
                    {
                        tailSize = 1.5f;
                        regular_particle_splash(PART_SMOKE, 3, 300, position, 0x303030, 2.4f, 50, -20);
                    }
                    particle_flare(position, position, 1, PART_MUZZLE_FLASH3, tailColor, 1.0f + rndscale(tailSize * 2));
                    break;
                }
                case Projectile_Rocket2:
                {
                    if (hasEnoughVelocity)
                    {
                        regular_particle_splash(PART_SMOKE, 5, 200, position, 0x555555, 1.60f, 10, 500);
                    }
                    if (proj.lifetime < attacks[proj.attack].lifetime - 100)
                    {
                        particle_flare(proj.lastPosition, position, 500, PART_TRAIL_STRAIGHT, 0xFFC864, 0.4f);
                    }
                    proj.lastPosition = position;
                    break;
                }
                case Projectile_Pulse:
                {
                    tailColor = 0xDD88DD;
                    const float tailGrowth = clamp(static_cast<float>(lastmillis - proj.millis) / 50.0f, 0.0f, 1.0f);
                    tailSize = lerp(0.2f, 2.0f, tailGrowth);
                    particle_flare(position, position, 1, PART_ORB, tailColor, tailSize);
                    break;
                }
                case Projectile_Plasma:
                {
                    tailColor = 0x00FFFF;
                    const float tailGrowth = clamp(static_cast<float>(lastmillis - proj.millis) / 1000.0f, 0.0f, 1.0f);
                    tailSize = 5.0f * ease::outelastic(tailGrowth);
                    particle_flare(position, position, 1, PART_ORB, tailColor, tailSize);
                    break;
                }
                case Projectile_Gib:
                {
                    if (weapon::blood && hasEnoughVelocity)
                    {
                        regular_particle_splash(PART_BLOOD, 0 + rnd(4), 400, position, getbloodcolor(proj.owner), 1.0f, 25);
                    }
                    break;
                }
                case Projectile_Debris:
                {
                    if (!hasEnoughVelocity)
                    {
                        break;
                    }
                    regular_particle_splash(PART_SMOKE, 5, 100, position, 0x222222, 1.80f, 30, 500);
                    regular_particle_splash(PART_SPARK, 1, 40, position, 0x903020, 1.20f, 10, 500);
                    particle_flare(proj.o, proj.o, 1, PART_EDIT, 0xF69D19, 0.5 + rndscale(2.0f));
                    break;
                }
                case Projectile_Bullet:
                {
                    const int gun = attacks[proj.attack].gun;
                    tailColor = gun == GUN_PISTOL ? 0x00FFFF : (gun == GUN_RAIL ? 0x77DD77 : 0xFFC864);
                    tailSize = gun == GUN_RAIL ? 1.0f : 0.75f;
                    const gameent* hud = followingplayer(self);
                    tailMinLength = proj.owner == hud ? 75.0f : 45.0f;
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

        void updateprojectiles(const int time)
        {
            if (projectiles.empty())
            {
                return;
            }
            loopv(projectiles)
            {
                ProjEnt& proj = *projectiles[i];
                const vec pos = proj.updateposition(time);
                vec old(proj.o);
                if (proj.flags & ProjFlag_Linear)
                {
                    weapon::hits.setsize(0);
                    if (proj.isLocal)
                    {
                        const vec halfdv = vec(proj.dv).mul(0.5f);
                        const vec bo = vec(proj.o).add(halfdv);
                        const float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[proj.attack].margin;
                        if (!betweenrounds)
                        {
                            const int flags = DYN_PLAYER | DYN_AI | DYN_PROJECTILE;
                            loopj(numdynents(flags))
                            {
                                dynent* o = iterdynents(j, flags);
                                if (proj.owner == o || o == &proj || o->o.reject(bo, o->radius + br)) continue;
                                if (o->type == ENT_PROJECTILE)
                                {
                                    ProjEnt* collideProj = (ProjEnt*)o;
                                    const int attack = proj.attack;
                                    kill(&proj, collideProj->attack);
                                    if (collideProj->isLocal)
                                    {
                                        kill(collideProj, attack);
                                    }
                                }
                                if (candealdamage(o, proj, pos))
                                {
                                    proj.state = CS_DEAD;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (proj.state != CS_DEAD)
                {
                    checklifetime(proj, time);
                    if ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_LAVA)
                    {
                        proj.state = CS_DEAD;
                    }
                    else
                    {
                        if (proj.flags & ProjFlag_Linear)
                        {
                            if (proj.flags & ProjFlag_Impact && proj.dist < 4)
                            {
                                if (proj.o != proj.to) // If original target was moving, re-evaluate endpoint.
                                {
                                    if (raycubepos(proj.o, proj.vel, proj.to, 0, RAY_CLIPMAT | RAY_ALPHAPOLY) >= 4)
                                    {
                                        continue;
                                    }
                                }
                                proj.state = CS_DEAD;
                            }
                        }
                        if (isattackprojectile(proj.projectile))
                        {
                            if (proj.flags & ProjFlag_Bounce)
                            {
                                const bool isBouncing = physics::isbouncing(&proj, proj.elasticity, 0.5f, proj.gravity);
                                const bool hasBounced = projs[proj.projectile].maxbounces && proj.bounces >= projs[proj.projectile].maxbounces;
                                if (!isBouncing || hasBounced)
                                {
                                    proj.state = CS_DEAD;
                                }
                            }
                        }
                    }
                    addprojectileeffects(proj, pos);
                }
                checkloopsound(&proj);
                if (proj.state == CS_DEAD)
                {
                    if (isattackprojectile(proj.projectile))
                    {
                        explodeprojectile(proj, pos, proj.isLocal);
                        if (proj.isLocal)
                        {
                            addmsg(N_EXPLODE, "rci3iv", proj.owner, lastmillis - maptime, proj.attack, proj.id, weapon::hits.length(), weapon::hits.length() * sizeof(hitmsg) / sizeof(int), weapon::hits.getbuf());
                        }
                    }
                    removeprojectile(proj);
                    delete projectiles.remove(i--);
                }
                else
                {
                    if (proj.flags & ProjFlag_Bounce)
                    {
                        proj.roll += old.sub(proj.o).magnitude() / (4 * RAD);
                        proj.offsetMillis = max(proj.offsetMillis - time, 0);
                        proj.limitOffset();
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
                ProjEnt& proj = *projectiles[i];
                if (proj.flags & ProjFlag_Junk)
                {
                    continue;
                }
                vec pos(proj.o);
                pos.add(vec(proj.offset).mul(proj.offsetMillis / float(OFFSET_MILLIS)));
                vec lightColor = vec(0, 0, 0);
                switch (proj.projectile)
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

        void spawnbouncer(const vec& from, gameent* d, const int type)
        {
            vec to(rnd(100) - 50, rnd(100) - 50, rnd(100) - 50);
            float elasticity = 0.6f;
            if (isejectedprojectile(type))
            {
                to = vec(-50, 1, rnd(30) - 15);
                to.rotate_around_z(d->yaw * RAD);
                elasticity = 0.4f;
            }
            if (to.iszero())
            {
                to.z += 1;
            }
            to.normalize();
            to.add(from);
            makeprojectile(d, from, to, true, 0, -1, type, type == Projectile_Debris ? 400 : rnd(1000) + 1000, rnd(100) + 20, 0.3f + rndscale(0.8f), elasticity);
        }

        void preloadprojectiles()
        {
            loopi(Projectile_Max)
            {
                const char* file = projs[i].directory;
                if (!file)
                {
                    continue;
                }
                if (projs[i].variants > 0)
                {
                    for (int j = 0; j < projs[i].variants; j++)
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

        vec manipulatemodel(ProjEnt& proj, float& yaw, float& pitch)
        {
            if (proj.flags & ProjFlag_Bounce)
            {
                const vec pos = proj.offsetposition();
                const vec vel(proj.vel);
                if (vel.magnitude() <= 25.0f)
                {
                    yaw = proj.lastYaw;
                }
                else
                {
                    vectoyawpitch(vel, yaw, pitch);
                    yaw += 90;
                    proj.lastYaw = yaw;
                }
                return pos;
            }
            else
            {
                const float dist = min(proj.o.dist(proj.to) / 32.0f, 1.0f);
                const vec pos = vec(proj.o).add(vec(proj.offset).mul(dist * proj.offsetMillis / float(OFFSET_MILLIS)));
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
                ProjEnt& proj = *projectiles[i];
                if (!proj.model[0])
                {
                    continue;
                }
                const vec pos = manipulatemodel(proj, yaw, pitch);
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

        void addprojectile(ProjEnt& proj)
        {
            if (proj.flags & ProjFlag_Junk)
            {
                junkProjectiles.add(&proj);
            }
            else if (proj.flags & ProjFlag_Weapon)
            {
                attackProjectiles.add(&proj);
            }
        }

        void removeprojectile(ProjEnt& proj)
        {
            if (proj.flags & ProjFlag_Junk)
            {
                junkProjectiles.removeobj(&proj);
            }
            else if (proj.flags & ProjFlag_Weapon)
            {
                attackProjectiles.removeobj(&proj);
            }
        }

        void removeprojectiles(gameent* owner)
        {
            if (!owner)
            {
                junkProjectiles.setsize(0);
                attackProjectiles.setsize(0);
                projectiles.deletecontents();
            }
            else
            {
                loopv(projectiles)
                {
                    ProjEnt& proj = *projectiles[i];
                    if (proj.owner != owner)
                    {
                        continue;
                    }
                    removeprojectile(proj);
                    delete projectiles[i];
                    projectiles.remove(i--);
                }
            }
        }

        void avoidprojectiles(ai::avoidset& obstacles, const float radius)
        {
            loopv(projectiles)
            {
                ProjEnt& proj = *projectiles[i];
                obstacles.avoidnear(NULL, proj.o.z + attacks[proj.attack].exprad + 1, proj.o, radius + attacks[proj.attack].exprad);
            }
        }
    }
};
