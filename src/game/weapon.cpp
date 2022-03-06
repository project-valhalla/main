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

#if 0
    #define MINDEBRIS 3
    VARP(maxdebris, MINDEBRIS, 10, 100);
#endif
    VARP(explosioneffect, 0, 1, 1);

    ICOMMAND(getweapon, "", (), intret(player1->gunselect));

    void gunselect(int gun, gameent *d)
    {
        if(gun!=d->gunselect)
        {
            addmsg(N_GUNSELECT, "rci", d, gun);
            playsound(S_WEAPLOAD, d);
        }
        disablezoom();
        d->gunselect = gun;
    }

    void nextweapon(int dir, bool force = false)
    {
        if(player1->state!=CS_ALIVE) return;
        dir = (dir < 0 ? NUMGUNS-1 : 1);
        int gun = player1->gunselect;
        loopi(NUMGUNS)
        {
            gun = (gun + dir)%NUMGUNS;
            if(force || player1->ammo[gun]) break;
        }
        if(gun != player1->gunselect) gunselect(gun, player1);
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
        if(player1->state!=CS_ALIVE || !validgun(gun)) return;
        if(force || player1->ammo[gun]) gunselect(gun, player1);
        else playsound(S_NOAMMO);
    }
    ICOMMAND(setweapon, "si", (char *name, int *force), setweapon(name, *force!=0));

    void cycleweapon(int numguns, int *guns, bool force = false)
    {
        if(numguns<=0 || player1->state!=CS_ALIVE) return;
        int offset = 0;
        loopi(numguns) if(guns[i] == player1->gunselect) { offset = i+1; break; }
        loopi(numguns)
        {
            int gun = guns[(i+offset)%numguns];
            if(gun>=0 && gun<NUMGUNS && (force || player1->ammo[gun]))
            {
                gunselect(gun, player1);
                return;
            }
        }
        playsound(S_NOAMMO);
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
        if(s!=GUN_SG && d->ammo[GUN_SG])  s = GUN_SG;
        else if(s!=GUN_SMG && d->ammo[GUN_SMG])     s = GUN_SMG;
        else if(s!=GUN_PULSE && d->ammo[GUN_PULSE])     s = GUN_PULSE;
        else if(s!=GUN_RL && d->ammo[GUN_RL])     s = GUN_RL;
        else if(s!=GUN_RAIL && d->ammo[GUN_RAIL])  s = GUN_RAIL;
        gunselect(s, d);
    }

    ICOMMAND(weapon, "V", (tagval *args, int numargs),
    {
        if(player1->state!=CS_ALIVE) return;
        loopi(3)
        {
            const char *name = i < numargs ? args[i].getstr() : "";
            if(name[0])
            {
                int gun = getweapon(name);
                if(validgun(gun) && gun != player1->gunselect && player1->ammo[gun]) { gunselect(gun, player1); return; }
            } else { weaponswitch(player1); return; }
        }
        playsound(S_NOAMMO);
    });

    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest)
    {
        vec offset;
        do offset = vec(rndscale(1), rndscale(1), rndscale(1)).sub(0.5f);
        while(offset.squaredlen() > 0.5f*0.5f);
        offset.mul((to.dist(from)/1024)*spread);
        offset.z /= 2;
        dest = vec(offset).add(to);
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
        }
    }

    void createrays(int atk, const vec &from, const vec &to)             // create random spread of rays
    {
        loopi(attacks[atk].rays) offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i]);
    }

    enum { BNC_GRENADE, BNC_ROCKET, BNC_GIB1, BNC_GIB2 };

    struct bouncer : physent
    {
        int lifetime, bounces;
        float lastyaw, roll, gravity, elasticity;
        bool local;
        gameent *owner;
        int bouncetype, variant;
        vec offset;
        int offsetmillis;
        float offsetheight;
        int id;
        int atk;
        int lastbounce, bouncesound, bouncerloopchan, bouncerloopsound;

        bouncer() : bounces(0), roll(0), gravity(0.8f), elasticity(0.6f), variant(0), lastbounce(0), bouncesound(-1), bouncerloopchan(-1), bouncerloopsound(-1)
        {
            type = ENT_BOUNCE;
        }
        ~bouncer()
        {
            if(bouncerloopchan >= 0) stopsound(bouncerloopsound, bouncerloopchan, 100);
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
            if(bouncetype >= BNC_GRENADE && bouncetype <= BNC_ROCKET && offsetmillis > 0 && offset.z < 0)
                offsetheight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
            else offsetheight = -1;
        }
    };

    vector<bouncer *> bouncers;

    void newbouncer(const vec &from, const vec &to, bool local, int id, gameent *owner, int atk, int type, int lifetime, int speed, float gravity = 0.8f, float elasticity = 0.6f)
    {
        bouncer &bnc = *bouncers.add(new bouncer);
        bnc.o = from;
        bnc.radius = bnc.xradius = bnc.yradius = 1.4f; //type==BNC_DEBRIS ? 0.5f : 1.5f;
        bnc.eyeheight = bnc.radius;
        bnc.aboveeye = bnc.radius;
        bnc.lifetime = lifetime;
        bnc.gravity = gravity;
        bnc.elasticity = elasticity;
        bnc.local = local;
        bnc.owner = owner;
        bnc.atk = atk;
        bnc.bouncetype = type;
        bnc.id = local ? lastmillis : id;

        switch(type)
        {
            case BNC_GRENADE:
            {
                bnc.collidetype = COLLIDE_ELLIPSE;
                bnc.bouncesound = S_GRENADE_BOUNCE;
                break;
            }
            case BNC_ROCKET:
            {
                bnc.collidetype = COLLIDE_ELLIPSE;
                bnc.bouncesound = S_ROCKET_BOUNCE;
                break;
            }
            case BNC_GIB1: bnc.variant = rnd(5); break;
            //case BNC_DEBRIS: bnc.variant = rnd(4); break;
        }

        vec dir(to);
        dir.sub(from).safenormalize();
        bnc.vel = dir;
        bnc.vel.mul(speed);

        avoidcollision(&bnc, dir, owner, 0.1f);

        if(type>=BNC_GRENADE && type<=BNC_ROCKET)
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
        if(lastmillis-b->lastbounce < 200) return;
        if(!(lookupmaterial(b->o)&MAT_WATER))
        {
            if(b->bouncesound >= 0 && b->vel.magnitude() > 5.0f)
                playsound(b->bouncesound, NULL, &b->o);
        }
        if(blood && b->bouncetype == BNC_GIB1 && b->bounces <= 2 && goreeffect <= 0)
            addstain(STAIN_BLOOD, vec(b->o).sub(vec(surface).mul(b->radius)), surface, 2.96f/b->bounces, (b->owner->zombie ? bvec(0xFF, 0x60, 0xFF) : bvec(0x60, 0xFF, 0xFF)), rnd(4));
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
                case BNC_GIB1:
                {
                    if(blood && goreeffect <= 0 && bnc.vel.magnitude() > 30.0f)
                        regular_particle_splash(PART_BLOOD1, 0+rnd(4), 400, pos, bnc.owner->bloodcolour(), 0.80f, 25);
                    break;
                }

                case BNC_GRENADE:
                case BNC_ROCKET:
                {
                    if(bnc.vel.magnitude() > 10.0f) regular_particle_splash(PART_SMOKE, 5, 200, pos, 0x555555, 1.60f, 10, 500);
                    break;
                }
            }
            if(bnc.bouncerloopsound >= 0) bnc.bouncerloopchan = playsound(bnc.bouncerloopsound, NULL, &pos, NULL, 0, -1, 100, bnc.bouncerloopchan);
            vec old(bnc.o);
            bool destroyed = false;
            if(bnc.bouncetype>=BNC_GIB1 && bnc.bouncetype <= BNC_GIB2)
            {
                // cheaper variable rate physics for debris, gibs, etc.
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((bnc.lifetime -= qtime)<0 || bounce(&bnc, qtime/1000.0f, 0.5f, 0.4f, 0.7f)) { destroyed = true; break; }
                }
            }
            else if(bnc.bouncetype >= BNC_GRENADE && bnc.bouncetype <= BNC_ROCKET)
            {
                destroyed = bounce(&bnc, bnc.elasticity, 0.5f, bnc.gravity) || (bnc.lifetime -= time)<0 || isdeadly(lookupmaterial(bnc.o)&MAT_LAVA) ||
                            (bnc.bouncetype == BNC_GRENADE && bnc.bounces >= 1);
            }
            if(destroyed)
            {
                if(bnc.bouncetype >= BNC_GRENADE && bnc.bouncetype <= BNC_ROCKET)
                {
                    int damage = attacks[bnc.atk].damage*(bnc.owner->damagemillis||bnc.owner->juggernaut?2:1);
                    hits.setsize(0);
                    explode(bnc.local, bnc.owner, bnc.o, bnc.vel, NULL, damage, bnc.atk);
                    addstain(STAIN_PULSE_SCORCH, bnc.o, vec(0, 0, 1), attacks[bnc.atk].exprad);
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

    void clearbouncers() { bouncers.deletecontents(); }

   enum { PROJ_PULSE = 0, PROJ_ENERGY, PROJ_ROCKET };

    struct projectile
    {
        vec dir, o, from, to, offset;
        float speed;
        gameent *owner;
        int atk;
        bool local, destroyed;
        int offsetmillis;
        int id, lifetime;
        int projtype;
        int projchan, projsound;

        projectile() : projchan(-1), projsound(-1)
        {
            destroyed = false;
        }
        ~projectile()
        {
            if(projchan >= 0) stopsound(projsound, projchan, 100);
            projsound = projchan = -1;
        }

    };
    vector<projectile> projs;

    void clearprojectiles() { projs.shrink(0); }

    void newprojectile(int type, const vec &from, const vec &to, float speed, bool local, int id, int lifetime, gameent *owner, int atk)
    {
        projectile &p = projs.add();
        p.projtype = type;
        p.dir = vec(to).sub(from).safenormalize();
        p.o = from;
        p.from = from;
        p.to = to;
        p.offset = hudgunorigin(attacks[atk].gun, from, to, owner);
        p.offset.sub(from);
        p.speed = speed;
        p.local = local;
        p.owner = owner;
        p.atk = atk;
        p.offsetmillis = OFFSETMILLIS;
        p.id = local ? lastmillis : id;
        p.lifetime = lifetime;
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

    void damageeffect(int damage, dynent *d, vec &p, int atk, bool thirdperson)
    {
        if(!damage) return;
        gameent *f = (gameent *)d, *h = hudplayer();
        vec o = d->o;
        o.z -= d->eyeheight/3;
        if(f==h) p = o;
        if(blood)
        {
            particle_splash(PART_BLOOD1, max(damage/12, rnd(3)+1), 160, p, f->bloodcolour(), 0.01f, 100, 0, 0.09f);
            particle_splash(PART_BLOOD2, damage, 200, p, f->bloodcolour(), 0.40f, 100, 3);
        }
        if(f->health > 0 && lastmillis-f->lastyelp > 600)
        {
            if(f==hudplayer()) damageblend(damage);
            else if(f->shield) playsound(S_SHIELD, NULL, &f->o);
            playsound(f->painsound(), f, &f->o);
            f->lastyelp = lastmillis;
        }
        if(f->shield && d!=hudplayer()) particle_splash(PART_SPARK2, 5, 100, p, 0xFFFF66, 0.40f, 200);
        if(validatk(atk) && attacks[atk].hitsound > 0) playsound(attacks[atk].hitsound, NULL, f==h ? NULL : &f->o);
        else if(validsatk(atk) && atk != ATK_TELEPORT) playsound(S_MELEE_HIT2, NULL, f==h ? NULL : &f->o);
        else
        {
            playsound(S_DAMAGE, NULL, f==h ? NULL : &f->o);
            return;
        }
        if(f->armourmillis) playsound(S_ARMOUR_ACTION, f, &f->o);
    }

    void spawnbouncer(const vec &p, const vec &vel, gameent *d, int type)
    {
        vec to(rnd(100)-50, rnd(100)-50, rnd(100)-50);
        if(to.iszero()) to.z += 1;
        to.normalize();
        to.add(p);
        newbouncer(p, to, true, 0, d, NULL, type, rnd(1000)+1000, rnd(100)+20);
    }

    void gibeffect(int damage, const vec &vel, gameent *d)
    {
        if(!gore) return;
        vec from = d->abovehead();
        if(goreeffect <= 0)
        {
            loopi(min(damage, 8)+1) spawnbouncer(from, vel, d, BNC_GIB1);
            spawnbouncer(d->headpos(), vel, d, BNC_GIB2);
            if(blood)
            {
                particle_splash(PART_BLOOD1, 3, 180, d->o, d->bloodcolour(), 3.0f+rndscale(5.0f), 150, 0, 0.1f);
                particle_splash(PART_BLOOD2, damage, 300, d->o, d->bloodcolour(), 0.89f, 300, 5);
            }
        }
        msgsound(S_GIB, d);
    }

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2 = 1, int flags = HIT_TORSO)
    {
        gameent *f = (gameent *)d;

        int dam = damage;
        if((!selfdam && f==at) || (!teamdam && isally(f, at))) dam = 0;
        if(f!=at && isally(f, at)) dam = max(dam/2, 1);
        if(dam > 0)
        {
            f->lastpain = lastmillis;
            if(at->type==ENT_PLAYER && f!=at && !isally(f, at)) at->totaldamage += dam;
            if(at==player1 && d!=at)
            {
                extern int hitsound;
                if(hitsound && at->lasthit != lastmillis)
                    playsound(isally(f, at) ? S_HIT_ALLY : (hitsound == 1 ? S_HIT1 : S_HIT2));
                at->lasthit = lastmillis;
            }
        }

        if(!m_mp(gamemode) || f==at) f->hitpush(damage, vel, at, atk);
        if(!m_mp(gamemode))
        {
            damaged(dam, f->o, f, at, atk, flags);
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
            if(at==player1 && dam > 0)
            {
                if(f==player1)
                {
                    if(lastmillis-f->lastyelp > 500) damageblend(dam);
                    damagecompass(dam, at ? at->o : f->o);
                }
                if(f->invulnmillis && f!=at && !at->invulnmillis) playsound(S_INVULNERABILITY_ACTION, f);
                if(flags & HIT_HEAD)
                {
                    extern int playheadshotsound;
                    if(playheadshotsound) playsound(S_HEAD_HIT, NULL, &f->o);
                }
            }
        }
    }

    void hitpush(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays, int flags)
    {
        gameent *f = (gameent *)d;
        if(f->armourmillis || f->juggernaut) damage /= 2;
        if(f->invulnmillis && f!=at && !at->invulnmillis) damage = 0;
        if(flags&HIT_HEAD && m_mayhem(mutators)) damage = f->health;
        hit(damage, d, at, vec(to).sub(from).safenormalize(), atk, from.dist(to), rays, flags);
    }

    float projdist(dynent *o, vec &dir, const vec &v, const vec &vel, int atk)
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
        if(server::betweenrounds || o->state!=CS_ALIVE) return;
        vec dir;
        float dist = projdist(o, dir, v, vel, atk);
        if(dist<attacks[atk].exprad)
        {
            float dam = damage*(1-dist/EXP_DISTSCALE/attacks[atk].exprad);
            if(o==at) damage /= EXP_SELFDAMDIV;
            if(damage > 0)
            {
                gameent *f = (gameent *)o;
                if(f->armourmillis || f->juggernaut) damage /= 2;
                if(f->invulnmillis && f!=at && !at->invulnmillis) damage = 0;
                hit(dam, o, at, dir, atk, dist);
                damageeffect(dam, o, o->o, atk);
            }
        }
    }

    void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int damage, int atk)
    {
        vec debrisorigin = vec(v).sub(vec(vel).mul(5));
        vec dir = vec(owner->o).sub(v).safenormalize(), p = vec(v).madd(dir, 10), dynlight = vec(1.0f, 3.0f, 4.0f);
        int fireball = 0x50CFE5;
        switch(atk)
        {
            case ATK_PULSE1:
            {
                dynlight = vec(2.0f, 1.5f, 2.0f);
                fireball = 0xEE88EE;
                particle_splash(PART_SPARK2, 5+rnd(20), 200, p, 0xEE88EE, 0.08f+rndscale(0.35f), 300, 2);
                playsound(S_PULSE_EXPLODE, NULL, &v);
                break;
            }
            case ATK_RL1:
            case ATK_RL2:
            case ATK_SG2:
            {
                dynlight = vec(2, 1.5f, 1);
                fireball = 0xC8E66B;
                particle_splash(PART_SPARK1, 30, 130, p, 0xFFC864, 8.0f, 150, 100);
                particle_splash(PART_SPARK2, 10, 200, p, 0xFFC864, 0.10f+rndscale(0.35f), 500, 2);
                particle_splash(PART_SMOKE, 30, 200, p, 0x444444, 8.8f, 200, 200, 0.01f);
                playsound(S_ROCKET_EXPLODE, NULL, &v);
                break;
            }
            case ATK_PISTOL2:
            {
                dynlight = vec(0, 1.5f, 1.5f);
                fireball = 0x00FFFF;
                particle_splash(PART_SPARK2, 0+rnd(20), 200, p, 0x00FFFF, 0.05f+rndscale(0.10f), 200, 5);
                playsound(S_PULSE_HIT);
                break;
            }
            default: break;
        }
        particle_fireball(v, 1.15f*attacks[atk].exprad, PART_PULSE_BURST, 300, fireball, 0.10f);
        adddynlight(safe ? v : debrisorigin, 2*attacks[atk].exprad, dynlight, 350, 40, 0, attacks[atk].exprad/2, vec(0.5f, 1.5f, 2.0f));
        if(lookupmaterial(v)==MAT_WATER) playsound(S_WATER_IMPACT_PROJ, NULL, &v);
        /*
        int numdebris = maxdebris > MINDEBRIS ? rnd(maxdebris-MINDEBRIS)+MINDEBRIS : min(maxdebris, MINDEBRIS);
        if(numdebris)
        {
            vec debrisvel = vec(vel).neg();
            loopi(numdebris)
                spawnbouncer(debrisorigin, debrisvel, owner, BNC_DEBRIS);
        }
        */
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
        if(p.atk == ATK_PULSE1) addstain(STAIN_PULSE_GLOW, pos, dir, rad, 0xEE88EE);
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
            case ATK_PULSE1:
            case ATK_RL1:
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
            case ATK_SG2:
            case ATK_RL2:
                loopv(bouncers)
                {
                    bouncer &b = *bouncers[i];
                    if(b.bouncetype < BNC_GRENADE && b.bouncetype > BNC_ROCKET) break;
                    if(b.owner == d && b.id == id && !b.local)
                    {
                        vec pos(b.o);
                        pos.add(vec(b.offset).mul(b.offsetmillis/float(OFFSETMILLIS)));
                        explode(b.local, b.owner, pos, b.vel, NULL, 0, atk);
                        vec dir = vec(b.vel).neg();
                        addstain(STAIN_PULSE_SCORCH, pos, dir, attacks[atk].exprad*0.75f);
                        delete bouncers.remove(i);
                        break;
                    }
                }
            default:
                break;
        }
    }

    bool projdamage(dynent *o, projectile &p, const vec &v, int damage)
    {
        if(server::betweenrounds || o->state!=CS_ALIVE) return false;
        if(!intersect(o, p.o, v, attacks[p.atk].margin)) return false;
        projsplash(p, v, o, damage);
        vec dir;
        projdist(o, dir, v, p.dir, p.atk);
        gameent *f = (gameent *)o;
        if(f->armourmillis || f->juggernaut) damage /= 2;
        if(f->invulnmillis && f!=p.owner && !p.owner->invulnmillis) damage = 0;
        hit(damage, o, p.owner, dir, p.atk, 0);
        damageeffect(damage, o, o->o, p.atk);
        return true;
    }

    void updateprojectiles(int time)
    {
        if(projs.empty()) return;
        gameent *noside = hudplayer();
        loopv(projs)
        {
            projectile &p = projs[i];
            p.offsetmillis = max(p.offsetmillis-time, 0);
            vec dv;
            float dist = p.to.dist(p.o, dv);
            float damage = attacks[p.atk].damage*(p.owner->damagemillis || p.owner->juggernaut ? 2 : 1);
            dv.mul(time/max(dist*1000/p.speed, float(time)));
            vec v = vec(p.o).add(dv);
            bool exploded = false;
            hits.setsize(0);
            if(p.local)
            {
                vec halfdv = vec(dv).mul(0.5f), bo = vec(p.o).add(halfdv);
                float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[p.atk].margin;
                loopj(numdynents())
                {
                    dynent *o = iterdynents(j);
                    if(p.owner==o || o->o.reject(bo, o->radius + br)) continue;
                    if(projdamage(o, p, v, damage)) { exploded = true; break; }
                }
            }
            if(!exploded)
            {
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((p.lifetime -= qtime)<0)
                    {
                        projsplash(p, v, NULL, damage);
                        exploded = true;
                    }
                }
                if(dist<4)
                {
                    if(p.o!=p.to) // if original target was moving, reevaluate endpoint
                    {
                        if(raycubepos(p.o, p.dir, p.to, 0, RAY_CLIPMAT|RAY_ALPHAPOLY)>=4) continue;
                    }
                    projsplash(p, v, NULL, damage);
                    exploded = true;
                }
                else
                {
                    vec pos = vec(p.offset).mul(p.offsetmillis/float(OFFSETMILLIS)).add(v);
                    int tailc = 0xFFFFFF, tails = 2.5f;
                    switch(p.projtype)
                    {
                        case PROJ_ROCKET:
                        {
                            tailc = 0xFFC864; tails = 1.5f;
                            regular_particle_splash(PART_SMOKE, 3, 300, pos, 0x303030, 2.4f, 50, -20);
                            if(p.lifetime<=attacks[p.atk].lifetime/2) regular_particle_splash(PART_SPARK1, 4, 180, pos, tailc, tails, 10, 0);
                            particle_splash(PART_PULSE_FRONT, 1, 1, pos, tailc, tails, 150, 20);
                            p.projsound = S_ROCKET_LOOP;
                            break;
                        }

                        case PROJ_PULSE:
                        {
                            tailc = 0xDD88DD;
                            particle_splash(PART_PULSE_FRONT, 1, 1, pos, tailc, tails, 150, 20);
                            p.projsound = S_PULSE_LOOP;
                            break;
                        }

                        case PROJ_ENERGY:
                        {
                            tails = 5.0f; tailc = 0x00FFFF;
                            particle_splash(PART_PULSE_FRONT, 1, 1, pos, tailc, tails, 150, 20);
                            p.projsound = S_PULSE_LOOP;
                            break;
                        }
                    }
                    if(p.owner != noside)
                    {
                        float len = min(20.0f, vec(p.offset).add(p.from).dist(pos));
                        vec dir = vec(dv).normalize(),
                            tail = vec(dir).mul(-len).add(pos),
                            head = vec(dir).mul(2.4f).add(pos);
                        particle_flare(tail, head, 1, PART_PULSE_SIDE, tailc, tails);
                    }
                    p.projchan = playsound(p.projsound, NULL, &pos, NULL, 0, -1, 100, p.projchan);
                }
            }
            if(p.destroyed || lookupmaterial(p.o)&MAT_LAVA)
            {
                projsplash(p, v, NULL, damage);
                exploded = true;
            }
            if(exploded || lookupmaterial(p.o)&MAT_LAVA)
            {
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
            case ATK_RAIL:
            case ATK_INSTA:
            {
                bool insta=atk==ATK_INSTA;
                adddynlight(vec(to).madd(dir, 4), 20, !insta? vec(0.25f, 1.0f, 0.75f):  vec(0.25f, 0.75f, 1.0f), 380, 75, DL_SHRINK);
                if(hit || water || glass) break;
                particle_splash(PART_SPARK1, 80, 80, to, !insta? 0x77DD77: 0x50CFE5, 1.25f, 100, 80);
                particle_splash(PART_SPARK2, 5+rnd(20), 200+rnd(380), to, !insta? 0x77DD77: 0x50CFE5, 0.1f+rndscale(0.3f), 200, 3);
                particle_splash(PART_SMOKE, 20, 180, to, 0x808080, 2.0f, 60, 80, 0.05f);
                addstain(STAIN_RAIL_HOLE, to, dir, 3.5f);
                addstain(STAIN_RAIL_GLOW, to, dir, 3.0f, !insta? 0x77DD77: 0x50CFE5);
                break;
            }

            case ATK_SG1:
            case ATK_SMG1:
            case ATK_SMG2:
            {
                adddynlight(vec(to).madd(dir, 4), 14, vec(0.5f, 0.375f, 0.25f), 140, 20);
                if(hit || water || glass) break;
                particle_splash(PART_SPARK1, 30, 50, to, 0xFFC864, 0.70f);
                particle_splash(PART_SPARK2, 0+rnd(6), 80+rnd(380), to, 0xFFC864, 0.05f+rndscale(0.09f), 250);
                particle_splash(PART_SMOKE, atk==ATK_SG1? 0+rnd(5): 5+rnd(10), 150, to, 0x606060, 1.8f+rndscale(2.2f), 100, 100, 0.01f);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(0.80f));
                break;

            }

            case ATK_PULSE2:
            {
                adddynlight(vec(to).madd(dir, 4), 20, vec(2.0f, 1.5f, 2.0f), 20);
                if(hit || water) break;
                particle_splash(PART_SPARK1, 10, 60, to, 0xDD88DD, 1.50f, 180, 6);
                particle_splash(PART_SPARK2, 0+rnd(5), 300, to, 0xEE88EE, 0.01f+rndscale(0.10f), 350, 2);
                if(from.dist(to) <= attacks[atk].range-1)
                    particle_splash(PART_SMOKE, 20, 120, to, 0x777777, 2.0f, 100, 80, 0.02f);
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 1.0f+rndscale(1.10f));
                playsound(attacks[atk].impactsound, NULL, &to);
                break;
            }

            case ATK_PISTOL1:
            {
                adddynlight(vec(to).madd(dir, 4), 10, vec(0, 1.5f, 1.5f), 200, 10, DL_SHRINK);
                particle_splash(PART_SPARK1, 10, 50, to, 0x00FFFF, 1.5f, 300, 50);
                if(hit || water || glass) break;
                particle_splash(PART_SPARK2, 0+rnd(10), 100+rnd(280), to, 0x00FFFF, 0.01f+rndscale(0.18f), 300, 2);
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 0.80f+rndscale(1.0f));
                break;
            }

            default: break;
        }
        if(attacks[atk].action == ACT_MELEE || atk == ATK_PULSE2 || hit) return;
        int impactsnd = attacks[atk].impactsound;
        if(water)
        {
            addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(0.80f));
            impactsnd = S_WATER_IMPACT;

        }
        else if(glass)
        {
            particle_splash(PART_GLASS, 20, 200, to, 0xFFFFFF, 0.10+rndscale(0.20f));
            addstain(STAIN_GLASS_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(1.0f));
            impactsnd = S_GLASS_IMPACT;
        }
        if(!(attacks[atk].rays > 1 && d==hudplayer()) && impactsnd) playsound(impactsnd, NULL, &to);
    }

    VARP(muzzleflash, 0, 1, 1);

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun, sound = attacks[atk].sound;
        float dist = from.dist(to);
        vec up = to;
        switch(atk)
        {
            case ATK_PULSE1:
                if(muzzleflash && d->muzzle.x >= 0)
                    particle_flare(d->muzzle, d->muzzle, 115, PART_PULSE_MUZZLE_FLASH, 0xDD88DD, 3.0f, d);
                newprojectile(PROJ_PULSE, from, to, attacks[atk].projspeed, local, id, attacks[atk].lifetime, d, atk);
                break;

            case ATK_PULSE2:
            {
                if(muzzleflash)
                {
                     if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 80, PART_PULSE_MUZZLE_FLASH, 0xDD88DD, 4.0f, d);
                     adddynlight(hudgunorigin(gun, d->o, to, d), 35, vec(2.0f, 1.5f, 2.0f), 80, 10, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(to, from, 60, PART_LIGHTNING, 0xDD88DD, 0.80f, d);
                particle_flare(to, from, 80, PART_LIGHTNING, 0xCC88CC, 1.10f, d);
                particle_splash(PART_SPARK1, 20, 200, to, 0xEE88EE, 0.45f);
                if(lastmillis-prevaction>280) particle_fireball(to, 6.0f, PART_PULSE_BURST, 200, 0xDD88DD, 1.0f);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_RAIL:
            {
                if(muzzleflash)
                {
                    if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 150, PART_RAIL_MUZZLE_FLASH, 0x77DD77, 0.1f, d, 0.3f);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 50, vec(1.20f, 2.0f, 1.20f), 150, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, PART_STREAK, 0x55DD55, 0.80f);
                particle_trail(PART_STEAM, 300, hudgunorigin(attacks[atk].gun, from, to, d), to, 0x66DD66, 0.2f, 0);
                particle_trail(PART_SMOKE, 140, hudgunorigin(attacks[atk].gun, from, to, d), to, 0x808080, 0.60f, 50);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_RL1:

                if(muzzleflash && d->muzzle.x >= 0)
                    particle_flare(d->muzzle, d->muzzle, 140, PART_PULSE_MUZZLE_FLASH, 0xEFE898, 0.20f, d, 0.4f);
                newprojectile(PROJ_ROCKET, from, to, attacks[atk].projspeed, local, id, attacks[atk].lifetime, d, atk);
                break;

            case ATK_RL2:
                up.z += dist/8;
                newbouncer(from, up, local, id, d, atk, BNC_ROCKET, attacks[atk].lifetime, attacks[atk].projspeed, 0.8f, 0.7f);
                break;

            case ATK_SG1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 70, PART_PULSE_MUZZLE_FLASH, 0xEFE598, 5.2f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.5f, 0.375f, 0.25f), 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(!local)
                {
                    createrays(atk, from, to);
                    loopi(attacks[atk].rays) rayhit(atk, d, from, rays[i], hit);
                }
                break;
            }

            case ATK_SG2:
                up.z += dist/16;
                newbouncer(from, up, local, id, d, atk, BNC_GRENADE, attacks[atk].lifetime, attacks[atk].projspeed, 1.0f, 0);
                break;

            case ATK_SMG1:
            case ATK_SMG2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 80, PART_RAIL_MUZZLE_FLASH, 0xEFE898, 5.0f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 40, vec(0.5f, 0.375f, 0.25f), atk==ATK_SMG1 ? 70 : 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(atk == ATK_SMG2) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_STREAK, 0xFFC864, 0.80f);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_PISTOL1:
            case ATK_PISTOL2:
            {
                if(muzzleflash)
                {
                   if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 50, PART_PULSE_MUZZLE_FLASH, 0x00FFFF, 4.0f, d);
                   adddynlight(hudgunorigin(attacks[atk].gun, d->o, to, d), 30, vec(0, 1.5f, 1.5f), 60, 20, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(atk == ATK_PISTOL2)
                {
                    newprojectile(PROJ_ENERGY, from, to, attacks[atk].projspeed, local, id, attacks[atk].lifetime, d, atk);
                    break;
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_STREAK, 0x00FFFF, 2.0f);
                particle_trail(PART_SPARK1, 12, hudgunorigin(attacks[atk].gun, from, to, d), to, 0x650ffc, 0.30f, 3);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_INSTA:

                if(muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 100, PART_RAIL_MUZZLE_FLASH, 0x50CFE5, 2.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 35, vec(0.25f, 0.75f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, PART_RAIL_TRAIL, 0x50CFE5, 1.0f);
                break;

            default: break;
        }
        bool looped = false;
        if(d->attacksound >= 0 && d->attacksound != sound) d->stopweaponsound();
        if(d->idlesound >= 0) d->stopidlesound();
        switch(sound)
        {
            case S_PULSE2A:
            {
                if(d->attacksound >= 0) looped = true;
                d->attacksound = sound;
                d->attackchan = playsound(sound, NULL, &d->o, NULL, 0, -1, 100, d->attackchan);
                if(lastmillis-prevaction>200 && !looped) playsound(S_PULSE2B, d);
                break;
            }
            case S_SG1A:
            case S_SG2:
            {
                playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
                if(d==hudplayer()) playsound(S_SG1B, d);
                break;
            }
            default: playsound(sound, NULL, d==hudplayer() ? NULL : &d->o);
        }
        if(lastmillis-prevaction>200 && !looped)
        {
            if(d->juggernaut)
            {
                playsound(S_JUGGERNAUT_ACTION, d);
                return;
            }
            if(d->damagemillis) playsound(S_DDAMAGE_ACTION, d);
            if(d->hastemillis)  playsound(S_HASTE_ACTION, d);
            if(d->ammomillis) playsound(S_UAMMO_ACTION, d);
        }
    }

    void particletrack(physent *owner, vec &o, vec &d)
    {
        if(owner->type!=ENT_PLAYER) return;
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
        if(owner->type!=ENT_PLAYER) return;
        gameent *pl = (gameent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattack < 0 || attacks[pl->lastattack].gun != pl->gunselect) return;
        o = pl->muzzle;
        hud = owner == hudplayer() ? vec(pl->o).add(vec(0, 0, 2)) : pl->muzzle;
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

    void hitscan(vec &from, vec &to, gameent *d, int atk)
    {
        int maxrays = attacks[atk].rays;
        if(server::betweenrounds)
        {
            if(maxrays > 1) { loopi(maxrays) rayhit(atk, d, from, rays[i]); }
            else rayhit(atk, d, from, to);
            return;
        }
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
                hitpush(numhits*damage, o, d, from, to, atk, numhits, flags);
                damageeffect(damage, o, rays[i], atk);
            }
        }
        else if((o = intersectclosest(from, to, d, margin, dist)))
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
            hitpush(attacks[atk].damage, o, d, from, to, atk, 1, flags);
            damageeffect(damage, o, to, atk);
        }
        else //if(attacks[atk].action==ACT_MELEE)
        {
            rayhit(atk, d, from, to);
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
            if(d==player1)
            {
                msgsound(S_NOAMMO, d);
                d->gunwait = 600;
                d->lastattack = -1;
                if(!d->ammo[gun]) weaponswitch(d);
            }
            return;
        }
        if(!d->ammomillis && !d->juggernaut) d->ammo[gun] -= attacks[atk].use;

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

        if(attacks[atk].rays > 1) createrays(atk, from, to);
        else if(attacks[atk].spread) offsetray(from, to, attacks[atk].spread, attacks[atk].range, to);

        hits.setsize(0);

        if(!attacks[atk].projspeed) hitscan(from, to, d, atk);

        shoteffects(atk, from, to, d, true, 0, prevaction);

        if(d==player1 || d->ai)
        {
            addmsg(N_SHOOT, "rci2i6iv", d, lastmillis-maptime, atk,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        }

        int gunwait = attacks[atk].attackdelay;
        if(d->hastemillis || d->juggernaut) gunwait /= 2;
        d->gunwait = gunwait;
        if(attacks[atk].action != ACT_MELEE && d->ai) d->gunwait += int(d->gunwait*(((101-d->skill)+rnd(111-d->skill))/100.f));
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
    }

    void specialattack(gameent *d, int atk, vec from, const vec &targ)
    {
        vec to = targ, dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        float shorten = attacks[atk].range && dist > attacks[atk].range ? attacks[atk].range : 0,
              barrier = raycube(d->o, dir, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
        if(barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
            shorten = barrier;
        if(shorten) to = vec(dir).mul(shorten).add(from);

        if(attacks[atk].rays > 1) createrays(atk, from, to);
        else if(attacks[atk].spread) offsetray(from, to, attacks[atk].spread, attacks[atk].range, to);

        hits.setsize(0);

        if(!attacks[atk].projspeed) hitscan(from, to, d, atk);

        if(d==player1 || d->ai)
        {
            addmsg(N_SPECIALATK, "rci2i6iv", d, lastmillis-maptime, atk,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        }
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

                case PROJ_ENERGY:
                {
                    adddynlight(pos, 20, vec(0, 1.50f, 1.50f));
                    break;
                }
            }
        }
        loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            if(bnc.bouncetype<BNC_GRENADE && bnc.bouncetype>BNC_ROCKET) continue;
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
        }
    }

    static const char * const gibnames[5] = { "gib/variant/gib01", "gib/variant/gib02", "gib/variant/gib03", "gib/variant/gib04", "gib/variant/gib05" };
    static const char * const fruitnames[5] = { "gib/fruit/apple", "gib/fruit/appleslice", "gib/fruit/pear", "gib/fruit/apple", "gib/fruit/appleslice" };
    static const char * const gunnames[5] = { "item/ammo/shells", "item/ammo/bullets", "worldgun/pulserifle", "item/ammo/rockets", "item/ammo/rrounds" };
    //static const char * const debrisnames[4] = { "debris/debris01", "debris/debris02", "debris/debris03", "debris/debris04" };

    void preloadbouncers()
    {
        loopi(sizeof(gibnames)/sizeof(gibnames[0])) preloadmodel(gibnames[i]);
        loopi(sizeof(fruitnames)/sizeof(fruitnames[0])) preloadmodel(fruitnames[i]);
        //loopi(sizeof(debrisnames)/sizeof(debrisnames[0])) preloadmodel(debrisnames[i]);
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
            if(bnc.bouncetype >= BNC_GIB1 && bnc.bouncetype <= BNC_GIB2)
            {
                float fade = 1;
                if(bnc.lifetime < 400) fade = bnc.lifetime/400.0f;
                const char *gib = goreeffect <= 0 ? gibnames[bnc.variant] : fruitnames[bnc.variant];
                switch(bnc.bouncetype)
                {
                    case BNC_GIB1: mdl = gib; break;
                    case BNC_GIB2: mdl = "gib/head"; break;
                    default: continue;
                }
                rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, 0, cull, NULL, NULL, 0, 0, fade);
            }
            else
            {
                switch(bnc.bouncetype)
                {
                    case BNC_GRENADE: mdl = "projectile/grenade"; break;
                    case BNC_ROCKET: mdl = "projectile/rocket"; break;
                    default: mdl = "projectile/grenade01"; break;
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
            yaw += 90;
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
            case S_PULSE2A:
                atk = ATK_PULSE2;
                break;
            default:
                return;
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
        if(player1->clientnum>=0 && player1->state==CS_ALIVE) shoot(player1, worldpos); // only shoot when connected to server
        updatebouncers(curtime); // need to do this after the player shoots so bouncers don't end up inside player's BB next frame
        gameent *following = followingplayer();
        if(!following) following = player1;
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
            if(bnc.bouncetype <= BNC_GRENADE && bnc.bouncetype >= BNC_ROCKET) continue;
            obstacles.avoidnear(NULL, bnc.o.z + attacks[bnc.atk].exprad + 1, bnc.o, radius + attacks[bnc.atk].exprad);
        }
    }
};

