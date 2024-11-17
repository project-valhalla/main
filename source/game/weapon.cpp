// weapon.cpp: all shooting and effects code, projectile management
#include "game.h"

namespace game
{
    static const int OFFSETMILLIS = 500;
    vec rays[GUN_MAXRAYS];

    struct hitmsg
    {
        int target, lifesequence, info1, info2, flags;
        ivec dir;
    };
    vector<hitmsg> hits;

    ICOMMAND(getweapon, "", (), intret(self->gunselect));

    void gunselect(int gun, gameent* d)
    {
        if (gun == d->gunselect || lastmillis - d->lastswitch < 100)
        {
            return;
        }
        addmsg(N_GUNSELECT, "rci", d, gun);
        d->gunselect = gun;
        d->lastattack = -1;
        doweaponchangeffects(d, gun);
    }

    void doweaponchangeffects(gameent* d, int gun)
    {
        d->lastswitch = lastmillis;
        sway.addevent(d, SwayEvent_Switch, 500, -15);
        d->stopchannelsound(Chan_Weapon, 200);
        if (!validgun(gun))
        {
            if(!validgun(d->gunselect)) return;
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
            disablezoom();
        }
    }

    void nextweapon(int dir, bool force = false)
    {
        if (self->state != CS_ALIVE)
        {
            return;
        }
        if (guns[self->gunselect].zoom && zoom)
        {
            zoomfov = clamp(zoomfov - dir, 10, 90);
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
            if (gun != self->gunselect) gunselect(gun, self);
        }
    }
    ICOMMAND(nextweapon, "ii", (int *dir, int *force), nextweapon(*dir, *force!=0));

    int getweapon(const char *name)
    {
        if(isdigit(name[0])) return parseint(name);
        else
        {
            int len = strlen(name);
            loopi(sizeof(guns)/sizeof(guns[0])) if(!strncasecmp(guns[i].name, name, len)) return i;
        }
        return -1;
    }

    void setweapon(const char *name, bool force = false)
    {
        int gun = getweapon(name);
        if(self->state!=CS_ALIVE || !validgun(gun)) return;
        if(force || self->ammo[gun]) gunselect(gun, self);
    }
    ICOMMAND(setweapon, "si", (char *name, int *force), setweapon(name, *force!=0));

    void cycleweapon(int numguns, int *guns, bool force = false)
    {
        if(numguns<=0 || self->state!=CS_ALIVE) return;
        int offset = 0;
        loopi(numguns) if(guns[i] == self->gunselect) { offset = i+1; break; }
        loopi(numguns)
        {
            int gun = guns[(i+offset)%numguns];
            if(gun>=0 && gun<NUMGUNS && (force || self->ammo[gun]))
            {
                gunselect(gun, self);
                return;
            }
        }
        playsound(S_WEAPON_NOAMMO);
    }
    ICOMMAND(cycleweapon, "V", (tagval *args, int numargs),
    {
         int numguns = min(numargs, 3);
         int guns[3];
         loopi(numguns) guns[i] = getweapon(args[i].getstr());
         cycleweapon(numguns, guns);
    });

    void weaponswitch(gameent *d)
    {
        if(d->state!=CS_ALIVE) return;
        int s = d->gunselect;
        if(s!=GUN_SCATTER && d->ammo[GUN_SCATTER])
        {
            s = GUN_SCATTER;
        }
        else if(s!=GUN_SMG && d->ammo[GUN_SMG])
        {
            s = GUN_SMG;
        }
        else if(s!=GUN_PULSE && d->ammo[GUN_PULSE])
        {
            s = GUN_PULSE;
        }
        else if(s!=GUN_ROCKET && d->ammo[GUN_ROCKET])
        {
            s = GUN_ROCKET;
        }
        else if(s!=GUN_RAIL && d->ammo[GUN_RAIL])
        {
            s = GUN_RAIL;
        }
        else if(s!=GUN_GRENADE && d->ammo[GUN_GRENADE])
        {
            s = GUN_GRENADE;
        }
        gunselect(s, d);
    }

