// weapon.cpp: all shooting and effects code, projectile management
#include "game.h"

namespace game
{
    static const int OFFSETMILLIS = 500;
    vec rays[MAXRAYS];

    struct hitmsg
    {
        int target, lifesequence, info1, info2, flags;
        ivec dir;
    };
    vector<hitmsg> hits;

    ICOMMAND(getweapon, "", (), intret(self->gunselect));

    void gunselect(int gun, gameent *d)
    {
        if(gun == d->gunselect || lastmillis - d->lastswitch < 100)
        {
            return;
        }
        addmsg(N_GUNSELECT, "rci", d, gun);
        d->gunselect = gun;
        d->lastswitch = lastmillis;
        d->lastattack = -1;
        if(d == self) disablezoom();
        stopsound(d->gunsound, d->gunchan);
        playsound(S_WEAPON_LOAD, d);
    }

    void nextweapon(int dir, bool force = false)
    {
        if(self->state!=CS_ALIVE) return;
        dir = (dir < 0 ? NUMGUNS-1 : 1);
        int gun = self->gunselect;
        loopi(NUMGUNS)
        {
            gun = (gun + dir)%NUMGUNS;
            if(force || self->ammo[gun]) break;
        }
        if(gun != self->gunselect) gunselect(gun, self);
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

    enum
    {
        BNC_GRENADE,
        BNC_GRENADE2,
        BNC_ROCKET,
        BNC_GIB,
        BNC_DEBRIS,
        BNC_CARTRIDGE,
    };

    inline bool weaponbouncer(int type) { return type >= BNC_GRENADE && type <= BNC_ROCKET; }

    struct bouncer : physent
    {
        gameent *owner;

        vec offset;

        bool local;

        float lastyaw, roll, gravity, elasticity, offsetheight;

        int id, atk, bouncetype, lifetime;
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
            if(weaponbouncer(bouncetype) && offsetmillis > 0 && offset.z < 0)
                offsetheight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
            else offsetheight = -1;
        }

        void setradius(int type)
        {
            float typeradius = 1.4f;
            if(type == BNC_CARTRIDGE) typeradius = 0.3f;
            radius = xradius = yradius = typeradius;
            eyeheight = aboveeye = radius;
        }
    };

    vector<bouncer *> bouncers;

    void newbouncer(gameent *owner, const vec &from, const vec &to, bool local, int id, int atk, int type, int lifetime, int speed, float gravity, float elasticity)
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

        switch(type)
        {
            case BNC_GRENADE:
            {
                bnc.collidetype = COLLIDE_ELLIPSE;
                break;
            }
            case BNC_GRENADE2:
            {
                bnc.collidetype = COLLIDE_ELLIPSE;
                bnc.bouncesound = S_BOUNCE_GRENADE;
                break;
            }
            case BNC_ROCKET:
            {
                bnc.collidetype = COLLIDE_ELLIPSE;
                bnc.bouncesound = S_BOUNCE_ROCKET;
                break;
            }
            case BNC_GIB: bnc.variant = rnd(5); break;
            case BNC_CARTRIDGE:
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

        if(weaponbouncer(bnc.bouncetype))
        {
            bnc.offset = hudgunorigin(attacks[bnc.atk].gun, from, to, owner);
            if(owner==hudplayer() && !isthirdperson()) bnc.offset.sub(owner->o).rescale(16).add(owner->o);
        }
        else bnc.offset = from;
        bnc.offset.sub(bnc.o);
        bnc.offsetmillis = OFFSETMILLIS;

        bnc.resetinterp();
    }

    VARP(blood, 0, 1, 1);
    VARP(goreeffect, 0, 0, 2);

    void bounced(physent *d, const vec &surface)
    {
        if(d->type != ENT_BOUNCE) return;
        bouncer *b = (bouncer *)d;
        b->bounces++;
        int type = b->bouncetype;
        if((type == BNC_CARTRIDGE && b->bounces > 1) // prevent bounce sound spam
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
            if(blood && type == BNC_GIB && b->bounces <= 2 && goreeffect <= 0)
            {
                addstain(STAIN_BLOOD, vec(b->o).sub(vec(surface).mul(b->radius)), surface, 2.96f/b->bounces, (b->owner->role == ROLE_ZOMBIE ? bvec(0xFF, 0x60, 0xFF) : bvec(0x60, 0xFF, 0xFF)), rnd(4));
            }
        }
        b->lastbounce = lastmillis;
    }

