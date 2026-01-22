/*
 * =====================================================================
 * projectile.cpp
 * Manages the creation and behavior of Projectiles in the game.
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
    namespace projectiles
    {
        vector<ProjEnt*> Projectiles, AttackProjectiles;

        // Get projectile by id.
        ProjEnt* get(const int id, const gameent* owner)
        {
            if (Projectiles.length() > 0)
            {
                loopv(Projectiles)
                {
                    ProjEnt* proj = Projectiles[i];
                    if (proj->id != id || proj->owner != owner)
                    {
                        continue;
                    }
                    return proj;
                }
            }
            return nullptr;
        }

        // Filter projectiles by owner.
        static vector<ProjEnt*> filterOwned(const gameent* owner)
        {
            vector<ProjEnt*> owned;
            for (int i = 0; i < Projectiles.length(); i++)
            {
                ProjEnt& proj = *Projectiles[i];
                if (proj.owner != owner)
                {
                    continue;
                }
                owned.add(&proj);
            }
            return owned;
        }

        static void add(ProjEnt& proj)
        {
            if (!isattackprojectile(proj.projectile) || proj.flags & ProjFlag_Invincible)
            {
                return;
            }
            AttackProjectiles.add(&proj);
        }

        static void remove(ProjEnt& proj)
        {
            if (isattackprojectile(proj.projectile))
            {
                AttackProjectiles.removeobj(&proj);
            }
            Projectiles.removeobj(&proj);
            delete &proj;
        }

        void make(gameent* owner, const vec& from, const vec& to, const bool isLocal, const int id, const int attack, const int type, const int lifetime, const int speed, const float gravity, const float elasticity, const int trackType)
        {
            ProjEnt& proj = *Projectiles.add(new ProjEnt);
            proj.set(type);
            add(proj);

            proj.owner = owner;
            proj.o = from;
            proj.from = from;
            proj.to = to;
            proj.setradius();
            proj.isLocal = isLocal;
            proj.id = id;
            proj.attack = attack;
            proj.lifetime = lifetime;
            proj.setSpeed(speed);
            proj.gravity = gravity;
            proj.elasticity = elasticity;
            proj.trackType = trackType;

            proj.setModel();

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
                proj.offset = hudgunorigin(proj.attack, from, to, owner);
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
                    playsound(proj->bounceSound, nullptr, &proj->o, nullptr, 0, 0, 0, -1);
                }
            }
            switch (proj->projectile)
            {
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
            if (d->type != ENT_PROJECTILE)
            {
                return;
            }
            ProjEnt* proj = (ProjEnt*)d;
            proj->bounces++;
            const int maxBounces = projs[proj->projectile].maxbounces;
            if ((maxBounces && proj->bounces > maxBounces) || lastmillis - proj->lastImpact < 100)
            {
                return;
            }
            applybounceeffects(proj, surface);
            proj->lastImpact = lastmillis;
        }

        void collidewithentity(physent* bouncer, physent* collideEntity)
        {
            /*
             * When a projectile collides with another alive entity.
             * Nothing yet.
             */
        }

        static float calculateDistance(dynent* target, vec& direction, const vec& position, const vec& velocity, const int flags)
        {
            vec middle = target->o;
            middle.z += (target->aboveeye - target->eyeheight) / 2;
            if (flags & ProjFlag_Linear || flags & ProjFlag_Track)
            {
                direction = vec(middle).sub(position).add(vec(velocity).mul(5)).safenormalize();
                const float low = min(target->o.z - target->eyeheight + target->radius, middle.z);
                const float high = max(target->o.z + target->aboveeye - target->radius, middle.z);
                const vec closest(target->o.x, target->o.y, clamp(position.z, low, high));

                // Return.
                return max(closest.dist(position) - target->radius, 0.0f);
            }
            float distance = middle.dist(position, direction);
            direction.div(distance);
            if (distance < 0)
            {
                distance = 0;
            }
            return distance;
        }

        static vec calculateDirection(dynent* target, const vec& position, const vec& velocity, const int flags)
        {
            vec direction;
            calculateDistance(target, direction, position, velocity, flags);
            return direction;
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

        // Conditions to confirm a valid target.
        static bool isValidTarget(dynent* target, ProjEnt& proj, const vec& position)
        {
            if (target == nullptr || target->state != CS_ALIVE)
            {
                return false;
            }
            if (proj.flags & ProjFlag_MultiHit && !proj.registerTarget(target))
            {
                return false;
            }
            return isIntersectingEntity(target, proj.o, position, proj.radius);
        }

        // Check if we hit a valid target with a direct shot.
        static void processDirectHit(dynent* target, ProjEnt& proj, const vec& position)
        {
            if (!isattackprojectile(proj.projectile))
            {
                return;
            }
            const vec direction = calculateDirection(target, position, proj.vel, proj.flags);

            // Send hit information separately for "multi-hit" projectiles.
            const bool shouldSend = proj.flags & ProjFlag_MultiHit;

            game::hit(target, proj.owner, target->o, direction, attacks[proj.attack].damage, proj.attack, 0, 1, proj.hitFlags, proj.id, shouldSend);
        }

        static void detectTargets(ProjEnt& proj, const vec& position)
        {
            if (!(proj.flags & ProjFlag_Linear) && !(proj.flags & ProjFlag_Track))
            {
                return;
            }
            hits.setsize(0);
            if (game::betweenrounds || !proj.isLocal)
            {
                return;
            }
            for (int i = 0; i < numdynents(); i++)
            {
                dynent* target = iterdynents(i);
                const bool isSelf = proj.owner == target || target == &proj;
                if (target == nullptr || isSelf)
                {
                    continue;
                }
                const bool isRejected = target->o.reject(proj.o, target->radius + proj.radius);
                if (isRejected || !isValidTarget(target, proj, position))
                {
                    continue;
                }

                // Projectile hit a target directly.
                processDirectHit(target, proj, position);

                // "Multi-hit" projectiles hit multiple targets, do not destroy after a hit.
                if (!(proj.flags & ProjFlag_MultiHit))
                {
                    proj.kill();
                }
                break;
            }
        }

        VARP(maxdebris, 10, 30, 1000);

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
                {
                    explosioncolor = 0xD47F00;
                    explosionlightcolor = vec(0.831f, 0.498f, 0.0f);
                    fade = 250;
                    if (isInWater)
                    {
                        return;
                    }
                    explosiontype = PART_EXPLOSION3;
                    particle_splash(PART_SPARK2, 100, 250, v, 0xFFC864, 0.10f + rndscale(0.50f), 600, 1);
                    particle_splash(PART_SMOKE, 100, 280, v, 0x222222, 1.0f, 250, 200, 20.0f);
                    particle_flare(v, v, 280, PART_EXPLODE1 + rnd(3), 0xFFC864, 0.0f, 80.0f);
                    particle_splash(PART_EXPLODE4, 20, 120, v, explosioncolor, 5.0f, 500, 50, 0.0f);
                    break;
                }

                case ATK_PULSE1:
                {
                    explosioncolor = 0xEE88EE;
                    explosionlightcolor = vec(0.933f, 0.533f, 0.933f);
                    fade = 250;
                    if (isInWater)
                    {
                        particle_flare(v, v, 300, PART_ELECTRICITY, explosioncolor, 5.0f, 12.0f);
                        return;
                    }
                    explosiontype = PART_EXPLOSION2;
                    particle_splash(PART_SPARK2, 5 + rnd(20), 200, v, explosioncolor, 0.08f + rndscale(0.35f), 400, 2);
                    particle_flare(v, v, 250, PART_EXPLODE1 + rnd(2), 0xF1B4F1, 0.5f, 15.0f);
                    particle_splash(PART_SMOKE, 60, 180, v, 0x222222, 2.5f + rndscale(3.8f), 180, 60);
                    break;
                }

                case ATK_GRENADE1:
                case ATK_GRENADE2:
                {
                    explosioncolor = 0x74BCF9;
                    explosionlightcolor = vec(0.455f, 0.737f, 0.976f);
                    explosiontype = PART_EXPLOSION2;
                    if (attack == ATK_GRENADE2)
                    {
                        fade = 400;
                        particle_splash(PART_SMOKE, 60, 500, v, 0x02448F, 30.0f, 180, 60, 0.1f);
                    }
                    else
                    {
                        fade = 200;
                    }
                    particle_flare(v, v, 280, PART_ELECTRICITY, 0x49A7F7, 30.0f);
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
                        particle_flare(v, v, 300, PART_ELECTRICITY, explosioncolor, 5.0f, 12.0f);
                        return;
                    }
                    particle_fireball(v, 1.0f, PART_EXPLOSION2, attack == ATK_PISTOL2 ? 200 : 500, 0x00FFFF, attacks[attack].exprad);
                    particle_splash(PART_SPARK2, 50, 180, v, 0x00FFFF, 0.18f, 500);
                    if (attack == ATK_PISTOL3)
                    {
                        particle_flare(v, v, 600, PART_EXPLODE1, explosioncolor, 50.0f, 40.0f);
                    }
                    else
                    {
                        particle_flare(v, v, 200, PART_RING, explosioncolor, 0.0f, 18.0f);
                    }
                    break;
                }
                default: break;
            }
            particle_fireball(v, maxsize, explosiontype, fade, explosioncolor, minsize);
            adddynlight(v, attacks[attack].exprad * 3, explosionlightcolor, fade, 40);
            playsound(attacks[attack].impactsound, nullptr, &v);
            // Spawn debris Projectiles.
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

        void calculatesplashdamage(dynent* o, const vec& position, const vec& velocity, gameent* at, const int attack, const int flags, const int hitFlags)
        {
            if (o == nullptr || (o->state != CS_ALIVE && o->state != CS_DEAD) || !validatk(attack) || attacks[attack].exprad == 0)
            {
                return;
            }
            vec direction;
            const float distance = calculateDistance(o, direction, position, velocity, flags);
            const int damage = static_cast<int>(attacks[attack].damage * (1 - distance / EXP_DISTSCALE / attacks[attack].exprad));
            if (distance < attacks[attack].exprad)
            {
                if (o->state == CS_ALIVE)
                {
                    game::hit(o, at, o->o, direction, damage, attack, distance, 0, hitFlags);
                }
                else
                {
                    physics::pushRagdoll(o, position, damage);
                }
            }
        }

        void applyradialeffect(const vec& position, const vec& velocity, gameent* owner, dynent* safe, const int attack, const int flags, const int hitFlags = 0)
        {
            if (attacks[attack].exprad == 0)
            {
                return;
            }
            const int entityFlags = DYN_PLAYER | DYN_AI | DYN_RAGDOLL;
            const int numdyn = numdynents(entityFlags);
            loopi(numdyn)
            {
                dynent* o = iterdynents(i, entityFlags);
                if (o->o.reject(position, o->radius + attacks[attack].exprad) || (safe && o == safe))
                {
                    continue;
                }
                calculatesplashdamage(o, position, velocity, owner, attack, flags, hitFlags);
            }
        }

        static void explode(ProjEnt& proj, const vec& v, const bool isLocal)
        {
            stain(proj.vel, proj.flags & ProjFlag_Linear ? v : proj.offsetPosition(), proj.attack);
            const vec pos = proj.flags & ProjFlag_Linear ? v : proj.o;
            addexplosioneffects(proj.owner, proj.attack, pos);
            if (betweenrounds || !isLocal)
            {
                return;
            }
            applyradialeffect(pos, proj.vel, proj.owner, &proj, proj.attack, proj.flags, proj.hitFlags);
        }

        void triggerExplosion(gameent* owner, const int attack, const vec& position, const vec& velocity)
        {
            addexplosioneffects(owner, attack, position);
            applyradialeffect(position, velocity, owner, nullptr, attack, 0);
        }

        /*
            Destroy tracking projectiles owned by a player on death,
            as we don't want to track body parts of a dead player.
        */
        static void removeTrackingProjectiles(vector<ProjEnt*>& owned)
        {
            if (owned.empty())
            {
                return;
            }
            for (int i = 0; i < owned.length(); i++)
            {
                ProjEnt& proj = *owned[i];
                if (proj.owner != self || proj.state != CS_ALIVE)
                {
                    continue;
                }
                if (proj.flags & ProjFlag_Track)
                {
                    // Kill (not remove or clear), as we need to remove the server version of this projectile too.
                    proj.kill();
                }
            }
        }

        // Check projectiles owned by a player.
        void checkOwned(const gameent* owner)
        {
            if (owner == nullptr)
            {
                return;
            }
            vector<ProjEnt*> owned = filterOwned(owner);
            if (owner->state == CS_DEAD)
            {
                removeTrackingProjectiles(owned);
            }
        }

        void destroy(ProjEnt& proj, const vec& position, const bool isLocal, const int attack)
        {
            // Explode projectile.
            if (proj.flags & ProjFlag_Explosive && isattackprojectile(proj.projectile) && validatk(proj.attack))
            {
                // Force attack update.
                if (validatk(attack) && proj.attack != attack)
                {
                    proj.attack = attack;
                }

                // Explode projectile.
                explode(proj, position, isLocal);
            }

            // Send destruction request to other clients.
            if (isLocal)
            {
                addmsg
                (
                    N_DESTROYPROJECTILE, "rci3iv",
                    proj.owner, lastmillis - maptime, proj.attack, proj.id,
                    hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf()
                );
            }

            // Delete projectile.
            remove(proj);
        }

        void detonate(gameent* owner, const int gun)
        {
            vector<ProjEnt*> owned = filterOwned(owner);

            // Reverse loop.
            for (int i = owned.length(); i-- > 0;)
            {
                ProjEnt& proj = *owned[i];
                if (!proj.isLocal || proj.state != CS_ALIVE || !validatk(proj.attack))
                {
                    continue;
                }
                if (proj.flags & ProjFlag_Explosive)
                {
                    const attackinfo attack = attacks[proj.attack];
                    if (attack.gun != gun)
                    {
                        continue;
                    }
                    proj.kill();
                    if (owner == self || owner->ai)
                    {
                        owner->delay[gun] = attack.attackdelay;
                        owner->lastAction[gun] = lastmillis;
                        owner->lastattack = proj.attack;
                        sendsound(guns[gun].abilitySound, owner);
                        owner->lastAbility[owner->Ability::lastUse] = lastmillis;
                        return;
                    }
                }
            }

            // We didn't detonate anything.
            sendsound(guns[gun].abilityFailSound, owner);
            owner->delay[owner->gunselect] = GUN_EMPTY_DELAY;
            owner->lastAbility[owner->Ability::lastAttempt] = lastmillis;
        }

        void damage(ProjEnt* proj, gameent* actor, const int attack)
        {
            if (!proj || proj->flags & ProjFlag_Invincible)
            {
                return;
            }
            if (isattackprojectile(proj->projectile) && validatk(attack))
            {
                if (proj->flags & ProjFlag_Loyal && attacks[proj->attack].gun != attacks[attack].gun)
                {
                    return;
                }
                if (!m_mp(gamemode) && actor)
                {
                    proj->owner = actor;
                }
                proj->attack = projs[proj->projectile].attack;
                proj->kill(true);
            }
        }

        void registerhit(dynent* target, gameent* actor, const int attack, const float dist, const int rays)
        {
            ProjEnt* proj = (ProjEnt*)target;
            if (!proj)
            {
                return;
            }
            if ((actor == self && proj->owner == self) || !m_mp(gamemode))
            {
                // Damage the projectile locally.
                damage(proj, actor, attack);
                return;
            }
            hitmsg& hit = hits.add();
            hit.target = proj->owner->clientnum;
            hit.lifesequence = -1;
            hit.dist = int(dist * DMF);
            hit.rays = rays;
            hit.flags = Hit_Projectile;
            hit.id = proj->id;
            hit.dir = ivec(0, 0, 0);
        }

        void handleliquidtransitions(ProjEnt& proj)
        {
            const bool isInWater = ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_WATER);
            const bool hasWaterTransitions = proj.projectile != Projectile_Debris;
            if (isInWater && projs[proj.projectile].flags & ProjFlag_Quench)
            {
                proj.kill();
                if (!hasWaterTransitions)
                {
                    return;
                }
            }
            const int transition = physics::liquidtransition(&proj, lookupmaterial(proj.o), isInWater);
            if (transition > 0)
            {
                int impactsound = S_INVALID;
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
                    playsound(impactsound, nullptr, &proj.o);
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
            if (proj->state != CS_DEAD)
            {
                proj->loopChannel = playsound(proj->loopSound, nullptr, &proj->o, nullptr, 0, -1, 100, proj->loopChannel);
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
                    proj.kill();
                }
            }
            else if (proj.flags & ProjFlag_Junk)
            {
                // Cheaper variable rate physics for debris, gibs and other "junk" Projectiles.
                for (int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if ((proj.lifetime -= qtime) < 0 || (proj.flags & ProjFlag_Bounce && physics::hasbounced(&proj, qtime / 1000.0f, 0.5f, 0.4f, 0.7f)))
                    {
                        proj.kill();
                    }
                }
            }
        }

        void addeffects(ProjEnt& proj, const vec& oldPosition)
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
                {
                    if (proj.lifetime < attacks[proj.attack].lifetime - 100)
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
                case Projectile_Pulse:
                {
                    tailColor = 0xDD88DD;
                    const float fade = projs[proj.projectile].fade;
                    const float progress = clamp(static_cast<float>(lastmillis - proj.millis) / fade, 0.0f, 1.0f);
                    tailSize = lerp(0.2f, 2.0f, progress);
                    particle_flare(position, position, 1, PART_ORB, tailColor, tailSize);
                    break;
                }
                case Projectile_Plasma:
                {
                    tailColor = 0x00FFFF;
                    const float fade = projs[proj.projectile].fade;
                    const float progress = clamp(static_cast<float>(lastmillis - proj.millis) / fade, 0.0f, 1.0f);
                    tailSize = 5.0f * ease::outelastic(progress);
                    particle_flare(position, position, 1, PART_ORB, tailColor, tailSize);
                    break;
                }
                case Projectile_Gib:
                {
                    if (blood && hasEnoughVelocity)
                    {
                        regular_particle_splash(PART_BLOOD, 1, 200, position, getbloodcolor(proj.owner), 2.5f, 50, 2, 0, 0.1f);
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
                case Projectile_Tracer:
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

        static vec updatePosition(ProjEnt& proj, const int time)
        {
            vec position = proj.o;
            if (proj.flags & ProjFlag_Linear)
            {
                proj.offsetMillis = max(proj.offsetMillis - time, 0);
                proj.dist = proj.to.dist(proj.o, proj.dv);
                proj.dv.mul(time / max(proj.dist * 1000 / proj.speed, float(time)));
                const vec offset = vec(proj.o).add(proj.dv);
                position = offset;
            }
            else if (proj.flags & ProjFlag_Bounce)
            {
                vec offset(proj.o);
                offset.add(vec(proj.offset).mul(proj.offsetMillis / float(OFFSET_MILLIS)));
                position = offset;
            }
            else if (proj.flags & ProjFlag_Track)
            {
                position = getTrackingPosition(proj.owner, proj.trackType);
            }
            return position;
        }

        void update(const int time)
        {
            if (Projectiles.empty())
            {
                return;
            }
            loopv(Projectiles)
            {
                ProjEnt& proj = *Projectiles[i];
                const vec oldPosition(proj.o);
                const vec position = updatePosition(proj, time);
                detectTargets(proj, position);
                if (proj.state != CS_DEAD)
                {
                    checklifetime(proj, time);
                    if ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_LAVA)
                    {
                        proj.kill();
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
                                proj.kill();
                            }
                        }
                        if (proj.flags & ProjFlag_Track)
                        {
                            const vec direction = vec(position).sub(oldPosition).normalize();
                            const float range = proj.vel.magnitude() * (time / 1000.0f);
                            vec hitPosition = vec(position);
                            const float ray = raycubepos(oldPosition, direction, hitPosition, range, RAY_CLIPMAT | RAY_ALPHAPOLY);
                            if (ray < range)
                            {
                                // Geometry collision.
                                if (proj.lastImpact == 0)
                                {
                                    // One impact per lifetime.
                                    applyImpactEffects(proj.attack, proj.owner, proj.o, proj.o);
                                    proj.lastImpact = lastmillis;
                                }
                                if (proj.flags & ProjFlag_Impact)
                                {
                                    proj.kill();
                                }
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
                                    proj.kill();
                                }
                            }
                        }
                    }
                    addeffects(proj, position);
                }
                checkloopsound(&proj);
                if (proj.state == CS_DEAD)
                {
                    destroy(proj, position);
                }
                else
                {
                    if (proj.flags & ProjFlag_Bounce)
                    {
                        if (proj.vel.magnitude() >= 25.0f)
                        {
                            const float displacement = vec(oldPosition).sub(proj.o).magnitude() / (4.0f * RAD);
                            proj.roll += displacement;
                            float pitch = 0;
                            vectoyawpitch(proj.vel, proj.yaw, pitch);
                            if (proj.flags & ProjFlag_Junk)
                            {
                                proj.pitch += displacement;
                            }
                            else
                            {
                                proj.pitch = pitch;
                            }
                            proj.yaw += 90;
                            proj.lastYaw = proj.yaw;
                        }
                        else
                        {
                            proj.yaw = proj.lastYaw;
                        }
                        proj.offsetMillis = max(proj.offsetMillis - time, 0);
                        proj.limitOffset();
                    }
                    else
                    {
                        proj.o = position;
                    }
                }
            }
        }

        void updatelights()
        {
            loopv(Projectiles)
            {
                ProjEnt& proj = *Projectiles[i];
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
                        lightColor = vec(2.0f, 1.5f, 2.0);
                        break;

                    case Projectile_Rocket:
                        lightColor = vec(1, 0.75f, 0.5f);
                        break;

                    case Projectile_Grenade:
                        lightColor = vec(0.25f, 0.25f, 1);
                        break;

                    case Projectile_Plasma:
                        lightColor = vec(0, 1.50f, 1.50f);
                        break;

                    // Nothing to do here.
                    default:
                        return;
                }
                adddynlight(pos, 35, lightColor);
            }
        }

        void spawnbouncer(const vec& from, gameent* d, const int type)
        {
            vec to(rnd(100) - 50, rnd(100) - 50, type == Projectile_Gib ? 50 + rnd(100) : rnd(100) - 50);
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
            make(d, from, to, true, 0, -1, type, 1000 + rnd(1000), rnd(100) + 20, 0.3f + rndscale(0.8f), elasticity);
        }

        void preload()
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

        void render()
        {
            for (int i = 0; i < Projectiles.length(); i++)
            {
                ProjEnt& proj = *Projectiles[i];
                if (!proj.model[0])
                {
                    continue;
                }
                const vec position = proj.manipulateModel();
                int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED;
                float fade = 1.0f;
                if (proj.lifetime >= 400)
                {
                    const float fadeTime = projs[proj.projectile].fade;
                    const float progress = clamp(static_cast<float>(lastmillis - proj.millis) / fadeTime, 0.0f, 1.0f);
                    fade *= progress;
                }
                else if (proj.flags & ProjFlag_Junk)
                {
                    fade = proj.lifetime / 400.0f;
                }
                rendermodel(proj.model, ANIM_MAPMODEL | ANIM_LOOP, position, proj.yaw, proj.pitch, proj.roll, cull, nullptr, nullptr, 0, 0, fade);
            }
        }

        void clear(gameent* owner)
        {
            if (owner != nullptr)
            {
                vector<ProjEnt*> owned = filterOwned(owner);
                if (owned.empty())
                {
                    return;
                }
                for (int i = 0; i < owned.length(); i++)
                {
                    ProjEnt& proj = *owned[i];
                    if (proj.owner != owner)
                    {
                        continue;
                    }
                    remove(proj);
                }
            }
            else
            {
                AttackProjectiles.setsize(0);
                Projectiles.deletecontents();
            }
        }
    }
};
