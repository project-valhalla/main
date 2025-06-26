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

    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest, gameent *d)
    {
        vec offset;
        do offset = vec(rndscale(1), rndscale(1), rndscale(1)).sub(0.5f);
        while(offset.squaredlen() > 0.5f * 0.5f);
        const bool isCrouched = d->physstate >= PHYS_SLOPE && d->crouching && d->crouched();
        offset.mul((to.dist(from) / 1024) * spread * (isCrouched ? 0.5f : 1.0f));
        offset.z /= 2;
        dest = vec(offset).add(to);
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
        }
    }

    void trackparticles(physent *owner, vec &o, vec &d)
    {
        if(owner->type != ENT_PLAYER && owner->type != ENT_AI) return;
        gameent *pl = (gameent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattack < 0 || attacks[pl->lastattack].gun != pl->gunselect) return;
        float dist = o.dist(d);
        o = pl->muzzle;
        if(dist <= 0) d = o;
        else
        {
            vecfromyawpitch(owner->yaw, owner->pitch, 1, 0, d);
            float newdist = raycube(owner->o, d, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
            d.mul(min(newdist, dist)).add(owner->o);
        }
    }

    void trackdynamiclights(physent *owner, vec &o, vec &hud)
    {
        if(owner->type!=ENT_PLAYER && owner->type != ENT_AI) return;
        gameent *pl = (gameent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattack < 0 || attacks[pl->lastattack].gun != pl->gunselect) return;
        o = pl->muzzle;
        hud = owner == followingplayer(self) ? vec(pl->o).add(vec(0, 0, 2)) : pl->muzzle;
    }

    bool canshoot(gameent* d, int atk, int gun, int projectile)
    {
        if (attacks[atk].action != ACT_MELEE && (!d->ammo[gun] || attacks[atk].use > d->ammo[gun]))
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

    void addrecoil(gameent* d, const vec& dir, const int atk)
    {
        int kickAmount = attacks[atk].recoilamount;
        if (kickAmount)
        {
            if (d->haspowerup(PU_DAMAGE))
            {
                const int recoilPowerupMultiplier = 2;
                kickAmount *= recoilPowerupMultiplier;
            }
            const bool isCrouched = d->physstate >= PHYS_SLOPE && d->crouching && d->crouched();
            if (kickAmount && !isCrouched)
            {
                vec kickback = vec(dir).mul(kickAmount * -2.5f);
                d->vel.add(kickback);
            }
            d->recoil = kickAmount;
        }
        else
        {
            const int recoilShake = 40; // Default shake value for weapons with no recoil.
            kickAmount = recoilShake;
        }
        camera::camera.addevent(d, camera::CameraEvent_Shake, kickAmount);
    }

    void updaterecoil(gameent* d, int curtime)
    {
        if (!d->recoil)
        {
            return;
        }

        const float amount = d->recoil * (curtime / 1000.0f);
        d->pitch += amount;
        float friction = 4.0f / curtime * 30.0f;
        d->recoil = d->recoil * (friction - 2.8f) / friction;
        camera::fixrange();
    }

    VARP(autoswitch, 0, 1, 1);

    void shoot(gameent *d, const vec &targ)
    {
        int prevaction = d->lastaction, attacktime = lastmillis-prevaction;
        if(attacktime<d->gunwait) return;
        d->gunwait = 0;
        if(!validact(d->attacking) || !validgun(d->gunselect)) return;
        int gun = d->gunselect, act = d->attacking, atk = guns[gun].attacks[act], projectile = attacks[atk].projectile;
        d->lastaction = lastmillis;
        d->lastattack = atk;
        if (!canshoot(d, atk, gun, projectile))
        {
            if (d == self)
            {
                sendsound(S_WEAPON_NOAMMO, d);
                d->gunwait = 600;
                d->lastattack = ATK_INVALID;
                if (autoswitch && !d->ammo[gun])
                {
                    weaponswitch(d);
                }
            }
            return;
        }
        if (!d->haspowerup(PU_AMMO)) d->ammo[gun] -= attacks[atk].use;

        vec from = d->o, to = targ, dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        addrecoil(d, dir, atk);
        float shorten = attacks[atk].range && dist > attacks[atk].range ? attacks[atk].range : 0,
              barrier = raycube(d->o, dir, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
        if(barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
            shorten = barrier;
        if(shorten) to = vec(dir).mul(shorten).add(from);

        if(attacks[atk].rays > 1)
        {
            loopi(attacks[atk].rays)
            {
                offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i], d);
            }
        }
        else if(attacks[atk].spread)
        {
            offsetray(from, to, attacks[atk].spread, attacks[atk].range, to, d);
        }

        hits.setsize(0);

        if (!isattackprojectile(attacks[atk].projectile))
        {
            scanhit(from, to, d, atk);
        }

        const int id = lastmillis - maptime;
        shoteffects(atk, from, to, d, true, id, prevaction);

        if(d==self || d->ai)
        {
            addmsg(N_SHOOT, "rci2i6iv", d, id, atk,
                   static_cast<int>(from.x * DMF), static_cast<int>(from.y * DMF), static_cast<int>(from.z * DMF),
                   static_cast<int>(to.x * DMF), static_cast<int>(to.y * DMF), static_cast<int>(to.z * DMF),
                   hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
        }
        if(!attacks[atk].isfullauto) d->attacking = ACT_IDLE;
        int gunwait = attacks[atk].attackdelay;
        if(d->haspowerup(PU_HASTE) || d->role == ROLE_BERSERKER) gunwait /= 2;
        d->gunwait = gunwait;
        if(d->gunselect == GUN_PISTOL && d->ai) d->gunwait += int(d->gunwait*(((101-d->skill)+rnd(111-d->skill))/100.f));
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
    }

    void checkattacksound(gameent *d, bool local)
    {
        int attack = ATK_INVALID;
        if (validact(d->attacking))
        {
            attack = guns[d->gunselect].attacks[d->attacking];
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
        if(validatk(attack) && d->lastattack == attack && lastmillis - d->lastaction < attacks[attack].attackdelay + 50 && isValidClient)
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

    void updateweapons(int curtime)
    {
        if (self->clientnum >= 0 && self->state == CS_ALIVE)
        {
            shoot(self, worldpos); // Only shoot when connected to a server.
            updaterecoil(self, curtime);
        }
        projectiles::update(curtime); // Need to do this after the player shoots so bouncers don't end up inside player's BB next frame.
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

    void impacteffects(int atk, gameent* d, const vec& from, const vec& to, bool hit = false)
    {
        if (!validatk(atk) || (!hit && isemptycube(to)) || from.dist(to) > attacks[atk].range) return;
        vec dir = vec(from).sub(to).safenormalize();
        int material = lookupmaterial(to);
        bool iswater = (material & MATF_VOLUME) == MAT_WATER, isglass = (material & MATF_VOLUME) == MAT_GLASS;
        switch (atk)
        {
            case ATK_SCATTER1:
            case ATK_SCATTER2:
            {
                adddynlight(vec(to).madd(dir, 4), 6, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if (hit || iswater || isglass) break;
                particle_splash(PART_SPARK2, 10, 80 + rnd(380), to, 0xFFC864, 0.1f, 250, 2, 0.001f);
                particle_splash(PART_SMOKE, 10, 250, to, 0x555555, 0.1f, 100, 100, 7.0f);
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.50f + rndscale(1.0f), 0xFFFFFF, rnd(4));
                break;
            }

            case ATK_SMG1:
            case ATK_SMG2:
            {
                adddynlight(vec(to).madd(dir, 4), 15, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if (hit || iswater || isglass) break;
                particle_fireball(to, 0.5f, PART_EXPLOSION2, 120, 0xFFC864, 2.0f);
                particle_splash(PART_EXPLODE4, 50, 40, to, 0xFFC864, 1.0f, 150, 2, 0.1f);
                particle_splash(PART_SPARK2, 30, 250, to, 0xFFC864, 0.05f + rndscale(0.2f), 250, 2, 0.001f);
                particle_splash(PART_SMOKE, 30, 250, to, 0x555555, 0.2f, 80, 100, 6.0f);
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.50f + rndscale(1.0f), 0xFFFFFF, rnd(4));
                break;
            }

            case ATK_PULSE2:
            {
                adddynlight(vec(to).madd(dir, 4), 80, vec(1.0f, 0.50f, 1.0f), 20);
                if (hit)
                {
                    particle_flare(to, to, 120, PART_ELECTRICITY, 0xEE88EE, 1.0f + rndscale(8.0f));
                    break;
                }
                if (iswater) break;
                particle_splash(PART_SPARK2, 10, 300, to, 0xEE88EE, 0.01f + rndscale(0.10f), 350, 2);
                particle_splash(PART_SMOKE, 20, 300, to, 0x777777, 0.1f, 100, 50, 3.5f + rndscale(4.5f));
                addstain(STAIN_SCORCH, to, vec(from).sub(to).normalize(), 1.0f + rndscale(1.5f));
                playsound(attacks[atk].impactsound, NULL, &to);
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            case ATK_INSTA:
            {
                bool isInstagib = attacks[atk].gun == GUN_INSTA;
                adddynlight(vec(to).madd(dir, 4), 60, !isInstagib ? vec(0.25f, 1.0f, 0.75f) : vec(0.5f, 0.5f, 0.5f), 180, 75, DL_EXPAND);
                if (hit)
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
            {
                adddynlight(vec(to).madd(dir, 4), 30, vec(0.25, 1.0f, 1.0f), 200, 10, DL_SHRINK);
                if (hit || iswater || isglass) break;
                particle_fireball(to, 2.2f, PART_EXPLOSION1, 140, 0x00FFFF, 0.1f);
                particle_splash(PART_SPARK2, 50, 250, to, 0x00FFFF, 0.08f + rndscale(0.18f), 150, 2, 0.001f);
                addstain(STAIN_SCORCH, to, vec(from).sub(to).normalize(), 0.80f + rndscale(1.0f));
                addstain(STAIN_GLOW1, to, dir, 1.50f, 0x00FFFF);
                break;
            }

            default: break;
        }

        if (hit || atk == ATK_PULSE2) return;

        int impactsound = attacks[atk].impactsound;

        bool ismelee = attacks[atk].action == ACT_MELEE;
        if (iswater)
        {
            if (!ismelee)
            {
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.80f);
            }
            if (attacks[atk].rays <= 1) // Temporary measure: avoid sound spam.
            {
                impactsound = S_IMPACT_WATER;
            }
        }
        else if (isglass)
        {
            particle_splash(PART_GLASS, 20, 200, to, 0xFFFFFF, 0.10 + rndscale(0.20f));
            addstain(STAIN_GLASS_HOLE, to, vec(from).sub(to).normalize(), ismelee ? 2.0f : 1.0f + rndscale(2.0f), 0xFFFFFF, rnd(4));
            if (attacks[atk].rays <= 1) // Temporary measure: avoid sound spam.
            {
                impactsound = S_IMPACT_GLASS;
            }
        }
        else if (ismelee)
        {
            addstain(STAIN_PUNCH_HOLE, to, vec(from).sub(to).normalize(), 2.0f);
        }

        if (validsound(impactsound))
        {
            playsound(impactsound, NULL, &to);
        }
    }

    VARP(muzzleflash, 0, 1, 1);

    void shoteffects(int atk, const vec& from, const vec& to, gameent* d, bool local, int id, int prevaction, bool hit)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun, sound = attacks[atk].sound, previousaction = lastmillis - prevaction;
        float dist = from.dist(to);
        gameent* hud = followingplayer(self);
        bool shouldeject = d->eject.x >= 0 && d == hud;
        vec up = vec(0, 0, 0);
        switch (atk)
        {
            case ATK_MELEE:
            case ATK_ZOMBIE:
            {
                if (!local)
                {
                    impacteffects(atk, d, from, to, hit);
                }
                break;
            }

            case ATK_SCATTER1:
            case ATK_SCATTER2:
            {
                if (d->muzzle.x >= 0 && muzzleflash)
                {
                    if (d == hud)
                    {
                        particle_flare(d->muzzle, d->muzzle, 850, PART_MUZZLE_SMOKE, 0x202020, 0.1f, d, 3.0f);
                        particle_flare(d->muzzle, d->muzzle, 200, PART_SPARKS, 0xEFE598, 0.1f, d, 3.0f + rndscale(5.0f));
                    }
                    particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH, 0xEFE598, 3.0f, d, 0.1f);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 200, vec(0.5f, 0.375f, 0.25f), 80, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                }
                if (!local)
                {
                    loopi(attacks[atk].rays)
                    {
                        offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i], d);
                        impacteffects(atk, d, from, rays[i], hit);
                    }
                }
                if (atk == ATK_SCATTER2)
                {
                    loopi(attacks[atk].rays)
                    {
                        particle_flare(hudgunorigin(gun, from, rays[i], d), rays[i], 80, PART_TRAIL, 0xFFC864, 0.95f);
                    }
                }
                break;
            }

            case ATK_SMG1:
            case ATK_SMG2:
            {
                if (d->muzzle.x >= 0 && muzzleflash)
                {
                    if (d == hud)
                    {
                        particle_flare(d->muzzle, d->muzzle, 300, PART_MUZZLE_SMOKE, 0xFFFFFF, 0.5f, d, 2.0f);
                        particle_flare(d->muzzle, d->muzzle, 200, PART_SPARKS, 0xEFE898, 0.1f, d, 4.0f);
                    }
                    particle_flare(d->muzzle, d->muzzle, 130, PART_MUZZLE_FLASH3, 0xEFE898, 0.1f, d, 1.8f);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 120, vec(0.5f, 0.375f, 0.25f), 80, 75, DL_EXPAND, 0, vec(0, 0, 0), d);
                }
                if (atk == ATK_SMG2) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0xFFC864, 0.95f);
                if (!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_PULSE1:
            {
                if (muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 100, PART_MUZZLE_FLASH2, 0xDD88DD, 2.5f, d);
                }
                break;
            }
            case ATK_PULSE2:
            {
                if (muzzleflash && d->muzzle.x >= 0)
                {
                    if (previousaction > 200)
                    {
                        particle_flare(d->muzzle, d->muzzle, 250, PART_MUZZLE_FLASH5, 0xDD88DD, 0.5f, d, 3.0f);
                    }
                    else particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH2, 0xDD88DD, 2.0f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 75, vec(1.0f, 0.50f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_LIGHTNING, 0xEE88EE, 1.0f, d);
                particle_fireball(to, 1.0f, PART_EXPLOSION2, 100, 0xDD88DD, 3.0f);
                if (!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_ROCKET1:
            {
                if (muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 150, PART_MUZZLE_FLASH4, 0xEFE898, 0.1f, d, 3.0f);
                }
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            {
                if (d->muzzle.x >= 0 && muzzleflash)
                {
                    if (d == hud)
                    {
                        particle_flare(d->muzzle, d->muzzle, 200, PART_SPARKS, 0x77DD77, 0.1f, d, 3.0f + rndscale(5.0f));
                        particle_flare(d->muzzle, d->muzzle, 450, PART_MUZZLE_SMOKE, 0x202020, 3.0f, d);
                    }
                    particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH, 0x77DD77, 1.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 100, vec(0.25f, 1.0f, 0.75f), 80, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                }
                if (atk == ATK_RAIL2) particle_trail(PART_SMOKE, 350, hudgunorigin(gun, from, to, d), to, 0xDEFFDE, 0.3f, 50);
                particle_flare(hudgunorigin(gun, from, to, d), to, 600, PART_TRAIL, 0x55DD55, 0.50f);
                if (!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                if (d->muzzle.x >= 0 && muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH5, 0x74BCF9, 0.2f, d, 3.5f);
                }
                up = vec(to).addz(dist / 8);
                break;
            }

            case ATK_PISTOL1:
            case ATK_PISTOL2:
            {
                if (muzzleflash && d->muzzle.x >= 0)
                {
                    if (atk == ATK_PISTOL1)
                    {
                        if (d == hud)
                        {
                            particle_flare(d->muzzle, d->muzzle, 200, PART_SPARKS, 0x00FFFF, 0.1f, d, 5.0f);
                        }
                        particle_flare(d->muzzle, d->muzzle, 120, PART_MUZZLE_FLASH3, 0x00FFFF, 0.1f, d, 2.5f);
                        particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0x00FFFF, 2.0f);
                        if (!local)
                        {
                            impacteffects(atk, d, from, to, hit);
                        }
                    }
                    else
                    {
                        particle_flare(d->muzzle, d->muzzle, 280, PART_MUZZLE_FLASH2, 0x00FFFF, 0.1f, d, 3.0f);
                    }
                    adddynlight(hudgunorigin(attacks[atk].gun, d->o, to, d), 80, vec(0.25f, 1.0f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                break;
            }

            case ATK_INSTA:

                if (muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH, 0x008080, 0.1f, d, 2.75f);
                    particle_flare(d->muzzle, d->muzzle, 450, PART_MUZZLE_SMOKE, 0x006060, 0.1f, d, 3.0f);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 80, vec(0.25f, 0.75f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_trail(PART_SMOKE, 350, hudgunorigin(gun, from, to, d), to, 0x006060, 0.3f, 50);
                particle_flare(hudgunorigin(gun, from, to, d), to, 100, PART_LIGHTNING, 0x008080, 1.0f);
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, PART_TRAIL_PROJECTILE, 0x008080, 1.0f);
                break;

            default: break;
        }
        const int projectile = attacks[atk].projectile;
        if (isvalidprojectile(projectile))
        {
            int attackrays = attacks[atk].rays;
            if (attackrays <= 1)
            {
                vec aim = up.iszero() ? to : up;
                projectiles::make(d, from, aim, local, id, atk, projectile, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
            }
            else loopi(attackrays)
            {
                projectiles::make(d, from, rays[i], local, id, atk, projectile, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
            }
        }
        if (validgun(gun))
        {
            const int ejectProjectile = guns[gun].ejectprojectile;
            if (isvalidprojectile(ejectProjectile) && shouldeject)
            {
                projectiles::spawnbouncer(d->eject, d, ejectProjectile);
            }
        }
        bool looped = false;
        if (validsound(d->chansound[Chan_Attack]) && d->chansound[Chan_Attack] != sound)
        {
            d->stopchannelsound(Chan_Attack, 300);
        }
        if (validsound(d->chansound[Chan_Idle]))
        {
            d->stopchannelsound(Chan_Idle, 400);
        }
        switch (sound)
        {
            case S_SG_A:
            {
                playsound(sound, NULL, d == hudplayer() ? NULL : &d->o);
                if (d == hud)
                {
                    d->playchannelsound(Chan_Weapon, S_SG_B);
                }
                break;
            }
            case S_PULSE2_A:
            {
                if (validsound(d->chansound[Chan_Attack]))
                {
                    looped = true;
                }
                d->playchannelsound(Chan_Attack, sound, 100, true);
                if (!looped)
                {
                    d->playchannelsound(Chan_Weapon, S_PULSE2_B);
                }
                break;
            }
            case S_RAIL_A:
            {
                playsound(sound, NULL, d == hudplayer() ? NULL : &d->o);
                if (d == hud)
                {
                    d->playchannelsound(Chan_Weapon, S_RAIL_B);
                }
                break;
            }
            default: playsound(sound, NULL, d == hudplayer() ? NULL : &d->o);
        }
        if (previousaction > 200 && !looped)
        {
            if (d->role == ROLE_BERSERKER)
            {
                playsound(S_BERSERKER_ACTION, d);
                return;
            }
            if (d->haspowerup(PU_DAMAGE) || d->haspowerup(PU_HASTE) || d->haspowerup(PU_AMMO))
            {
                playsound(S_ACTION_DAMAGE + d->poweruptype - 1, d);
            }
        }
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

    bool isintersecting(dynent* d, const vec& from, const vec& to, float margin, float& dist)   // if lineseg hits entity bounding box
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight + margin;
        top.z += d->aboveeye + margin;
        return linecylinderintersect(from, to, bottom, top, d->radius + margin, dist);
    }

    dynent* intersectclosest(const vec& from, const vec& to, gameent* at, float margin, float& bestdist, const int flags)
    {
        dynent* best = NULL;
        bestdist = 1e16f;
        loopi(numdynents(flags))
        {
            dynent* o = iterdynents(i, flags);
            if (o == at || o->state != CS_ALIVE)
            {
                continue;
            }
            float dist;
            if (!isintersecting(o, from, to, margin, dist))
            {
                continue;
            }
            if (dist < bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
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
                if ((hits[i] = intersectclosest(from, rays[i], d, attacks[atk].margin, dist, scanFlags)))
                {
                    applyhitflags(hits[i], from, rays[i], atk, dist, hitFlags);
                    shorten(from, rays[i], dist);
                    isHit = true;
                }
                impacteffects(atk, d, from, rays[i], isHit);
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
            if ((o = intersectclosest(from, to, d, attacks[atk].margin, dist, scanFlags)))
            {
                applyhitflags(o, from, to, atk, dist, hitFlags);
                shorten(from, to, dist);
                hit(o, d, to, vec(to).sub(from).safenormalize(), attacks[atk].damage, atk, from.dist(to), 1, hitFlags);
                isHit = true;
            }
            impacteffects(atk, d, from, to, isHit);
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
                particle_flare(hit, hit, 180, PART_SPARK3, 0xFFFF66, 0.0f, NULL, 15.0f);
            }
            if (blood && color != -1)
            {
                particle_flare(hit, hit, 280, PART_BLOOD, color, 0.1f, NULL, 6.5f);
                particle_splash(PART_BLOOD2, 100, 280, hit, color, 1.4f, 150, 2, 0.001f);
                particle_splash(PART_BLOOD, damage / 10, 1000, hit, color, 2.60f);
            }
            else
            {
                particle_flare(hit, hit, 350, PART_SPARK3, 0xFFFF66, 0.1f, NULL, 16.0f);
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
            particle_flare(hit, hit, 250, PART_SPARK3, 0xFFFF66, 0.1f, NULL, 15.0f);
            playsound(S_BOUNCE_ROCKET, NULL, &hit);
        }
    }

    FVARP(goredistance, 0, 50.0f, 100.0f);

    void gibeffect(int damage, const vec& vel, gameent* d)
    {
        const vec from = d->abovehead();
        loopi(min(damage, 8) + 1)
        {
            projectiles::spawnbouncer(from, d, Projectile_Gib);
        }
        if (blood)
        {
            particle_flare(d->o, d->o, 320, PART_BLOOD, getbloodcolor(d), 0.5f, NULL, 30.0f);
            particle_splash(PART_BLOOD2, damage, 300, d->o, getbloodcolor(d), 0.89f, 300, 5, 2.0f);
            addstain(STAIN_BLOOD, d->o, d->vel.neg(), 25, getbloodcolor(d), rnd(4));
        }
        if (blood && (d == self || (goredistance && camera1->o.dist(from) <= goredistance)))
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
            hit.id = 0;
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

    void hit(dynent* target, gameent* actor, const vec& hitPosition, const vec& velocity, int damage, const int attack, const float dist, const int rays, const int flags)
    {
        if (!target)
        {
            return;
        }
        gameent* f = (gameent*)target;
        damage = calculatedamage(damage, f, actor, attack, flags);
        switch (target->type)
        {
            case ENT_PLAYER:
            {
                registerhit(f, actor, hitPosition, velocity, damage, attack, dist, rays, flags);
                break;
            }
            case ENT_AI:
            {
                hitmonster(damage, (monster*)f, actor, attack, velocity, flags);
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
        // Apply hit effects like blood, shakes, etc.
        if (target)
        {
            applyhiteffects(damage, f, actor, hitPosition, attack, flags, true);
        }
    }

    void gunselect(int gun, gameent* d)
    {
        d->lastswitchattempt = lastmillis;
        if (gun == d->gunselect || lastmillis - d->lastswitch < 100)
        {
            return;
        }
        addmsg(N_GUNSELECT, "rci", d, gun);
        d->gunselect = gun;
        d->lastattack = ATK_INVALID;
        doweaponchangeffects(d, gun);
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
        if (d == self)
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
            loopi(NUMGUNS)
            {
                gun = (gun + dir) % NUMGUNS;
                if (force || self->ammo[gun]) break;
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
        if (force || self->ammo[gun])
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
        loopi(numguns) if (guns[i] == self->gunselect)
        {
            offset = i + 1;
            break;
        }
        loopi(numguns)
        {
            int gun = guns[(i + offset) % numguns];
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
         loopi(numguns) guns[i] = getweapon(args[i].getstr());
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

    vec hudgunorigin(int gun, const vec& from, const vec& to, gameent* d)
    {
        if (camera::camera.zoomstate.isenabled() && d == self)
        {
            return d->feetpos(4);
        }
        else if (d->muzzle.x >= 0)
        {
            return d->muzzle;
        }

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

    swayinfo sway;

    void swayinfo::updatedirection(gameent* owner)
    {
        float k = pow(0.7f, curtime / 10.0f);
        dir.mul(k);
        vec vel(owner->vel);
        vel.add(owner->falling);
        dir.add(vec(vel).mul((1 - k) / (8 * max(vel.magnitude(), owner->speed))));

        yaw = owner->yaw;
        pitch = owner->pitch;
        roll = owner->roll * swayrollfactor;
    }

    void swayinfo::update(gameent* owner)
    {
        if (owner->physstate >= PHYS_SLOPE || owner->climbing)
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

        vecfromyawpitch(owner->yaw, 0, 0, 1, o);

        float steps = dist / swaystep * M_PI;

        o.mul(swayside * sinf(steps));

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

        trans.add(vec(directionforward).mul(swayside * magic4 * 2.0f));
        trans.add(vec(directionside).mul(swayside * magic5 * 2.0f));
        trans.add(vec(o).mul(-4.0f));
        trans.z += swayup * magic2 * 1.5f;
        trans.z += swayup * (fabs(sinf(steps)) - 1);
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

        rotationyaw += cameravelocity.x * -0.3f;
        rotationpitch += cameravelocity.y * -0.3f;
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
                        const float curve = sinf(progress * M_PI) * event.factor;
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