    void updatebouncers(int time)
    {
       loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            switch(bnc.bouncetype)
            {
                case BNC_GRENADE:
                case BNC_ROCKET:
                {
                    if(bnc.vel.magnitude() > 20.0f) regular_particle_splash(PART_SMOKE, 5, 200, pos, 0x555555, 1.60f, 10, 500);
                    break;
                }

                case BNC_GRENADE2:
                {
                    if(bnc.vel.magnitude() > 10.0f) regular_particle_splash(PART_RING, 1, 200, pos, 0x74BCF9, 1.0f, 1, 500);
                    break;
                }

                case BNC_GIB:
                {
                    if(blood && goreeffect <= 0 && bnc.vel.magnitude() > 30.0f)
                        regular_particle_splash(PART_BLOOD, 0+rnd(4), 400, pos, getbloodcolor(bnc.owner), 0.80f, 25);
                    break;
                }

                case BNC_DEBRIS:
                {
                    if (bnc.vel.magnitude() <= 30.0f) break;
                    regular_particle_splash(PART_SMOKE, 5, 100, pos, 0x555555, 1.80f, 30, 500);
                    regular_particle_splash(PART_SPARK, 1, 40, pos, 0xF83B09, 1.20f, 10, 500);
                    particle_flare(bnc.o, bnc.o, 1, PART_EDIT, 0xFFC864, 0.5+rndscale(1.5f));
                }
            }
            if(bnc.bouncerloopsound >= 0) bnc.bouncerloopchan = playsound(bnc.bouncerloopsound, NULL, &pos, NULL, 0, -1, 100, bnc.bouncerloopchan);
            vec old(bnc.o);
            bool destroyed = false;
            if(bnc.bouncetype >= BNC_GIB && bnc.bouncetype <= BNC_CARTRIDGE)
            {
                // cheaper variable rate physics for debris, gibs, etc.
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((bnc.lifetime -= qtime)<0 || bounce(&bnc, qtime/1000.0f, 0.5f, 0.4f, 0.7f)) { destroyed = true; break; }
                }
            }
            else if(weaponbouncer(bnc.bouncetype))
            {
                destroyed = bounce(&bnc, bnc.elasticity, 0.5f, bnc.gravity) || (bnc.lifetime -= time)<0 || isdeadly(lookupmaterial(bnc.o)&MAT_LAVA) ||
                            ((bnc.bouncetype == BNC_GRENADE && bnc.bounces >= 1) || (bnc.bouncetype == BNC_ROCKET && bnc.bounces >= 2));
            }
            if(destroyed)
            {
                if(weaponbouncer(bnc.bouncetype))
                {
                    int damage = attacks[bnc.atk].damage;
                    hits.setsize(0);
                    explode(bnc.local, bnc.owner, bnc.o, bnc.vel, NULL, damage, bnc.atk);
                    addstain(STAIN_PULSE_SCORCH, bnc.offsetpos(), vec(bnc.vel).neg(), attacks[bnc.atk].exprad*0.75f);
                    if(bnc.atk == ATK_GRENADE) addstain(STAIN_PULSE_GLOW, bnc.offsetpos(), vec(bnc.vel).neg(), attacks[bnc.atk].exprad/2, 0x74BCF9);
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

    void removebouncers(gameent *owner)
    {
        loopv(bouncers) if(bouncers[i]->owner==owner) { delete bouncers[i]; bouncers.remove(i--); }
    }

    void clearbouncers()
    {
        bouncers.deletecontents();
    }

   enum
   {
       PROJ_PULSE,
       PROJ_PLASMA,
       PROJ_ROCKET
    };

    struct projectile
    {
        gameent *owner;

        vec dir, o, from, to, offset;

        bool local, destroyed;

        float speed;

        int id, atk, projtype, lifetime, offsetmillis;

        int projchan, projsound;

        projectile() : projchan(-1), projsound(-1)
        {
            destroyed = false;
        }
        ~projectile()
        {
            if(projchan >= 0) stopsound(projsound, projchan);
            projsound = projchan = -1;
        }

    };
    vector<projectile> projs;

    void clearprojectiles()
    {
        projs.shrink(0);
    }

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
    }

    void removeprojectiles(gameent *owner)
    {
        // can't use loopv here due to strange GCC optimizer bug
        int len = projs.length();
        loopi(len) if(projs[i].owner==owner)
        {
            stopsound(projs[i].projsound, projs[i].projchan);
            projs.remove(i--);
            len--;
        }
    }

    void damageeffect(int damage, dynent *d, vec p, int atk, int color)
    {
        gameent *f = (gameent *)d,
                *hud = followingplayer(self);
        if(f == hud) p.z += 0.6f*(d->eyeheight + d->aboveeye) - d->eyeheight;
        if(f->haspowerup(PU_INVULNERABILITY))
        {
            msgsound(S_ACTION_INVULNERABILITY, f);
            particle_splash(PART_SPARK2, 100, 150, p, getplayercolor(f, f->team), 0.50f);
            return;
        }
        if(!damage) return;
        if(blood && color != -1)
        {
            particle_splash(PART_BLOOD, damage/10, 1000, p, color, 2.60f);
            particle_splash(PART_BLOOD2, 200, 250, p, color, 0.50f);
        }
        else
        {
            particle_splash(PART_SPARK2, damage/10, 500, p, 0xFFFF66, 0.9f, 300);
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
        if(f->shield) particle_splash(PART_SPARK2, 5, 100, p, 0xFFFF66, 0.40f, 200);
        if(validatk(atk) && attacks[atk].hitsound) playsound(attacks[atk].hitsound, f);
        else playsound(S_PLAYER_DAMAGE, f);
        if(f->haspowerup(PU_ARMOR)) playsound(S_ACTION_ARMOUR, f);
    }

    void spawnbouncer(const vec &from, gameent *d, int type)
    {
        vec to(rnd(100)-50, rnd(100)-50, rnd(100)-50);
        float elasticity = 0.6f;
        if(type == BNC_CARTRIDGE)
        {
            to = vec(-50, 1, rnd(30)-15);
            to.rotate_around_z(d->yaw*RAD);
            elasticity = 0.4f;
        }
        if(to.iszero()) to.z += 1;
        to.normalize();
        to.add(from);
        newbouncer(d, from, to, true, 0, -1, type, type == BNC_DEBRIS ? 400 : rnd(1000)+1000, rnd(100)+20, 0.3f + rndscale(0.8f), elasticity);
    }

    void gibeffect(int damage, const vec &vel, gameent *d)
    {
        if(!gore) return;
        vec from = d->abovehead();
        if(goreeffect <= 0)
        {
            loopi(min(damage, 8)+1) spawnbouncer(from, d, BNC_GIB);
            if(blood)
            {
                particle_splash(PART_BLOOD, 3, 180, d->o, getbloodcolor(d), 3.0f+rndscale(5.0f), 150, 0, 0.1f);
                particle_splash(PART_BLOOD2, damage, 300, d->o, getbloodcolor(d), 0.89f, 300, 5);
            }
        }
        msgsound(S_GIB, d);
    }

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2 = 1, int flags = HIT_TORSO)
    {
        gameent *f = (gameent *)d;

        f->lastpain = lastmillis;
        if(at->type==ENT_PLAYER && f!=at && !isally(f, at)) at->totaldamage += damage;
        if(at==self && d!=at)
        {
            extern int hitsound;
            if(hitsound && at->lasthit != lastmillis)
                playsound(isally(f, at) ? S_HIT_ALLY : S_HIT);
            at->lasthit = lastmillis;
        }
        if(f->type==ENT_AI || !m_mp(gamemode) || f==at)
        {
            f->hitpush(damage, vel, at, atk);
        }
        if(f->type == ENT_AI) hitmonster(damage, (monster *)f, at, vel, atk);
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
            if(at==self)
            {
                if(f == self)
                {
                    if(lastmillis-f->lastyelp > 500) damageblend(damage);
                    if(f != at) damagecompass(damage, at ? at->o : f->o);
                }
                if(flags & HIT_HEAD)
                {
                    extern int playheadshotsound;
                    if(playheadshotsound) playsound(S_HIT_WEAPON_HEAD, NULL, &f->o);
                }
            }
        }
    }