    ICOMMAND(weapon, "V", (tagval *args, int numargs),
    {
        if(self->state!=CS_ALIVE) return;
        loopi(3)
        {
            const char *name = i < numargs ? args[i].getstr() : "";
            if(name[0])
            {
                int gun = getweapon(name);
                if(validgun(gun) && gun != self->gunselect && self->ammo[gun]) { gunselect(gun, self); return; }
            } else { weaponswitch(self); return; }
        }
        playsound(S_WEAPON_NOAMMO);
    });

    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest, gameent *d)
    {
        vec offset;
        do offset = vec(rndscale(1), rndscale(1), rndscale(1)).sub(0.5f);
        while(offset.squaredlen() > 0.5f * 0.5f);
        offset.mul((to.dist(from) / 1024) * spread / (d->crouched() && d->crouching ? 1.5f : 1));
        offset.z /= 2;
        dest = vec(offset).add(to);
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
        }
    }

    VARP(hudgun, 0, 1, 1);

    vec hudgunorigin(int gun, const vec& from, const vec& to, gameent* d)
    {
        if (zoom && d == self)
        {
            return d->feetpos(4);
        }
        else if (d->muzzle.x >= 0)
        {
            return d->muzzle;
        }

        vec offset(from);
        if (d != hudplayer() || isthirdperson())
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

    enum
    {
        BNC_GRENADE,
        BNC_GRENADE2,
        BNC_ROCKET,
        BNC_GIB,
        BNC_DEBRIS,
        BNC_EJECT,
    };

    inline bool isweaponbouncer(int type) { return type >= BNC_GRENADE && type <= BNC_ROCKET; }

    struct bouncer : physent
    {
        gameent *owner;

        vec offset, lastpos;

        bool local;

        float lastyaw, roll, gravity, elasticity, offsetheight;

        int id, atk, gun, bouncetype, lifetime;
        int variant, bounces, offsetmillis;
        int lastbounce, bouncesound, bouncerloopchan, bouncerloopsound;

        bouncer() : roll(0), variant(0), bounces(0), lastbounce(0), bouncesound(-1), bouncerloopchan(-1), bouncerloopsound(-1)
        {
            type = ENT_BOUNCE;
        }
        ~bouncer()
        {
            if(bouncerloopchan >= 0) stopsound(bouncerloopsound, bouncerloopchan);
            bouncerloopsound = bouncerloopchan = -1;
        }

        vec offsetpos()
        {
            vec pos(o);
            if(offsetmillis > 0)
            {
                pos.add(vec(offset).mul(offsetmillis/float(OFFSETMILLIS)));
                if(offsetheight >= 0) pos.z = max(pos.z, o.z - max(offsetheight - eyeheight, 0.0f));
            }
            return pos;
        }

        void limitoffset()
        {
            if(isweaponbouncer(bouncetype) && offsetmillis > 0 && offset.z < 0)
                offsetheight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
            else offsetheight = -1;
        }

        void setradius(int type)
        {
            float typeradius = 1.4f;
            if(type == BNC_EJECT) typeradius = 0.3f;
            else if (type == BNC_DEBRIS) typeradius = 0.9f;
            radius = xradius = yradius = typeradius;
            eyeheight = aboveeye = radius;
        }

        void checkliquid()
        {
            int material = lookupmaterial(o);
            bool isinwater = isliquidmaterial(material & MATF_VOLUME);
            inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;
        }
    };

    vector<bouncer *> bouncers;

    void newbouncer(gameent *owner, const vec &from, const vec &to, bool local, int id, int atk, int type, int lifetime, int speed, float gravity, float elasticity, int gun = -1)
    {
        bouncer &bnc = *bouncers.add(new bouncer);
        bnc.owner = owner;
        bnc.o = from;
        bnc.setradius(type);
        bnc.local = local;
        bnc.id = local ? lastmillis : id;
        bnc.atk = atk;
        bnc.bouncetype = type;
        bnc.lifetime = lifetime;
        bnc.speed = speed;
        bnc.gravity = gravity;
        bnc.elasticity = elasticity;
        if(validgun(gun)) bnc.gun = gun;

        switch(type)
        {
            case BNC_GRENADE2:
                bnc.collidetype = COLLIDE_ELLIPSE;
                // fall through
            case BNC_GRENADE:
                bnc.bouncesound = S_BOUNCE_GRENADE;
                break;

            case BNC_ROCKET:
            {
                bnc.collidetype = COLLIDE_ELLIPSE;
                bnc.bouncesound = S_BOUNCE_ROCKET;
                break;
            }
            case BNC_GIB: bnc.variant = rnd(5); break;
            case BNC_EJECT:
            {
                int gun = bnc.owner->gunselect;
                bnc.bouncesound = gun == GUN_SCATTER? S_BOUNCE_CARTRIDGE_SG: (gun == GUN_SMG? S_BOUNCE_CARTRIDGE_SMG: S_BOUNCE_CARTRIDGE_RAILGUN);
                break;
            }
        }

        vec dir(to);
        dir.sub(from).safenormalize();
        bnc.vel = dir;
        bnc.vel.mul(speed);

        avoidcollision(&bnc, dir, owner, 0.1f);

        if(isweaponbouncer(bnc.bouncetype))
        {
            bnc.offset = hudgunorigin(attacks[bnc.atk].gun, from, to, owner);
            if(owner==hudplayer() && !isthirdperson()) bnc.offset.sub(owner->o).rescale(16).add(owner->o);
        }
        else bnc.offset = from;
        bnc.offset.sub(bnc.o);
        bnc.offsetmillis = OFFSETMILLIS;

        bnc.resetinterp();

        bnc.lastpos = owner->o;
        bnc.checkliquid();
    }

    VARP(blood, 0, 1, 1);

    void bounced(physent *d, const vec &surface)
    {
        if(d->type != ENT_BOUNCE) return;
        bouncer *b = (bouncer *)d;
        b->bounces++;
        int type = b->bouncetype;
        if((type == BNC_EJECT && b->bounces > 1) // prevent bounce sound spam
           || lastmillis - b->lastbounce < 100)
        {
            return;
        }
        if(!(lookupmaterial(b->o) & MAT_WATER))
        {
            if(b->bouncesound >= 0 && b->vel.magnitude() > 5.0f)
            {
                playsound(b->bouncesound, NULL, &b->o, NULL, 0, 0, 0, -1);
            }
            if(blood && type == BNC_GIB && b->bounces <= 2)
            {
                addstain(STAIN_BLOOD, vec(b->o).sub(vec(surface).mul(b->radius)), surface, 2.96f/b->bounces, bvec::hexcolor(getbloodcolor(b->owner)), rnd(4));
            }
            if(type == BNC_ROCKET) particle_splash(PART_SPARK2, 20, 150, b->o, 0xFFC864, 0.3f, 250, 1);
        }
        b->lastbounce = lastmillis;
    }

    void projstain(vec dir, const vec &pos, int atk)
    {
        vec negdir = vec(dir).neg();
        float radius = attacks[atk].exprad;
        addstain(STAIN_SCORCH, pos, negdir, radius * 0.75f);
        if(lookupmaterial(pos) & MAT_WATER) return; // no glow in water
        int gun = attacks[atk].gun;
        if(gun != GUN_ROCKET)
        {
            int color = 0x00FFFF;
            if(gun == GUN_PULSE) color = 0xEE88EE;
            else if(gun == GUN_GRENADE) color = 0x74BCF9;
            addstain(STAIN_GLOW2, pos, negdir, radius * 0.5f, color);
        }
    }

    void updatebouncers(int time)
    {
       loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            bool isinwater = ((lookupmaterial(bnc.o) & MATF_VOLUME) == MAT_WATER);
            int transition = physics::liquidtransition(&bnc, lookupmaterial(bnc.o), isinwater);
            if (transition > 0)
            {
                if (bnc.radius < 1)
                {
                    particle_splash(PART_SPLASH, 3, 150, bnc.o, 0xFFFFFF, 1.5f, 100, 2);
                }
                else
                {
                    particle_splash(PART_WATER, 200, 250, bnc.o, 0xFFFFFF, 0.09f, 500, 1);
                    particle_splash(PART_SPLASH, 10, 80, bnc.o, 0xFFFFFF, 7.0f, 250, -1);
                }
                if (transition == LiquidTransition_In)
                {
                    playsound(bnc.radius < 1.0f ? S_IMPACT_WATER : S_IMPACT_WATER_PROJ, NULL, &bnc.o);
                }
            }
            switch(bnc.bouncetype)
            {
                case BNC_ROCKET:
                {
                    if(bnc.vel.magnitude() > 20.0f) regular_particle_splash(PART_SMOKE, 5, 200, pos, 0x555555, 1.60f, 10, 500);
                    if(bnc.lifetime < attacks[bnc.atk].lifetime - 100)
                    {
                         particle_flare(bnc.lastpos, pos, 500, PART_TRAIL_STRAIGHT, 0xFFC864, 0.4f);
                    }
                    bnc.lastpos = pos;
                    break;
                }

                case BNC_GRENADE2:
                case BNC_GRENADE:
                {
                    if(bnc.vel.magnitude() > 10.0f) regular_particle_splash(PART_RING, 1, 200, pos, 0x74BCF9, 1.0f, 1, 500);
                    if(bnc.bouncetype == BNC_GRENADE2)
                    {
                        if(bnc.lifetime < attacks[bnc.atk].lifetime - 100) particle_flare(bnc.lastpos, pos, 500, PART_TRAIL_STRAIGHT, 0x74BCF9, 0.4f);
                    }
                    bnc.lastpos = pos;
                    break;
                }

                case BNC_GIB:
                {
                    if (blood && bnc.vel.magnitude() > 30.0f)
                    {
                        regular_particle_splash(PART_BLOOD, 0 + rnd(4), 400, pos, getbloodcolor(bnc.owner), 0.80f, 25);
                    }
                    break;
                }

                case BNC_DEBRIS:
                {
                    if (bnc.vel.magnitude() <= 30.0f) break;
                    regular_particle_splash(PART_SMOKE, 5, 100, pos, 0x222222, 1.80f, 30, 500);
                    regular_particle_splash(PART_SPARK, 1, 40, pos, 0x903020, 1.20f, 10, 500);
                    particle_flare(bnc.o, bnc.o, 1, PART_EDIT, 0xF69D19, 0.5 + rndscale(2.0f));
                }
            }
            if(bnc.bouncerloopsound >= 0) bnc.bouncerloopchan = playsound(bnc.bouncerloopsound, NULL, &pos, NULL, 0, -1, 100, bnc.bouncerloopchan);
            vec old(bnc.o);
            bool destroyed = false;
            if(bnc.bouncetype >= BNC_GIB && bnc.bouncetype <= BNC_EJECT)
            {
                // cheaper variable rate physics for debris, gibs, etc.
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((bnc.lifetime -= qtime)<0 || physics::hasbounced(&bnc, qtime/1000.0f, 0.5f, 0.4f, 0.7f))
                    {
                        destroyed = true;
                        break;
                    }
                }
            }
            else if(isweaponbouncer(bnc.bouncetype))
            {
                destroyed = !physics::isbouncing(&bnc, bnc.elasticity, 0.5f, bnc.gravity)
                            || (bnc.lifetime -= time) < 0
                            || isdeadlymaterial(lookupmaterial(bnc.o) & MAT_LAVA)
                            || ((bnc.bouncetype == BNC_GRENADE2 && bnc.bounces >= 1)
                            || (bnc.bouncetype == BNC_ROCKET && bnc.bounces >= 2));
            }
            if(destroyed)
            {
                if(isweaponbouncer(bnc.bouncetype))
                {
                    int damage = attacks[bnc.atk].damage;
                    hits.setsize(0);
                    explode(bnc.local, bnc.owner, bnc.o, bnc.vel, NULL, damage, bnc.atk);
                    projstain(bnc.vel, bnc.offsetpos(), bnc.atk);
                    if(bnc.local)
                        addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, bnc.atk, bnc.id-maptime,
                                                    hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                }
                stopsound(bnc.bouncerloopsound, bnc.bouncerloopchan);
                delete bouncers.remove(i--);
            }
            else
            {
                bnc.roll += old.sub(bnc.o).magnitude()/(4*RAD);
                bnc.offsetmillis = max(bnc.offsetmillis-time, 0);
                bnc.limitoffset();
            }
        }
    }

    enum
    {
       PROJ_PULSE,
       PROJ_PLASMA,
       PROJ_ROCKET
    };

    struct projectile : physent
    {
        gameent *owner;

        vec dir, o, from, to, offset;

        bool local;

        float speed;

        int id, atk, projtype, lifetime, offsetmillis;

        int projchan, projsound;

        projectile() : projchan(-1), projsound(-1)
        {
        }
        ~projectile()
        {
            if(projchan >= 0) stopsound(projsound, projchan);
            projsound = projchan = -1;
        }

        void checkliquid()
        {
            int material = lookupmaterial(o);
            bool isinwater = isliquidmaterial(material & MATF_VOLUME);
            inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;
        }
    };
    vector<projectile> projs;

    void newprojectile(gameent *owner, const vec &from, const vec &to, bool local, int id, int atk, int type)
    {
        projectile &p = projs.add();
        p.owner = owner;
        p.dir = vec(to).sub(from).safenormalize();
        p.o = from;
        p.from = from;
        p.to = to;
        p.offset = hudgunorigin(attacks[atk].gun, from, to, owner);
        p.offset.sub(from);
        p.local = local;
        p.id = local ? lastmillis : id;
        p.atk = atk;
        p.projtype = type;

        p.speed = attacks[atk].projspeed;
        p.lifetime = attacks[atk].lifetime;
        p.offsetmillis = OFFSETMILLIS;
        p.checkliquid();
    }

    void damageeffect(int damage, dynent *d, vec p, int atk, int color, bool headshot)
    {
        gameent *f = (gameent *)d, *hud = followingplayer(self);
        if(f == hud)
        {
            p.z += 0.6f*(d->eyeheight + d->aboveeye) - d->eyeheight;
        }
        if(f->haspowerup(PU_INVULNERABILITY) || f->shield)
        {
            particle_splash(PART_SPARK2, 100, 150, p, f->haspowerup(PU_INVULNERABILITY) ? getplayercolor(f, f->team) : 0xFFFF66, 0.50f);
            if(f->haspowerup(PU_INVULNERABILITY))
            {
                playsound(S_ACTION_INVULNERABILITY, f);
                return;
            }
        }
        if(blood && color != -1)
        {
            particle_splash(PART_BLOOD, damage/10, 1000, p, color, 2.60f);
            particle_splash(PART_BLOOD2, 200, 250, p, color, 0.50f);
        }
        else
        {
            particle_flare(p, p, 100, PART_MUZZLE_FLASH3, 0xFFFF66, 3.5f);
            particle_splash(PART_SPARK2, damage/5, 500, p, 0xFFFF66, 0.5f, 300);
        }
        if(f->health > 0 && lastmillis-f->lastyelp > 600)
        {
            if(f != hud && f->shield) playsound(S_SHIELD_HIT, f);
            if(f->type == ENT_PLAYER)
            {
                playsound(getplayermodelinfo(f).painsound, f);
                f->lastyelp = lastmillis;
            }
        }
        if(validatk(atk))
        {
            if(headshot)
            {
                playsound(S_HIT_WEAPON_HEAD, NULL, &f->o);
            }
            else if(attacks[atk].hitsound)
            {
                playsound(attacks[atk].hitsound, NULL, &f->o);
            }
        }
        else playsound(S_PLAYER_DAMAGE, NULL, &f->o);
        if(f->haspowerup(PU_ARMOR)) playsound(S_ACTION_ARMOR, NULL, &f->o);
    }

    void spawnbouncer(const vec &from, gameent *d, int type, int gun = -1)
    {
        vec to(rnd(100)-50, rnd(100)-50, rnd(100)-50);
        float elasticity = 0.6f;
        if(type == BNC_EJECT)
        {
            to = vec(-50, 1, rnd(30)-15);
            to.rotate_around_z(d->yaw*RAD);
            elasticity = 0.4f;
        }
        if(to.iszero()) to.z += 1;
        to.normalize();
        to.add(from);
        newbouncer(d, from, to, true, 0, -1, type, type == BNC_DEBRIS ? 400 : rnd(1000)+1000, rnd(100)+20, 0.3f + rndscale(0.8f), elasticity, gun);
    }

    void gibeffect(int damage, const vec &vel, gameent *d)
    {
        if(!gore) return;
        vec from = d->abovehead();
        loopi(min(damage, 8) + 1)
        {
            spawnbouncer(from, d, BNC_GIB);
        }
        if(blood)
        {
            particle_splash(PART_BLOOD, 3, 180, d->o, getbloodcolor(d), 3.0f+rndscale(5.0f), 150, 0);
            particle_splash(PART_BLOOD2, damage, 300, d->o, getbloodcolor(d), 0.89f, 300, 5);
            addstain(STAIN_BLOOD, d->o, d->vel.neg(), 25, getbloodcolor(d), rnd(4));
        }
        playsound(S_GIB, d);
    }

    VARP(monsterdeadpush, 1, 5, 20);

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2 = 1, int flags = HIT_TORSO)
    {
        gameent *f = (gameent *)d;
        if(f->type == ENT_PLAYER && !isinvulnerable(f, at)) f->lastpain = lastmillis;
        if(at->type==ENT_PLAYER && f!=at && !isally(f, at))
        {
            at->totaldamage += damage;
        }
        if(at == self && d != at)
        {
            extern int hitsound;
            if(hitsound && at->lasthit != lastmillis) {
                playsound(isally(f, at) ? S_HIT_ALLY : S_HIT);
            }
            at->lasthit = lastmillis;
        }
        if(f->type != ENT_AI && (!m_mp(gamemode) || f==at))
        {
            f->hitpush(damage, vel, at, atk);
        }
        if(f->type == ENT_AI)
        {
            hitmonster(damage, (monster *)f, at, atk, flags);
            f->hitpush(damage * (f->health <= 0 ? monsterdeadpush : 1), vel, at, atk);
        }
        else if(!m_mp(gamemode))
        {
            damaged(damage, f->o, f, at, atk, flags);
        }
        else
        {
            hitmsg &h = hits.add();
            h.target = f->clientnum;
            h.lifesequence = f->lifesequence;
            h.info1 = int(info1*DMF);
            h.info2 = info2;
            h.flags = flags;
            h.dir = f==at ? ivec(0, 0, 0) : ivec(vec(vel).mul(DNF));

            if(at == self && f == at) setdamagehud(damage, f, at);
        }
    }

    int calcdamage(int damage, gameent *target, gameent *actor, int atk, int flags)
    {
        if(target != actor && isinvulnerable(target, actor))
        {
            return 0;
        }
        if (!(flags & HIT_MATERIAL))
        {
            if (attacks[atk].headshotdam && !attacks[atk].projspeed) // weapons deal locational damage only if headshot damage is specified (except for projectiles)
            {
                if (flags & HIT_HEAD)
                {
                    if(m_mayhem(mutators)) return (damage = target->health); // force death if it's a blow to the head when the Mayhem mutator is enabled
                    else damage += attacks[atk].headshotdam;
                }
                if (flags & HIT_LEGS) damage /= 2;
            }
            if (actor->haspowerup(PU_DAMAGE) || actor->role == ROLE_BERSERKER) damage *= 2;
            if (isally(target, actor) || target == actor) damage /= DAM_ALLYDIV;
        }
        if (target->haspowerup(PU_ARMOR) || target->role == ROLE_BERSERKER) damage /= 2;
        if(!damage) damage = 1;
        return damage;
    }

    void calcpush(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays, int flags)
    {
        if(betweenrounds) return;
        hit(damage, d, at, vec(to).sub(from).safenormalize(), atk, from.dist(to), rays, flags);
    }

    float projdist(dynent *o, vec &dir, const vec &v, const vec &vel)
    {
        vec middle = o->o;
        middle.z += (o->aboveeye-o->eyeheight)/2;
        dir = vec(middle).sub(v).add(vec(vel).mul(5)).safenormalize();

        float low = min(o->o.z - o->eyeheight + o->radius, middle.z),
              high = max(o->o.z + o->aboveeye - o->radius, middle.z);
        vec closest(o->o.x, o->o.y, clamp(v.z, low, high));
        return max(closest.dist(v) - o->radius, 0.0f);
    }


    void radialeffect(dynent *o, const vec &v, const vec &vel, int damage, gameent *at, int atk)
    {
        if(o->state!=CS_ALIVE) return;
        vec dir;
        float dist = projdist(o, dir, v, vel);
        if(dist<attacks[atk].exprad)
        {
            float radiusdamage = damage*(1-dist/EXP_DISTSCALE/attacks[atk].exprad), damage = calcdamage(radiusdamage, (gameent *)o, at, atk);
            hit(damage, o, at, dir, atk, dist);
            damageeffect(damage, o, o->o, atk, getbloodcolor(o));
        }
    }

    VARP(maxdebris, 10, 60, 1000);

    void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int damage, int atk)
    {
        if(!attacks[atk].projspeed) return;
        int explosioncolor = 0x50CFE5, explosiontype = PART_EXPLOSION1;
        int fade = 400;
        float min = 0.10f, max = attacks[atk].exprad * 1.15f;
        bool water = (lookupmaterial(v) & MATF_VOLUME) == MAT_WATER;
        if(!water) switch(atk)
        {
            case ATK_ROCKET1:
            case ATK_ROCKET2:
            {
                explosioncolor = 0xd47f00;
                explosiontype = PART_EXPLOSION3;
                particle_flare(v, v, 280, PART_EXPLODE1 + rnd(3), 0xFFC864, 56.0f);
                particle_splash(PART_SPARK2, 100, 250, v, explosioncolor, 0.10f+rndscale(0.50f), 600, 1);
                particle_splash(PART_SMOKE, 100, 280, v, 0x222222, 10.0f, 250, 200);
                break;
            }
            case ATK_PULSE1:
            {
                explosioncolor = 0xEE88EE;
                explosiontype = PART_EXPLOSION2;
                particle_splash(PART_SPARK2, 5+rnd(20), 200, v, explosioncolor, 0.08f+rndscale(0.35f), 400, 2);
                particle_flare(v, v, 250, PART_EXPLODE2, 0xf1b4f1, 15.0f);
                particle_splash(PART_SMOKE, 60, 180, v, 0x222222, 2.5f+rndscale(3.8f), 180, 60);
                break;
            }
            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                explosioncolor = 0x74BCF9;
                explosiontype = PART_EXPLOSION2;
                fade = 200;
                particle_flare(v, v, 280, PART_ELECTRICITY, 0x49A7F7, 30.0f);
                break;
            }
            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                explosioncolor = 0x00FFFF;
                min = attacks[atk].exprad * 1.15f;
                max = 0;
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
        particle_fireball(v, max, explosiontype, fade, explosioncolor, min);
        adddynlight(v, attacks[atk].exprad * 3, vec::hexcolor(explosioncolor), fade, 40);
        playsound(attacks[atk].impactsound, NULL, &v);
        if (water)
        {
            particle_splash(PART_WATER, 200, 250, v, 0xFFFFFF, 0.09f, 500, 1);
            particle_splash(PART_SPLASH, 100, 80, v, 0xFFFFFF, 7.0f, attacks[atk].exprad * 10, -1);
        }
        else // no debris in water
        {
            int numdebris = rnd(maxdebris - 5) + 5;
            vec debrisvel = vec(owner->o).sub(v).safenormalize();
            vec debrisorigin(v);
            if (atk == ATK_ROCKET1)
            {
                debrisorigin.add(vec(debrisvel).mul(8));
            }
            if (numdebris && (attacks[atk].gun == GUN_ROCKET || attacks[atk].gun == GUN_SCATTER))
            {
                loopi(numdebris)
                {
                    spawnbouncer(debrisorigin, owner, BNC_DEBRIS);
                }
            }
        }
        if(betweenrounds || !local) return;
        int numdyn = numdynents();
        loopi(numdyn)
        {
            dynent *o = iterdynents(i);
            if(o->o.reject(v, o->radius + attacks[atk].exprad) || o==safe) continue;
            radialeffect(o, v, vel, damage, owner, atk);
        }
    }

    void projsplash(projectile &p, const vec &v, dynent *safe, int damage)
    {
        explode(p.local, p.owner, v, p.dir, safe, damage, p.atk);
        projstain(p.dir, v, p.atk);
    }

    void explodeeffects(int atk, gameent *d, bool local, int id)
    {
        if(local) return;
        switch(atk)
        {
            case ATK_ROCKET2:
            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                loopv(bouncers)
                {
                    bouncer &bnc = *bouncers[i];
                    if(!isweaponbouncer(bnc.bouncetype)) break;
                    if(bnc.owner == d && bnc.id == id && !bnc.local)
                    {
                        explode(bnc.local, bnc.owner, bnc.offsetpos(), bnc.vel, NULL, 0, atk);
                        projstain(bnc.vel, bnc.offsetpos(), bnc.atk);
                        delete bouncers.remove(i);
                        break;
                    }
                }
                break;
            }
            case ATK_PULSE1:
            case ATK_ROCKET1:
            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                loopv(projs)
                {
                    projectile &p = projs[i];
                    if(p.owner == d && p.id == id && !p.local)
                    {
                        if(atk == ATK_PISTOL_COMBO) p.atk = atk;
                        else if(p.atk != atk) continue;
                        vec pos = vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)).add(p.o);
                        explode(p.local, p.owner, pos, p.dir, NULL, 0, atk);
                        projstain(p.dir, pos, p.atk);
                        stopsound(p.projsound, p.projchan);
                        projs.remove(i);
                        break;
                    }
                }
                break;
            }
            default: break;
        }
    }

    bool projdamage(dynent *o, projectile &p, const vec &v, int damage)
    {
        if(betweenrounds || o->state!=CS_ALIVE) return false;
        if(!intersect(o, p.o, v, attacks[p.atk].margin)) return false;
        projsplash(p, v, o, damage);
        vec dir;
        projdist(o, dir, v, p.dir);
        gameent *f = (gameent *)o;
        int cdamage = calcdamage(damage, f, p.owner, p.atk);
        hit(cdamage, o, p.owner, dir, p.atk, 0);
        damageeffect(cdamage, o, o->o, p.atk, getbloodcolor(o));
        return true;
    }

    void updateprojectiles(int time)
    {
        if(projs.empty()) return;
        loopv(projs)
        {
            projectile &p = projs[i];
            p.offsetmillis = max(p.offsetmillis-time, 0);
            vec dv;
            float dist = p.to.dist(p.o, dv);
            dv.mul(time/max(dist*1000/p.speed, float(time)));
            vec v = vec(p.o).add(dv);
            float damage = attacks[p.atk].damage;
            bool exploded = false;
            hits.setsize(0);
            if(p.local)
            {
                vec halfdv = vec(dv).mul(0.5f), bo = vec(p.o).add(halfdv);
                float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[p.atk].margin;
                if(!betweenrounds) loopj(numdynents())
                {
                    dynent *o = iterdynents(j);
                    if(p.owner==o || o->o.reject(bo, o->radius + br)) continue;
                    if(projdamage(o, p, v, damage))
                    {
                        exploded = true;
                        break;
                    }
                }
            }
            if(!exploded)
            {
                bool isinwater = ((lookupmaterial(p.o) & MATF_VOLUME) == MAT_WATER);
                int transition = physics::liquidtransition(&p, lookupmaterial(p.o), isinwater);
                if (transition > 0)
                {
                    particle_splash(PART_WATER, 200, 250, p.o, 0xFFFFFF, 0.09f, 500, 1);
                    particle_splash(PART_SPLASH, 10, 80, p.o, 0xFFFFFF, 7.0f, 250, -1);
                    playsound(S_IMPACT_WATER_PROJ, NULL, &p.o);
                }
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((p.lifetime -= qtime)<0)
                    {
                        exploded = true;
                        projsplash(p, v, NULL, damage);
                    }
                }
                if(lookupmaterial(p.o) & MAT_LAVA)
                {
                    exploded = true;
                    projsplash(p, v, NULL, damage);
                }
                else if(dist<4)
                {
                    if(p.o!=p.to) // if original target was moving, re-evaluate endpoint
                    {
                        if(raycubepos(p.o, p.dir, p.to, 0, RAY_CLIPMAT|RAY_ALPHAPOLY)>=4) continue;
                    }
                    exploded = true;
                    projsplash(p, v, NULL, damage);
                }
                else
                {
                    vec pos = vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)).add(v);
                    int tailc = 0xFFFFFF, tails = 2.0f;
                    switch(p.projtype)
                    {
                        case PROJ_ROCKET:
                        {
                            tailc = 0xFFC864; tails = 1.5f;
                            regular_particle_splash(PART_SMOKE, 3, 300, pos, 0x303030, 2.4f, 50, -20);
                            if (p.lifetime <= attacks[p.atk].lifetime / 2)
                            {
                                regular_particle_splash(PART_EXPLODE2, 4, 180, pos, tailc, tails, 10, 0);
                            }
                            particle_flare(pos, pos, 1, PART_MUZZLE_FLASH3, tailc, 1.0f + rndscale(tails * 2));
                            p.projsound = S_ROCKET_LOOP;
                            break;
                        }

                        case PROJ_PULSE:
                        {
                            tailc = 0xDD88DD;
                            particle_flare(pos, pos, 1, PART_ORB, tailc, 1.0f + rndscale(tails));
                            p.projsound = S_PULSE_LOOP;
                            break;
                        }

                        case PROJ_PLASMA:
                        {
                            tails = 6.0f; tailc = 0x00FFFF;
                            particle_flare(pos, pos, 1, PART_ORB, tailc, p.owner == self || p.owner->type == ENT_AI ? tails : tails-2.0f);
                            p.projsound = S_PISTOL_LOOP;
                            break;
                        }
                    }
                    float len = min(80.0f, vec(p.offset).add(p.from).dist(pos));
                    vec dir = vec(dv).normalize(), tail = vec(dir).mul(-len).add(pos), head = vec(dir).mul(2.4f).add(pos);
                    particle_flare(tail, head, 1, PART_TRAIL_PROJECTILE, tailc, tails);
                    p.projchan = playsound(p.projsound, NULL, &pos, NULL, 0, -1, 100, p.projchan);
                }
            }
            if(exploded)
            {
                if(p.local)
                {
                    addmsg(N_EXPLODE, "rci3iv", p.owner, lastmillis-maptime, p.atk, p.id-maptime,
                           hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                }
                stopsound(p.projsound, p.projchan);
                projs.remove(i--);
            }
            else p.o = v;
        }
    }

    void impacteffects(int atk, gameent *d, const vec &from, const vec &to, bool hit = false)
    {
        if(!validatk(atk) || (!hit && isemptycube(to)) || from.dist(to) > attacks[atk].range) return;
        vec dir = vec(from).sub(to).safenormalize();
        int material = lookupmaterial(to);
        bool iswater = (material & MATF_VOLUME) == MAT_WATER, isglass = (material & MATF_VOLUME) == MAT_GLASS;
        switch(atk)
        {
            case ATK_SCATTER1:
            case ATK_SCATTER2:
            {
                adddynlight(vec(to).madd(dir, 4), 6, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if(hit || iswater || isglass) break;
                particle_splash(PART_SPARK2, 10, 80+rnd(380), to, 0xFFC864, 0.1f, 250);
                particle_splash(PART_SMOKE, 10, 150, to, 0x606060, 1.8f + rndscale(2.2f), 100, 100);
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.50f + rndscale(1.0f), 0xFFFFFF, rnd(4));
                break;
            }

            case ATK_SMG1:
            case ATK_SMG2:
            {
                adddynlight(vec(to).madd(dir, 4), 15, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if(hit || iswater || isglass) break;
                particle_fireball(to, 0.5f, PART_EXPLOSION2, 120, 0xFFC864, 2.0f);
                particle_splash(PART_EXPLODE4, 50, 40, to, 0xFFC864, 1.0f);
                particle_splash(PART_SPARK2, 30, 150, to, 0xFFC864, 0.05f + rndscale(0.09f), 250);
                particle_splash(PART_SMOKE, 30, 180, to, 0x444444, 2.20f, 80, 100);
                addstain(STAIN_BULLETHOLE_SMALL, to, vec(from).sub(to).normalize(), 0.50f + rndscale(1.0f), 0xFFFFFF, rnd(4));
                break;
            }

            case ATK_PULSE2:
            {
                adddynlight(vec(to).madd(dir, 4), 80, vec(1.0f, 0.50f, 1.0f), 20);
                if(hit)
                {
                    particle_flare(to, to, 120, PART_ELECTRICITY, 0xEE88EE, 8.0f);
                    break;
                }
                if(iswater) break;
                particle_splash(PART_SPARK2, 10, 300, to, 0xEE88EE, 0.01f + rndscale(0.10f), 350, 2);
                particle_splash(PART_SMOKE, 20, 150, to, 0x777777, 2.0f, 100, 50);
                addstain(STAIN_SCORCH, to, vec(from).sub(to).normalize(), 1.0f + rndscale(1.10f));
                playsound(attacks[atk].impactsound, NULL, &to);
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            case ATK_INSTA:
            {
                bool insta = attacks[atk].gun == GUN_INSTA;
                adddynlight(vec(to).madd(dir, 4), 60, !insta ? vec(0.25f, 1.0f, 0.75f) :  vec(0.25f, 0.75f, 1.0f), 180, 75, DL_EXPAND);
                if(hit)
                {
                    if(insta) particle_flare(to, to, 200, PART_ELECTRICITY, 0x50CFE5, 6.0f);
                    break;
                }
                if(iswater || isglass) break;
                particle_splash(PART_EXPLODE4, 80, 80, to, !insta ? 0x77DD77 : 0x50CFE5, 1.25f, 100, 80);
                particle_splash(PART_SPARK2, 5 + rnd(20), 200 + rnd(380), to, !insta ? 0x77DD77 : 0x50CFE5, 0.1f + rndscale(0.3f), 200, 3);
                particle_splash(PART_SMOKE, 20, 180, to, 0x808080, 2.0f, 60, 80);
                addstain(STAIN_BULLETHOLE_BIG, to, dir, 2.5f);
                addstain(STAIN_GLOW1, to, dir, 2.0f, !insta ? 0x77DD77 : 0x50CFE5);
                break;
            }

            case ATK_PISTOL1:
            {
                adddynlight(vec(to).madd(dir, 4), 30, vec(0.25, 1.0f, 1.0f), 200, 10, DL_SHRINK);
                if(hit || iswater || isglass) break;
                particle_fireball(to, 2.2f, PART_EXPLOSION1, 140, 0x00FFFF, 0.1f);
                particle_splash(PART_SPARK2, 50, 180, to, 0x00FFFF, 0.08f+rndscale(0.18f));
                addstain(STAIN_SCORCH, to, vec(from).sub(to).normalize(), 0.80f+rndscale(1.0f));
                addstain(STAIN_GLOW1, to, dir, 1.50f, 0x00FFFF);
                break;
            }

            default: break;
        }

        if (hit || atk == ATK_PULSE2) return;

        int impactsound = attacks[atk].impactsound;

        bool ismelee = attacks[atk].action == ACT_MELEE;
        if(iswater)
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
        else if(isglass)
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

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun, sound = attacks[atk].sound, previousaction = lastmillis - prevaction;
        float dist = from.dist(to);
        gameent* hud = followingplayer(self);
        bool shouldeject = d->eject.x >= 0 && d == hud;
        vec up = to;
        switch(atk)
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
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    if (d == hud)
                    {
                        particle_flare(d->muzzle, d->muzzle, 450, PART_MUZZLE_SMOKE, 0x202020, 3.0f, d);
                        particle_flare(d->muzzle, d->muzzle, 120, PART_SPARKS, 0xEFE598, 2.50f + rndscale(3.50f), d);
                    }
                    particle_flare(d->muzzle, d->muzzle, 60, PART_MUZZLE_FLASH, 0xEFE598, 2.4f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 100, vec(0.5f, 0.375f, 0.25f), 80, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(shouldeject) spawnbouncer(d->eject, d, BNC_EJECT, gun);
                if(!local)
                {
                    loopi(attacks[atk].rays)
                    {
                        offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i], d);
                        impacteffects(atk, d, from, rays[i], hit);
                    }
                }
                loopi(attacks[atk].rays)
                {
                    particle_flare(hudgunorigin(gun, from, rays[i], d), rays[i], 80, PART_TRAIL, 0xFFC864, 0.95f);
                }
                break;
            }

            case ATK_SMG1:
            case ATK_SMG2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    if (d == hud)
                    {
                        particle_flare(d->muzzle, d->muzzle, 300, PART_MUZZLE_SMOKE, 0xFFFFFF, 2.0f, d);
                        particle_flare(d->muzzle, d->muzzle, 160, PART_SPARKS, 0xEFE898, 2.0f, d);
                    }
                    particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH3, 0xEFE898, 1.8f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 80, vec(0.5f, 0.375f, 0.25f), 80, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(shouldeject) spawnbouncer(d->eject, d, BNC_EJECT, gun);
                if(atk == ATK_SMG2) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0xFFC864, 0.95f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_PULSE1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 100, PART_MUZZLE_FLASH2, 0xDD88DD, 2.5f, d);
                }
                newprojectile(d, from, to, local, id, atk, PROJ_PULSE);
                break;
            }
            case ATK_PULSE2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    if (previousaction > 200)
                    {
                        particle_flare(d->muzzle, d->muzzle, 250, PART_MUZZLE_FLASH5, 0xDD88DD, 2.5f, d);
                    }
                    else particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH2, 0xDD88DD, 2.0f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 75, vec(1.0f, 0.50f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_LIGHTNING, 0xEE88EE, 1.0f, d);
                particle_fireball(to, 1.0f, PART_EXPLOSION2, 100, 0xDD88DD, 3.0f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_ROCKET1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 60, PART_MUZZLE_FLASH4, 0xEFE898, 3.0f, d);
                }
                newprojectile(d, from, to, local, id, atk, PROJ_ROCKET);
                break;
            }
            case ATK_ROCKET2:
            {
                // up.z += dist/8;
                newbouncer(d, from, to, local, id, atk, BNC_ROCKET, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    if (d == hud)
                    {
                        particle_flare(d->muzzle, d->muzzle, 120, PART_SPARKS, 0x77DD77, 1.50f + rndscale(3.0f), d);
                        particle_flare(d->muzzle, d->muzzle, 450, PART_MUZZLE_SMOKE, 0x202020, 3.0f, d);
                    }
                    particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH, 0x77DD77, 1.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 100, vec(0.25f, 1.0f, 0.75f), 80, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                }
                if(shouldeject) spawnbouncer(d->eject, d, BNC_EJECT, gun);
                if(atk == ATK_RAIL2) particle_trail(PART_SMOKE, 350, hudgunorigin(gun, from, to, d), to, 0xDEFFDE, 0.3f, 50);
                particle_flare(hudgunorigin(gun, from, to, d), to, 600, PART_TRAIL, 0x55DD55, 0.50f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 100, PART_MUZZLE_FLASH5, 0x74BCF9, 2.8f, d);
                }
                up.z += dist/(atk == ATK_GRENADE1 ? 8 : 16);
                newbouncer(d, from, up, local, id, atk, atk == ATK_GRENADE1 ? BNC_GRENADE : BNC_GRENADE2, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
                break;
            }

            case ATK_PISTOL1:
            case ATK_PISTOL2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    if (atk == ATK_PISTOL1)
                    {
                        if (d == hud)
                        {
                            particle_flare(d->muzzle, d->muzzle, 120, PART_SPARKS, 0x00FFFF, 3.0f, d);
                        }
                        particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH3, 0x00FFFF, 2.50f, d);
                    }
                    else particle_flare(d->muzzle, d->muzzle, 200, PART_MUZZLE_FLASH2, 0x00FFFF, 1.20f, d);
                    adddynlight(hudgunorigin(attacks[atk].gun, d->o, to, d), 80, vec(0.25f, 1.0f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(atk == ATK_PISTOL2)
                {
                    newprojectile(d, from, to, local, id, atk, PROJ_PLASMA);
                    break;
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0x00FFFF, 2.0f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_INSTA:

                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 60, PART_MUZZLE_FLASH, 0x50CFE5, 1.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 80, vec(0.25f, 0.75f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(gun, from, to, d), to, 100, PART_LIGHTNING, 0x50CFE5, 1.0f);
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, PART_TRAIL, 0x50CFE5, 1.0f);
                break;

            default: break;
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
        switch(sound)
        {
            case S_SG_A:
            {
                playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
                if(d == hud)
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
                if (previousaction > 200 && !looped)
                {
                    d->playchannelsound(Chan_Weapon, S_PULSE2_B);
                }
                break;
            }
            case S_RAIL_A:
            case S_RAIL_INSTAGIB:
            {
                playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
                if(d == hud)
                {
                    d->playchannelsound(Chan_Weapon, S_RAIL_B);
                }
                break;
            }
            default: playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
        }
        if(previousaction > 200 && !looped)
        {
            if(d->role == ROLE_BERSERKER)
            {
                playsound(S_BERSERKER_ACTION, d);
                return;
            }
            if(d->haspowerup(PU_DAMAGE) || d->haspowerup(PU_HASTE) || d->haspowerup(PU_AMMO))
            {
                playsound(S_ACTION_DAMAGE+d->poweruptype - 1, d);
            }
        }
    }

    void particletrack(physent *owner, vec &o, vec &d)
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

    void dynlighttrack(physent *owner, vec &o, vec &hud)
    {
        if(owner->type!=ENT_PLAYER && owner->type != ENT_AI) return;
        gameent *pl = (gameent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattack < 0 || attacks[pl->lastattack].gun != pl->gunselect) return;
        o = pl->muzzle;
        hud = owner == followingplayer(self) ? vec(pl->o).add(vec(0, 0, 2)) : pl->muzzle;
    }

    float intersectdist = 1e16f;

    bool isheadhitbox(dynent *d, const vec &from, const vec &to, float dist)
    {
        vec bottom(d->head), top(d->head);
        top.z += d->headradius;
        return linecylinderintersect(from, to, bottom, top, d->headradius, dist);
    }

    bool islegshitbox(dynent *d, const vec &from, const vec &to, float dist)
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight;
        top.z -= d->eyeheight/2.5f;
        return linecylinderintersect(from, to, bottom, top, d->legsradius, dist);
    }

    bool intersect(dynent *d, const vec &from, const vec &to, float margin, float &dist)   // if lineseg hits entity bounding box
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight + margin;
        top.z += d->aboveeye + margin;
        return linecylinderintersect(from, to, bottom, top, d->radius + margin, dist);
    }

    dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin, float &bestdist)
    {
        dynent *best = NULL;
        bestdist = 1e16f;
        loopi(numdynents())
        {
            dynent *o = iterdynents(i);
            if(o==at || o->state!=CS_ALIVE) continue;
            float dist;
            if(!intersect(o, from, to, margin, dist)) continue;
            if(dist<bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
        return best;
    }

    void shorten(const vec &from, vec &target, float dist)
    {
        target.sub(from).mul(min(1.0f, dist)).add(from);
    }

    bool scanprojs(vec &from, vec &to, gameent *d, int atk)
    {
        if(betweenrounds) return false;
        vec stepv;
        float dist = to.dist(from, stepv);
        int steps = clamp(int(dist * 2), 1, 200);
        stepv.div(steps);
        vec point = from;
        loopi(steps)
        {
            point.add(stepv);
            loopv(projs)
            {
                projectile &p = projs[i];
                if (p.projtype != PROJ_PLASMA || (d != p.owner && p.owner->type != ENT_AI)) continue;
                if (attacks[atk].gun == GUN_PISTOL && p.o.dist(point) <= attacks[p.atk].margin)
                {
                    p.atk = ATK_PISTOL_COMBO;
                    projsplash(p, p.o, NULL, attacks[p.atk].damage);
                    if(d == self || d->ai)
                    {
                        addmsg(N_EXPLODE, "rci3iv", p.owner, lastmillis-maptime, p.atk, p.id-maptime,
                               hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                    }
                    stopsound(p.projsound, p.projchan);
                    projs.remove(i--);
                    return true;
                }
            }
        }
        return false;
    }

    void hitscan(vec &from, vec &to, gameent *d, int atk)
    {
        int maxrays = attacks[atk].rays;
        if(scanprojs(from, to, d, atk)) return;
        dynent *o;
        float dist;
        int margin = attacks[atk].margin, damage = attacks[atk].damage, flags = HIT_TORSO;
        bool hitlegs = false, hithead = false;
        if(attacks[atk].rays > 1)
        {
            dynent *hits[GUN_MAXRAYS];
            loopi(maxrays)
            {
                if(!betweenrounds && (hits[i] = intersectclosest(from, rays[i], d, margin, dist)))
                {
                    hitlegs = islegshitbox(hits[i], from, rays[i], dist);
                    hithead = isheadhitbox(hits[i], from, rays[i], dist);
                    shorten(from, rays[i], dist);
                    impacteffects(atk, d, from, rays[i], true);
                }
                else impacteffects(atk, d, from, rays[i]);
            }
            if(betweenrounds) return;
            loopi(maxrays) if(hits[i])
            {
                o = hits[i];
                hits[i] = NULL;
                int numhits = 1;
                for(int j = i+1; j < maxrays; j++) if(hits[j] == o)
                {
                    hits[j] = NULL;
                    numhits++;
                }
                if(attacks[atk].headshotdam) // if an attack does not have headshot damage, then it does not deal locational damage
                {
                    if(hithead) flags |= HIT_HEAD;
                    if(hitlegs) flags |= HIT_LEGS;
                }
                damage = calcdamage(damage, (gameent *)o, d, atk, flags);
                calcpush(numhits*damage, o, d, from, to, atk, numhits, flags);
                damageeffect(damage, o, rays[i], atk, getbloodcolor(o), flags & HIT_HEAD);
            }
        }
        else
        {
            if(!betweenrounds && (o = intersectclosest(from, to, d, margin, dist)))
            {
                hitlegs = islegshitbox(o, from, to, dist);
                hithead = isheadhitbox(o, from, to, dist);
                shorten(from, to, dist);
                impacteffects(atk, d, from, to, true);
                if(attacks[atk].headshotdam) // if an attack does not have headshot damage, then it does not deal locational damage
                {
                    if(hithead) flags = HIT_HEAD;
                    else if(hitlegs) flags = HIT_LEGS;
                }
                damage = calcdamage(damage, (gameent *)o, d, atk, flags);
                calcpush(damage, o, d, from, to, atk, 1, flags);
                damageeffect(damage, o, to, atk, getbloodcolor(o), flags & HIT_HEAD);
                if(d == followingplayer(self) && attacks[atk].action == ACT_MELEE)
                {
                    physics::addroll(d, damage / 2.0f);
                }
            }
            else
            {
                impacteffects(atk, d, from, to);
            }
        }
    }

    void shoot(gameent *d, const vec &targ)
    {
        int prevaction = d->lastaction, attacktime = lastmillis-prevaction;
        if(attacktime<d->gunwait) return;
        d->gunwait = 0;
        if(!d->attacking || !validgun(d->gunselect)) return;
        int gun = d->gunselect, act = d->attacking, atk = guns[gun].attacks[act];
        d->lastaction = lastmillis;
        d->lastattack = atk;
        if(attacks[atk].action!=ACT_MELEE && (!d->ammo[gun] || attacks[atk].use > d->ammo[gun]))
        {
            if(d==self)
            {
                msgsound(S_WEAPON_NOAMMO, d);
                d->gunwait = 600;
                d->lastattack = -1;
                if(!d->ammo[gun]) weaponswitch(d);
            }
            return;
        }
        if(!d->haspowerup(PU_AMMO)) d->ammo[gun] -= attacks[atk].use;

        vec from = d->o, to = targ, dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        int kickamount = attacks[atk].kickamount;
        if(d->haspowerup(PU_DAMAGE)) kickamount *= 2;
        if(kickamount && !(d->physstate >= PHYS_SLOPE && d->crouching && d->crouched()))
        {
            vec kickback = vec(dir).mul(kickamount*-2.5f);
            d->vel.add(kickback);
        }
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

        if(!attacks[atk].projspeed) hitscan(from, to, d, atk);

        shoteffects(atk, from, to, d, true, 0, prevaction);

        if(d==self || d->ai)
        {
            addmsg(N_SHOOT, "rci2i6iv", d, lastmillis-maptime, atk,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        }
        if(!attacks[atk].isfullauto) d->attacking = ACT_IDLE;
        int gunwait = attacks[atk].attackdelay;
        if(d->haspowerup(PU_HASTE) || d->role == ROLE_BERSERKER) gunwait /= 2;
        d->gunwait = gunwait;
        if(d->gunselect == GUN_PISTOL && d->ai) d->gunwait += int(d->gunwait*(((101-d->skill)+rnd(111-d->skill))/100.f));
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
        d->pitchrecoil = kickamount * 0.10f;
    }

    void updaterecoil(gameent *d, int curtime)
    {
        if(!d->pitchrecoil || !curtime) return;
        const float amount = d->pitchrecoil * (curtime / 1000.0f) * d->speed * 0.12f;
        d->pitch += amount;
        float friction = 4.0f / curtime * 30.0f;
        d->pitchrecoil = d->pitchrecoil * (friction - 2.8f) / friction;
        fixcamerarange();
    }

    void adddynlights()
    {
        loopv(projs)
        {
            projectile &p = projs[i];
            if(p.projtype < PROJ_PULSE && p.projtype > PROJ_ROCKET) continue;
            vec pos(p.o);
            pos.add(vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)));
            switch(p.projtype)
            {
                case PROJ_PULSE:
                {
                    adddynlight(pos, 25, vec(2.0f, 1.5f, 2.0));
                    break;
                }

                case PROJ_ROCKET:
                {
                    adddynlight(pos, 50, vec(2, 1.5f, 1), 0, 0, 0, 10, vec(0.5f, 0.375f, 0.25f));
                    break;
                }

                case PROJ_PLASMA:
                {
                    adddynlight(pos, 20, vec(0, 1.50f, 1.50f));
                    break;
                }
            }
        }
        loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            if(!isweaponbouncer(bnc.bouncetype)) continue;
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            switch(bnc.bouncetype)
            {
                case BNC_GRENADE:
                case BNC_GRENADE2:
                {
                    adddynlight(pos, 8, vec(0.25f, 0.25f, 1));
                    break;
                }
            }
        }
    }

    static const char * const projectilenames[6] = { "projectile/grenade", "projectile/grenade", "projectile/rocket", "projectile/eject/01", "projectile/eject/02", "projectile/eject/03" };
    static const char * const gibnames[5] = { "projectile/gib/gib01", "projectile/gib/gib02", "projectile/gib/gib03", "projectile/gib/gib04", "projectile/gib/gib05" };

    void preloadbouncers()
    {
        loopi(sizeof(projectilenames)/sizeof(projectilenames[0])) preloadmodel(projectilenames[i]);
        loopi(sizeof(gibnames)/sizeof(gibnames[0])) preloadmodel(gibnames[i]);
    }

    void renderbouncers()
    {
        float yaw, pitch;
        loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            vec pos = bnc.offsetpos();
            vec vel(bnc.vel);
            if(vel.magnitude() <= 25.0f) yaw = bnc.lastyaw;
            else
            {
                vectoyawpitch(vel, yaw, pitch);
                yaw += 90;
                bnc.lastyaw = yaw;
            }
            const char *mdl = NULL;
            int cull = MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED;
            if(bnc.bouncetype >= BNC_GIB && bnc.bouncetype <= BNC_EJECT)
            {
                float fade = 1;
                if(bnc.lifetime < 400) fade = bnc.lifetime/400.0f;
                switch(bnc.bouncetype)
                {
                    case BNC_GIB: mdl = gibnames[bnc.variant]; break;
                    case BNC_EJECT:
                    {
                        if(bnc.gun == GUN_SCATTER) mdl = "projectile/eject/03";
                        else if(bnc.gun == GUN_RAIL) mdl = "projectile/eject/02";
                        else mdl = "projectile/eject/01";
                        break;
                    }
                    default: continue;
                }
                rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, 0, cull, NULL, NULL, 0, 0, fade);
            }
            else
            {
                switch(bnc.bouncetype)
                {
                    case BNC_ROCKET: mdl = "projectile/rocket"; break;
                    default: mdl = "projectile/grenade"; break;
                }
            }
            rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, bnc.roll, cull);
        }
    }

    void renderprojectiles()
    {
        float yaw, pitch;
        loopv(projs)
        {
            projectile &p = projs[i];
            float dist = min(p.o.dist(p.to)/32.0f, 1.0f);
            vec pos = vec(p.o).add(vec(p.offset).mul(dist*p.offsetmillis/float(OFFSETMILLIS))),
                v = dist < 1e-6f ? p.dir : vec(p.to).sub(pos).normalize();
            // the amount of distance in front of the smoke trail needs to change if the model does
            vectoyawpitch(v, yaw, pitch);
            v.mul(3);
            v.add(pos);
            const char *mdl = NULL;
            switch(p.projtype)
            {
                case PROJ_ROCKET: mdl = "projectile/rocket"; break;
                default: mdl = ""; break;
            }
            rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, v, yaw, pitch, 0, MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED);
        }
    }

    void checkattacksound(gameent *d, bool local)
    {
        int atk = guns[d->gunselect].attacks[d->attacking];
        switch(d->chansound[Chan_Attack])
        {
            case S_PULSE2_A: atk = ATK_PULSE2; break;
            default: return;
        }
        if(atk >= 0 && atk < NUMATKS && d->clientnum >= 0 && d->state == CS_ALIVE &&
           d->lastattack == atk && lastmillis - d->lastaction < attacks[atk].attackdelay + 50)
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
        int sound = -1;
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
        updateprojectiles(curtime);
        if(self->clientnum>=0 && self->state==CS_ALIVE) shoot(self, worldpos); // only shoot when connected to server
        updatebouncers(curtime); // need to do this after the player shoots so bouncers don't end up inside player's BB next frame
        updaterecoil(self, curtime);
        gameent *following = followingplayer();
        if(!following) following = self;
        loopv(players)
        {
            gameent *d = players[i];
            checkattacksound(d, d==following);
            checkidlesound(d, d==following);
        }
    }

    void removeprojectiles(gameent* owner)
    {
        if (!owner)
        {
            projs.shrink(0);
            bouncers.deletecontents();
        }
        else
        {
            // projectiles
            int len = projs.length();
            loopi(len) if (projs[i].owner == owner) // can't use loopv here due to strange GCC optimizer bug
            {
                stopsound(projs[i].projsound, projs[i].projchan);
                projs.remove(i--);
                len--;
            }
            // bouncers
            loopv(bouncers) if (bouncers[i]->owner == owner)
            {
                delete bouncers[i];
                bouncers.remove(i--);
            }
        }
    }

    void avoidprojectiles(ai::avoidset &obstacles, float radius)
    {
        loopv(projs)
        {
            projectile &p = projs[i];
            obstacles.avoidnear(NULL, p.o.z + attacks[p.atk].exprad + 1, p.o, radius + attacks[p.atk].exprad);
        }
        loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            if(!isweaponbouncer(bnc.bouncetype)) continue;
            obstacles.avoidnear(NULL, bnc.o.z + attacks[bnc.atk].exprad + 1, bnc.o, radius + attacks[bnc.atk].exprad);
        }
    }

    VARP(hudgunsway, 0, 1, 1);

    FVAR(swaystep, 1, 35.8f, 100);
    FVAR(swayside, 0, 0.02f, 1);
    FVAR(swayup, -1, 0.03f, 1);
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
        trans.z -= speedfactor * 0.2f;

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
        o.add(trans);

        yaw += rotationyaw;
        pitch += rotationpitch;
        roll += rotationroll;

        o.add(dir).add(owner->o);
        if (!hudgunsway) o = owner->o;
    }

    void swayinfo::addevent(gameent* owner, int type, int duration, int factor)
    {
        if (owner != followingplayer(self)) // The first-person weapon sway is rendered only for ourselves or the player being spectated.
        {
            return;
        }

        swayEvent& swayevent = swayevents.add();
        swayevent.type = type;
        swayevent.millis = lastmillis;
        swayevent.duration = duration;
        swayevent.factor = factor;
        swayevent.pitch = 0;
    }

    void swayinfo::processevents()
    {
        loopv(swayevents)
        {
            swayEvent& events = swayevents[i];
            if (lastmillis - events.millis <= events.duration)
            {
                switch (events.type)
                {
                    case SwayEvent_Land:
                    case SwayEvent_Switch:
                    {
                        float progress = clamp((lastmillis - events.millis) / (float)events.duration, 0.0f, 1.0f);
                        events.pitch = events.factor * sinf(progress * PI);
                        pitch += events.pitch;
                        break;
                    }
                }
            }
            else
            {
                swayevents.remove(i--);
                continue;
            }
        }
    }
};
