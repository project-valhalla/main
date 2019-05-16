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
    VARP(showexplosionradius, 0, 0, 1);
    VARP(explosioneffect, 0, 1, 1);
    VARP(teamcoloureffects, 0, 0, 1);

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

    enum { BNC_GRENADE1, BNC_GRENADE2, BNC_GRENADE3, BNC_MINE, BNC_ROCKET, BNC_GIB1, BNC_GIB2 };

    struct bouncer : physent
    {
        int lifetime, bounces;
        float lastyaw, roll, gravity, elasticity;
        bool local, destroyed;
        gameent *owner;
        int bouncetype, variant;
        vec offset;
        int offsetmillis;
        int id;
        int atk;
        int bouncesound, lastbounce;
        int bouncerchan, bncsound;

        bouncer() : bounces(0), roll(0), gravity(0.8f), elasticity(0.6f), variant(0), bouncesound(-1), lastbounce(0), bouncerchan(-1), bncsound(-1)
        {
            type = ENT_BOUNCE;
            destroyed = false;
        }
        ~bouncer()
        {
            if(bouncerchan >= 0) stopsound(bncsound, bouncerchan, 100);
            bncsound = bouncerchan = -1;
        }
    };

    vector<bouncer *> bouncers;

    void newbouncer(const vec &from, const vec &to, bool local, int id, gameent *owner, int atk, int type, int lifetime, int speed, float gravity = 0.8f, float elasticity = 0.6f)
    {
        bouncer &bnc = *bouncers.add(new bouncer);
        bnc.o = from;
        bnc.radius = bnc.xradius = bnc.yradius = 1.5f; //type==BNC_DEBRIS ? 0.5f : 1.5f;
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
            case BNC_GRENADE1:
            case BNC_GRENADE2:
            case BNC_GRENADE3:
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
            case BNC_MINE: bnc.collidetype = COLLIDE_ELLIPSE;
            case BNC_GIB1: bnc.variant = rnd(5); break;
            //case BNC_DEBRIS: bnc.variant = rnd(4); break;
        }

        vec dir(to);
        dir.sub(from).safenormalize();
        bnc.vel = dir;
        bnc.vel.mul(speed);

        avoidcollision(&bnc, dir, owner, 0.1f);

        if(type>=BNC_GRENADE1 && type<=BNC_ROCKET)
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
            vec check;
            switch(bnc.bouncetype)
            {
                case BNC_GIB1:
                {
                    if(blood && goreeffect <= 0 && bnc.vel.magnitude() > 30.0f)
                        regular_particle_splash(PART_BLOOD1, rnd(2) ? 0 : 4, 500, pos, bnc.owner->bloodcolour(), 0.80f, 25);
                    break;
                }

                case BNC_GRENADE1:
                case BNC_ROCKET:
                {
                    if(bnc.vel.magnitude() > 10.0f) regular_particle_splash(PART_SMOKE, 5, 200, pos, teamcoloureffects? teamtextcolor[bnc.owner->team] : 0x555555, 1.60f, 10, 500);
                    break;
                }

                case BNC_GRENADE2:
                case BNC_GRENADE3:
                {
                    if(bnc.vel.magnitude() > 20.0f) regular_particle_splash(PART_SPARK1, 10, 180, pos, teamcoloureffects? teamtextcolor[bnc.owner->team] : 0x202080, 0.70f, 2, 60);
                    break;
                }

                case BNC_MINE:
                {
                    if(bnc.vel.magnitude() > 20.0f) regular_particle_splash(PART_SPARK1, 10, 180, pos, teamcoloureffects? teamtextcolor[bnc.owner->team] : 0xEE88EE, 0.70f, 2, 60);
                    bnc.bncsound = S_PULSE3_LOOP;
                    break;
                }
            }
            if(bnc.bncsound >= 0) bnc.bouncerchan = playsound(bnc.bncsound, NULL, &pos, NULL, 0, -1, 100, bnc.bouncerchan);
            vec old(bnc.o);
            bool stopped = false;
            if(bnc.bouncetype>=BNC_GIB1 && bnc.bouncetype <= BNC_GIB2)
            {
                // cheaper variable rate physics for debris, gibs, etc.
                for(int rtime = time; rtime > 0;)
                {
                    int qtime = min(80, rtime);
                    rtime -= qtime;
                    if((bnc.lifetime -= qtime)<0 || bounce(&bnc, qtime/1000.0f, 0.5f, 0.4f, 0.7f)) { stopped = true; break; }
                }
            }
            else if(bnc.bouncetype >= BNC_GRENADE1 && bnc.bouncetype <= BNC_ROCKET)
            {
                switch(bnc.bouncetype)
                {
                    case BNC_GRENADE1:
                    {
                        bnc.destroyed = bnc.bounces >= 1;
                        break;
                    }
                    case BNC_GRENADE3:
                    {
                        if(bnc.bounces >= 1) bnc.gravity = 0;
                        break;
                    }
                    case BNC_MINE:
                    {
                        if(bnc.bounces >= 1) bnc.gravity = 0;
                        loopi(numdynents())
                        {
                            dynent *o = iterdynents(i);
                            if(o->state!=CS_ALIVE) break;
                            if(bnc.lifetime > 250 && o != bnc.owner && bnc.o.dist(o->o) < attacks[bnc.atk].exprad)
                            {
                                playsound(S_PULSE3_DETO, NULL, &bnc.o);
                                bnc.lifetime = 180;
                            }
                        }
                        break;
                    }
                    default: break;
                }
                stopped = bounce(&bnc, bnc.elasticity, 0.5f, bnc.gravity) || (bnc.lifetime -= time)<0;
            }
            int material = lookupmaterial(bnc.o);
            if((stopped || bnc.destroyed) || isdeadly(material&MAT_LAVA))
            {
                if(bnc.bouncetype >= BNC_GRENADE1 && bnc.bouncetype <= BNC_ROCKET)
                {
                    int damage = attacks[bnc.atk].damage*(bnc.owner->damagemillis?2:1);
                    hits.setsize(0);
                    explode(bnc.local, bnc.owner, bnc.o, bnc.vel, NULL, damage, bnc.atk);
                    addstain(STAIN_PULSE_SCORCH, bnc.o, vec(0, 0, 1), attacks[bnc.atk].exprad);
                    if(bnc.local)
                        addmsg(N_EXPLODE, "rci3iv", bnc.owner, lastmillis-maptime, bnc.atk, bnc.id-maptime,
                                                    hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                }
                stopsound(bnc.bncsound, bnc.bouncerchan);
                delete bouncers.remove(i--);
            }
            else
            {
                bnc.roll += old.sub(bnc.o).magnitude()/(4*RAD);
                bnc.offsetmillis = max(bnc.offsetmillis-time, 0);
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
        if(damage < 1 || server::betweenrounds || (validatk(atk) && m_headhunter(mutators) && attacks[atk].projspeed)) return;
        gameent *f = (gameent *)d;
        vec o = d->o, v = p;
        gameent *h = hudplayer();
        if(f==h)
        {
            o.z -= d->eyeheight/3;
            v = o;
        }
        if(blood)
        {
            particle_splash(PART_BLOOD1, max(damage/12, rnd(3)+1), 95, v, f->bloodcolour(), 0.01f, 150, 0, 0.2f);
            particle_splash(PART_BLOOD2, damage*12, 180, v, f->bloodcolour(), 0.40f, 100, 3);
            particle_splash(PART_BLOOD1, max(damage/10, rnd(3)+1), 1000, v, f->bloodcolour(), 1.96f, 150, 1);
        }
        if(f->health > 0 && lastmillis-f->lastyelp > 600)
        {
            if(f==hudplayer()) damageblend(damage);
            playsound(f->painsound(), f, &f->o);
            f->lastyelp = lastmillis;
        }
        if(validatk(atk) && attacks[atk].hitsound > 0) playsound(attacks[atk].hitsound, NULL, f==h ? NULL : &f->o);
        else if(validsatk(atk) && atk != ATK_TELEPORT) playsound(S_MELEE_HIT2, NULL, f==h ? NULL : &f->o);
        else
        {
            playsound(S_DAMAGE, NULL, f==h ? NULL : &f->o);
            return;
        }
        if(f->shield && d!=player1)
        {
            adddynlight(p, 35, vec(2, 1.5f, 1), 80, 30);
            particle_splash(PART_SPARK1, 10, 30, v, 0xFFFF44, 1.0f);
            particle_splash(PART_SPARK1, 20, 50, v, 0xFFFF55, 1.0f);
            particle_splash(PART_SPARK1, 10, 30, v, 0xFFFF66, 1.50f);
            if(!m_effic(mutators)) playsound(S_SHIELD, f, &f->o);
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
            if(blood && d->headless)
                particle_splash(PART_BLOOD2, damage/2, 280, d->headpos(), d->bloodcolour(), 0.2f, 350, 1, 0.02f);
            else
            {
                loopi(min(damage, 8)+1) spawnbouncer(from, vel, d, BNC_GIB1);
                spawnbouncer(d->headpos(), vel, d, BNC_GIB2);
                if(blood)
                {
                    particle_splash(PART_BLOOD1, max(damage/8, rnd(3)+1), 800, d->o, d->bloodcolour(), 3.0f);
                    particle_splash(PART_BLOOD1, 3, 180, d->o, d->bloodcolour(), 1.20f, 150, 0, 0.5f);
                    particle_splash(PART_BLOOD2, damage*2, 300, d->o, d->bloodcolour(), 0.89f, 300, 5);
                }
            }
        }
        else if(goreeffect >= 2)
        {
            loopi(min(damage, 8)+1) spawnbouncer(from, vel, d, BNC_GIB1);
            particle_splash(PART_BLOOD1, min(damage/8, 2), 180, d->o, 0x6050FFF, 0.15f, 80, 0, 0.4f);
            particle_splash(PART_BLOOD2, damage*2, 300, d->o, 0x6050FFF, 0.95f, 300, 5);
        }
        else
        {
            particle_fireball(d->o, 5.0f, PART_EXPLOSION, 400, getplayercolor(d, d->team), d->radius);
            particle_splash(PART_SPARK1, damage*6, 400, d->o, getplayercolor(d, d->team), 0.60f, 170, 8);
            particle_splash(PART_SPARK1, 2, 260, d->o, getplayercolor(d, d->team), 0.10f, 150, 0, 0.5f);
        }
        msgsound(S_GIB, d);
    }

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2 = 1, int flags = HIT_TORSO)
    {
        gameent *f = (gameent *)d;

        int dam = damage;
        if((m_round && server::betweenrounds) || (m_headhunter(mutators) && !(flags & HIT_HEAD)) || (!selfdam && f==at) ||
           (!teamdam && isally(f, at))) dam = 0;
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
                    f->headless = true;
                    if(!isally(f, at) && !m_headhunter(mutators))
                    {
                        if(validatk(atk))
                        {
                            extern int playheadshotsound;
                            if((playheadshotsound == 1 && attacks[atk].bonusdam) || (playheadshotsound > 1 && lastmillis-lastheadshot > 1000))
                                playsound(attacks[atk].action != ACT_MELEE? S_ANNOUNCER_HEADSHOT: S_ANNOUNCER_FACE_PUNCH, NULL, NULL, NULL, SND_ANNOUNCER);
                            lastheadshot = lastmillis;
                        }
                        playsound(S_HEAD_HIT, NULL, &f->o);
                    }
                }
                else f->headless = false;
            }
        }
    }

    void hitpush(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays, int flags)
    {
        if(server::betweenrounds) return;
        gameent *f = (gameent *)d;
        if(f->armourmillis) damage /= 2;
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
        if(o->state!=CS_ALIVE) return;
        vec dir;
        float dist = projdist(o, dir, v, vel, atk);
        if(dist<attacks[atk].exprad)
        {
            float dam = damage*(1-dist/EXP_DISTSCALE/attacks[atk].exprad);
            if(o==at) damage /= EXP_SELFDAMDIV;
            if(damage > 0)
            {
                gameent *f = (gameent *)o;
                if(f->armourmillis) damage /= 2;
                if(f->invulnmillis && f!=at && !at->invulnmillis) damage = 0;
                hit(dam, o, at, dir, atk, dist);
                damageeffect(dam, o, o->o, atk);
            }
        }
    }

    void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int damage, int atk)
    {
        int mat = lookupmaterial(v);
        vec debrisorigin = vec(v).sub(vec(vel).mul(5));
        vec dir = vec(owner->o).sub(v).safenormalize();
        vec p = vec(v).madd(dir, 10);
        int teamcolour = teamtextcolor[owner->team];
        if(explosioneffect) switch(atk)
        {
            case ATK_PULSE1:
            {
                particle_splash(PART_SPARK1, 200, 300, v, teamcoloureffects? teamcolour: 0xEE88EE, 0.45f);
                particle_fireball(v, 1.15f*attacks[atk].exprad, PART_PULSE_BURST, int(attacks[atk].exprad*20), teamcoloureffects? teamcolour: 0xEE88EE, 0.10f);
                particle_flare(v, v, 180, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xEE88EE, 0.40f, NULL, 0.7f);
                adddynlight(safe ? v : debrisorigin, 2*attacks[atk].exprad, vec(1.7f, 1, 1.8f), 350, 40, 0, attacks[atk].exprad/2, vec(1.5f, 0.5f, 2.0f));
                playsound(S_PULSE_EXPLODE, NULL, &v);
                break;
            }
            case ATK_PULSE3:
            {
                particle_splash(PART_SPARK1, attacks[atk].exprad*3, 120, p, teamcoloureffects? teamcolour: 0xEE88EE, 4.80f, 200, 3, 0.02f);
                particle_splash(PART_SPARK1, 5, 180, p, teamcoloureffects? teamcolour: 0xEE77EE, 0.08f, 100, 200, 0.50f);
                particle_splash(PART_SPARK1, 100, 80, p, teamcoloureffects? teamcolour: 0xEE66EE, 5.20f, 60, 80);
                particle_splash(mat&MAT_WATER ? PART_STEAM : PART_SMOKE, 160, 120, v, 0x666666, 6, 500, 200, 0.5f);
                particle_fireball(v, 1.0f*attacks[atk].exprad, PART_PULSE_BURST, int(attacks[atk].exprad*20), teamcoloureffects? teamcolour: 0xEE88EE, 0.8f);
                adddynlight(v, 3*attacks[atk].exprad, vec(1.7f, 1, 1.8f), 1000, 50, DL_FLASH, attacks[atk].exprad/2, vec(0.5f, 1.4f, 2.0f));
                playsound(S_PULSE3_EXPLODE, NULL, &v);
                break;
            }
            case ATK_RL1:
            case ATK_RL2:
            case ATK_SG2:
            {
                particle_splash(PART_SPARK1, 4, 150, v, teamcoloureffects? teamcolour: 0xFFD47B, 1.0f, 10, 0, 0.5f);
                particle_splash(PART_SPARK1, attacks[atk].exprad*4, 130, v, teamcoloureffects? teamcolour: 0xB8E47B, 6.60f, 200, 3);
                particle_fireball(v, attacks[atk].exprad, PART_PULSE_BURST, 280, teamcoloureffects? teamcolour: 0xFFA47B, 1.0f);
                particle_splash(PART_SPARK1, 30, 130, p, teamcoloureffects? teamcolour: 0xFFD47B, 7.5f, 150, -1);
                particle_splash(PART_SPARK1, 20, 150, p, teamcoloureffects? teamcolour: 0xFFD47B, 6.50f, 100, 50);
                rnd(2) ? particle_splash(PART_SPARK2, 0+rnd(50), 380, p, teamcoloureffects? teamcolour : 0xC8E66B, 0.20f+rndscale(0.35f), 350, 2):
                         particle_splash(PART_SPARK1, 200, 180, p, teamcoloureffects? teamcolour: 0xFFD47B, 0.30f, 300, 4);
                particle_splash(PART_SPARK1, 100, 80, p, rnd(2) ? 0x401510 : 0xB8E47B, 5.20f, 60, 80);
                particle_splash(mat&MAT_WATER ? PART_STEAM : PART_SMOKE, 150, 130, p, 0x555555, 5.8f, 250, 200, 0.5f);
                adddynlight(p, 2*attacks[atk].exprad, vec(2, 1.5f, 1), 350, 40, 0, attacks[atk].exprad/2, vec(0.5f, 1.5f, 2.0f));
                playsound(S_ROCKET_EXPLODE, NULL, &v);
                break;
            }
            case ATK_PISTOL2:
            {
                particle_splash(PART_SPARK1, 50, 30, v, teamcoloureffects? teamcolour: 0xb2d3f9, 2.50f);
                particle_splash(PART_SPARK1, 20, 50, v, teamcoloureffects? teamcolour: 0xb2d3f9, 2.50f, 150, -1);
                particle_splash(PART_SPARK1, 10, 30, v, teamcoloureffects? teamcolour: 0xb2d3f9, 2.0f, 200);
                particle_splash(PART_SPARK2, 0+rnd(40), 200+rnd(260), p, teamcoloureffects? teamcolour : 0xb4d5f5, 0.08f+rndscale(0.24f), 320, 2);
                adddynlight(p, 2*attacks[atk].exprad, vec(1.0f, 3.0f, 4.0f), 350, 20, 0, attacks[atk].exprad/2, vec(0.5f, 1.5f, 2.0f));
                playsound(S_PULSE_HIT, NULL, &v);
                break;
            }
            case ATK_GL1:
            case ATK_GL2:
            {
                particle_splash(PART_SPARK1, attacks[atk].exprad*3, 120, p, teamcoloureffects? teamcolour: 0x606090, 4.80f, 200, 3, 0.02f);
                particle_splash(PART_SPARK1, 5, 180, p, teamcoloureffects? teamcolour: 0x707088, 0.08f, 300, 200, 0.50f);
                particle_splash(PART_SPARK1, 100, 80, p, teamcoloureffects? teamcolour: 0x252580, 5.20f, 60, 80);
                particle_splash(mat&MAT_WATER ? PART_STEAM : PART_SMOKE, 160, 120, v, 0x666666, 6, 500, 200, 0.5f);
                particle_fireball(v, 1.0f*attacks[atk].exprad, PART_PULSE_BURST, int(attacks[atk].exprad*20), teamcoloureffects? teamcolour: 0x50CFE8, 0.8f);
                adddynlight(v, 3*attacks[atk].exprad, vec(0.30f, 0.50f, 1), 1000, 50, DL_FLASH, attacks[atk].exprad/2, vec(0.5f, 1.4f, 2.0f));
                playsound(S_GRENADE_EXPLODE, NULL, &v);
                break;
            }
            default:
                particle_splash(PART_SPARK1, 200, 300, p, teamcoloureffects? teamcolour: 0x50CFE5, 0.45f);
                particle_fireball(p, 1.15f*attacks[atk].exprad, PART_PULSE_BURST, int(attacks[atk].exprad*20), teamcoloureffects? teamcolour: 0x50CFE5, 0.10f);
                particle_flare(p, p, 180, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0x50CFE5, 0.10f, NULL, 2.0f);
                adddynlight(safe ? p : debrisorigin, 2*attacks[atk].exprad, vec(1.0f, 3.0f, 4.0f), 350, 40, 0, attacks[atk].exprad/2, vec(0.5f, 1.5f, 2.0f));
                playsound(S_PULSE_EXPLODE, NULL, &v);
                break;
        }
        if(showexplosionradius) particle_fireball(v, attacks[atk].exprad, PART_EXPLOSION, 300, 0xFFFFFF, attacks[atk].exprad);
        if(atk != ATK_PULSE1 && (mat&MATF_VOLUME) == MAT_GLASS)
        {
            particle_splash(PART_GLASS, 0+rnd(50), 260, p, 0xFFFFFF, 0.23f+rndscale(0.40f), 350, 2);
            playsound(S_GLASS_IMPACT_PROJ, NULL, &v);
        }
        if((mat&MATF_VOLUME) == MAT_WATER)
        {
            particle_splash(PART_WATER, 400, 300, p, 0xffffff, 0.01f+rndscale(0.25f), 480, 2);
            particle_splash(PART_STEAM, 240, 200, p, 0xffffff, 4.0f, 150, 50);
            playsound(S_WATER_IMPACT_PROJ, NULL, &v);
        }
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
            case ATK_PULSE3:
            case ATK_SG2:
            case ATK_RL2:
            case ATK_GL1:
            case ATK_GL2:
                loopv(bouncers)
                {
                    bouncer &b = *bouncers[i];
                    if(b.bouncetype < BNC_GRENADE1 && b.bouncetype > BNC_ROCKET) break;
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
        if(o->state!=CS_ALIVE) return false;
        if(!intersect(o, p.o, v, attacks[p.atk].margin)) return false;
        projsplash(p, v, o, damage);
        vec dir;
        projdist(o, dir, v, p.dir, p.atk);
        gameent *f = (gameent *)o;
        if(f->armourmillis) damage /= 2;
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
            float damage = attacks[p.atk].damage*(p.owner->damagemillis ? 2 : 1);
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
                if((p.projtype == PROJ_PULSE || p.projtype == PROJ_ENERGY) && lookupmaterial(p.o)&MAT_WATER)
                    p.lifetime /= 2;
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
                    int tailp = PART_PULSE_SIDE, tailpc = 0xFFFFFF, teamcolour = teamtextcolor[p.owner->team];
                    switch(p.projtype)
                    {
                        case PROJ_ROCKET:
                        {
                            /*
                            if(raycubelos(pos, camera1->o, check))
                            {
                                particle_flare(pos, pos, 1, PART_SPARK1LE1, 0xFFC864, 2.0f+rndscale(3), NULL);
                                particle_flare(pos, pos, 1, PART_SPARK1LE2, 0xFFC864, 3.0f+rndscale(5), NULL);
                            }
                            */
                            regular_particle_splash(PART_SMOKE, 3, 300, pos, teamcoloureffects? teamcolour: 0x303030, 2.4f, 50, -20);
                            if(p.lifetime>attacks[p.atk].range-500) regular_particle_splash(PART_SPARK1, 4, 180, pos, teamcoloureffects? teamcolour: 0x401812, 1.8f, 10, 0);
                            particle_splash(PART_PULSE_FRONT, 1, 1, pos, teamcoloureffects? teamcolour: 0xFFC864, 1.4f, 150, 20);
                            tailp = PART_PULSE_SIDE;
                            tailpc = 0xFFC864;
                            p.projsound = S_ROCKET_LOOP;
                            break;
                        }

                        case PROJ_PULSE:
                        {
                            particle_splash(PART_PULSE_FRONT, 1, 1, pos, teamcoloureffects? teamcolour: 0xDD88DD, 2.4f, 150, 20);
                            tailp = PART_PULSE_SIDE;
                            tailpc = teamcoloureffects? teamcolour: 0xDD88DD;
                            p.projsound = S_PULSE_LOOP;
                            break;
                        }

                        case PROJ_ENERGY:
                        {
                            particle_fireball(pos, 3.50f, PART_PULSE_BURST, 1, teamcoloureffects? teamcolour: 0xFFC1FF);
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
                        particle_flare(tail, head, 1, tailp, tailpc, 2.5f);
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
        int teamcolour = teamtextcolor[d->team], mat = lookupmaterial(to);
        bool water = (mat&MATF_VOLUME) == MAT_WATER;
        switch(atk)
        {
            case ATK_PUNCH:
                if(water || d->o.dist(to) > attacks[atk].range/2) break;
                 particle_splash(PART_SPARK1, 100, 40, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xB2D3F9, 1.0f);
                 adddynlight(vec(to).madd(dir, 4), 100, vec(1.0f, 3.0f, 4.30f), 800, 100);
                 if(!hit) addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 1.20f+rndscale(1.50f));
                 break;

            case ATK_RAIL:
            case ATK_INSTA:
            {
                adddynlight(vec(to).madd(dir, 4), 20, vec(0.25f, 1.0f, 0.75f), 380, 75, DL_SHRINK);
                particle_splash(PART_SPARK1, 200, 250, to, teamcoloureffects? teamcolour: 0x88DD88, 0.45f);
                particle_splash(PART_SPARK1, 50, 100, to, teamcoloureffects? teamcolour: 0x88DD88, 2.5f, 20);
                if(hit || water) break;
                particle_splash(PART_SPARK1, 20, 120, to, teamcoloureffects? teamcolour: 0x88DD88, 2.5f, 30, -1);
                particle_splash(PART_SPARK1, 10, 110, to, teamcoloureffects? teamcolour: 0x88DD88, 3.5f, 50, -1);
                particle_splash(PART_SPARK1, 50, 200, to, teamcoloureffects? teamcolour: 0x88DD88, 0.18f, 200, 1);
                particle_splash(PART_SPARK1, 50, 40, to, teamcoloureffects? teamcolour: 0x88DD88, 2.0f);
                particle_splash(PART_SPARK1, 20, 60, to, teamcoloureffects? teamcolour: 0x88DD88, 2.0f, 150, -1);
                particle_splash(PART_SPARK1, 10, 40, to, teamcoloureffects? teamcolour: 0x88DD88, 2.50f, 200);
                particle_splash(PART_SPARK1, 80, 70, to, teamcoloureffects? teamcolour: 0x88DD88, 1.88f, 180, 6, 0.1f);
                particle_splash(PART_SMOKE, 100, 180, to, 0x808080, 2.9f, 60, 80, 0.1f);
                particle_fireball(to, 1.0f, PART_PULSE_BURST, 220, teamcoloureffects? teamcolour: 0x88DD88, 5.0f);
                particle_splash(PART_SPARK1, 200, 170, to, teamcoloureffects? teamcolour: 0x88DD88, 0.24f, 200, 5);
                addstain(STAIN_RAIL_HOLE, to, dir, 3.5f);
                addstain(STAIN_RAIL_GLOW, to, dir, 3.0f, teamcoloureffects? teamcolour: 0x88DD88);
                break;
            }

            case ATK_SG1:
            {
                adddynlight(vec(to).madd(dir, 4), 14, vec(0.5f, 0.375f, 0.25f), 200, 20);
                if(hit || water) break;
                particle_splash(PART_SPARK1, 50, 50, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xB8E47B, 0.70f);
                particle_splash(PART_SPARK1, 20, 60, to, teamcoloureffects? teamcolour: 0xB8E47B, 0.85f, 180, 2, 0.07f);
                particle_splash(PART_SPARK1, 10, 70, to, teamcoloureffects? teamcolour: 0xB8E47B, 1.0f, 200, 2, 0.03f);
                particle_splash(PART_SPARK2, 0+rnd(10), 80+rnd(380), to, teamcoloureffects? teamcolour : 0xC8E66B, 0.05f+rndscale(0.09f), 250);
                particle_splash(PART_SMOKE, rnd(2) ? 0 : 70, 180, vec(to).madd(dir, 4), 0x606060, 1.0f+rndscale(1.8f), 80, 100, 0.05f);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f+rndscale(0.80f));
                break;

            }

            case ATK_SMG1:
            case ATK_SMG2:
            case ATK_SMG3:
            {
                adddynlight(vec(to).madd(dir, 4), 20, vec(0.5f, 0.375f, 0.25f), 200, 20);
                if(hit || water) break;
                particle_splash(PART_SPARK1, 100, 80, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xB8E47B, 1.20f, 60, 80);
                particle_splash(PART_SPARK1, 60, 30, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xB8E47B, 1.30f);
                particle_splash(PART_SPARK1, 30, 40, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xB8E47B, 1.40f, 100, -1);
                particle_splash(PART_SPARK1, 90, 50, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xB8E47B, 1.60f, 200, 1);
                particle_splash(PART_SPARK2, 0+rnd(30), 60+rnd(380), vec(to).madd(dir, 4), teamcoloureffects? teamcolour : 0xC8E66B, 0.01f+rndscale(0.2f), 300, 2);
                particle_splash(PART_SMOKE, 80, 180, vec(to).madd(dir, 4), 0x303030, 2.2f, 60, 69, 0.06f);
                particle_fireball(to, 0.20f, PART_PULSE_BURST, 30, 0xB8E47B, 1.0f);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.50f+rndscale(1.0f));
                break;
            }

            case ATK_PULSE2:
            {
                adddynlight(vec(to).madd(dir, 4), 20, vec(1.7f, 1, 1.8f), 70);
                particle_splash(PART_SPARK1, 20, 50, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xEE88EE, 1.50f, 180);
                particle_splash(PART_SPARK1, 10, 30, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xEE88EE, 2.0f, 200);
                if(hit || water) break;
                particle_splash(PART_SPARK2, 0+rnd(30), 320, vec(to).madd(dir, 4), 0xFFFFFF, 0.01f+rndscale(0.10f), 350, 2);
                particle_fireball(to, 0.20f, PART_EXPLOSION, 30, teamcoloureffects? teamcolour: 0xEE88EE, 1.0f);
                if(from.dist(to) <= attacks[atk].range-1)
                    particle_splash(PART_SMOKE, 80, 180, vec(to).madd(dir, 4), 0x777777, 2.20f, 100, 80, 0.05f);
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 1.0f+rndscale(1.10f));
                break;
            }

            case ATK_PISTOL1:
            {
                particle_splash(PART_SPARK1, 50, 30, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xb2d3f9, 2.0f);
                particle_splash(PART_SPARK1, 20, 50, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xb2d3f9, 2.0f, 150, -1);
                particle_splash(PART_SPARK1, 10, 30, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xb2d3f9, 2.50f, 200);
                particle_flare(vec(to).madd(dir, 4), vec(to).madd(dir, 4), 60, PART_SPARK1, teamcoloureffects? teamcolour: 0xb2d3f9, 1.50f, NULL, 0.9f);
                particle_fireball(vec(to).madd(dir, 4), 2.0f, PART_PULSE_BURST, 120, teamcoloureffects? teamcolour: 0xb2d3f9, 1.0f);
                adddynlight(vec(to).madd(dir, 4), 20, vec(1.0f, 3.0f, 4.0f), 200, 10, DL_SHRINK, 10, vec(0.5f, 1.5f, 2.0f));
                if(!hit)
                {
                    particle_splash(PART_SPARK2, 0+rnd(30), 180+rnd(280), vec(to).madd(dir, 4), teamcoloureffects? teamcolour : 0xb4d5f5, 0.01f+rndscale(0.18f), 300, 2);
                    addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 0.80f+rndscale(1.0f));
                }
                break;
            }

            default: break;
        }
        if((attacks[atk].action == ACT_MELEE && d->o.dist(to) > attacks[atk].range/2) || hit) return;
        bool oneray = attacks[atk].rays <= 1;
        if(water)
        {
            particle_splash(PART_WATER, attacks[atk].rays > 1 ? 50 : 200, 200, vec(to).madd(dir, 5), 0xFFFFFF, 0.18f, 280, 2);
            particle_splash(PART_STEAM, 30, 120, vec(to).madd(dir, 6), 0xFFFFFF, 1.0f, 80, 100, 0.05f);
            if(oneray) playsound(S_WATER_IMPACT, NULL, &to);
        }
        else if((mat&MATF_VOLUME) == MAT_GLASS && atk != ATK_PULSE2)
        {
            particle_splash(PART_GLASS, 0+rnd(50), 150+rnd(250), to, 0xFFFFFF, 0.20f+rndscale(0.25f), 200, 1);
            if(oneray) playsound(S_GLASS_IMPACT, NULL, &to);
        }
        if(attacks[atk].impactsound >= 0)
        {
            if(!oneray)
            {
            }
            else playsound(attacks[atk].impactsound, NULL, &to);
        }
    }

    VARP(muzzleflash, 0, 1, 1);

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun, sound = attacks[atk].sound, teamcolour = teamtextcolor[d->team];
        float dist = from.dist(to);
        vec up = to;
        vec dir = vec(from).sub(to).safenormalize();
        switch(atk)
        {
            case ATK_PULSE1:
                if(muzzleflash && d->muzzle.x >= 0)
                    particle_flare(d->muzzle, d->muzzle, 115, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour : 0xDD88DD, 3.0f, d);
                newprojectile(PROJ_PULSE, from, to, attacks[atk].projspeed, local, id, attacks[atk].range, d, atk);
                break;

            case ATK_PULSE2:
            {
                if(muzzleflash)
                {
                     if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 80, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour : 0xDD88DD, 4.0f, d);
                     adddynlight(hudgunorigin(gun, d->o, to, d), 35, vec(1.8f, 1, 1.9f), 80, 10, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(vec(to).madd(dir, 4), from, 60, PART_LIGHTNING, teamcoloureffects? teamcolour : 0xDD88DD, 0.80f, d);
                particle_flare(vec(to).madd(dir, 4), from, 70, PART_LIGHTNING, teamcoloureffects? teamcolour : 0xEE88EE, 1.0f, d);
                particle_flare(vec(to).madd(dir, 4), from, 80, PART_LIGHTNING, teamcoloureffects? teamcolour : 0xCC88CC, 1.10f, d);
                particle_splash(PART_SPARK1, 200, 120, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xEE88EE, 0.45f);
                particle_splash(PART_SPARK1, 50, 30, vec(to).madd(dir, 4), teamcoloureffects? teamcolour: 0xEE88EE, 1.0f);
                if(lastmillis-prevaction>280 && d->attacksound<0)
                {
                    particle_flare(vec(to).madd(dir, 4), vec(to).madd(dir, 4), 180, PART_SPARK1, teamcoloureffects? teamcolour : 0xEE88EE, 1.20f, NULL, 0.5f);
                    particle_flare(vec(to).madd(dir, 4), vec(to).madd(dir, 4), 150, PART_RAIL_MUZZLE_FLASH, teamcoloureffects? teamcolour : 0xEE88EE, 1.40f, NULL, 0.6f);
                    particle_splash(PART_SMOKE, 80, 180, vec(to).madd(dir, 4), 0x666666, 2.5f, 60, 80, 0.1f);
                }
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_PULSE3:
                up.z += dist/8;
                newbouncer(from, up, local, id, d, atk, BNC_MINE, attacks[atk].lifetime, attacks[atk].projspeed, 0.8f, 0);
                break;

            case ATK_RAIL:
            case ATK_INSTA:
            {
                if(muzzleflash)
                {
                    if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 148, PART_RAIL_MUZZLE_FLASH, teamcoloureffects? teamcolour : 0x88DD88, 0.1f, d, 0.3f);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 50, vec(0.25f, 1.0f, 0.75f), 150, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                }
                particle_trail(PART_SPARK1, 500, hudgunorigin(attacks[atk].gun, from, to, d), to, teamcoloureffects? teamcolour : 0x88DD88, 1.0f, NULL);
                particle_trail(PART_SMOKE, 400, hudgunorigin(attacks[atk].gun, from, to, d), to, 0x808080, 1.9f, 50);
                particle_trail(PART_SPARK1, 550, hudgunorigin(attacks[atk].gun, from, to, d), to, teamcoloureffects? teamcolour : 0x55DD55, 0.4f, NULL);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_RL1:

                if(muzzleflash && d->muzzle.x >= 0)
                    particle_flare(d->muzzle, d->muzzle, 140, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xEFE898, 0.20f, d, 0.4f);
                newprojectile(PROJ_ROCKET, from, to, attacks[atk].projspeed, local, id, attacks[atk].range, d, atk);
                break;

            case ATK_RL2:
                up.z += dist/8;
                newbouncer(from, up, local, id, d, atk, BNC_ROCKET, attacks[atk].lifetime, attacks[atk].projspeed, 0.8f, 0.7f);
                break;

            case ATK_SG1:
            {
                if(muzzleflash)
                {
                    if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 70, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xEFE598, 5.2f, d);
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
                newbouncer(from, up, local, id, d, atk, BNC_GRENADE1, attacks[atk].lifetime, attacks[atk].projspeed, 1.0f);
                break;

            case ATK_SMG1:
            case ATK_SMG2:
            {
                if(atk != ATK_SMG2) {
                    if(muzzleflash) particle_flare(d->muzzle, d->muzzle, 80, PART_RAIL_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xEFE898, 5.0f, d); }
                else
                {
                    if(muzzleflash) particle_flare(d->muzzle, d->muzzle, 120, PART_RAIL_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xEFE898, 0.30f, d, 0.4f);
                    if(dist <= attacks[atk].range) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_STREAK, teamcoloureffects? teamcolour: 0xFFC864, 1.0f);
                }
                if(muzzleflash) adddynlight(hudgunorigin(gun, d->o, to, d), 40, vec(0.5f, 0.375f, 0.25f), atk==ATK_SMG1 ? 70 : 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_SMG3:
            {
                if(muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 120, PART_RAIL_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xEFE898, 0.30f, d, 0.4f);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 40, vec(0.5f, 0.375f, 0.25f), atk==ATK_SMG1 ? 70 : 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(!local)
                {
                    createrays(atk, from, to);
                    loopi(attacks[atk].rays) rayhit(atk, d, from, rays[i], hit);
                }
                if(dist <= attacks[atk].range) {
                    loopi(attacks[atk].rays) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), rays[i], 80, PART_STREAK, teamcoloureffects? teamcolour: 0xFFC864, 1.0f); }
                break;
            }

            case ATK_GL1:
            case ATK_GL2:
                up.z += dist/8;
                newbouncer(from, up, local, id, d, atk, atk == ATK_GL1 ? BNC_GRENADE2 : BNC_GRENADE3, attacks[atk].lifetime, 0.8f, attacks[atk].projspeed, atk == ATK_GL1? 0: 0.6f);
                break;

            case ATK_PISTOL1:
            case ATK_PISTOL2:
            {
                if(muzzleflash)
                {
                   if(d->muzzle.x >= 0) particle_flare(d->muzzle, d->muzzle, 50, PART_PULSE_MUZZLE_FLASH, teamcoloureffects? teamcolour: 0xB2D3F9, 6.0f, d);
                    adddynlight(hudgunorigin(attacks[atk].gun, d->o, to, d), 30, vec(1.0f, 3.0f, 4.0f), 60, 20, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(atk == ATK_PISTOL1 && dist <= attacks[atk].range)
                    particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_STREAK, teamcoloureffects? teamcolour: 0xB2D3F9, 2.0f);
                if(atk == ATK_PISTOL2) newprojectile(PROJ_ENERGY, from, to, attacks[atk].projspeed, local, id, attacks[atk].range, d, atk);
                else if(!local) rayhit(atk, d, from, to, hit);
                break;
            }

            case ATK_PUNCH:
                 if(muzzleflash) adddynlight(from, 30, vec(1.0f, 3.0f, 4.30f), 400, 100, 0, 0, vec(0, 0, 0), d);
                break;

            default:
                break;
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
                playsound(S_SG1B, d);
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
        top.z -= d->eyeheight/3;
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
        dynent *o;
        float dist;
        int maxrays = attacks[atk].rays, margin = attacks[atk].margin, damage = attacks[atk].damage*(d->damagemillis ? 2 : 1), flags = HIT_TORSO;
        if(attacks[atk].rays > 1)
        {
            dynent *hits[MAXRAYS];
            loopi(maxrays)
            {
                if((hits[i] = intersectclosest(from, rays[i], d, margin, dist)))
                {
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
                if(intersecthead(o, from, rays[i], dist))
                {
                    if((m_headhunter(mutators) || m_locationaldam(mutators)) && !attacks[atk].bonusdam)
                        damage *= 2;
                }
                else if(m_headhunter(mutators)) return;
                if(m_locationaldam(mutators) && intersectlegs(o, from, rays[i], dist))
                {
                    damage /= 2;
                    flags |= HIT_LEGS;
                }
                hitpush(numhits*damage, o, d, from, to, atk, numhits, flags);
                damageeffect(damage, o, rays[i], atk);
            }
        }
        else if((o = intersectclosest(from, to, d, margin, dist)))
        {
            const bool headshot = atk == ATK_STOMP || (validatk(atk) && (attacks[atk].bonusdam || m_headshot(mutators)) && !attacks[atk].projspeed && intersecthead(o, from, to, dist));
            shorten(from, to, dist);
            rayhit(atk, d, from, to, true);
            if(!headshot && (atk == ATK_STOMP || m_headhunter(mutators))) return;
            if(headshot)
            {
                if((m_headhunter(mutators) || m_locationaldam(mutators)) && !attacks[atk].bonusdam) damage *= 2;
                else damage += attacks[atk].bonusdam;
                flags = HIT_HEAD;
            }
            else if(m_locationaldam(mutators))
            {
                if(intersectlegs(o, from, to, dist))
                {
                    damage /= 2;
                    flags = HIT_LEGS;
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
        if(!m_unlimitedammo(mutators) && atk > 0 && !d->ammomillis) d->ammo[gun] -= attacks[atk].use;

        vec from = d->o, to = targ, dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        bool kicked = false;
        if(attacks[atk].kickamount && d->o.dist(to) <= 25 && !(d->physstate >= PHYS_SLOPE && d->crouching && d->crouched()))
        {
            vec kickback = vec(dir).mul(attacks[atk].kickamount*-2.5f);
            d->vel.add(kickback);
            kicked = true;
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
            addmsg(N_SHOOT, "rci2i7iv", d, lastmillis-maptime, atk,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), kicked,
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        }

        int gunwait = attacks[atk].attackdelay;
        if(d->hastemillis) gunwait /= 2;
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
                    adddynlight(pos, 25, vec(1.8f, 1, 1.9f));
                    break;
                }

                case PROJ_ROCKET:
                {
                    adddynlight(pos, 50, vec(2, 1.5f, 1), 0, 0, 0, 10, vec(0.5f, 0.375f, 0.25f));
                    break;
                }

                case PROJ_ENERGY:
                {
                    adddynlight(pos, 20, vec(0.25f, 0.75f, 1.0f));
                    break;
                }
            }
        }
        loopv(bouncers)
        {
            bouncer &bnc = *bouncers[i];
            if(bnc.bouncetype<BNC_GRENADE1 && bnc.bouncetype>BNC_ROCKET) continue;
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
            switch(bnc.bouncetype)
            {
                case BNC_GRENADE2:
                case BNC_GRENADE3:
                {
                    adddynlight(pos, 12, vec(0.25f, 0.30f, 1));
                    break;
                }

                case BNC_MINE:
                {
                    adddynlight(pos, 15, vec(1.8f, 1, 1.9f));
                    break;
                }
            }
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
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/float(OFFSETMILLIS)));
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
                    case BNC_GRENADE1: mdl = "projectile/grenade01"; break;
                    case BNC_GRENADE2: case BNC_GRENADE3: case BNC_MINE: mdl = "projectile/grenade02"; break;
                    case BNC_ROCKET: mdl = "projectile/rocket"; break;
                    default: mdl = "projectile/grenade01"; break;
                }
            }
            rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, cull);
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
            rendermodel(mdl, ANIM_MAPMODEL|ANIM_LOOP, v, yaw, pitch, MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED);
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
            if(bnc.bouncetype <= BNC_GRENADE1 && bnc.bouncetype >= BNC_ROCKET) continue;
            obstacles.avoidnear(NULL, bnc.o.z + attacks[bnc.atk].exprad + 1, bnc.o, radius + attacks[bnc.atk].exprad);
        }
    }
};