    int calcdamage(int damage, gameent *target, gameent *actor, int atk, int flags = HIT_TORSO)
    {
        if(target != actor)
        {
            if(target->haspowerup(PU_INVULNERABILITY) && !actor->haspowerup(PU_INVULNERABILITY))
            {
                return 0;
            }
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
            if (actor->haspowerup(PU_DAMAGE) || actor->role == ROLE_JUGGERNAUT) damage *= 2;
            if (isally(target, actor) || target == actor) damage /= ALLY_DAMDIV;
        }
        if (target->haspowerup(PU_ARMOR) || target->role == ROLE_JUGGERNAUT) damage /= 2;
        if(!damage) damage = 1;
        return damage;
    }

    void calcpushdamage(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays, int flags)
    {
        gameent *f = (gameent *)d;
        hit(calcdamage(damage, f, at, atk, flags), d, at, vec(to).sub(from).safenormalize(), atk, from.dist(to), rays, flags);
    }

    float projdist(dynent *o, vec &dir, const vec &v, const vec &vel)
    {
        vec middle = o->o;
        middle.z += (o->aboveeye-o->eyeheight)/2;
        float dist = middle.dist(v, dir);
        dir.div(dist);
        if(dist<0) dist = 0;
        return dist;
    }

    void radialeffect(dynent *o, const vec &v, const vec &vel, int damage, gameent *at, int atk)
    {
        if(server::betweenrounds || o->state!=CS_ALIVE) return;
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
        vec dynlight = vec(1.0f, 3.0f, 4.0f);
        int fireball = 0x50CFE5;
        switch(atk)
        {
            case ATK_SCATTER2:
            case ATK_ROCKET1:
            case ATK_ROCKET2:
            {
                dynlight = vec(0.5f, 0.375f, 0.25f);
                fireball = 0xC8E66B;
                particle_splash(PART_SPARK, 50, 190, v, 0xF3A612, 0.50f + rndscale(0.90f), 180, 40, 0.3f);
                particle_splash(PART_SPARK2, 100, 250, v, 0xFFC864, 0.10f+rndscale(0.50f), 600, 1);
                particle_splash(PART_SMOKE, 50, 280, v, 0x444444, 10.0f, 250, 200, 0.09f);
                break;
            }
            case ATK_PULSE1:
            {
                dynlight = vec(1.0f, 0.50f, 1.0f);
                fireball = 0xEE88EE;
                particle_splash(PART_SPARK2, 5+rnd(20), 200, v, 0xEE88EE, 0.08f+rndscale(0.35f), 400, 2);
                break;
            }
            case ATK_GRENADE:
            {
                dynlight = vec(0, 0.25f, 1.0f);
                fireball = 0x74BCF9;
                particle_flare(v, v, 120, PART_ELECTRICITY, fireball, 25.0f, NULL, 0.5f);
                break;
            }
            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                dynlight = vec(0.25f, 1.0f, 1.0f);
                fireball = 0x00FFFF;
                particle_fireball(v, 1.0f, PART_EXPLOSION2, atk == ATK_PISTOL2 ? 200 : 500, 0x00FFFF, attacks[atk].exprad);
                particle_splash(PART_SPARK2, 3+rnd(20), 200, v, 0x00FFFF, 0.05f+rndscale(0.10f), 180, 5);
                break;
            }
            default: break;
        }
        particle_fireball(v, 1.15f*attacks[atk].exprad, atk == ATK_PULSE1 || atk == ATK_GRENADE? PART_EXPLOSION2: PART_EXPLOSION1, atk == ATK_GRENADE ? 200 : 400, fireball, 0.10f);
        adddynlight(v, 2*attacks[atk].exprad, dynlight, 350, 40, 0, attacks[atk].exprad/2, vec(0.5f, 1.5f, 2.0f));
        playsound(attacks[atk].impactsound, NULL, &v);
        if(lookupmaterial(v) & MAT_WATER) playsound(S_IMPACT_WATER_PROJ, NULL, &v);
        else // no debris in water
        {
            int numdebris = rnd(maxdebris - 5) + 5;
            vec debrisvel = vec(owner->o).sub(v).safenormalize(),
                debrisorigin(v);
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
        if(!local) return;
        int numdyn = numdynents();
        loopi(numdyn)
        {
            dynent *o = iterdynents(i);
            if(o->o.reject(v, o->radius + attacks[atk].exprad) || o==safe) continue;
            radialeffect(o, v, vel, damage, owner, atk);
        }
    }

