/*
 * =====================================================================
 * weapon.cpp
 * Manage weapons and shooting effects.
 * =====================================================================
 *
 * This file implements the behaviour and mechanics of weapons, such as:
 * - Hit detection and effects.
 * - Shooting mechanics and effects.
 * - Damage calculation.
 * - Weapon selection and first-person effects.
 * 
 * =====================================================================
 */

#include "game.h"

namespace game
{
    vector<hitmsg> hits;

    vec rays[GUN_MAXRAYS];

    // Calculate the current recoil spread based on player state and weapon.
    static const int getSpread(gameent* player, const int attack)
    {
        const int shots = player->recoil.shots;
        int spread = attacks[attack].spread;
        if (spread)
        {
            if (shots)
            {
                const float progress = static_cast<float>(shots) / player->recoil.maxShots;
                const float newSpread = spread + int(recoils[attack].maxSpread * progress);
                spread = newSpread;
            }
            if (!player->onfloor())
            {
                // Accuracy decreases while in air.
                spread = static_cast<int>(spread * 1.25f);
            }
            else if (player->crouching && player->crouched() && !player->sliding(lastmillis))
            {
                // Accuracy increases while crouched.
                spread = static_cast<int>(spread * 0.75f);
            }
        }
        return spread;
    }

    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest, gameent *d)
    {
        vec offset;
        do offset = vec(rndscale(1), rndscale(1), rndscale(1)).sub(0.5f);
        while(offset.squaredlen() > 0.5f * 0.5f);
        const bool isCrouched = d->onfloor() && d->crouching && d->crouched();
        offset.mul((to.dist(from) / 1024) * spread * (isCrouched ? 0.5f : 1.0f));
        offset.z /= 2;
        dest = vec(offset).add(to);
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
        }
    }

    bool canshoot(gameent* d, int atk, int gun, int projectile)
    {
        if (!d->hasAmmo(atk))
        {
            return false;
        }
        if (isvalidprojectile(projectile))
        {
            bool isinwater = ((lookupmaterial(d->o)) & MATF_VOLUME) == MAT_WATER;
            if (isinwater && projs[projectile].flags & ProjFlag_Quench)
            {
                return false;
            }
        }
        return true;
    }

    static void pushAttacker(gameent* player, const vec& direction, const int amount)
    {
        if (player == nullptr)
        {
            return;
        }
        const vec push = vec(direction).mul(amount);
        player->vel.add(push);
    }

    void applyWeaponPush(gameent* player, const vec& direction, const float amount)
    {
        if (player == nullptr || player->onfloor() || (player->crouching && player->crouched()))
        {
            /*
                Do not apply weapon push if player is on ground or crouched,
                as we want to "destabilise" reckless players only.
            */
            return;
        }

        // This is not for melee or throw attacks.
        const int action = attacks[player->lastattack].action;
        const bool isWeaponAttack = validact(action) && action != ACT_MELEE && action != ACT_THROW;
        if (!isWeaponAttack)
        {
            return;
        }

        pushAttacker(player, direction, amount);
    }

    static void applyMeleePush(gameent* player, const vec& direction, const int attack)
    {
        if (!validatk(attack))
        {
            return;
        }
        const attackinfo& context = attacks[attack];
        if (context.action != ACT_MELEE)
        {
            return;
        }
        const int projectileType = context.projectile;
        const float radius = projs[projectileType].radius;
        dynent* intersected = intersectClosest(player, player->o, worldpos, radius);
        gameent* target = (gameent*)intersected;
        if (target == nullptr)
        {
            // We stop caring if we have no target to reach.
            return;
        }
        const float distance = player->o.dist(target->o);
        const float range = attacks[attack].range;
        if (range == 0)
        {
            return;
        }
        const float maxRange = context.range;
        const float minRange = radius;
        if (distance <= minRange || distance > maxRange)
        {
            return;
        }
        const int maxPush = recoils[attack].push;
        const float pushScale = clamp((distance - minRange) / (minRange * 2), 0.0f, 1.0f);
        const float amount = maxPush * pushScale * 2.5f;
        pushAttacker(player, direction, amount);
    }

    // Update the spread based on "burst shots", called on recoil addition.
    static void addBurstShots(gameent* player, const int attack)
    {
        if (attacks[attack].spread == 0)
        {
            return;
        }
        const int recoilTime = attacks[attack].attackdelay + GUN_RECOIL_DELAY;
        if (lastmillis - player->recoil.lastShot > recoilTime)
        {
            // Reset shot count if enough time has passed since the last shot.
            player->recoil.shots = 0;
        }
        else
        {
            // Increment shot count up to the maximum.
            player->recoil.shots = min(player->recoil.shots + 1, player->recoil.maxShots);
        }
        player->recoil.lastShot = lastmillis;
    }

    // Apply recoil effects to the player when firing a weapon.
    static void addRecoil(gameent* player, const vec& direction, const int attack)
    {
        if (player == nullptr || !validatk(attack))
        {
            return;
        }

        // First off, update weapon "burst shots".
        addBurstShots(player, attack);

        const int recoilAmount = recoils[attack].recoil;
        const float powerupMultiplier = player->haspowerup(PU_DAMAGE) ? 2.0f : 1.0f;
        if (recoilAmount)
        {
            // Reset recoil index if enough time has passed since the last shot.
            const int recoilTime = attacks[attack].attackdelay + GUN_RECOIL_DELAY;
            if (lastmillis - player->recoil.time > recoilTime)
            {
                player->recoil.index = 0;
            }

            // Increment shot index, pick pattern entry.
            const int patternSize = sizeof(recoils[attack].recoilPattern) / sizeof(recoils[attack].recoilPattern[0]);
            player->recoil.index = min(player->recoil.index + 1, patternSize);
            const int index = max(0, player->recoil.index - 1);
            const vec2& pattern = recoils[attack].recoilPattern[min(index, patternSize - 1)];

            // Scale pattern.
            const float recoilScale = recoilAmount * recoils[attack].recoilScale * powerupMultiplier;
            const float multiplier = player->crouching && player->crouched() ? 0.75f : 1.0f;
            float pitchKick = pattern.x * recoilScale * multiplier;
            float yawKick = pattern.y * recoilScale * multiplier;
            const float random = clamp(player->recoil.index / float(patternSize), 0.f, 1.f);
            yawKick += -0.25f + rndscale(0.25f) * random;
            pitchKick += -0.1f + rndscale(0.1f) * random;

            // Add the visual Kick directly for responsiveness.
            player->recoil.kick.x += pitchKick;
            player->recoil.kick.y += yawKick;
            player->pitch += pitchKick;
            player->yaw += yawKick;
        }
        player->recoil.time = lastmillis;
        player->recoil.recovery = recoils[attack].recoilRecovery;

        // Push the attacker and shake their camera if relevant.
        int shakeAmount = recoils[attack].shake;
        int pushAmount = recoils[attack].push;
        if (shakeAmount)
        {
            const float amount = shakeAmount * powerupMultiplier;
            camera::camera.addevent(player, camera::CameraEvent_Shake, amount);
        }
        if (pushAmount)
        {
            const float amount = pushAmount * powerupMultiplier * -2.5f;
            applyWeaponPush(player, direction, amount);
        }
    }

    void updateRecoil(gameent* d, const int curtime)
    {
        const float time = curtime / 1000.0f;
        const vec2 oldKick = d->recoil.kick;
        const float decay = expf(-d->recoil.recovery * time);
        d->recoil.kick.mul(decay);

        // Remove recovered portion from actual pitch/yaw.
        vec2 deltaKick = oldKick;
        deltaKick.sub(d->recoil.kick);
        float scaleFactor = 1.0f;
        if (d->recoil.maxShots)
        {
            scaleFactor = 1.0f - ((0.25f / d->recoil.maxShots) * d->recoil.shots);
            scaleFactor = clamp(scaleFactor, 0.75f, 1.0f);
        }
        d->pitch -= deltaKick.x * scaleFactor;
        d->yaw -= deltaKick.y;

        // Once the Kick is near zero, reset it (together with the index if necessary).
        if (fabs(d->recoil.kick.x) < 0.001f && fabs(d->recoil.kick.y) < 0.001f)
        {
            d->recoil.kick = vec2(0, 0);
            const int recoilTime = attacks[d->lastattack].attackdelay + GUN_RECOIL_DELAY;
            if (lastmillis - d->recoil.time > recoilTime)
            {
                // Reset the index, so the next shot restarts the pattern.
                d->recoil.index = 0;
            }
        }

        // Clamp pitch/yaw to valid ranges. 
        camera::fixrange();
    }

    static void doAttack(gameent* d, const vec& target, const int attack, const int weapon)
    {
        vec from = d->o, to = target, dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        addRecoil(d, dir, attack);
        if (attacks[attack].action == ACT_MELEE)
        {
            applyMeleePush(d, dir, attack);
        }
        float shorten = attacks[attack].range && dist > attacks[attack].range ? attacks[attack].range : 0,
            barrier = raycube(d->o, dir, dist, RAY_CLIPMAT | RAY_ALPHAPOLY);
        if (barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
            shorten = barrier;
        if (shorten) to = vec(dir).mul(shorten).add(from);
        const int spread = getSpread(d, attack);
        if (attacks[attack].rays > 1)
        {
            loopi(attacks[attack].rays)
            {
                offsetray(from, to, spread, attacks[attack].range, rays[i], d);
            }
        }
        else if (attacks[attack].spread)
        {
            offsetray(from, to, spread, attacks[attack].range, to, d);
        }
        hits.setsize(0);
        if (!isattackprojectile(attacks[attack].projectile))
        {
            scanhit(from, to, d, attack);
        }
        const int id = lastmillis - maptime;
        applyShotEffects(attack, d, from, to, id, false, true);
        if (d == self || d->ai)
        {
            addmsg
            (
                N_SHOOT, "rci9iv", d, id, attack, weapon,
                static_cast<int>(from.x * DMF), static_cast<int>(from.y * DMF), static_cast<int>(from.z * DMF),
                static_cast<int>(to.x * DMF), static_cast<int>(to.y * DMF), static_cast<int>(to.z * DMF),
                hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf()
            );
        }
    }

    void applyDelay(gameent* d, const int attack)
    {
        d->applyAttackDelay(attack);
        if (d->ai)
        {
            const int gun = attacks[attack].gun;
            if (gun == GUN_PISTOL)
            {
                d->delay[gun] += int(d->delay[gun] * (((101 - d->skill) + rnd(111 - d->skill)) / 100.f));
            }
        }
    }

    void throwAttack(gameent* player, const vec& to)
    {
        const int attack = ATK_GRENADE1;
        const int weapon = player->gunselect;
        doAttack(player, to, attack, weapon);
        applyDelay(player, attack);

        // Reset the timestamp so we can throw again.
        player->lastthrow = 0;
    }

    /*
        If an invalid attack is triggered via shooting, check if it is a special case.
        Otherwise simply do nothing (and effectively prevent the player from firing).
    */
    static void checkAbility(gameent* player, const int weapon)
    {
        switch (weapon)
        {
            case GUN_ROCKET:
                projectiles::detonate(player, weapon);
                break;

            // Nothing to do here.
            default:
                break;
        }

        // If we reach this point, we are performing an ability and not firing anymore.
        player->attacking = ACT_IDLE;
    }

    VARP(autoswitch, 0, 1, 1);

    void shoot(gameent* d, const vec& targ)
    {
        /*
            Add a small delay after switching weapons before allowing shooting.
            If we are throwing, then prevent the user from firing or throwing again.
        */
        const bool hasSwitchDelay = d->lastswitch && lastmillis - d->lastswitch < GUN_SWITCH_DELAY;
        const bool hasThrowDelay = d->lastthrow && lastmillis - d->lastthrow <= GUN_THROW_DELAY;
        if (hasSwitchDelay || hasThrowDelay)
        {
            return;
        }

        const int weapon = d->gunselect;
        const int action = d->attacking;
        const int attack = guns[weapon].attacks[action];
        if (!validact(action) || !validgun(weapon))
        {
            return;
        }

        /*
            If the attack we are using is not valid, must be something else.
            Check if the weapon can perform an ability, otherwise simply return.
        */
        if (!validatk(attack))
        {
            checkAbility(d, weapon);
            return;
        }

        if (!d->canShoot(attack, weapon, lastmillis))
        {
            return;
        }
        if (d->delay[weapon] > 0)
        {
            d->delay[weapon] = 0;
        }
        d->lastattack = attack;
        d->setLastAction(attack, lastmillis);
        if (attacks[attack].action == ACT_THROW || attacks[attack].action == GUN_MELEE)
        {
            // Disable zoom only if we're the shooting player or spectating them.
            if (d == followingplayer(self))
            {
                camera::camera.zoomstate.disable();
            }

            // If the action is a throw and we have no throw timestamp.
            if (attacks[attack].action == ACT_THROW && !d->lastthrow)
            {
                d->prepareThrow(lastmillis);

                // Return cause we don't need all the stuff below for now.
                return;
            }
        }

        const int projectile = attacks[attack].projectile;
        if (!canshoot(d, attack, weapon, projectile))
        {
            if (d == self)
            {
                sendsound(S_WEAPON_NOAMMO, d);
                d->delay[weapon] = GUN_EMPTY_DELAY;
                d->lastattack = ATK_INVALID;
                if (autoswitch && d->ammo[weapon] <= 0)
                {
                    weaponswitch(d);
                    d->attacking = ACT_IDLE; // Cancel the attack since we are switching weapons.
                }
            }
            return;
        }
        d->useAmmo(attack);

        doAttack(d, targ, attack, weapon);

        if (!attacks[attack].isfullauto)
        {
            d->attacking = ACT_IDLE;
        }
        applyDelay(d, attack);
        d->totalshots += attacks[attack].damage * attacks[attack].rays;
    }

    void checkattacksound(gameent *d, bool local)
    {
        const int gun = d->gunselect;
        int attack = ATK_INVALID;
        if (validact(d->attacking))
        {
            attack = guns[gun].attacks[d->attacking];
        }
        switch(d->chansound[Chan_Attack])
        {
            case S_PULSE2_A:
            {
                attack = ATK_PULSE2;
                break;
            }
            default: return;
        }
        const bool isValidClient = d->clientnum >= 0 && d->state == CS_ALIVE;
        if(validatk(attack) && d->lastattack == attack && lastmillis - d->lastAction[gun] < attacks[attack].attackdelay + 50 && isValidClient)
        {
            const int channel = Chan_Attack;
            d->chan[channel] = playsound(d->chansound[channel], NULL, local ? NULL : &d->o, NULL, 0, -1, -1, d->chan[channel]);
            if (d->chan[channel] < 0)
            {
                d->chansound[channel] = -1;
            }
        }
        else
        {
            d->stopchannelsound(Chan_Idle);
            d->stopchannelsound(Chan_Attack);
        }
    }

    void checkidlesound(gameent *d, bool local)
    {
        int sound = S_INVALID;
        if(d->clientnum >= 0 && d->state == CS_ALIVE && !validsound(d->chansound[Chan_Attack])) switch(d->gunselect)
        {
            case GUN_ZOMBIE:
            {
                sound = S_ZOMBIE_IDLE;
                break;
            }
        }
        if(d->chansound[Chan_Idle] != sound)
        {
            d->stopchannelsound(Chan_Idle);
            if (validsound(sound))
            {
                d->chan[Chan_Idle] = playsound(sound, NULL, local ? NULL : &d->o, NULL, 0, -1, 1200, d->chan[Chan_Idle], 500);
                if (d->chan[Chan_Idle] >= 0)
                {
                    d->chansound[Chan_Idle] = sound;
                }
            }
        }
        else if (validsound(sound))
        {
            d->chan[Chan_Idle] = playsound(sound, NULL, local ? NULL : &d->o, NULL, 0, -1, 1200, d->chan[Chan_Idle], 500);
            if (d->chan[Chan_Idle] < 0)
            {
                d->chansound[Chan_Idle] = -1;
            }
        }
    }

    void updateThrow(gameent* player)
    {
        if (!player->lastthrow)
        {
            return;
        }
        const int elapsed = lastmillis - player->lastthrow;
        const int delay = GUN_THROW_DELAY;
        if (elapsed >= delay)
        {
            // If it's a bot, use the bot's target.
            const vec to = player->ai ? player->ai->target : worldpos;

            throwAttack(player, to);
        }
    }

    void updatePlayerWeapons(gameent* player, const vec& to, const int time)
    {
        // Only allow shooting if the player has joined a game.
        const bool isConnected = !mainmenu && player->clientnum >= 0;
        if (!(isConnected && player->state == CS_ALIVE))
        {
            return;
        }

        shoot(player, to);
        updateRecoil(player, time);
        updateThrow(player);
    }

    void updateweapons(const int curtime)
    {
        self->updateWeaponDelay(lastmillis);
        updatePlayerWeapons(self, worldpos, curtime);

        /*
            Need to do this after the player shoots,
            so bouncing projectiles don't end up inside the player's bounding box next frame.
        */
        projectiles::update(curtime);

        gameent* following = followingplayer();
        if (!following) following = self;
        loopv(players)
        {
            gameent* d = players[i];
            checkattacksound(d, d == following);
            checkidlesound(d, d == following);
        }
    }

    void clearweapons()
    {
        sway.events.shrink(0);
        hits.setsize(0);
    }

    void applyImpactEffects(const int attack, gameent* player, const vec& from, const vec& to, const bool isHit)
    {
        if (!validatk(attack) || (!isHit && isemptycube(to)) || from.dist(to) > attacks[attack].range) return;
        vec dir = vec(from).sub(to).safenormalize();
        int material = lookupmaterial(to);
        bool iswater = (material & MATF_VOLUME) == MAT_WATER, isglass = (material & MATF_VOLUME) == MAT_GLASS;
        switch (attack)
        {
            case ATK_MELEE:
                particle_flare(to, to, 100, PART_SPARK3, 0xFFFF66, 0.0f, 15.0f, player, TRACK_HAND_LEFT);
                camera::camera.addevent(player, camera::CameraEvent_Shake, recoils[attack].shake);
                break;

            case ATK_SCATTER1:
            case ATK_SCATTER2:
                adddynlight(vec(to).madd(dir, 4), 6, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if (isHit || iswater || isglass) break;
                if (attack == ATK_SCATTER2)
                {
                    particle_splash(PART_SPARK2, 10, 120, to, 0xFFC864, 0.25f, 250, 2, 0.001f);
                    particle_splash(PART_SMOKE, 10, 250, to, 0x555555, 0.1f, 100, 100, 10.0f);
                }
                else
                {
                    particle_splash(PART_SPARK2, 10, 80 + rnd(380), to, 0xFFC864, 0.1f, 250, 2, 0.001f);
                    particle_splash(PART_SMOKE, 10, 250, to, 0x555555, 0.1f, 100, 100, 7.0f);
                }
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.50f + rndscale(1.0f), 0xFFFFFF, rnd(4));
                break;

            case ATK_SMG1:
            case ATK_SMG2:
                adddynlight(vec(to).madd(dir, 4), 15, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if (isHit || iswater || isglass) break;
                particle_fireball(to, 0.5f, PART_EXPLOSION2, 120, 0xFFC864, 2.0f);
                particle_splash(PART_EXPLODE4, 50, 40, to, 0xFFC864, 1.0f, 150, 2, 0.1f);
                particle_splash(PART_SPARK2, 30, attack == ATK_SMG2 ? 100 : 250, to, 0xFFC864, 0.05f + rndscale(0.2f), 250, 2, 0.001f);
                particle_splash(PART_SMOKE, 30, 250, to, 0x555555, 0.2f, 80, 100, 6.0f);
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.50f + rndscale(1.0f), 0xFFFFFF, rnd(4));
                break;
            
            case ATK_PULSE2:

                adddynlight(vec(to).madd(dir, 4), 80, vec(1.0f, 0.50f, 1.0f), 20);
                if (isHit)
                {
                    particle_flare(to, to, 120, PART_ELECTRICITY, 0xEE88EE, 1.0f + rndscale(8.0f));
                    break;
                }
                if (iswater) break;
                particle_splash(PART_SPARK2, 10, 300, to, 0xEE88EE, 0.01f + rndscale(0.10f), 350, 2);
                particle_splash(PART_SMOKE, 20, 300, to, 0x777777, 0.1f, 100, 50, 3.5f + rndscale(4.5f));
                addstain(STAIN_SCORCH, to, vec(from).sub(to).normalize(), 1.0f + rndscale(1.5f));
                playsound(attacks[attack].impactsound, NULL, &to);
                break;

            case ATK_RAIL1:
            case ATK_RAIL2:
            case ATK_INSTA:
            {
                bool isInstagib = attacks[attack].gun == GUN_INSTA;
                adddynlight(vec(to).madd(dir, 4), 60, !isInstagib ? vec(0.25f, 1.0f, 0.75f) : vec(0.5f, 0.5f, 0.5f), 180, 75, DL_EXPAND);
                if (isHit)
                {
                    if (isInstagib)
                    {
                        particle_flare(to, to, 200, PART_ELECTRICITY, 0x008080, 2.0f + rndscale(5.0f));
                    }
                    break;
                }
                if (iswater || isglass) break;
                particle_splash(PART_EXPLODE4, 80, 80, to, !isInstagib ? 0x77DD77 : 0x008080, 1.25f, 100, 80);
                particle_splash(PART_SPARK2, 5 + rnd(20), 200 + rnd(380), to, !isInstagib ? 0x77DD77 : 0x008080, 0.1f + rndscale(0.3f), 200, 3, 0.001f);
                particle_splash(PART_SMOKE, 30, 250, to, 0x555555, 0.1f, 60, 80, 8.0f);
                addstain(STAIN_BULLETHOLE_BIG, to, dir, 2.5f);
                addstain(STAIN_GLOW1, to, dir, 2.0f, !isInstagib ? 0x77DD77 : 0x008080);
                break;
            }

            case ATK_PISTOL1:
                adddynlight(vec(to).madd(dir, 4), 30, vec(0.25, 1.0f, 1.0f), 200, 10, DL_SHRINK);
                if (isHit || iswater || isglass) break;
                particle_fireball(to, 2.2f, PART_EXPLOSION1, 140, 0x00FFFF, 0.1f);
                particle_splash(PART_SPARK2, 50, 250, to, 0x00FFFF, 0.08f + rndscale(0.18f), 150, 2, 0.001f);
                addstain(STAIN_SCORCH, to, vec(from).sub(to).normalize(), 0.80f + rndscale(1.0f));
                addstain(STAIN_GLOW1, to, dir, 1.50f, 0x00FFFF);
                break;

            default:
                break;
        }

        if (isHit || attack == ATK_PULSE2) return;

        int impactsound = attacks[attack].impactsound;

        bool ismelee = attacks[attack].action == ACT_MELEE;
        if (iswater)
        {
            if (!ismelee)
            {
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.80f);
            }
            if (attacks[attack].rays <= 1) // Temporary measure: avoid sound spam.
            {
                impactsound = S_IMPACT_WATER;
            }
        }
        else if (isglass)
        {
            particle_splash(PART_GLASS, 20, 200, to, 0xFFFFFF, 0.10 + rndscale(0.20f));
            addstain(STAIN_GLASS_HOLE, to, vec(from).sub(to).normalize(), ismelee ? 2.0f : 1.0f + rndscale(2.0f), 0xFFFFFF, rnd(4));
            if (attacks[attack].rays <= 1) // Temporary measure: avoid sound spam.
            {
                impactsound = S_IMPACT_GLASS;
            }
        }
        else if (ismelee)
        {
            
        }

        if (validsound(impactsound))
        {
            playsound(impactsound, NULL, &to);
        }
    }

    VARP(muzzleflash, 0, 1, 1);

    // Create visual effects from a shot.
    void applyShotEffects(const int attack, gameent* player, vec& from, vec& to, const int id, const bool isHit, const bool isLocal)
    {
        const int weapon = attacks[attack].gun;
        const int previousAction = validgun(weapon) ? player->lastAction[weapon] : 0;
        const int sound = attacks[attack].sound;
        float dist = from.dist(to);
        const gameent* hudPlayer = followingplayer(self);
        const bool shouldEject = player->eject.x >= 0 && player == hudPlayer;
        int trackType = TRACK_ORIGIN;
        switch (attack)
        {
            case ATK_MELEE:
            case ATK_ZOMBIE:
                from = player->hand;
                trackType = TRACK_HAND_LEFT;
                break;

            case ATK_SCATTER1:
            case ATK_SCATTER2:
                if (player->muzzle.x >= 0 && muzzleflash)
                {
                    if (attack == ATK_SCATTER2 && player ==  hudPlayer)
                    {
                        particle_flare(player->muzzle, player->muzzle, 50, PART_MUZZLE_FLASH, 0xEFE598, 2.0f, 0, player, TRACK_MUZZLE);
                    }
                    else
                    {
                        particle_flare(player->muzzle, player->muzzle, 850, PART_MUZZLE_SMOKE, 0x202020, 0.1f, 3.0f, player, TRACK_MUZZLE);
                        particle_flare(player->muzzle, player->muzzle, 200, PART_SPARKS, 0xEFE598, 0.1f, 3.0f, player, TRACK_MUZZLE);
                        particle_flare(player->muzzle, player->muzzle, 200, PART_MUZZLE_FLASH, 0xEFE598, 3.0f, 0.1f, player, TRACK_MUZZLE);
                    }
                    adddynlight(hudgunorigin(attack, player->o, to, player), 200, vec(0.5f, 0.375f, 0.25f), 80, 75, DL_SHRINK, 0, vec(0, 0, 0), player, TRACK_MUZZLE);
                    if (attack == ATK_SCATTER2)
                    {
                        loopi(attacks[attack].rays)
                        {
                            particle_flare(hudgunorigin(attack, from, rays[i], player), rays[i], 50, PART_TRAIL, 0xFFC864, 1.0f);
                        }
                    }
                }
                if (!isLocal)
                {
                    loopi(attacks[attack].rays)
                    {
                        offsetray(from, to, attacks[attack].spread, attacks[attack].range, rays[i], player);
                        applyImpactEffects(attack, player, from, rays[i], isHit);
                    }
                }
                break;

            case ATK_SMG1:
            case ATK_SMG2:
                if (player->muzzle.x >= 0 && muzzleflash)
                {
                    if (attack == ATK_SMG2 && player ==  hudPlayer)
                    {
                        particle_flare(player->muzzle, player->muzzle, 50, PART_MUZZLE_FLASH3, 0xEFE898, 0.1f, 1.8f, player, TRACK_MUZZLE);
                        particle_flare(hudgunorigin(weapon, from, to, player), to, 50, PART_TRAIL, 0xFFC864, 1.0f);
                    }
                    else
                    {
                        particle_flare(player->muzzle, player->muzzle, 300, PART_MUZZLE_SMOKE, 0xFFFFFF, 0.5f, 2.0f, player, TRACK_MUZZLE);
                        particle_flare(player->muzzle, player->muzzle, 120, PART_SPARKS, 0xEFE898, 0.1f, 3.0f, player, TRACK_MUZZLE);
                        particle_flare(player->muzzle, player->muzzle, 130, PART_MUZZLE_FLASH3, 0xEFE898, 0.1f, 1.8f, player, TRACK_MUZZLE);
                    }
                    adddynlight(hudgunorigin(attack, player->o, to, player), 120, vec(0.5f, 0.375f, 0.25f), 80, 75, DL_EXPAND, 0, vec(0, 0, 0), player, TRACK_MUZZLE);
                }
                if (!isLocal)
                {
                    applyImpactEffects(attack, player, from, to, isHit);
                }
                break;

            case ATK_PULSE1:
                if (muzzleflash && player->muzzle.x >= 0)
                {
                    particle_flare(player->muzzle, player->muzzle, 100, PART_MUZZLE_FLASH2, 0xDD88DD, 2.5f, 0, player, TRACK_MUZZLE);
                }
                break;

            case ATK_PULSE2:
                if (muzzleflash && player->muzzle.x >= 0)
                {
                    if (previousAction > 200)
                    {
                        particle_flare(player->muzzle, player->muzzle, 250, PART_MUZZLE_FLASH5, 0xDD88DD, 0.5f, 3.0f, player, TRACK_MUZZLE);
                    }
                    else particle_flare(player->muzzle, player->muzzle, 80, PART_MUZZLE_FLASH2, 0xDD88DD, 2.0f, 0, player, TRACK_MUZZLE);
                    adddynlight(hudgunorigin(attack, player->o, to, player), 75, vec(1.0f, 0.50f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), player, TRACK_MUZZLE);
                }
                particle_flare(hudgunorigin(attack, from, to, player), to, 80, PART_LIGHTNING, 0xEE88EE, 1.0f, 0, player, TRACK_MUZZLE);
                particle_fireball(to, 1.0f, PART_EXPLOSION2, 100, 0xDD88DD, 3.0f);
                if (!isLocal)
                {
                    applyImpactEffects(attack, player, from, to, isHit);
                }
                break;

            case ATK_ROCKET1:
                if (muzzleflash && player->muzzle.x >= 0)
                {
                    particle_flare(player->muzzle, player->muzzle, 150, PART_MUZZLE_FLASH4, 0xEFE898, 0.1f, 3.0f, player, TRACK_MUZZLE);
                }
                break;

            case ATK_RAIL1:
            case ATK_RAIL2:
                if (player->muzzle.x >= 0 && muzzleflash)
                {
                    if (player ==  hudPlayer)
                    {
                        particle_flare(player->muzzle, player->muzzle, 180, PART_SPARKS, 0x77DD77, 0.1f, 3.0f, player, TRACK_MUZZLE);
                        particle_flare(player->muzzle, player->muzzle, 450, PART_MUZZLE_SMOKE, 0x202020, 3.0f, 0, player, TRACK_MUZZLE);
                    }
                    particle_flare(player->muzzle, player->muzzle, 80, PART_MUZZLE_FLASH, 0x77DD77, 1.75f, 0, player, TRACK_MUZZLE);
                    adddynlight(hudgunorigin(attack, player->o, to, player), 100, vec(0.25f, 1.0f, 0.75f), 80, 75, DL_SHRINK, 0, vec(0, 0, 0), player, TRACK_MUZZLE);
                }
                if (attack == ATK_RAIL2) particle_trail(PART_SMOKE, 350, hudgunorigin(attack, from, to, player), to, 0xDEFFDE, 0.3f, 50);
                particle_flare(hudgunorigin(attack, from, to, player), to, 600, PART_TRAIL, 0x55DD55, 0.50f);
                if (!isLocal)
                {
                    applyImpactEffects(attack, player, from, to, isHit);
                }
                break;

            case ATK_GRENADE1:
                // Hand is not rendered when fully zoomed in. Use player origin.
                from = camera::camera.zoomstate.isenabled() ? player->o : player->hand;

                to.addz(dist / 8);
                break;

            case ATK_PISTOL1:
            case ATK_PISTOL2:
                if (muzzleflash && player->muzzle.x >= 0)
                {
                    if (attack == ATK_PISTOL1)
                    {
                        if (player ==  hudPlayer)
                        {
                            particle_flare(player->muzzle, player->muzzle, 180, PART_SPARKS, 0x00FFFF, 0.1f, 3.0f, player, TRACK_MUZZLE);
                        }
                        particle_flare(player->muzzle, player->muzzle, 120, PART_MUZZLE_FLASH3, 0x00FFFF, 0.1f, 2.5f, player, TRACK_MUZZLE);
                        particle_flare(hudgunorigin(attack, from, to, player), to, 80, PART_TRAIL, 0x00FFFF, 2.0f);
                        if (!isLocal)
                        {
                            applyImpactEffects(attack, player, from, to, isHit);
                        }
                    }
                    else
                    {
                        particle_flare(player->muzzle, player->muzzle, 280, PART_MUZZLE_FLASH2, 0x00FFFF, 0.1f, 3.0f, player, TRACK_MUZZLE);
                    }
                    adddynlight(hudgunorigin(attack, player->o, to, player), 80, vec(0.25f, 1.0f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), player, TRACK_MUZZLE);
                }
                break;

            case ATK_INSTA:
                if (muzzleflash && player->muzzle.x >= 0)
                {
                    particle_flare(player->muzzle, player->muzzle, 200, PART_MUZZLE_FLASH, 0x008080, 0.1f, 2.75f, player, TRACK_MUZZLE);
                    particle_flare(player->muzzle, player->muzzle, 450, PART_MUZZLE_SMOKE, 0x006060, 0.1f, 3.0f, player, TRACK_MUZZLE);
                    adddynlight(hudgunorigin(attack, player->o, to, player), 80, vec(0.25f, 0.75f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), player, TRACK_MUZZLE);
                }
                particle_trail(PART_SMOKE, 350, hudgunorigin(attack, from, to, player), to, 0x006060, 0.3f, 50);
                particle_flare(hudgunorigin(attack, from, to, player), to, 100, PART_LIGHTNING, 0x008080, 1.0f);
                particle_flare(hudgunorigin(attack, from, to, player), to, 500, PART_TRAIL_PROJECTILE, 0x008080, 1.0f);
                break;

            default:
                break;
        }
        const int projectile = attacks[attack].projectile;
        if (isvalidprojectile(projectile))
        {
            int attackrays = attacks[attack].rays;
            if (attackrays <= 1)
            {
                projectiles::make(player, from, to, isLocal, id, attack, projectile, attacks[attack].lifetime, attacks[attack].projspeed, attacks[attack].gravity, attacks[attack].elasticity, trackType);
            }
            else loopi(attackrays)
            {
                projectiles::make(player, from, rays[i], isLocal, id, attack, projectile, attacks[attack].lifetime, attacks[attack].projspeed, attacks[attack].gravity, attacks[attack].elasticity, trackType);
            }
        }
        if (validgun(weapon))
        {
            const int ejectProjectile = guns[weapon].ejectprojectile;
            if (isvalidprojectile(ejectProjectile) && shouldEject)
            {
                projectiles::spawnbouncer(player->eject, player, ejectProjectile);
            }
        }
        if (validsound(player->chansound[Chan_Attack]) && player->chansound[Chan_Attack] != sound)
        {
            player->stopchannelsound(Chan_Attack, 300);
        }
        if (validsound(player->chansound[Chan_Idle]))
        {
            player->stopchannelsound(Chan_Idle, 400);
        }
        bool isLooped = false;
        switch (sound)
        {
            case S_SG_A:
                playsound(sound, nullptr, player == self ? nullptr : &player->o);
                if (player ==  hudPlayer)
                {
                    player->playchannelsound(Chan_Weapon, S_SG_B);
                }
                break;

            case S_PULSE2_A:
                if (validsound(player->chansound[Chan_Attack]))
                {
                    isLooped = true;
                }
                player->playchannelsound(Chan_Attack, sound, 100, true);
                if (!isLooped)
                {
                    player->playchannelsound(Chan_Weapon, S_PULSE2_B);
                }
                break;

            case S_RAIL_A:
                playsound(sound, nullptr, player == self ? nullptr : &player->o);
                if (player ==  hudPlayer)
                {
                    player->playchannelsound(Chan_Weapon, S_RAIL_B);
                }
                break;

            default:
                playsound(sound, nullptr, player == self ? nullptr : &player->o);
        }
        if (previousAction > 200 && !isLooped)
        {
            if (player->role == ROLE_BERSERKER)
            {
                playsound(S_BERSERKER_ACTION, player);
            }
            else if (player->haspowerup(PU_DAMAGE) || player->haspowerup(PU_HASTE) || player->haspowerup(PU_AMMO))
            {
                playsound(S_ACTION_DAMAGE + player->poweruptype - 1, player);
            }
        }
    }

    void updateShotEvent(gameent* player, const int attack)
    {
        if (player == nullptr)
        {
            return;
        }

        // Ensure synchronisation between weapon and attack.
        const int attackWeapon = attacks[attack].gun;
        if (validgun(attackWeapon) && player->gunselect != attackWeapon)
        {
            player->gunselect = attackWeapon;
        }

        player->applyAttackDelay(attack);
        player->setLastAction(attack, lastmillis);
        player->useAmmo(attack);
        player->lastattack = attack;
    }

    int calculatedamage(int damage, gameent* target, gameent* actor, int atk, int flags)
    {
        if (target != actor && isinvulnerable(target, actor))
        {
            return 0;
        }
        if (!(flags & Hit_Environment))
        {
            if (attacks[atk].damage < 0 || (m_insta(mutators) && actor->type == ENT_AI && target->type == ENT_PLAYER))
            {
                target->shield = 0;
                return target->health;
            }
            if (attacks[atk].headshotdamage)
            {
                // Weapons deal locational damage only if headshot damage is specified.
                if (flags & Hit_Head)
                {
                    if (m_mayhem(mutators))
                    {
                        target->shield = 0;
                        return target->health;
                    }
                    else damage += attacks[atk].headshotdamage;
                }
                if (flags & Hit_Legs) damage /= 2;
            }
            if (actor->haspowerup(PU_DAMAGE) || actor->role == ROLE_BERSERKER) damage *= 2;
            if (isally(target, actor) || target == actor) damage /= DAMAGE_ALLYDIV;
        }
        if (target->haspowerup(PU_ARMOR) || target->role == ROLE_BERSERKER) damage /= 2;
        if (!damage) damage = 1;
        return damage;
    }

    float intersectdist = 1e16f;

    bool isheadhitbox(dynent* d, const vec& from, const vec& to, float dist)
    {
        vec bottom(d->head), top(d->head);
        top.z += d->headradius;
        return linecylinderintersect(from, to, bottom, top, d->headradius, dist);
    }

    bool islegshitbox(dynent* d, const vec& from, const vec& to, float dist)
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight;
        top.z -= d->eyeheight / 2.5f;
        return linecylinderintersect(from, to, bottom, top, d->legsradius, dist);
    }

    // Checks whether ray hits entity bounding box.
    bool isIntersectingEntity(dynent* entity, const vec& from, const vec& to, const float margin, float& dist)
    {
        vec bottom(entity->o), top(entity->o);
        bottom.z -= entity->eyeheight + margin;
        top.z += entity->aboveeye + margin;
        return linecylinderintersect(from, to, bottom, top, entity->radius + margin, dist);
    }

    dynent* intersectClosest(gameent* actor, const vec& from, const vec& to, const float margin, const int flags, float& intersectDistance)
    {
        dynent* best = nullptr;
        float bestDistance = 1e16f;
        loopi(numdynents(flags))
        {
            dynent* entity = iterdynents(i, flags);
            if (entity == nullptr || entity == actor || entity->state != CS_ALIVE)
            {
                continue;
            }
            float distance = 0;
            if (!isIntersectingEntity(entity, from, to, margin, distance))
            {
                continue;
            }
            if (distance < bestDistance)
            {
                best = entity;
                bestDistance = distance;
            }
        }
        intersectDistance = bestDistance;
        return best;
    }

    void applyhitflags(dynent* target, const vec& from, const vec& to, const int attack, const float dist, int& flags)
    {
        if (!target)
        {
            return;
        }
        if (target->type == ENT_PLAYER || target->type == ENT_AI)
        {
            if (attacks[attack].headshotdamage)
            {
                /* If a target is an inanimate object, we don't care about locational damage.
                 * If an attack has no headshot damage, it means locational damage is not applied.
                 * In other words, the attack deals the same damage regardless of where it hits the target.
                 * Example: melee attacks.
                 */
                if (isheadhitbox(target, from, to, dist))
                {
                    flags = Hit_Head;
                }
                if (islegshitbox(target, from, to, dist))
                {
                    flags = Hit_Legs;
                }
            }
        }
        else if (target->type == ENT_PROJECTILE)
        {
            flags = Hit_Projectile;
        }
    }

    void shorten(const vec& from, vec& target, float dist)
    {
        target.sub(from).mul(min(1.0f, dist)).add(from);
    }

    void scanhit(vec& from, vec& to, gameent* d, int atk)
    {
        dynent* o;
        float dist;
        const int scanFlags = DYN_PLAYER | DYN_AI | DYN_PROJECTILE;
        int hitFlags = Hit_Torso;
        if (attacks[atk].rays > 1)
        {
            const int attackRays = attacks[atk].rays;
            dynent* hits[GUN_MAXRAYS];
            loopi(attackRays)
            {
                bool isHit = false;
                if ((hits[i] = intersectClosest(d, from, rays[i], 0, scanFlags, dist)))
                {
                    applyhitflags(hits[i], from, rays[i], atk, dist, hitFlags);
                    shorten(from, rays[i], dist);
                    isHit = true;
                }
                applyImpactEffects(atk, d, from, rays[i], isHit);
            }
            loopi(attackRays)
            {
                if (hits[i])
                {
                    o = hits[i];
                    hits[i] = NULL;
                    int numhits = 1;
                    for (int j = i + 1; j < attackRays; j++) if (hits[j] == o)
                    {
                        hits[j] = NULL;
                        numhits++;
                    }
                    hit(o, d, rays[i], vec(to).sub(from).safenormalize(), attacks[atk].damage * numhits, atk, from.dist(to), numhits, hitFlags);
                }
            }
        }
        else
        {
            bool isHit = false;
            if ((o = intersectClosest(d, from, to, 0, scanFlags, dist)))
            {
                applyhitflags(o, from, to, atk, dist, hitFlags);
                shorten(from, to, dist);
                hit(o, d, to, vec(to).sub(from).safenormalize(), attacks[atk].damage, atk, from.dist(to), 1, hitFlags);
                isHit = true;
            }
            applyImpactEffects(atk, d, from, to, isHit);
        }
    }

    VARP(blood, 0, 1, 1);

    void damageeffect(int damage, dynent* d, vec hit, int atk, bool headshot)
    {
        if (d->type == ENT_PLAYER || d->type == ENT_AI)
        {
            gameent* f = (gameent*)d, * hud = followingplayer(self);
            const int color = getbloodcolor(f);
            if (f == hud)
            {
                hit.z += 0.6f * (d->eyeheight + d->aboveeye) - d->eyeheight;
            }
            if (f->haspowerup(PU_INVULNERABILITY))
            {
                particle_splash(PART_SPARK2, 100, 150, hit, getplayercolor(f, f->team), 0.50f);
                if (f->haspowerup(PU_INVULNERABILITY))
                {
                    playsound(S_ACTION_INVULNERABILITY, f);
                    return;
                }
            }
            if (f->shield)
            {
                particle_flare(hit, hit, 180, PART_SPARK3, 0xFFFF66, 0.0f, 15.0f);
            }
            if (blood && color != -1)
            {
                particle_flare(hit, hit, 300, PART_BLOOD, color, 0.1f, 13.0f);
                particle_splash(PART_BLOOD2, damage, 100 + rnd(400), hit, color, 2.0f, 200, 5, 0.01f);
            }
            else
            {
                particle_flare(hit, hit, 350, PART_SPARK3, 0xFFFF66, 0.1f, 16.0f);
                particle_splash(PART_SPARK2, damage / 2, 200, hit, 0xFFFF66, 0.5f, 300, 2, 0.001f);
            }
            if (f->health > 0 && lastmillis - f->lastyelp > 600)
            {
                if (f != hud && f->shield)
                {
                    playsound(S_SHIELD_HIT, f);
                }
                if (f->type == ENT_PLAYER)
                {
                    int painsound = getplayermodelinfo(f).painsound;
                    if (validsound(painsound))
                    {
                        playsound(painsound, f);
                    }
                    f->lastyelp = lastmillis;
                }
            }
            if (validatk(atk))
            {
                if (headshot)
                {
                    playsound(S_HIT_WEAPON_HEAD, NULL, &f->o);
                }
                else
                {
                    int hitsound = attacks[atk].hitsound;
                    if (validsound(hitsound))
                    {
                        playsound(attacks[atk].hitsound, NULL, &f->o);
                    }
                }
            }
            else playsound(S_PLAYER_DAMAGE, NULL, &f->o);
            if (f->haspowerup(PU_ARMOR)) playsound(S_ACTION_ARMOR, NULL, &f->o);
        }
        else if (d->type == ENT_PROJECTILE)
        {
            particle_flare(hit, hit, 250, PART_SPARK3, 0xFFFF66, 0.1f, 15.0f);
            playsound(S_BOUNCE_ROCKET, NULL, &hit);
        }
    }

    FVARP(goredistance, 0, 50.0f, 100.0f);
    VARP(goremax, 0, 10, 10);

    void gibeffect(int damage, const vec& vel, gameent* d)
    {
        for (int i = 0; i < goremax; i++)
        {
            const vec position = d->feetpos(1 + i);
            projectiles::spawnbouncer(position, d, Projectile_Gib);
        }
        if (blood)
        {
            const vec from = d->abovehead();
            particle_flare(from, from, 500, PART_BLOOD, getbloodcolor(d), 0.5f, 38.0f);
            particle_splash(PART_BLOOD2, damage, 300, d->o, getbloodcolor(d), 0.1f, 500, 5, 2.0f);
            addstain(STAIN_BLOOD, d->o, d->vel.neg(), 20, getbloodcolor(d), rnd(4));
        }
        if (blood && (d == self || (goredistance && camera1->o.dist(d->o) <= goredistance)))
        {
            const int goreDamage = damage * 2;
            camera::camera.addevent(self, camera::CameraEvent_Shake, goreDamage);
            addbloodsplatter(goreDamage, getbloodcolor(d));
        }
        playsound(S_GIB, NULL, &d->o);
    }

    VARP(hitsound, 0, 0, 1);

    void damageentity(int damage, gameent* d, gameent* actor, int atk, int flags, bool local = true)
    {
        if (intermission || (d->state != CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING))
        {
            return;
        }
        if (local)
        {
            damage = d->dodamage(damage, flags & Hit_Environment);
        }
        else if (actor == self)
        {
            return;
        }
        ai::damaged(d, actor);
        if (local && d->health <= 0)
        {
            kill(d, actor, atk);
        }
    }
    
    VARP(damageindicator, 0, 0, 1);

    void applyhiteffects(int damage, gameent* target, gameent* actor, const vec& position, int atk, int flags, bool local)
    {
        if (!target || (!local && actor == self && !(flags & Hit_Environment)))
        {
            return;
        }
        gameent* hud = followingplayer(self);
        if (target->type == ENT_PLAYER || target->type == ENT_AI)
        {
            if (actor)
            {
                if (!isinvulnerable(target, actor))
                {
                    target->lastpain = lastmillis;
                    if (target != actor && !isally(target, actor))
                    {
                        actor->totaldamage += damage;
                    }
                }
                if ((local || (!local && hud != self)) && actor == hud && target != actor)
                {
                    if (hitsound && actor->lasthit != lastmillis)
                    {
                        playsound(isally(target, actor) ? S_HIT_ALLY : S_HIT);
                    }
                    actor->lasthit = lastmillis;
                }
                if (damageindicator && actor == self && target != self)
                {
                    defformatstring(damageString, "%d", damage);
                    const int color = damage >= 50 && damage < 75 ? 0xFFA500 : (damage >= 75 ? 0xFF0000 : 0xFFFF00);
                    particle_textcopy(position, damageString, PART_TEXT, 2000, color, 2.5f, -0.5f, 6.0f);
                }
            }
            if (target == hud)
            {
                setdamagehud(damage, target, actor);
            }
        }
        if (attacks[atk].action == ACT_MELEE && (target == hud || (actor && actor == hud)))
        {
            // Simulate a strong impact.
            const int shake = damage * 2;
            camera::camera.addevent(target, camera::CameraEvent_Shake, shake);
            camera::camera.addevent(actor, camera::CameraEvent_Shake, shake);
        }
        damageeffect(damage, target, position, atk, flags & Hit_Head);
    }

    void dodamage(const int damage, gameent* target, gameent* actor, const vec& position, const int atk, const int flags, const bool isLocal)
    {
        if (!target)
        {
            return;
        }
        if (target->type == ENT_PLAYER || target->type == ENT_AI)
        {
            damageentity(damage, target, actor, atk, flags, isLocal);
        }
        applyhiteffects(damage, target, actor, position, atk, flags, isLocal);
    }

    void registerhit(gameent* target, gameent* actor, const vec& hitPosition, const vec& velocity, int damage, int attack, float dist, int rays, int flags)
    {
        if (!m_mp(gamemode) || target == actor)
        {
            target->hitpush(damage, velocity, actor, attack);
        }
        if (!m_mp(gamemode))
        {
            damageentity(damage, target, actor, attack, flags);
        }
        else
        {
            hitmsg& hit = hits.add();
            hit.target = target->clientnum;
            hit.lifesequence = target->lifesequence;
            hit.dist = int(dist * DMF);
            hit.rays = rays;
            hit.flags = flags;
            hit.id = -1;
            if (target == actor)
            {
                hit.dir = ivec(0, 0, 0);
            }
            else
            {
                hit.dir = ivec(vec(velocity).mul(DNF));
            }
        }
    }

    void hit(dynent* target, gameent* actor, const vec& hitPosition, const vec& velocity, int damage, const int attack, const float dist, const int rays, const int flags, const int id, const bool shouldSend)
    {
        gameent* player = (gameent*)target;
        if (player == nullptr || actor == nullptr)
        {
            return;
        }
        damage = calculatedamage(damage, player, actor, attack, flags);
        switch (target->type)
        {
            case ENT_PLAYER:
            {
                registerhit(player, actor, hitPosition, velocity, damage, attack, dist, rays, flags);
                break;
            }
            case ENT_AI:
            {
                monster* npc = (monster*)player;
                hitmonster(damage, npc, actor, attack, velocity, flags);
                break;
            }
            case ENT_PROJECTILE:
            {
                projectiles::registerhit(target, actor, attack, dist, rays);
                break;
            }
            default:
            {
                break;
            }
        }

        // Send hit message to server after registering it, if requested.
        if (shouldSend)
        {
            addmsg(N_HIT, "rci2iv", actor, id, attack, hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
        }

        // Apply hit effects like blood, shakes, etc.
        applyhiteffects(damage, player, actor, hitPosition, attack, flags, true);
    }

    void gunselect(int gun, gameent* d)
    {
        if (!mainmenu)
        {
            d->lastswitchattempt = lastmillis;
            if (gun == d->gunselect || lastmillis - d->lastswitch < 100)
            {
                return;
            }
            d->lastWeaponUsed = d->gunselect;
            addmsg(N_GUNSELECT, "rci", d, gun);
            d->lastattack = ATK_INVALID;
            doweaponchangeffects(d, gun);
        }
        d->gunselect = gun;
    }
    ICOMMAND(getweapon, "", (), intret(self->gunselect));
    ICOMMAND(isgunselect, "s", (char* gun),
    {
        gameent * d = followingplayer(self);
        int weapon = getweapon(gun);
        intret(validgun(weapon) && d->gunselect == weapon ? 1 : 0);
    });

    void doweaponchangeffects(gameent* d, int gun)
    {
        d->lastswitch = lastmillis;
        sway.addevent(d, SwayEvent_Switch, 500, -15);
        d->stopchannelsound(Chan_Weapon, 200);
        if (!validgun(gun))
        {
            if (!validgun(d->gunselect)) return;
            gun = d->gunselect;
        }
        int switchsound = guns[gun].switchsound;
        if (d != followingplayer(self))
        {
            switchsound = S_WEAPON_LOAD;
        }
        else if (validsound(switchsound))
        {
            d->chansound[Chan_Weapon] = switchsound;
            d->chan[Chan_Weapon] = playsound(d->chansound[Chan_Weapon], d, NULL, NULL, 0, 0, 0, d->chan[Chan_Weapon]);
        }
        if (d == followingplayer(self))
        {
            camera::camera.zoomstate.disable();
        }
    }

    void nextweapon(int dir, bool force = false)
    {
        if (self->state != CS_ALIVE)
        {
            return;
        }
        if (camera::zoom && guns[self->gunselect].zoom)
        {
            camera::zoomfov = clamp(camera::zoomfov - dir, 10, 90);
        }
        else
        {
            dir = (dir < 0 ? NUMGUNS - 1 : 1);
            int gun = self->gunselect;
            for (int i = 0; i < NUMGUNS; i++)
            {
                gun = (gun + dir) % NUMGUNS;
                if (force || self->ammo[gun])
                {
                    break;
                }
            }
            if (gun != self->gunselect)
            {
                gunselect(gun, self);
            }
            else
            {
                self->lastswitchattempt = lastmillis;
            }
        }
    }
    ICOMMAND(nextweapon, "ii", (int* dir, int* force), nextweapon(*dir, *force != 0));

    // Select the last weapon used, or the previous weapon in the wheel if no ammo for last weapon.
    void selectPreviousWeapon()
    {
        if (self->state != CS_ALIVE)
        {
            return;
        }
        const int weapon = self->lastWeaponUsed; // Try to select the last weapon used first.
        if (weapon == self->gunselect)
        {
            return;
        }
        if (!validgun(self->lastWeaponUsed) || !self->ammo[weapon])
        {
            // Select a weapon in the wheel.
            const int direction = -1; // -1 for previous weapon in the wheel.
            nextweapon(direction);
        }
        else
        {
            gunselect(weapon, self);
        }
    }
    ICOMMAND(lastweapon, "", (), selectPreviousWeapon());

    void selectRelatedWeapon()
    {
        if (self->state != CS_ALIVE || mainmenu)
        {
            return;
        }
        struct RelatedWeapon
        {
            const int from;
            const int to;
        };
        static const RelatedWeapon related[] =
        {
            { GUN_SMG,     GUN_SCATTER },
            { GUN_SCATTER, GUN_SMG     },
            { GUN_PULSE,   GUN_ROCKET  },
            { GUN_PISTOL,  GUN_RAIL    },
            { GUN_ROCKET,  GUN_PULSE   },
            { GUN_RAIL,    GUN_SCATTER }
        };
        const size_t relatedSize = sizeof(related) / sizeof(related[0]);
        int weapon = GUN_INVALID;
        for (size_t i = 0; i < relatedSize; ++i)
        {
            if (related[i].from == self->gunselect)
            {
                weapon = related[i].to;
                break;
            }
        }
        if (!validgun(weapon) || weapon == self->gunselect)
        {
            return;
        }
        gunselect(weapon, self);
    }
    ICOMMAND(relatedweapon, "", (), selectRelatedWeapon());

    int getweapon(const char* name)
    {
        if (isdigit(name[0])) return parseint(name);
        else
        {
            int len = strlen(name);
            loopi(sizeof(guns) / sizeof(guns[0])) if (!strncasecmp(guns[i].name, name, len)) return i;
        }
        return -1;
    }

    void setweapon(const char* name, bool force = false)
    {
        int gun = getweapon(name);
        if (self->state != CS_ALIVE || !validgun(gun))
        {
            return;
        }
        if (force || self->ammo[gun] || mainmenu)
        {
            gunselect(gun, self);
        }
        else
        {
            self->lastswitchattempt = lastmillis;
        }
    }
    ICOMMAND(setweapon, "si", (char* name, int* force), setweapon(name, *force != 0));

    void cycleweapon(int numguns, int* guns, bool force = false)
    {
        if (numguns <= 0 || self->state != CS_ALIVE)
        {
            return;
        }
        int offset = 0;
        for (int i = 0; i < NUMGUNS; i++)
        {
            if (guns[i] == self->gunselect)
            {
                offset = i + 1;
                break;
            }
        }
        for (int i = 0; i < numguns; i++)
        {
            const int gun = guns[(i + offset) % numguns];
            if (gun >= 0 && gun < NUMGUNS && (force || self->ammo[gun]))
            {
                gunselect(gun, self);
                return;
            }
        }
        self->lastswitchattempt = lastmillis;
        playsound(S_WEAPON_NOAMMO);
    }
    ICOMMAND(cycleweapon, "V", (tagval* args, int numargs),
    {
         int numguns = min(numargs, 3);
         int guns[3];
         for (int i = 0; i < numguns; i++)
         {
             guns[i] = getweapon(args[i].getstr());
         }
         cycleweapon(numguns, guns);
    });

    void weaponswitch(gameent* d)
    {
        if (d->state != CS_ALIVE)
        {
            return;
        }

        static const int weaponPriority[] = { GUN_SMG, GUN_SCATTER, GUN_PULSE, GUN_ROCKET, GUN_RAIL, GUN_GRENADE, GUN_INSTA, GUN_ZOMBIE, GUN_PISTOL };
        for (int weapon : weaponPriority)
        {
            if (weapon != d->gunselect && d->ammo[weapon])
            {
                gunselect(weapon, d);
                break;
            }
        }
    }

    ICOMMAND(weapon, "V", (tagval* args, int numargs),
    {
        if (self->state != CS_ALIVE)
        {
            return;
        }
        loopi(3)
        {
            const char* name = i < numargs ? args[i].getstr() : "";
            if (name[0])
            {
                int gun = getweapon(name);
                if (validgun(gun) && gun != self->gunselect && self->ammo[gun])
                { 
                    gunselect(gun, self);
                    return;
                }
            }
            else
            { 
                weaponswitch(self);
                return;
            }
        }
        self->lastswitchattempt = lastmillis;
        playsound(S_WEAPON_NOAMMO);
    });

    void autoswitchweapon(gameent* d, int type)
    {
        /* This function makes our client switch to the weapon we just picked up,
         * meaning the argument is the type of the item.
         * To switch to a specific weapon, we have other specialised functions.
         */
        if (d != self || !autoswitch || type < I_AMMO_SG || type > I_AMMO_GRENADE)
        {
            /* We stop caring if this is not our client,
             * auto-switch is disabled or the item is not a weapon.
             */
            return;
        }
        const bool isAttacking = validact(d->attacking) || camera::camera.zoomstate.isinprogress();
        if (isAttacking)
        {
            // Do not interrupt someone during a fight.
            d->lastswitchattempt = lastmillis; // Let the player know we tried to switch (and possibly reflect that on the HUD).
            return;
        }
        itemstat& is = itemstats[type - I_AMMO_SG];
        if (d->gunselect != is.info && !d->ammo[is.info])
        {
            gunselect(is.info, self);
        }
    }

    int checkweaponzoom()
    {
        gameent* hud = followingplayer(self);
        if (hud->state == CS_ALIVE || hud->state == CS_LAGGED)
        {
            return guns[hud->gunselect].zoom;
        }
        return Zoom_None;
    }

    VARP(hudgun, 0, 1, 1);

    vec hudgunorigin(int attack, const vec& from, const vec& to, gameent* d)
    {
        // When zoomed in, originate from the feet position to avoid obstructing view.
        if (camera::camera.zoomstate.isenabled() && guns[d->gunselect].zoom == Zoom_Scope && d == self)
        {
            return d->feetpos(4);
        }
        else
        {
            vec position = d->o;
            if (attacks[attack].action == ACT_THROW && d->hand.x >= 0) // Check if the hand position is useful and defined.
            {
                // "Thrown" projectiles should originate from the hand position.
                position = d->hand;
            }
            else if (d->muzzle.x >= 0) // Check if the muzzle position is defined.
            {
                // Others should originate from the muzzle position.
                position = d->muzzle;
            }
            return position;
        }

        // Fallback to offset if no muzzle or hand position is defined.
        vec offset(from);
        if (d != hudplayer() || camera::isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            if (d->type != ENT_AI)
            {
                offset.z += (d->aboveeye + d->eyeheight) * 0.75f - d->eyeheight;
                vecfromyawpitch(d->yaw, 0, 0, -1, right);
                offset.add(right.mul(0.5f * d->radius));
                offset.add(front);
            }
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if (hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        else offset.sub(vec(camup).mul(0.8f));
        return offset;
    }

    VARP(hudgunsway, 0, 1, 1);

    FVAR(swaystep, 1, 35.0f, 100);
    FVAR(swayside, 0, 0.025f, 1);
    FVAR(swayup, -1, 0.02f, 1);
    FVAR(swayrollfactor, 1, 4.2f, 30);
    FVAR(swaydecay, 0.1f, 0.996f, 0.9999f);
    FVAR(swayinertia, 0.0f, 0.04f, 1.0f);
    FVAR(swaymaxinertia, 0.0f, 15.0f, 1000.0f);
    FVAR(swayfallmin, -200.0f, -80.0f, 0);
    FVAR(swayfallfactor, 0, 0.035f, 1);
    FVAR(swayfallsmoothfactor, 0, 0.8f, 1);

    swayinfo sway;

    void swayinfo::updatedirection(gameent* owner)
    {
        float k = pow(0.7f, curtime / 10.0f);
        dir.mul(k);
        vec vel(owner->vel);
        vel.add(owner->falling);
        const float zoomFactor = 1.0f - camera::camera.zoomstate.progress;
        dir.add(vec(vel).mul((1 - k) / (8 * max(vel.magnitude(), owner->speed)) * zoomFactor));

        yaw = owner->yaw;
        pitch = owner->pitch;
        roll = owner->roll * swayrollfactor;
    }

    FVAR(weaponoffsetx, -5.0f, -2.955f, 5.0f);
    FVAR(weaponoffsety, -5.0f, -1.136f, 5.0f);
    FVAR(weaponoffsetz, -5.0f, -2.120f, 5.0f);

    void swayinfo::updatePosition(const gameent* owner, vec& position)
    {
        const vec basePosition = vec(weaponoffsetx, weaponoffsety, weaponoffsetz);
        position = basePosition;
        const vec zoomPosition = guns[owner->gunselect].zoomPosition;
        const float progress = camera::camera.zoomstate.progress;
        position.lerp(basePosition, zoomPosition, 1.0f - expf(-4.0f * progress));
    }

    void swayinfo::update(gameent* owner, vec& position)
    {
        if (owner->onfloor())
        {
            speed = min(sqrtf(owner->vel.x * owner->vel.x + owner->vel.y * owner->vel.y), owner->speed);
            dist += speed * curtime / 1000.0f;
            dist = fmod(dist, 2 * swaystep);
            fade = 1;
        }
        else if (fade > 0)
        {
            dist += speed * fade * curtime / 1000.0f;
            dist = fmod(dist, 2 * swaystep);
            fade -= 0.5f * (curtime * owner->speed) / (swaystep * 1000.0f);
        }

        updatedirection(owner);
        processevents();
        updatePosition(owner, position);

        vecfromyawpitch(owner->yaw, 0, 0, 1, o);

        float steps = dist / swaystep * M_PI;
        const float zoomFactor = 1.0f - camera::camera.zoomstate.progress;
        o.mul(swayside * sinf(steps) * zoomFactor);

        vec side = vec((owner->yaw + 90) * RAD, 0.0f), trans = vec(0, 0, 0);

        static float speed = 0.0f;
        float currentspeed = owner->vel.magnitude();
        speed += (owner->vel.magnitude() - speed) * curtime * (currentspeed < speed ? 0.01f : 0.001f);

        float speedfactor = clamp(speed / 150.0f, 0.0f, 1.0f);

        // Magic floats to generate the animation cycle.
        const float magic1 = cosf(steps) + 1;
        const float magic2 = sinf(steps * 2.0f) + 1;
        const float magic3 = (magic1 * magic1 * 0.25f) - 0.5f;
        const float magic4 = (magic2 * magic2 * 0.25f) - 0.5f;
        const float magic5 = sinf(lastmillis * 0.001f); // Low frequency detail.

        // Walk cycle animation.
        vec directionforward = vec(owner->yaw * RAD, 0.0f), directionside = vec((owner->yaw + 90) * RAD, 0.0f);
        float rotationyaw = 0, rotationpitch = 0, rotationroll = 0;
        trans.add(vec(directionforward).mul(swayside * magic4 * 2.0f * zoomFactor));
        trans.add(vec(directionside).mul(swayside * magic5 * 2.0f * zoomFactor));
        trans.add(vec(o).mul(-4.0f));
        trans.z += swayup * magic2 * 1.5f;
        trans.z += swayup * (fabs(sinf(steps)) - 1) * zoomFactor;
        trans.z -= speedfactor * 0.15f;

        rotationyaw += swayside * magic3 * 24.0f;
        rotationpitch += swayup * magic2 * -10.0f;

        // "Look-around" animation.
        static int lastsway = 0;
        static vec2 lastcamera = vec2(camera1->yaw, camera1->pitch);
        static vec2 cameravelocity = vec2(0, 0);

        if (lastmillis != lastsway) // Prevent running the inertia math multiple times in the same frame.
        {
            vec2 currentcamera = vec2(camera1->yaw, camera1->pitch);
            vec2 camerarotation = vec2(lastcamera).sub(currentcamera);

            if (camerarotation.x > 180.0f) camerarotation.x -= 360.0f;
            else if (camerarotation.x < -180.0f) camerarotation.x += 360.0f;

            cameravelocity.mul(powf(swaydecay, curtime));
            cameravelocity.add(vec2(camerarotation).mul(swayinertia));
            cameravelocity.clamp(-swaymaxinertia, swaymaxinertia);

            lastcamera = currentcamera;
            lastsway = lastmillis;
        }
        trans.add(side.mul(cameravelocity.x * 0.06f));
        trans.z += cameravelocity.y * 0.045f;

        verticalVelocity = verticalVelocity * swayfallsmoothfactor + owner->falling.z * (1.0f - swayfallsmoothfactor);
        verticalVelocity = clamp(verticalVelocity, swayfallmin, 0.0f);
        const float fallSwayPitch = verticalVelocity * swayfallfactor;

        rotationyaw += cameravelocity.x * -0.3f;
        rotationpitch += (cameravelocity.y * -0.3f) - fallSwayPitch;
        rotationroll += cameravelocity.x * -0.5f;

        camera::camera.velocity.x = cameravelocity.x * -0.3f + (camera::camera.direction.x * 0.5f);
        camera::camera.velocity.y = cameravelocity.y * -0.3f + (camera::camera.direction.y * 0.5f) + (camera::camera.direction.z * 0.25f);

        o.add(trans);

        yaw += rotationyaw;
        pitch += rotationpitch;
        roll += rotationroll;

        o.add(dir).add(owner->o);
        if (!hudgunsway) o = owner->o;
    }

    void swayinfo::addevent(const gameent* owner, int type, int duration, float factor)
    {
        if (!hudgunsway || !owner || owner != followingplayer(self))
        {
            // The first-person weapon sway is rendered only for ourselves or the player being spectated.
            return;
        }
        swayEvent& swayevent = events.add();
        swayevent.type = type;
        swayevent.millis = lastmillis;
        swayevent.duration = duration;
        swayevent.factor = factor;
    }

    void swayinfo::processevents()
    {
        loopv(events)
        {
            swayEvent& event = events[i];
            const int elapsed = lastmillis - event.millis;
            if (elapsed > event.duration)
            {
                events.remove(i--);
            }
            else
            {
                switch (event.type)
                {
                    case SwayEvent_Jump:
                    {
                        const float progress = clamp((lastmillis - event.millis) / static_cast<float>(event.duration), 0.0f, 1.0f);
                        const float zoomFactor = 1.0f - camera::camera.zoomstate.progress;
                        const float curve = sinf(progress * M_PI) * event.factor * zoomFactor;
                        pitch += curve;
                        dir.z -= curve * 0.012f;
                        break;
                    }

                    case SwayEvent_Land:
                    case SwayEvent_LandHeavy:
                    case SwayEvent_Switch:
                    {
                        const float progress = clamp((lastmillis - event.millis) / static_cast<float>(event.duration), 0.0f, 1.0f);
                        const float curve = event.factor * sinf(progress * M_PI);
                        if (event.type == SwayEvent_LandHeavy)
                        {
                            roll += curve;
                            dir.z += curve * 0.05f;
                        }
                        pitch += curve;
                        break;
                    }

                    case SwayEvent_Crouch:
                    {
                        const float progress = clamp((lastmillis - event.millis) / static_cast<float>(event.duration), 0.0f, 1.0f);
                        const float curve = ease::outback(progress) * sinf(progress * M_PI) * event.factor;
                        roll += curve;
                        dir.z += curve * 0.03f;
                        break;
                    }
                }
            }
        }
    }
};
