#include "game.h"

namespace game
{
    bool intermission = false, gamewaiting = false;
    bool betweenrounds = false, hunterchosen = false;
    int maptime = 0, maprealtime = 0, maplimit = -1;
    int lastspawnattempt = 0;

    gameent *self = NULL; // ourselves (our client)
    vector<gameent *> players; // other clients
    vector<gameent *> bestplayers;
    vector<int> bestteams;

    ICOMMAND(numplayers, "", (), intret(players.length()));

    void taunt(gameent *d)
    {
        if(d->state!=CS_ALIVE || lastmillis-d->lasttaunt<1000) return;
        d->lasttaunt = lastmillis;
        addmsg(N_TAUNT, "rc", self);
        playsound(getplayermodelinfo(d).tauntsound, d);
        self->attacking = ACT_IDLE;
    }
    ICOMMAND(taunt, "", (), taunt(self));

    int following = -1;

    VARFP(specmode, 0, 0, 2,
    {
        if(!specmode) stopfollowing();
        else if(following < 0) nextfollow();
    });

    gameent *followingplayer(gameent *fallback)
    {
        if(self->state!=CS_SPECTATOR || following<0) return fallback;
        gameent *target = getclient(following);
        if(target && target->state!=CS_SPECTATOR) return target;
        return fallback;
    }

    ICOMMAND(getfollow, "", (),
    {
        gameent *f = followingplayer();
        intret(f ? f->clientnum : -1);
    });

    bool canfollow(gameent *s, gameent *f) { return f->state!=CS_SPECTATOR && !(m_round && f->state==CS_DEAD) && ((!cmode && s->state==CS_SPECTATOR) || (cmode && cmode->canfollow(s, f))); }

    void stopfollowing()
    {
        if(following<0) return;
        following = -1;
        camera::thirdperson = 0;
    }

    void follow(char *arg)
    {
        int cn = -1;
        if(arg[0])
        {
            if(self->state != CS_SPECTATOR) return;
            cn = parseplayer(arg);
            if(cn == self->clientnum) cn = -1;
        }
        if(cn < 0 && (following < 0 || specmode)) return;
        following = cn;
    }
    COMMAND(follow, "s");

    void nextfollow(int dir)
    {
        if(self->state!=CS_SPECTATOR) return;
        int cur = following >= 0 ? following : (dir > 0 ? clients.length() - 1 : 0);
        loopv(clients)
        {
            cur = (cur + dir + clients.length()) % clients.length();
            if(clients[cur] && canfollow(self, clients[cur]))
            {
                following = cur;
                return;
            }
        }
        stopfollowing();
    }
    ICOMMAND(nextfollow, "i", (int *dir), nextfollow(*dir < 0 ? -1 : 1));

    void checkfollow()
    {
        if(self->state != CS_SPECTATOR)
        {
            if(following >= 0) stopfollowing();
        }
        else
        {
            if(following >= 0)
            {
                gameent *d = clients.inrange(following) ? clients[following] : NULL;
                if(!d || d->state == CS_SPECTATOR) stopfollowing();
            }
            if(following < 0 && specmode) nextfollow();
        }
    }

    const char *getclientmap() { return clientmap; }

    void resetgamestate()
    {
        projectile::removeprojectiles();
        clearmonsters();
        entities::resettriggers();
    }

    int vooshgun;

    gameent *spawnstate(gameent *d) // reset player state not persistent across spawns
    {
        d->respawn();
        d->spawnstate(gamemode, mutators, vooshgun);
        return d;
    }

    bool canspawn()
    {
        if (!m_round)
        {
            return true;
        }
        else if ((m_hunt && !hunterchosen) || gamewaiting)
        {
            return true;
        }
        return false;
    }
    ICOMMAND(canspawn, "", (), intret(canspawn() ? 1 : 0));

    const int getrespawndelay(gameent* d)
    {
        if (cmode)
        {
            return cmode->respawnwait();
        }
        else if (m_norespawndelay && d->aitype == AI_NONE)
        {
            /* Allow immediate respawns in modes where waiting isn't needed,
             * except for bots.
             */
            return 0;
        }
        return SPAWN_DELAY;
    }
    ICOMMAND(getrespawndelay, "", (), intret(getrespawndelay(self)));

    VARP(queuerespawn, 0, 1, 1);