    void stain(const projectile &p, const vec &pos)
    {
        vec dir = vec(p.dir).neg();
        float rad = attacks[p.atk].exprad*0.75f;
        addstain(STAIN_PULSE_SCORCH, pos, dir, rad);
        if(p.atk == ATK_PISTOL2) addstain(STAIN_PULSE_GLOW, pos, dir, rad, 0x00FFFF);
    }

    void projsplash(projectile &p, const vec &v, dynent *safe, int damage)
    {
        explode(p.local, p.owner, v, p.dir, safe, damage, p.atk);
        stain(p, v);
    }

    void explodeeffects(int atk, gameent *d, bool local, int id)
    {
        if(local) return;
        switch(atk)
        {
            case ATK_SCATTER2:
            case ATK_ROCKET2:
                loopv(bouncers)
                {
                    bouncer &bnc = *bouncers[i];
                    if(!weaponbouncer(bnc.bouncetype)) break;
                    if(bnc.owner == d && bnc.id == id && !bnc.local)
                    {
                        explode(bnc.local, bnc.owner, bnc.offsetpos(), bnc.vel, NULL, 0, atk);
                        addstain(STAIN_PULSE_SCORCH, bnc.offsetpos(), vec(bnc.vel).neg(), attacks[bnc.atk].exprad*0.75f);
                        delete bouncers.remove(i);
                        break;
                    }
                }
                break;
            case ATK_PULSE1:
            case ATK_ROCKET1:
            case ATK_PISTOL1:
                loopv(projs)
                {
                    projectile &p = projs[i];
                    if(p.atk == atk && p.owner == d && p.id == id && !p.local)
                    {
                        vec pos = vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)).add(p.o);
                        explode(p.local, p.owner, pos, p.dir, NULL, 0, atk);
                        stain(p, pos);
                        stopsound(p.projsound, p.projchan);
                        projs.remove(i);
                        break;
                    }
                }
                break;
            default: break;
        }
    }

    bool projdamage(dynent *o, projectile &p, const vec &v, int damage)
    {
        if(server::betweenrounds || o->state!=CS_ALIVE) return false;
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
        gameent *noside = followingplayer(self);
        loopv(projs)
        {
            projectile &p = projs[i];
            p.offsetmillis = max(p.offsetmillis-time, 0);
            vec dv;
            float dist = p.to.dist(p.o, dv);
            dv.mul(time/max(dist*1000/p.speed, float(time)));
            vec v = vec(p.o).add(dv);
            float damage = attacks[p.atk].damage;
            hits.setsize(0);
            if(p.local)
            {
                vec halfdv = vec(dv).mul(0.5f), bo = vec(p.o).add(halfdv);
                float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[p.atk].margin;
                loopj(numdynents())
                {
                    dynent *o = iterdynents(j);
                    if(p.owner==o || o->o.reject(bo, o->radius + br)) continue;
                    if(projdamage(o, p, v, damage))
                    {
                        p.destroyed = true;
                        break;
                    }
                }
            }
            if(!p.destroyed)
            {
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((p.lifetime -= qtime)<0)
                    {
                        p.destroyed = true;
                    }
                }
                if(lookupmaterial(p.o) & MAT_LAVA)
                {
                    p.destroyed = true;
                }
                else if(dist<4)
                {
                    if(p.o!=p.to) // if original target was moving, re-evaluate endpoint
                    {
                        if(raycubepos(p.o, p.dir, p.to, 0, RAY_CLIPMAT|RAY_ALPHAPOLY)>=4) continue;
                    }
                    p.destroyed = true;
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
                            if(p.lifetime<=attacks[p.atk].lifetime/2) regular_particle_splash(PART_SPARK, 4, 180, pos, tailc, tails, 10, 0);
                            particle_splash(PART_SPARK, 1, 1, pos, tailc, tails, 150, 20);
                            p.projsound = S_ROCKET_LOOP;
                            break;
                        }

                        case PROJ_PULSE:
                        {
                            tailc = 0xDD88DD;
                            particle_splash(PART_ORB, 1, 1, pos, tailc, tails, 150, 20);
                            p.projsound = S_PULSE_LOOP;
                            break;
                        }

                        case PROJ_PLASMA:
                        {
                            tails = 5.0f; tailc = 0x00FFFF;
                            particle_splash(PART_ORB, 1, 1, pos, tailc, tails, 150, 20);
                            p.projsound = S_PISTOL_LOOP;
                            break;
                        }
                    }
                    if(p.owner != noside)
                    {
                        float len = min(20.0f, vec(p.offset).add(p.from).dist(pos));
                        vec dir = vec(dv).normalize(),
                            tail = vec(dir).mul(-len).add(pos),
                            head = vec(dir).mul(2.4f).add(pos);
                        particle_flare(tail, head, 1, PART_TRAIL_PROJECTILE, tailc, tails);
                    }
                    p.projchan = playsound(p.projsound, NULL, &pos, NULL, 0, -1, 100, p.projchan);
                }
            }
            if(p.destroyed)
            {
                projsplash(p, v, NULL, damage);
                if(p.local)
                    addmsg(N_EXPLODE, "rci3iv", p.owner, lastmillis-maptime, p.atk, p.id-maptime,
                            hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                stopsound(p.projsound, p.projchan);
                projs.remove(i--);
            }
            else p.o = v;
        }
    }

    void rayhit(int atk, gameent *d, const vec &from, const vec &to, bool hit = false)
    {
        if(!validatk(atk) || from.dist(to) > attacks[atk].range) return;
        vec dir = vec(from).sub(to).safenormalize();
        int mat = lookupmaterial(to);
        bool water = (mat&MATF_VOLUME) == MAT_WATER, glass = (mat&MATF_VOLUME) == MAT_GLASS;
        switch(atk)
        {
            case ATK_SCATTER1:
            {
                adddynlight(vec(to).madd(dir, 4), 6, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if(hit || water || glass) break;
                particle_splash(PART_SPARK2, 1+rnd(6), 80+rnd(380), to, 0xFFC864, 0.08f + rndscale(0.3f), 250);
                particle_splash(PART_SMOKE, 1+rnd(5), 150, to, 0x606060, 1.8f+rndscale(2.2f), 100, 100, 0.01f);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(0.80f));
            }
            break;

            case ATK_SMG1:
            case ATK_SMG2:
            {
                adddynlight(vec(to).madd(dir, 4), 15, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if(hit || water || glass) break;
                particle_fireball(to, 0.5f, PART_EXPLOSION1, 120, 0xFFC864, 2.0f);
                particle_splash(PART_SPARK, 50, 40, to, 0xFFC864, 1.0f);
                particle_splash(PART_SPARK2, 10+rnd(30), 150, to, 0xFFC864, 0.05f+rndscale(0.09f), 250);
                particle_splash(PART_SMOKE, 30, 180, to, 0x444444, 2.20f, 80, 100, 0.01f);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(0.80f));
                break;

            }

            case ATK_PULSE2:
            {
                adddynlight(vec(to).madd(dir, 4), 80, vec(1.0f, 0.50f, 1.0f), 20);
                if(hit || water) break;
                if(from.dist(to) <= attacks[atk].range-1)
                {
                    particle_splash(PART_SPARK2, 1+rnd(5), 300, to, 0xEE88EE, 0.01f+rndscale(0.10f), 350, 2);
                    particle_splash(PART_SMOKE, 20, 120, to, 0x777777, 2.0f, 100, 80, 0.02f);
                }
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 1.0f+rndscale(1.10f));
                playsound(attacks[atk].impactsound, NULL, &to);
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            case ATK_INSTA:
            {
                bool insta = attacks[atk].gun == GUN_INSTA;
                adddynlight(vec(to).madd(dir, 4), 60, !insta? vec(0.25f, 1.0f, 0.75f):  vec(0.25f, 0.75f, 1.0f), 180, 75, DL_EXPAND);
                if(hit || water || glass) break;
                particle_splash(PART_SPARK, 80, 80, to, !insta? 0x77DD77: 0x50CFE5, 1.25f, 100, 80);
                particle_splash(PART_SPARK2, 5+rnd(20), 200+rnd(380), to, !insta? 0x77DD77: 0x50CFE5, 0.1f+rndscale(0.3f), 200, 3);
                particle_splash(PART_SMOKE, 20, 180, to, 0x808080, 2.0f, 60, 80, 0.05f);
                addstain(STAIN_RAIL_HOLE, to, dir, 3.5f);
                addstain(STAIN_RAIL_GLOW, to, dir, 3.0f, !insta? 0x77DD77: 0x50CFE5);
                break;
            }

            case ATK_PISTOL1:
            {
                adddynlight(vec(to).madd(dir, 4), 30, vec(0.25, 1.0f, 1.0f), 200, 10, DL_SHRINK);
                if(hit || water || glass) break;
                particle_fireball(to, 2.2f, PART_EXPLOSION1, 140, 0x00FFFF, 0.1f);
                particle_splash(PART_SPARK2, 50, 180, to, 0x00FFFF, 0.08f+rndscale(0.18f));
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 0.80f+rndscale(1.0f));
                addstain(STAIN_RAIL_GLOW, to, dir, 1.50f, 0x00FFFF);
                break;
            }

            default: break;
        }
        if(attacks[atk].action == ACT_MELEE || atk == ATK_PULSE2 || atk == ATK_ZOMBIE || hit) return;
        int impactsnd = attacks[atk].impactsound;
        if(water)
        {
            addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(0.80f));
            impactsnd = S_IMPACT_WATER;

        }
        else if(glass)
        {
            particle_splash(PART_GLASS, 20, 200, to, 0xFFFFFF, 0.10+rndscale(0.20f));
            addstain(STAIN_GLASS_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(1.0f));
            impactsnd = S_IMPACT_GLASS;
        }
        if(!(attacks[atk].rays > 1 && d==hudplayer()) && impactsnd) playsound(impactsnd, NULL, &to);
    }

    VARP(muzzleflash, 0, 1, 1);

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun, sound = attacks[atk].sound;
        float dist = from.dist(to);
        vec up = to;
        gameent *hud = followingplayer(self);
        switch(atk)
        {
            case ATK_SCATTER1:
            {
                if(d->muzzle.x >= 0)
                {
                    if(muzzleflash)
                    {
                        particle_flare(d->muzzle, d->muzzle, 70, PART_MUZZLE_FLASH, 0xEFE598, 3.80f, d);
                        adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.5f, 0.375f, 0.25f), 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                    }
                    if(d==hudplayer()) spawnbouncer(d->muzzle, d, BNC_CARTRIDGE); // using muzzle vec temporarily
                }
                if(!local)
                {
                    loopi(attacks[atk].rays)
                    {
                        offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i], d);
                        rayhit(atk, d, from, rays[i], hit);
                    }
                }
                loopi(attacks[atk].rays) particle_flare(hudgunorigin(gun, from, rays[i], d), rays[i], 80, PART_TRAIL, 0xFFC864, 0.95f);
                break;
            }
            case ATK_SCATTER2:
            {
                up.z += dist/16;
                newbouncer(d, from, up, local, id, atk, BNC_GRENADE, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
                break;
            }

            case ATK_SMG1:
            case ATK_SMG2:
            {
                if(d->muzzle.x >= 0)
                {
                    if(muzzleflash)
                    {
                        particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH, 0xEFE898, 2.5f, d);
                        adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.5f, 0.375f, 0.25f), atk==ATK_SMG1 ? 70 : 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                    }
                    if(d==hudplayer()) spawnbouncer(d->muzzle, d, BNC_CARTRIDGE); // using muzzle vec temporarily
                }
                if(atk == ATK_SMG2) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0xFFC864, 0.95f);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_PULSE1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 115, PART_MUZZLE_FLASH2, 0xDD88DD, 1.8f, d);
                }
                newprojectile(d, from, to, local, id, atk, PROJ_PULSE);
                break;
            }
            case ATK_PULSE2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                     particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH2, 0xDD88DD, 1.4f, d);
                     adddynlight(hudgunorigin(gun, d->o, to, d), 30, vec(1.0f, 0.50f, 1.0f), 80, 10, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_LIGHTNING, 0xEE88EE, 1.0f, d);
                particle_fireball(to, 1.0f, PART_EXPLOSION2, 100, 0xDD88DD, 3.0f);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_ROCKET1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, PART_MUZZLE_FLASH2, 0xEFE898, 0.1f, d, 0.2f);
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
                if(d->muzzle.x >= 0)
                {
                    if(muzzleflash)
                    {
                        particle_flare(d->muzzle, d->muzzle, 100, PART_MUZZLE_FLASH, 0x77DD77, 0.1f, d, 0.3f);
                        adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.25f, 1.0f, 0.75f), 150, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                    }
                    if(d==hudplayer()) spawnbouncer(d->muzzle, d, BNC_CARTRIDGE); // using muzzle vec temporarily
                }
                if(atk == ATK_RAIL2) particle_trail(PART_SMOKE, 350, hudgunorigin(gun, from, to, d), to, 0xDEFFDE, 0.3f, 50);
                particle_flare(hudgunorigin(gun, from, to, d), to, 600, PART_TRAIL, 0x55DD55, 0.50f);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_GRENADE:
            {
                if(d->muzzle.x >= 0)
                {
                    if(muzzleflash)
                    {
                        particle_flare(d->muzzle, d->muzzle, 100, PART_ELECTRICITY, 0x74BCF9, 0.1f, d, 0.3f);
                    }
                }
                up.z += dist/8;
                newbouncer(d, from, up, local, id, atk, BNC_GRENADE2, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
                break;
            }

            case ATK_PISTOL1:
            case ATK_PISTOL2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                   particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH3, 0x00FFFF, 2.50f, d);
                   adddynlight(hudgunorigin(attacks[atk].gun, d->o, to, d), 30, vec(0.25f, 1.0f, 1.0f), 60, 20, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(atk == ATK_PISTOL2)
                {
                    newprojectile(d, from, to, local, id, atk, PROJ_PLASMA);
                    break;
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0x00FFFF, 2.0f);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_INSTA:

                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 100, PART_MUZZLE_FLASH, 0x50CFE5, 2.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.25f, 0.75f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(gun, from, to, d), to, 100, PART_LIGHTNING, 0x50CFE5, 1.0f);
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, PART_TRAIL, 0x50CFE5, 1.0f);
                break;

            default: break;
        }
        bool looped = false;
        if(d->attacksound >= 0 && d->attacksound != sound) d->stopweaponsound();
        if(d->idlesound >= 0) d->stopidlesound();
        switch(sound)
        {
            case S_SG1_A:
            case S_SG2:
            {
                playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
                if(d == hud)
                {
                    d->gunsound = S_SG1_B;
                    d->gunchan = playsound(d->gunsound, d, NULL, NULL, 0, 0, 0, d->gunchan);
                }
                break;
            }
            case S_PULSE2_A:
            {
                if(d->attacksound >= 0) looped = true;
                d->attacksound = sound;
                d->attackchan = playsound(sound, NULL, &d->o, NULL, 0, -1, 100, d->attackchan);
                if(lastmillis - prevaction > 200 && !looped) playsound(S_PULSE2_B, d);
                break;
            }
            case S_RAIL_A:
            case S_RAIL_INSTAGIB:
            {
                playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
                if(d == hud)
                {
                    d->gunsound = S_RAIL_B;
                    d->gunchan = playsound(d->gunsound, d, NULL, NULL, 0, 0, 0, d->gunchan);
                }
                break;
            }
            default: playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
        }
        if(lastmillis-prevaction>200 && !looped)
        {
            if(d->role == ROLE_JUGGERNAUT)
            {
                playsound(S_JUGGERNAUT_ACTION, d);
                return;
            }
            if(d->haspowerup(PU_DAMAGE) || d->haspowerup(PU_HASTE) || d->haspowerup(PU_AMMO))
            {
                playsound(S_ACTION_DAMAGE+d->poweruptype-1, d);
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

    bool intersecthead(dynent *d, const vec &from, const vec &to, float &dist)
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->aboveeye-2.0f;
        top.z += d->aboveeye;
        return linecylinderintersect(from, to, bottom, top, d->headradius, dist);
    }

    bool intersectlegs(dynent *d, const vec &from, const vec &to, float &dist)
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
                if (p.projtype != PROJ_PLASMA) continue;
                if (attacks[atk].gun == GUN_PISTOL && p.o.dist(point) <= attacks[p.atk].margin)
                {
                    shorten(from, to, dist);
                    rayhit(atk, d, from, to);
                    if(p.local && (d==self || d->ai))
                    {
                        p.owner = d;
                        p.atk = ATK_PISTOL_COMBO;
                        p.destroyed = true;
                    }
                    return true;
                }
            }
        }
        return false;
    }

    void hitscan(vec &from, vec &to, gameent *d, int atk)
    {
        int maxrays = attacks[atk].rays;
        if(server::betweenrounds)
        {
            if(maxrays > 1) { loopi(maxrays) rayhit(atk, d, from, rays[i]); }
            else rayhit(atk, d, from, to);
            return;
        }
        if(scanprojs(from, to, d, atk)) return;
        dynent *o;
        float dist;
        int margin = attacks[atk].margin, damage = attacks[atk].damage, flags = HIT_TORSO;
        bool hitlegs = false, hithead = false;
        if(attacks[atk].rays > 1)
        {
            dynent *hits[MAXRAYS];
            loopi(maxrays)
            {
                if((hits[i] = intersectclosest(from, rays[i], d, margin, dist)))
                {
                    hitlegs = intersectlegs(hits[i], from, rays[i], dist);
                    hithead = intersecthead(hits[i], from, rays[i], dist);
                    shorten(from, rays[i], dist);
                    rayhit(atk, d, from, rays[i], true);
                }
                else rayhit(atk, d, from, rays[i]);
            }
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
                    if(hithead)
                    {
                        damage += attacks[atk].headshotdam;
                        flags |= HIT_HEAD;
                    }
                    if(hitlegs)
                    {
                        damage /= 2;
                        flags |= HIT_LEGS;
                    }
                }
                calcpushdamage(numhits*damage, o, d, from, to, atk, numhits, flags);
                damageeffect(damage, o, rays[i], atk, getbloodcolor(o));
            }
        }
        else
        {
            if((o = intersectclosest(from, to, d, margin, dist)))
            {
                hithead = intersecthead(o, from, to, dist);
                hitlegs = intersectlegs(o, from, to, dist);
                shorten(from, to, dist);
                rayhit(atk, d, from, to, true);
                if(attacks[atk].headshotdam) // if an attack does not have headshot damage, then it does not deal locational damage
                {
                    if(hithead)
                    {
                        damage += attacks[atk].headshotdam;
                        flags |= HIT_HEAD;
                    }
                    else if(hitlegs)
                    {
                        damage /= 2;
                        flags |= HIT_LEGS;
                    }
                }
                calcpushdamage(attacks[atk].damage, o, d, from, to, atk, 1, flags);
                damageeffect(damage, o, to, atk, getbloodcolor(o));
            }
            else
            {
                rayhit(atk, d, from, to);
            }
        }
    }

    void shoot(gameent *d, const vec &targ)
    {
        int prevaction = d->lastaction, attacktime = lastmillis-prevaction;
        if(attacktime<d->gunwait) return;
        d->gunwait = 0;
        if(!d->attacking) return;
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
        if(attacks[atk].kickamount && !(d->physstate >= PHYS_SLOPE && d->crouching && d->crouched()))
        {
            vec kickback = vec(dir).mul(attacks[atk].kickamount*-2.5f);
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
        if(d->haspowerup(PU_HASTE) || d->role == ROLE_JUGGERNAUT) gunwait /= 2;
        d->gunwait = gunwait;
        if(attacks[atk].action != ACT_MELEE && d->ai) d->gunwait += int(d->gunwait*(((101-d->skill)+rnd(111-d->skill))/100.f));
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
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
            if(!weaponbouncer(bnc.bouncetype)) continue;
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            switch(bnc.bouncetype)
            {
                case BNC_GRENADE:
                {
                    adddynlight(pos, 20, vec(1, 0.25f, 0.25f));
                    break;
                }

                case BNC_GRENADE2:
                {
                    adddynlight(pos, 8, vec(0.25f, 0.25f, 1));
                    break;
                }
            }
        }
    }

    static const char * const projectilenames[4] = { "projectile/grenade", "projectile/grenade/v2", "projectile/rocket", "projectile/eject/cartridge01" };
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
            if(bnc.bouncetype >= BNC_GIB && bnc.bouncetype <= BNC_CARTRIDGE)
            {
                float fade = 1;
                if(bnc.lifetime < 400) fade = bnc.lifetime/400.0f;
                switch(bnc.bouncetype)
                {
                    case BNC_GIB: mdl = gibnames[bnc.variant]; break;
                    case BNC_CARTRIDGE: mdl = "projectile/eject/cartridge01"; break;
                    default: continue;
                }
                rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, 0, cull, NULL, NULL, 0, 0, fade);
            }
            else
            {
                switch(bnc.bouncetype)
                {
                    case BNC_GRENADE: mdl = "projectile/grenade"; break;
                    case BNC_GRENADE2: mdl = "projectile/grenade/v2"; break;
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

    void removeweapons(gameent *d)
    {
        removebouncers(d);
        removeprojectiles(d);
    }

    void checkattacksound(gameent *d, bool local)
    {
        int atk = guns[d->gunselect].attacks[d->attacking];
        switch(d->attacksound)
        {
            case S_PULSE2_A: atk = ATK_PULSE2; break;
            default: return;
        }
        if(atk >= 0 && atk < NUMATKS &&
           d->clientnum >= 0 && d->state == CS_ALIVE &&
           d->lastattack == atk && lastmillis - d->lastaction < attacks[atk].attackdelay + 50)
        {
            d->attackchan = playsound(d->attacksound, NULL, local ? NULL : &d->o, NULL, 0, -1, -1, d->attackchan);
            if(d->attackchan < 0) d->attacksound = -1;
        }
        else
        {
            d->stopweaponsound();
        }
    }

    void checkidlesound(gameent *d, bool local)
    {
        int sound = -1;
        if(d->clientnum >= 0 && d->state == CS_ALIVE && d->attacksound < 0) switch(d->gunselect)
        {
            case GUN_ZOMBIE:
            {
                sound = S_ZOMBIE_IDLE;
                break;
            }
        }
        if(d->idlesound != sound)
        {
            if(d->idlesound >= 0) d->stopidlesound();
            if(sound >= 0)
            {
                d->idlechan = playsound(sound, NULL, local ? NULL : &d->o, NULL, 0, -1, 1200, d->idlechan, 500);
                if(d->idlechan >= 0) d->idlesound = sound;
            }
        }
        else if(sound >= 0)
        {
            d->idlechan = playsound(sound, NULL, local ? NULL : &d->o, NULL, 0, -1, 1200, d->idlechan, 500);
            if(d->idlechan < 0) d->idlesound = -1;
        }
    }

    void updateweapons(int curtime)
    {
        updateprojectiles(curtime);
        if(self->clientnum>=0 && self->state==CS_ALIVE) shoot(self, worldpos); // only shoot when connected to server
        updatebouncers(curtime); // need to do this after the player shoots so bouncers don't end up inside player's BB next frame
        gameent *following = followingplayer();
        if(!following) following = self;
        loopv(players)
        {
            gameent *d = players[i];
            checkattacksound(d, d==following);
            checkidlesound(d, d==following);
        }
    }

    void avoidweapons(ai::avoidset &obstacles, float radius)
    {
        loopv(projs)
        {
            projectile &p = projs[i];
            obstacles.avoidnear(NULL, p.o.z + attacks[p.atk].exprad + 1, p.o, radius + attacks[p.atk].exprad);
        }
        loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            if(!weaponbouncer(bnc.bouncetype)) continue;
            obstacles.avoidnear(NULL, bnc.o.z + attacks[bnc.atk].exprad + 1, bnc.o, radius + attacks[bnc.atk].exprad);
        }
    }
};