    void respawnself()
    {
        if(ispaused()) return;
        const int delay = getrespawndelay(self);
        if(queuerespawn && lastmillis - self->lastpain <= delay)
        {
            self->respawnqueued = true;
            return;
        }
        if(m_invasion)
        {
            if(self->lives <= 0)
            {
                // if we have no more lives in Invasion, we try the same map again just like in Sauer
                changemap(clientmap, gamemode, mutators);
                return;
            }
            if(!m_insta(mutators)) healmonsters(); // give monsters a health bonus each time we die
        }
        if(m_mp(gamemode))
        {
            int seq = (self->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
            if(self->respawned!=seq)
            {
                addmsg(N_TRYSPAWN, "rc", self);
                self->respawned = seq;
            }
        }
        else
        {
            spawnplayer(self);
            if(cmode) cmode->respawned(self);
        }
        execident("on_spawn");
    }

    gameent *pointatplayer()
    {
        loopv(players) if(players[i] != self && weapon::isintersecting(players[i], self->o, worldpos)) return players[i];
        return NULL;
    }

    gameent *hudplayer()
    {
        if((camera::thirdperson && camera::allowthirdperson()) || specmode > 1) return self;
        return followingplayer(self);
    }

    VARP(smoothmove, 0, 75, 100);
    VARP(smoothdist, 0, 32, 64);

    void predictplayer(gameent *d, bool move)
    {
        d->o = d->newpos;
        d->yaw = d->newyaw;
        d->pitch = d->newpitch;
        d->roll = d->newroll;
        if(move)
        {
            physics::moveplayer(d, 1, false);
            d->newpos = d->o;
        }
        float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove;
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw<0) d->yaw += 360;
            else if(d->yaw>=360) d->yaw -= 360;
            d->pitch += d->deltapitch*k;
            d->roll += d->deltaroll*k;
        }
    }

    void otherplayers(int curtime)
    {
        loopv(players)
        {
            gameent *d = players[i];
            if(d == self || d->ai) continue;

            if(d->state==CS_DEAD && d->ragdoll) moveragdoll(d);
            else if(!intermission && d->state==CS_ALIVE)
            {
                if(lastmillis - d->lastaction >= d->gunwait) d->gunwait = 0;
                if(d->powerupmillis || d->role == ROLE_BERSERKER)
                {
                    entities::updatepowerups(curtime, d);
                }
            }
            const int lagtime = totalmillis-d->lastupdate;
            if(!lagtime || intermission) continue;
            else if(lagtime>1000 && d->state==CS_ALIVE)
            {
                d->state = CS_LAGGED;
                continue;
            }
            if(d->state==CS_ALIVE || d->state==CS_EDITING)
            {
                physics::crouchplayer(d, 10, false);
                if(smoothmove && d->smoothmillis>0) predictplayer(d, true);
                else physics::moveplayer(d, 1, false);
            }
            else if(d->state==CS_DEAD && !d->ragdoll && lastmillis-d->lastpain<2000) physics::moveplayer(d, 1, true);
        }
    }

    int waterchan = -1;

    void updateworld()        // main game update loop
    {
        if(!maptime)
        {
            maptime = lastmillis;
            maprealtime = totalmillis;
            return;
        }
        if(!curtime)
        {
            gets2c();
            if(self->clientnum>=0) c2sinfo();
            return;
        }

        physics::physicsframe();
        ai::navigate();
        if(self->state == CS_ALIVE && !intermission)
        {
            if(self->powerupmillis || self->role == ROLE_BERSERKER)
            {
                entities::updatepowerups(curtime, self);
            }
        }
        weapon::updateweapons(curtime);
        otherplayers(curtime);
        camera::camera.update();
        announcer::update();
        ai::update();
        moveragdolls();
        gets2c();
        updatemonsters(curtime);
        managelowhealthscreen();
        if(connected)
        {
            if(self->state == CS_DEAD)
            {
                if(self->ragdoll) moveragdoll(self);
                else if(lastmillis-self->lastpain<2000)
                {
                    self->move = self->strafe = 0;
                    physics::moveplayer(self, 10, true);
                }
                if(lastmillis - self->lastpain > getrespawndelay(self))
                {
                    if(self->respawnqueued)
                    {
                        respawnself();
                        self->respawnqueued = false;
                    }
                }
            }
            else if(!intermission)
            {
                if(self->ragdoll) cleanragdoll(self);
                physics::crouchplayer(self, 10, true);
                physics::moveplayer(self, 10, true);
                entities::checkitems(self);
                if(cmode) cmode->checkitems(self);
            }
            else if(self->state == CS_SPECTATOR) physics::moveplayer(self, 10, true);
        }
        int mat = lookupmaterial(camera1->o);
        if(self->state!=CS_EDITING && mat&MAT_WATER) waterchan = playsound(S_UNDERWATER, NULL, NULL, NULL, 0, -1, 200, waterchan);
        else
        {
            if(waterchan >= 0)
            {
                stopsound(S_UNDERWATER, waterchan, 500);
                waterchan = -1;
            }
        }
        if(self->clientnum>=0) c2sinfo();   // do this last, to reduce the effective frame lag
    }

    void trackparticles(physent* owner, vec& o, vec& d)
    {
        weapon::trackparticles(owner, o, d);
    }

    void trackdynamiclights(physent* owner, vec& o, vec& hud)
    {
        weapon::trackdynamiclights(owner, o, hud);
    }

    void addgamedynamiclights()
    {
        projectile::updateprojectilelights();
    }

    float proximityscore(float x, float lower, float upper)
    {
        if (x <= lower) return 1.0f;
        if (x >= upper) return 0.0f;
        float a = x - lower, b = x - upper;
        return (b * b) / (a * a + b * b);
    }

    static inline float harmonicmean(float a, float b)
    {
        return a + b > 0 ? 2 * a * b / (a + b) : 0.0f;
    }

    // Avoid spawning near other players.
    float ratespawn(dynent* pl, const extentity& e)
    {
        gameent* d = (gameent*)pl;
        vec loc = vec(e.o).addz(d->eyeheight);
        float maxrange = !m_noitems(mutators) ? 400.0f : (cmode ? 300.0f : 110.0f);
        float minplayerdist = maxrange;
        loopv(players)
        {
            const gameent* p = players[i];
            if (p == d)
            {
                if (m_noitems(mutators) || (p->state != CS_ALIVE && lastmillis - p->lastpain > 3000))
                {
                    continue;
                }
            }
            else if (p->state != CS_ALIVE || isally(p, d)) continue;

            vec dir = vec(p->o).sub(loc);
            float dist = dir.squaredlen();
            if (dist >= minplayerdist * minplayerdist) continue;
            dist = sqrtf(dist);
            dir.mul(1 / dist);

            // Scale actual distance if not in line of sight.
            if (raycube(loc, dir, dist) < dist) dist *= 1.5f;
            minplayerdist = min(minplayerdist, dist);
        }
        float rating = 1.0f - proximityscore(minplayerdist, 80.0f, maxrange);
        return cmode ? harmonicmean(rating, cmode->ratespawn(d, e)) : rating;
    }

    void pickgamespawn(gameent* d)
    {
        int forcedentity = m_story && d == self && entities::respawnent >= 0 ? entities::respawnent : -1;
        int tag = m_teammode ? d->team : 0;
        if (cmode)
        {
            cmode->pickspawn(d);
        }
        else
        {
            findplayerspawn(d, forcedentity, tag);
        }
    }

    void spawnplayer(gameent *d)   // place at random spawn
    {
        pickgamespawn(d);
        spawnstate(d);
        if(d == self)
        {
            if (editmode)
            {
                d->state = CS_EDITING;
            }
            else if (d->state != CS_SPECTATOR)
            {
                d->state = CS_ALIVE;
            }
        }
        else
        {
            d->state = CS_ALIVE;
        }
        checkfollow();
        spawneffect(d);
    }

    void spawneffect(gameent *d)
    {
        stopownersounds(d);
        if (d->type == ENT_PLAYER)
        {
            if (d == followingplayer(self))
            {
                clearscreeneffects();
                addscreenflash(SPAWN_DURATION);
                camera::camera.addevent(d, camera::CameraEvent_Spawn, 380);
            }
            adddynlight(d->o, 100, vec(1, 1, 1), SPAWN_DURATION, 100, DL_EXPAND | L_NOSHADOW);
            weapon::doweaponchangeffects(d);
            playsound(S_PLAYER_SPAWN, d);
        }
        else
        {
            const int color = 0x00e661;
            particle_flare(d->o, d->o, 350, PART_EXPLODE1, color, 2.0f, NULL, d->radius + 50.0f);
            particle_flare(d->o, d->o, 280, PART_ELECTRICITY, color, 2.0f, NULL, d->radius + 30.0f);
            adddynlight(d->o, 100, vec::hexcolor(color), 350, 100, DL_EXPAND | L_NOSHADOW);
            playsound(S_MONSTER_SPAWN, d);
        }
        d->lastspawn = lastmillis;
    }

    const int getrespawnwait(gameent* d)
    {
        if (m_norespawndelay || self->state != CS_DEAD)
        {
            return 0;
        }
        int wait = cmode ? cmode->respawnwait() : SPAWN_DELAY;
        return max(0, wait - (lastmillis - d->lastpain));
    }
    ICOMMAND(getrespawnwait, "", (int* seconds), intret(getrespawnwait(self)));

    void respawn()
    {
        if(self->state==CS_DEAD)
        {
            self->attacking = ACT_IDLE;
            const int wait = getrespawnwait(self);
            if(wait > 0)
            {
                lastspawnattempt = lastmillis;
                return;
            }
            respawnself();
        }
    }
    COMMAND(respawn, "");
    ICOMMAND(getlastspawnattempt, "", (), intret(lastspawnattempt));

    // inputs

    inline bool checkaction(int& act, const int gun)
    {
        if (self->attacking == ACT_MELEE)
        {
            camera::camera.zoomstate.disable();
            return true;
        }

        const int zoomtype = weapon::checkweaponzoom();
        if (zoomtype != Zoom_None)
        {   
            if (act == ACT_SECONDARY)
            {
                msgsound(S_WEAPON_ZOOM, self);
                if (identexists("dozoom"))
                {
                    execident("dozoom");
                }
                else
                {
                    camera::zoom = camera::zoom ? -1 : 1;
                }
                if (self->attacking == ACT_PRIMARY)
                {
                    /* When zooming in while firing with the primary mode,
                     * automatically switch to the secondary, to ensure consistency.
                     */
                    return true;
                }
                return false;
            }
            if (act == ACT_PRIMARY)
            {
                if (camera::camera.zoomstate.isenabled())
                {
                    act = ACT_SECONDARY;
                }
            }
        }
        return true;
    }

    void doaction(int act)
    {
        if (!connected || intermission || lastmillis - self->lasttaunt < 1000 || !checkaction(act, self->gunselect))
        {
            return;
        }

        if ((self->attacking = act))
        {
            respawn();
        }
    }
    ICOMMAND(primary, "D", (int *down), doaction(*down ? ACT_PRIMARY : ACT_IDLE));
    ICOMMAND(secondary, "D", (int *down), doaction(*down ? ACT_SECONDARY : ACT_IDLE));
    ICOMMAND(melee, "D", (int *down), doaction(*down ? ACT_MELEE : ACT_IDLE));

    bool isally(const gameent *a, const gameent *b)
    {
        return (m_teammode && validteam(a->team) && validteam(b->team) && sameteam(a->team, b->team))
               || (m_role && a->role == b->role) || (m_invasion && a->type == ENT_PLAYER && b->type == ENT_PLAYER);
    }

    bool isinvulnerable(gameent *target, gameent *actor)
    {
        return target->haspowerup(PU_INVULNERABILITY) && !actor->haspowerup(PU_INVULNERABILITY);
    }

    bool ismonster(gameent *d)
    {
        return m_invasion && d->type == ENT_AI;
    }

    bool editing()
    { 
        return m_edit;
    }

    VARP(deathscream, 0, 1, 1);

    void managedeatheffects(gameent* d)
    {
        if (!d)
        {
            return;
        }

        if (d->deathstate == Death_Gib)
        {
            weapon::gibeffect(max(-d->health, 0), d->vel, d);
        }
        if (deathscream && d->type == ENT_PLAYER)
        {
            bool isfirstperson = d == self && camera::isfirstpersondeath();
            if (!isfirstperson)
            {
                int diesound = getplayermodelinfo(d).diesound[d->deathstate];
                if (validsound(diesound))
                {
                    playsound(diesound, d);
                }
            }
        }
        // Fiddle around with velocity to produce funny results.
        if (d->deathstate == Death_Headshot)
        {
            d->vel.x = d->vel.y = 0;
        }
        else if (d->deathstate == Death_Shock)
        {
            static const float GUN_PULSE_SHOCK_IMPULSE = 95.0f; // Funny upward velocity applied to targets dying of electrocution.
            d->vel.z = max(d->vel.z, GUN_PULSE_SHOCK_IMPULSE);
            particle_flare(d->o, d->o, 100, PART_ELECTRICITY, 0xDD88DD, 12.0f);
        }
    }

    void setdeathstate(gameent *d, bool restore)
    {
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        loopi(Chan_Num)
        {
            // Free up sound channels used for player actions.
            d->stopchannelsound(i);
        }
        stopownersounds(d);
        if(!restore)
        {
            managedeatheffects(d);
            d->deaths++;
        }
        if(d == self)
        {
            camera::restore(restore);
            d->attacking = ACT_IDLE;
            if(camera::isfirstpersondeath())
            {
                stopsounds(SND_UI | SND_ANNOUNCER);
                playsound(S_DEATH);
            }
            if(m_invasion) self->lives--;
            if(camera::thirdperson) camera::thirdperson = 0;
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            d->smoothmillis = 0;
        }
    }

    struct Killfeed
    {
        enum Type
        {
            REGULAR       = 0,
            MONSTER       = 1,
            ASSASSINATION = 2,
            DEATH         = 3,
            MEDAL         = 4
        };

        enum Crit
        {
            ZOOM        = 1 << 0,
            HEADSHOT    = 1 << 1,
            EXPLOSION   = 1 << 2
        };

        enum Weapon
        {
            MELEE = -1
        };

        int type = Type::REGULAR;
        int actor = -1;
        int victim = -1;
        int weapon = -1;
        int attack = -1;
        int crit = 0;
        int deathState = Death_Default;
        int medal = 0;

        void setCrit(const int flags, const int atk)
        {
            crit = 0;
            if (flags & KILL_HEADSHOT)
            {
                crit |= Crit::HEADSHOT;
            }
            if (flags & KILL_EXPLOSION)
            {
                crit |= Crit::EXPLOSION;
            }
            const int gun = attacks[atk].gun;
            const bool isZoom = attacks[atk].action == ACT_SECONDARY && guns[gun].zoom != Zoom_None;
            if (isZoom)
            {
                crit |= Crit::ZOOM;
            }
        }

        struct Hud
        {
            int lastKillVictim = -1;
            int lastKillWeapon = -1;
            int lastKillAttack = -1;
            int lastKillDeathState = Death_Default;
            int lastKillerWeapon = -1;
            int lastKillerAttack = -1;
            int lastDeathState = Death_Default;
        };
        Hud hud;
    };
    Killfeed killFeed;

    void printkillfeedannouncement(const int announcement, gameent* actor)
    {
        if (m_betrayal)
        {
            return;
        }
        conoutf(CON_FRAGINFO, "%s \f2%s", colorname(actor), announcer::announcements[announcement].message);
        killFeed.type = killFeed.Type::MEDAL;
        killFeed.actor = actor->clientnum;
        killFeed.medal = announcement;
        execident("on_obituary");
    }

    void writeobituary(gameent *d, gameent *actor, int atk, int flags)
    {
        // Console messages and killfeed updates.
        gameent* hud = followingplayer(self);
        const char *act = "killed";
        const int weapon = attacks[atk].gun;
        if (actor->type == ENT_AI)
        {
            if (d->type == ENT_PLAYER)
            {
                conoutf(CON_FRAGINFO, "%s \fs\f2was %s by a\fr %s", teamcolorname(d), act, actor->name);
                killFeed.type = killFeed.Type::MONSTER;
            }
            else
            {
                return;
            }
        }
        else
        {
            if (flags & KILL_TRAITOR)
            {
                act = "assassinated";
                conoutf(CON_FRAGINFO, "%s \fs\f2was %s\fr", colorname(d), act);
                killFeed.type = killFeed.Type::ASSASSINATION;
                playsound(S_TRAITOR_KILL);
            }
            else if (d == actor)
            {
                if (validatk(atk) && weapon == GUN_ZOMBIE)
                {
                    act = "got infected";
                }
                else
                {
                    act = "died";
                }
                conoutf(CON_FRAGINFO, "%s \fs\f2%s\fr", teamcolorname(d), act);
                killFeed.type = killFeed.Type::DEATH;
            }
            else if (validatk(atk))
            {
                if (isally(d, actor))
                {
                    conoutf(CON_FRAGINFO, "%s \fs\f2%s an ally (%s)\fr", teamcolorname(actor), act, teamcolorname(d));
                }
                else
                {
                    conoutf(CON_FRAGINFO, "%s \fs\f2%s\fr %s", teamcolorname(actor), act, teamcolorname(d));
                }
                killFeed.type = killFeed.Type::REGULAR;
                killFeed.weapon = atk == ATK_MELEE ? killFeed.Weapon::MELEE : weapon;
            }
            killFeed.actor = actor->clientnum;
            killFeed.victim = d->clientnum;
            if (d == hud)
            {
                d->lastattacker = actor->clientnum;
            }
            else if (actor == hud)
            {
                killFeed.hud.lastKillVictim = d->clientnum;
            }
        }
        killFeed.attack = atk;
        killFeed.setCrit(flags, atk);
        killFeed.deathState = d->deathstate;
        if (d == hud)
        {
            killFeed.hud.lastKillerAttack = atk;
            killFeed.hud.lastDeathState = d->deathstate;
            killFeed.hud.lastKillerWeapon = validgun(weapon) ? weapon : GUN_MELEE;
        }
        else if (actor == hud)
        {
            killFeed.hud.lastKillAttack = atk;
            killFeed.hud.lastKillWeapon = weapon;
            killFeed.hud.lastKillDeathState = d->deathstate;
        }
        // Hooks.
        if(d == self)
        {
            execident("on_death");
            if (d == actor)
            {
                execident("on_suicide");
            }
        }
        else if(actor == self)
        {
            if (isally(actor, d))
            {
                execident("on_teamkill");
            }
            else
            {
                execident("on_kill");
            }
        }
        execident("on_obituary");
    }
    ICOMMAND(getkillfeedtype, "", (), intret(killFeed.type));
    ICOMMAND(getkillfeedactor, "", (), intret(killFeed.actor));
    ICOMMAND(getkillfeedvictim, "", (), intret(killFeed.victim));
    ICOMMAND(getkillfeedweapon, "", (), intret(killFeed.weapon));
    ICOMMAND(getkillfeedattack, "", (), intret(killFeed.attack));
    ICOMMAND(getkillfeedcrit, "", (), intret(killFeed.crit));
    ICOMMAND(getkillfeeddeath, "", (), intret(killFeed.deathState));
    ICOMMAND(getkillfeedmedal, "", (), intret(killFeed.medal));
    ICOMMAND(getlastkillvictim, "", (), intret(killFeed.hud.lastKillVictim));
    ICOMMAND(getlastkillweapon, "", (), intret(killFeed.hud.lastKillWeapon));
    ICOMMAND(getlastkillattack, "", (), intret(killFeed.hud.lastKillAttack));
    ICOMMAND(getlastkilldeathstate, "", (), intret(killFeed.hud.lastKillDeathState));
    ICOMMAND(getlastkillerweapon, "", (), intret(killFeed.hud.lastKillerWeapon));
    ICOMMAND(getlastkillerattack, "", (), intret(killFeed.hud.lastKillerAttack));
    ICOMMAND(getlastdeathstate, "", (), intret(killFeed.hud.lastDeathState));
    ICOMMAND(getlastkiller, "", (),
    {
        gameent * d = followingplayer(self);
        if (d)
        {
            intret(d->lastattacker);
        }
    });

    VARR(mapdeath, 0, Death_Default, Death_Num);
    VARP(gore, 0, 1, 1);

    int getdeathstate(gameent* d, int atk, int flags)
    {
        if (gore && d->shouldgib())
        {
            return Death_Gib;
        }
        if (flags)
        {
            if (flags & KILL_HEADSHOT)
            {
                return Death_Headshot;
            }
        }
        if (validatk(atk))
        {
            return attacks[atk].deathstate;
        }
        return mapdeath;
    }

    VARP(killsound, 0, 1, 1);

    void kill(gameent *d, gameent *actor, int atk, int flags)
    {
        if(d->state==CS_EDITING)
        {
            d->editstate = CS_DEAD;
            d->deaths++;
            if(d!=self) d->resetinterp();
            return;
        }
        else if((d->state!=CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING) || intermission) return;
        if (actor == followingplayer(self))
        {
            if (actor->role == ROLE_BERSERKER)
            {
                playsound(S_BERSERKER);
            }
            else if (killsound)
            {
                if (actor == d)
                {
                    if (validatk(atk))
                    {
                        playsound(S_KILL_SELF);
                    }
                }
                else if (isally(actor, d) && !m_hideallies)
                {
                    playsound(S_KILL_ALLY);
                }
                else playsound(S_KILL);
            }
        }
        // Update player state and reset AI.
        d->deathstate = getdeathstate(d, atk, flags);
        setdeathstate(d);
        ai::kill(d, actor);
        // Write obituary (console messages, kill feed) and manage announcements.
        writeobituary(d, actor, atk, flags);
        announcer::parseannouncements(d, actor, flags);
    }

    void updatetimer(int time, int type)
    {
        maplimit = lastmillis + time * 1000;
        if (type == TimeUpdate_Intermission)
        {
            // End the game and start the intermission period,
            // to allow players to vote the next map and catch a breather.
            intermission = true;
            self->attacking = ACT_IDLE;
            self->pitch = self->roll = 0;
            if(cmode) cmode->gameover();
            conoutf(CON_GAMEINFO, "\f2Intermission: game has ended!");
            bestteams.shrink(0);
            bestplayers.shrink(0);
            if(m_teammode) getbestteams(bestteams);
            else getbestplayers(bestplayers);

            if(validteam(self->team) ? bestteams.htfind(self->team) >= 0 : bestplayers.find(self) >= 0)
            {
                playsound(S_INTERMISSION_WIN);
                announcer::playannouncement(S_ANNOUNCER_WIN);
            }
            else
            {
                playsound(S_INTERMISSION);
            }
            camera::camera.zoomstate.disable();
            execident("on_intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(self->frags));
    ICOMMAND(getflags, "", (), intret(self->flags));
    ICOMMAND(getdeaths, "", (), intret(self->deaths));
    ICOMMAND(getaccuracy, "", (), intret((self->totaldamage*100)/max(self->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(self->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(self->totalshots));
    ICOMMAND(getlastswitchattempt, "", (),
    {
        gameent * d = followingplayer(self);
        intret(d->lastswitchattempt);
    });
    ICOMMAND(getlastswitch, "", (),
    {
        gameent * d = followingplayer(self);
        intret(d->lastswitch);
    });

    vector<gameent *> clients;

    gameent *newclient(int cn)   // ensure valid entity
    {
        if(cn < 0 || cn > max(0xFF, MAXCLIENTS + MAXBOTS))
        {
            neterr("clientnum", false);
            return NULL;
        }

        if(cn == self->clientnum) return self;

        while(cn >= clients.length()) clients.add(NULL);
        if(!clients[cn])
        {
            gameent *d = new gameent;
            d->clientnum = cn;
            clients[cn] = d;
            players.add(d);
        }
        return clients[cn];
    }

    gameent *getclient(int cn)   // ensure valid entity
    {
        if(cn == self->clientnum) return self;
        return clients.inrange(cn) ? clients[cn] : NULL;
    }

    void clientdisconnected(int cn, bool notify)
    {
        if(!clients.inrange(cn)) return;
        unignore(cn);
        gameent *d = clients[cn];
        if(d)
        {
            if(notify && d->name[0])
            {
                if(d->aitype == AI_NONE)
                {
                    conoutf(CON_CHAT, "%s \fs\f4left the game\fr", colorname(d));
                }
                else conoutf(CON_GAMEINFO, "\fs\f2Bot removed:\fr %s", colorname(d));
            }
            projectile::removeprojectiles(d);
            removetrackedparticles(d);
            removetrackeddynlights(d);
            if(cmode) cmode->removeplayer(d);
            removegroupedplayer(d);
            players.removeobj(d);
            DELETEP(clients[cn]);
            cleardynentcache();
        }
        if(following == cn)
        {
            if(specmode) nextfollow();
            else stopfollowing();
        }
    }

    void clearclients(bool notify)
    {
        loopv(clients) if(clients[i]) clientdisconnected(i, notify);
    }

    void initclient()
    {
        self = spawnstate(new gameent);
        filtertext(self->name, "player", false, false, true, false, MAXNAMELEN);
        players.add(self);
    }

    VARP(showmodeinfo, 0, 1, 1);

    void cleargame()
    {
        projectile::removeprojectiles();
        weapon::clearweapons();
        clearmonsters();
        clearragdolls();
        clearteaminfo();
        camera::reset();
        announcer::reset();
    }

    void startgame()
    {
        cleargame();

        // reset perma-state
        loopv(players) players[i]->startgame();

        setclientmode();

        intermission = betweenrounds = false;
        if(!m_round || m_hunt) gamewaiting = false;
        maptime = maprealtime = 0;
        maplimit = -1;

        if(cmode)
        {
            cmode->preload();
            cmode->setup();
        }

        const char *info = !m_story && m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
        if(showmodeinfo && info)
        {
            conoutf("%s", info);
            if(mutators) loopi(NUMMUTATORS)
            {
                if(!(mutators & mutator[i].flags)) continue;
                conoutf("%s", mutator[i].info);
            }
        }

        syncplayer();

        camera::camera.zoomstate.disable();

        execident("on_mapstart");
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);

        entities::respawnent = -1;
        if(!m_mp(gamemode)) spawnplayer(self);
        else findplayerspawn(self, -1, m_teammode ? self->team : 0);
        entities::resetspawns();
        copystring(clientmap, name ? name : "");

        sendmapinfo();
    }

    SVAR(tips, "");

    const char* getmapinfo()
    {
        const bool hasModeInfo = !m_story && showmodeinfo && m_valid(gamemode);
        static char info[1000];
        info[0] = '\0';
        if (hasModeInfo)
        {
            strcat(info, gamemodes[gamemode - STARTGAMEMODE].info);
        }
        if (tips[0] != '\0')
        {
            vector<char*> tipList;
            explodelist(tips, tipList);
            if (!tipList.empty())
            {
                strncat(info, "\n\n", sizeof(info) - strlen(info) - 1);
                strncat(info, tipList[rnd(tipList.length())], sizeof(info) - strlen(info) - 1);
            }
            tipList.deletearrays();
        }
        return hasModeInfo ? info : NULL;
    }

    const char *getscreenshotinfo()
    {
        return server::modename(gamemode, NULL);
    }

    void msgsound(int n, physent *d)
    {
        if(d->state == CS_DEAD || d->state == CS_SPECTATOR) return;
        if(!d || d == self)
        {
            addmsg(N_SOUND, "ci", d, n);
            playsound(n, NULL, NULL);
        }
        else
        {
            if(d->type==ENT_PLAYER && ((gameent *)d)->ai) {
                addmsg(N_SOUND, "ci", d, n);
            }
            playsound(n, d);
        }
    }

    int numdynents(const int flags)
    {
        int length = 0;
        if (flags & DYN_PLAYER)
        {
            length += players.length();
        }
        if (flags & DYN_AI)
        {
            length += monsters.length();
        }
        if (flags & DYN_PROJECTILE)
        {
            length += projectile::attackProjectiles.length();
        }
        return length;
    }

    dynent *iterdynents(int i, const int flags)
    {
        if (flags & DYN_PLAYER || !flags)
        {
            /* Return valid entities when flags are absent to prevent null pointer dereferencing.
             * Need at least one valid iteration for essential entities like "camera1" or "player".
             */
            if (i < players.length())
            {
                return players[i];
            }
            i -= players.length();
        }
        if (flags & DYN_AI)
        {
            if (i < monsters.length())
            {
                return (dynent*)monsters[i];
            }
            i -= monsters.length();
        }
        if (flags & DYN_PROJECTILE)
        {
            if (i < projectile::attackProjectiles.length())
            {
                return (dynent*)projectile::attackProjectiles[i];
            }
            i -= projectile::attackProjectiles.length();
        }
        return NULL;
    }

    bool duplicatename(gameent *d, const char *name = NULL, const char *alt = NULL)
    {
        if(!name) name = d->name;
        if(alt && d != self && !strcmp(name, alt)) return true;
        loopv(players) if(d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    const char *colorname(gameent *d, const char *name, const char * alt, const char *color)
    {
        if(!name) name = alt && d == self ? alt : d->name;
        bool dup = !name[0] || duplicatename(d, name, alt);
        if(dup || color[0])
        {
            if(dup) return tempformatstring("\fs%s%s \f5(%d)\fr", color, name, d->clientnum);
            return tempformatstring("\fs%s%s\fr", color, name);
        }
        return name;
    }

    VARP(teamcolortext, 0, 1, 1);

    const char *teamcolorname(gameent *d, const char *alt)
    {
        const int team = teamcolortext && m_teammode && validteam(d->team) ? d->team : 0;
        return colorname(d, NULL, alt, teamtextcode[team]);
    }

    const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt)
    {
        if(!teamcolortext || !m_teammode || !validteam(team)) return alt;
        return tempformatstring("\fs%s%s%s%s\fr", teamtextcode[team], prefix, teamnames[team], suffix);
    }

    bool isghost(gameent *d)
    {
        return m_round && (d->state==CS_DEAD || (d->state==CS_SPECTATOR && d->ghost));
    }

    const char *chatcolor(gameent *d)
    {
        if(isghost(d)) return "\f4";
        else if(d->state==CS_SPECTATOR) return "\f8";
        else return "\ff";
    }

    void hurt(gameent* d)
    {
        // Apply environmental damage locally when inside harmful materials like lava.
        const bool isValidEntity = d->type == ENT_PLAYER || d->type == ENT_AI;
        if (m_mp(gamemode) || !isValidEntity || d->state != CS_ALIVE || d->haspowerup(PU_INVULNERABILITY))
        {
            /* This is local, so we call this function to hurt a local player or NPC.
             * Of course, we are going to stop caring if the entity is not alive or invulnerable.
             */
            return;
        }

        if (lastmillis - d->lasthurt >= DAMAGE_ENVIRONMENT_DELAY)
        {
            // If the delay has elapsed, apply environmental damage to the entity.
            weapon::damageplayer(DAMAGE_ENVIRONMENT, d, d, d->o, -1, Hit_Environment, true);
            d->lasthurt = lastmillis;
        }
    }

    void suicide(gameent *d)
    {
        if(d==self || (d->type==ENT_PLAYER && ((gameent *)d)->ai))
        {
            if(d->state!=CS_ALIVE) return;
            if (!m_mp(gamemode))
            {
                kill(d, d, -1);
            }
            else
            {
                int seq = (d->lifesequence<<16) | ((lastmillis / 1000) & 0xFFFF);
                if(d->suicided!=seq)
                { 
                    addmsg(N_SUICIDE, "rc", d);
                    d->suicided = seq;
                }
            }
        }
        else if(d->type == ENT_AI) suicidemonster((monster *)d);
    }
    ICOMMAND(suicide, "", (), suicide(self));

    int maxsoundradius(int n)
    {
        switch(n)
        {
            case S_BERSERKER:
            case S_BERSERKER_LOOP:
            case S_ROCKET_EXPLODE:
                return 600;

            case S_JUMP1:
            case S_JUMP2:
            case S_LAND:
            case S_ITEM_SPAWN:
            case S_WEAPON_LOAD:
            case S_WEAPON_NOAMMO:
                return 350;

            case S_FOOTSTEP:
            case S_FOOTSTEP_DIRT:
            case S_FOOTSTEP_METAL:
            case S_FOOTSTEP_WOOD:
            case S_FOOTSTEP_DUCT:
            case S_FOOTSTEP_SILKY:
            case S_FOOTSTEP_SNOW:
            case S_FOOTSTEP_ORGANIC:
            case S_FOOTSTEP_GLASS:
            case S_FOOTSTEP_WATER:
                return 300;

            case S_BOUNCE_EJECT1:
            case S_BOUNCE_EJECT2:
            case S_BOUNCE_EJECT3:
                return 100;

            default: return 500;
        }
    }

    const char *mastermodecolor(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodecolors)/sizeof(mastermodecolors[0])) ? mastermodecolors[n-MM_START] : unknown;
    }

    const char *mastermodeicon(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodeicons)/sizeof(mastermodeicons[0])) ? mastermodeicons[n-MM_START] : unknown;
    }

    ICOMMAND(servinfomode, "i", (int *i), GETSERVINFOATTR(*i, 0, mode, intret(mode)));
    ICOMMAND(servinfomodename, "i", (int *i),
        GETSERVINFOATTR(*i, 0, mode,
        {
            const char *name = server::modeprettyname(mode, NULL);
            if(name) result(name);
        }));
    ICOMMAND(servinfomastermode, "i", (int *i), GETSERVINFOATTR(*i, 2, mm, intret(mm)));
    ICOMMAND(servinfomastermodename, "i", (int *i),
        GETSERVINFOATTR(*i, 2, mm,
        {
            const char *name = server::mastermodename(mm, NULL);
            if(name) stringret(newconcatstring(mastermodecolor(mm, ""), name));
        }));
    ICOMMAND(servinfomastermodeicon, "i", (int *i),
        GETSERVINFOATTR(*i, 2, mm,
        {
            result(si->maxplayers > 0 && si->numplayers >= si->maxplayers ? "server_full" : mastermodeicon(mm, "server_unknown"));
        }));
    ICOMMAND(servinfotime, "ii", (int *i, int *raw),
        GETSERVINFOATTR(*i, 1, secs,
        {
            secs = clamp(secs, 0, 59*60+59);
            if(*raw) intret(secs);
            else
            {
                int mins = secs/60;
                secs %= 60;
                result(tempformatstring("%d:%02d", mins, secs));
            }
        }));

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *gameconfig() { return "config/game.cfg"; }
    const char *savedconfig() { return "config/saved.cfg"; }
    const char *defaultconfig() { return "config/default.cfg"; }
    const char *autoexec() { return "autoexec.cfg"; }
    const char *savedservers() { return "config/server/history.cfg"; }

    void loadconfigs()
    {
        execfile("config/server/auth.cfg", false);
    }

    bool clientoption(const char *arg) { return false; }
}
