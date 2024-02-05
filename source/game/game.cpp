#include "game.h"

extern int zoom;

namespace game
{
    bool intermission = false;
    int maptime = 0, maprealtime = 0, maplimit = -1;
    int lastspawnattempt = 0;

    gameent *self = NULL; // ourselves (our client)
    vector<gameent *> players; // other clients
    vector<gameent *> bestplayers;
    vector<int> bestteams;

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
        thirdperson = 0;
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
        clearprojectiles();
        clearbouncers();
        clearmonsters();
    }

    int vooshgun;

    gameent *spawnstate(gameent *d) // reset player state not persistent across spawns
    {
        d->respawn();
        d->spawnstate(gamemode, mutators, vooshgun);
        return d;
    }

    VARP(queuerespawn, 0, 1, 1);

    void respawnself()
    {
        if(ispaused()) return;
        if(queuerespawn && lastmillis - self->lastpain <= RESPAWN_WAIT)
        {
            self->respawnqueued = true;
            return;
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
            hidescoreboard();
            if(cmode) cmode->respawned(self);
        }
        execident("on_spawn");
    }

    gameent *pointatplayer()
    {
        loopv(players) if(players[i] != self && intersect(players[i], self->o, worldpos)) return players[i];
        return NULL;
    }

    gameent *hudplayer()
    {
        if((thirdperson && allowthirdperson()) || specmode > 1) return self;
        return followingplayer(self);
    }

    void setupcamera()
    {
        gameent *target = followingplayer();
        if(target)
        {
            self->yaw = target->yaw;
            self->pitch = target->state==CS_DEAD ? 0 : target->pitch;
            self->o = target->o;
            self->resetinterp();
        }
    }

    bool detachcamera()
    {
        gameent *d = followingplayer();
        if(d) return specmode > 1 || d->state == CS_DEAD;
        return (intermission && self->state != CS_SPECTATOR) || self->state == CS_DEAD;
    }

    bool collidecamera()
    {
        switch(self->state)
        {
            case CS_EDITING: return false;
            case CS_SPECTATOR: return followingplayer()!=NULL;
        }
        return true;
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
            moveplayer(d, 1, false);
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
                if(d->powerupmillis || d->role == ROLE_JUGGERNAUT)
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
                crouchplayer(d, 10, false);
                if(smoothmove && d->smoothmillis>0) predictplayer(d, true);
                else moveplayer(d, 1, false);
            }
            else if(d->state==CS_DEAD && !d->ragdoll && lastmillis-d->lastpain<2000) moveplayer(d, 1, true);
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

        physicsframe();
        ai::navigate();
        if(self->state != CS_DEAD && !intermission)
        {
            if(self->powerupmillis || self->role == ROLE_JUGGERNAUT)
            {
                entities::updatepowerups(curtime, self);
            }
        }
        updateweapons(curtime);
        otherplayers(curtime);
        ai::update();
        moveragdolls();
        gets2c();
        updatemonsters(curtime);
        if(connected)
        {
            if(self->state == CS_DEAD)
            {
                if(self->ragdoll) moveragdoll(self);
                else if(lastmillis-self->lastpain<2000)
                {
                    self->move = self->strafe = 0;
                    moveplayer(self, 10, true);
                }
                if(self->respawnqueued && lastmillis - self->lastpain > RESPAWN_WAIT)
                {
                    respawnself();
                    self->respawnqueued = false;
                }
            }
            else if(!intermission)
            {
                if(self->ragdoll) cleanragdoll(self);
                crouchplayer(self, 10, true);
                moveplayer(self, 10, true);
                swayhudgun(curtime);
                entities::checkitems(self);
                if(cmode) cmode->checkitems(self);
            }
            else if(self->state == CS_SPECTATOR) moveplayer(self, 10, true);
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

    void spawnplayer(gameent *d)   // place at random spawn
    {
        if(cmode) cmode->pickspawn(d);
        else findplayerspawn(d, -1, m_teammode ? d->team : 0);
        spawnstate(d);
        if(d==self)
        {
            if(editmode) d->state = CS_EDITING;
            else if(d->state != CS_SPECTATOR) d->state = CS_ALIVE;
        }
        else d->state = CS_ALIVE;
        checkfollow();
    }

    void spawneffect(gameent *d)
    {
        if(d==followingplayer(self))
        {
            clearscreeneffects();
            addscreenfx(200);
        }
        stopownersounds(d);
        playsound(S_SPAWN, d);
        particle_splash(PART_SPARK2, 250, 200, d->o, getplayercolor(d, d->team), 0.60f, 200, 5);
    }

    void respawn()
    {
        if(self->state==CS_DEAD)
        {
            self->attacking = ACT_IDLE;
            int wait = cmode ? cmode->respawnwait(self) : 0;
            if(wait>0)
            {
                lastspawnattempt = lastmillis;
                return;
            }
            respawnself();
        }
    }
    COMMAND(respawn, "");

    // inputs

    inline bool checkaction(int &act, const int gun)
    {
        if(guns[gun].haszoom)
        {
            if(act == ACT_SECONDARY)
            {
                execident("dozoom");
                return false;
            }
            if(act == ACT_PRIMARY)
            {
               if(zoom) act = ACT_SECONDARY;
            }
        }
        return true;
    }

    void doaction(int act)
    {
        if(!connected || intermission || lastmillis-self->lasttaunt < 1000) return;
        if(!checkaction(act, self->gunselect)) return;
        if((self->attacking = act)) respawn();
    }
    ICOMMAND(primary, "D", (int *down), doaction(*down ? ACT_PRIMARY : ACT_IDLE));
    ICOMMAND(secondary, "D", (int *down), doaction(*down ? ACT_SECONDARY : ACT_IDLE));
    ICOMMAND(melee, "D", (int *down), doaction(*down ? ACT_MELEE : ACT_IDLE));

    bool canjump()
    {
        if(!connected || intermission) return false;
        respawn();
        return self->state!=CS_DEAD;
    }

    bool cancrouch()
    {
        if(!connected || intermission) return false;
        return self->state!=CS_DEAD;
    }

    bool allowmove(physent *d)
    {
        if(d->type!=ENT_PLAYER || d->state == CS_SPECTATOR) return true;
        return !intermission && !(gore && ((gameent *)d)->gibbed());
    }

    bool isally(gameent *a, gameent *b)
    {
        return (validteam(a->team) && validteam(b->team) && sameteam(a->team, b->team))
               || (m_infection && ((a->role == ROLE_ZOMBIE && b->role == ROLE_ZOMBIE)
               || (a->role != ROLE_ZOMBIE && b->role != ROLE_ZOMBIE)))
               || (m_invasion && a->type == ENT_PLAYER && b->type == ENT_PLAYER);
    }

    bool isinvulnerable(gameent *target, gameent *actor)
    {
        return target->haspowerup(PU_INVULNERABILITY) && !actor->haspowerup(PU_INVULNERABILITY);
    }

    bool allowthirdperson()
    {
        return self->state==CS_SPECTATOR || m_edit;
    }
    ICOMMAND(allowthirdperson, "", (), intret(allowthirdperson()));

    bool editing() { return m_edit; }

    bool shoulddrawzoom()
    {
        return zoom && guns[self->gunselect].haszoom
               && (self->state == CS_ALIVE || self->state == CS_LAGGED);
    }

    FVARP(damagerolldiv, 0, 2.0f, 5.0f);

    void damagehud(int damage, gameent *d, gameent *actor)
    {
        damageblend(damage);
        if(d != actor) damagecompass(damage, actor->o);
        if(!damagerolldiv) return;
        float damroll = damage/damagerolldiv;
        d->roll += d->roll > 0 ? damroll : (d->roll < 0 ? -damroll : (rnd(2) ? damroll : -damroll));
    }

    VARP(hitsound, 0, 0, 1);

    void damaged(int damage, vec &p, gameent *d, gameent *actor, int atk, int flags, bool local)
    {
        if((d->state!=CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING) || intermission)
        {
            return;
        }
        if(local)
        {
            damage = d->dodamage(damage);
        }
        else if(actor == self && !(flags & HIT_MATERIAL)) return;
        else if(!isinvulnerable(d, actor)) d->lastpain = lastmillis;

        damageeffect(damage, d, p, atk, getbloodcolor(d), flags & HIT_HEAD);

        if(isinvulnerable(d, actor)) return;

        gameent *hud = hudplayer();
        if(hud != self && actor == hud && d != actor)
        {
            if(hitsound && actor->lasthit != lastmillis)
            {
                playsound(isally(d, actor) ? S_HIT_ALLY : S_HIT);
                actor->lasthit = lastmillis;
            }
        }
        if(d == hud)
        {
            damagehud(damage, d, actor);
        }

        ai::damaged(d, actor);
        if(local && d->health <= 0)
        {
            kill(d, actor, NULL);
        }
    }

    VARP(gore, 0, 1, 1);
    VARP(deathfromabove, 0, 1, 1);

    void deathstate(gameent *d, bool restore)
    {
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        stopownersounds(d);
        if(!restore)
        {
            if(gore && d->gibbed()) gibeffect(max(-d->health, 0), d->vel, d);
            else if(attacks[d->deathattack].action != ACT_MELEE)
            {
                playsound(getplayermodelinfo(d).diesound, d); // silent melee kills?
            }
            d->deaths++;
        }
        if(d==self)
        {
            disablezoom();
            d->attacking = ACT_IDLE;
            if(!restore && deathfromabove)
            {
                d->pitch = -90; // lower your pitch to see your death from above
            }
            d->roll = 0;
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            d->smoothmillis = 0;
        }
        d->stopweaponsound();
        d->stoppowerupsound();
    }

    int killfeedactorcn = -1, killfeedtargetcn = -1, killfeedweaponinfo = -1;
    bool killfeedheadshot = false;

    void writeobituary(gameent *d, gameent *actor, int atk, int flags)
    {
        // console messages
        gameent *h = followingplayer(self);
        if(!h) h = self;
        int contype = d==h || actor==h ? CON_FRAG_SELF : CON_FRAG_OTHER;
        const char *act = "killed";
        if(flags & KILL_TRAITOR)
        {
            act = "was assassinated";
            conoutf(contype, "%s \fs\f2%s\fr", teamcolorname(d), act);
            killfeedweaponinfo = -3;
        }
        else if(d == actor && d->role != ROLE_ZOMBIE)
        {
            act = "suicided";
            conoutf(contype, "%s \fs\f2%s\fr", teamcolorname(d), act);
            killfeedweaponinfo = -2;
        }
        else
        {
            if(attacks[atk].gun == GUN_ZOMBIE)
            {
                if(d==actor) act = "got infected";
                else act = "infected";
            }
            if(isally(d, actor)) conoutf(contype, "%s \fs\f2%s an ally (\fr%s\fs\f2)\fr", teamcolorname(actor), act, teamcolorname(d));
            else conoutf(contype, "%s \fs\f2%s\fr %s", teamcolorname(actor), act, teamcolorname(d));
            killfeedweaponinfo = attacks[atk].action == ACT_MELEE ? -1 : attacks[atk].gun;
        }
        // kill feed
        killfeedactorcn = actor->clientnum;
        killfeedtargetcn = d->clientnum;
        killfeedheadshot = flags & KILL_HEADSHOT;
        execident("on_killfeed");
    }
    ICOMMAND(getkillfeedactor, "", (), intret(killfeedactorcn));
    ICOMMAND(getkillfeedtarget, "", (), intret(killfeedtargetcn));
    ICOMMAND(getkillfeedweap, "", (), intret(killfeedweaponinfo));
    ICOMMAND(getkillfeedcrit, "", (), intret(killfeedheadshot? 1: 0));

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
        writeobituary(d, actor, atk, flags); // obituary (console messages, kill feed)
        if(flags)
        {
            const char *spree = "";
            if(actor->aitype == AI_BOT) taunt(actor); // bots taunting players when getting extraordinary kills
            if(actor == followingplayer(self))
            {
                if(flags & KILL_FIRST)
                {
                    playsound(S_ANNOUNCER_FIRST_BLOOD, NULL, NULL, NULL, SND_ANNOUNCER);
                    conoutf(CON_GAMEINFO, "%s \f2drew first blood!", colorname(actor));
                }
                if(flags & KILL_SPREE)
                {
                    playsound(S_ANNOUNCER_KILLING_SPREE, NULL, NULL, NULL, SND_ANNOUNCER);
                    spree = "\f2killing";
                }
                if(flags & KILL_SAVAGE)
                {
                    playsound(S_ANNOUNCER_SAVAGE, NULL, NULL, NULL, SND_ANNOUNCER);
                    spree = "\f6savage";
                }
                if(flags & KILL_UNSTOPPABLE)
                {
                    playsound(S_ANNOUNCER_UNSTOPPABLE, NULL, NULL, NULL, SND_ANNOUNCER);
                    spree = "\f3unstoppable";
                }
                if(flags & KILL_LEGENDARY)
                {
                    playsound(S_ANNOUNCER_LEGENDARY, NULL, NULL, NULL, SND_ANNOUNCER);
                    spree = "\f5legendary";
                }
                if(spree[0] != '\0') conoutf(CON_GAMEINFO, "%s \f2is on a \fs%s\fr spree!", colorname(actor), spree);
            }
        }
        if(actor == followingplayer(self))
        {
            if(killsound && actor != d) playsound(isally(d, actor) ? S_KILL_ALLY : S_KILL);
            if(flags & KILL_HEADSHOT) playsound(S_ANNOUNCER_HEADSHOT, NULL, NULL, NULL, SND_ANNOUNCER);
        }
        // update player state and reset ai
        d->deathattack = atk;
        deathstate(d);
        ai::kill(d, actor);
        // events
        if(d == self)
        {
            execident("on_death");
            if(d == actor) execident("on_suicide");
        }
        else if(actor == self)
        {
            execident("on_kill");
            if(isally(actor, d)) execident("on_teamkill");
        }
    }

    void timeupdate(int secs)
    {
        if(secs > 0) // set client side timer
        {
            maplimit = lastmillis + secs*1000;
        }
        else // end the game and start intermission timer
        {
            maplimit = lastmillis + 45*1000;
            intermission = true;
            self->attacking = ACT_IDLE;
            if(cmode) cmode->gameover();
            conoutf(CON_GAMEINFO, "\f2Intermission: game has ended!");
            bestteams.shrink(0);
            bestplayers.shrink(0);
            if(m_teammode) getbestteams(bestteams);
            else getbestplayers(bestplayers);

            if(validteam(self->team) ? bestteams.htfind(self->team)>=0 : bestplayers.find(self)>=0)
            {
                playsound(S_INTERMISSION_WIN);
                playsound(S_ANNOUNCER_WIN, NULL, NULL, NULL, SND_ANNOUNCER);
            }
            else playsound(S_INTERMISSION);
            disablezoom();
            execident("on_intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(self->frags));
    ICOMMAND(getflags, "", (), intret(self->flags));
    ICOMMAND(getdeaths, "", (), intret(self->deaths));
    ICOMMAND(getaccuracy, "", (), intret((self->totaldamage*100)/max(self->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(self->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(self->totalshots));

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
            removeweapons(d);
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

    void startgame()
    {
        clearprojectiles();
        clearbouncers();
        clearmonsters();
        clearragdolls();

        clearteaminfo();

        // reset perma-state
        loopv(players) players[i]->startgame();

        setclientmode();

        intermission = false;
        maptime = maprealtime = 0;
        maplimit = -1;

        if(cmode)
        {
            cmode->preload();
            cmode->setup();
        }

        const char *info = m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
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

        hidescoreboard();
        disablezoom();

        execident("on_mapstart");
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);

        if(!m_mp(gamemode)) spawnplayer(self);
        else findplayerspawn(self, -1, m_teammode ? self->team : 0);
        entities::resetspawns();
        copystring(clientmap, name ? name : "");

        sendmapinfo();
    }

    vector<char *> tips;
    ICOMMAND(registertip, "s", (char *tip), { tips.add(newstring(tip)); });

    const char *getmapinfo()
    {
        bool hasmodeinfo = showmodeinfo && m_valid(gamemode);
        static char info[1000];
        info[0] = '\0';
        if(hasmodeinfo) strcat(info, gamemodes[gamemode - STARTGAMEMODE].info);
        if(!tips.empty())
        {
             if(hasmodeinfo) strcat(info, "\n\n");
             strcat(info, tips[rnd(tips.length())]);
        }
        return showmodeinfo && m_valid(gamemode) ? info : NULL;
    }

    const char *getscreenshotinfo()
    {
        return server::modename(gamemode, NULL);
    }

    int footstepsound(int surface, int material)
    {
        if(material & MAT_WATER) return S_FOOTSTEP_WATER;
        else switch(surface)
        {
            case 1: return S_FOOTSTEP_SOFT; break;
            case 2: return S_FOOTSTEP_METAL; break;
            case 3: return S_FOOTSTEP_WOOD; break;
            default: return S_FOOTSTEP; break;
        }
    }

    VARP(footstepssound, 0, 1, 1);
    VARP(footstepdelay, 1, 44000, 50000);

    void footstep(physent *pl, int sound)
    {
        if(!footstepssound || pl->physstate < PHYS_SLOPE
           || (pl->crouching && pl->crouched()) || pl->blocked)
        {
            return;
        }
        gameent *d = (gameent *)pl;
        if(d->move || d->strafe)
        {
            if(lastmillis - d->lastfootstep < (footstepdelay / d->vel.magnitude())) return;
            else playsound(sound, d);
        }
        d->lastfootstep = lastmillis;
    }

    void triggerphysicsevent(physent *pl, int event, int material)
    {
        if(pl->state > CS_DEAD) return;
        switch(event)
        {
            case PHYSEVENT_JUMP:
            {
                if(material & MAT_WATER || !(pl == self || pl->type != ENT_PLAYER || ((gameent *)pl)->ai))
                {
                    break;
                }
                if(!pl->timeinair) msgsound(S_JUMP1, pl);
                else msgsound(S_JUMP2, pl);
                break;
            }

            case PHYSEVENT_LAND_SHORT:
            case PHYSEVENT_FOOTSTEP:
            {
                if(!(pl == self || pl->type != ENT_PLAYER || ((gameent *)pl)->ai)) break;
                int surface = lookuptexturematerial(pl->feetpos(-1)),
                    sound = footstepsound(surface, lookupmaterial(pl->feetpos()));
                if(event == PHYSEVENT_FOOTSTEP) footstep(pl, sound);
                else msgsound(sound, pl);
                break;
            }

            case PHYSEVENT_LAND_MEDIUM:
            {
                if(!(pl == self || pl->type != ENT_PLAYER || ((gameent *)pl)->ai)) break;
                msgsound(material & MAT_WATER ? S_LAND_WATER : S_LAND, pl);
                ((gameent *)pl)->lastland = lastmillis;
                break;
            }

            case PHYSEVENT_RAGDOLL_COLLIDE:
            {
                playsound(S_CORPSE, NULL, pl == self ? NULL : &pl->o);
                break;
            }

            case PHYSEVENT_LIQUID_IN:
            {
                playsound(material == MAT_LAVA ? S_LAVA_IN : S_WATER_IN, NULL, pl == self ? NULL : &pl->o);
                break;
            }

            case PHYSEVENT_LIQUID_OUT:
            {
                if(material == MAT_LAVA) break;
                playsound(S_WATER_OUT, NULL, pl == self ? NULL : &pl->o);
                break;
            }

            default: break;
        }
    }

    void dynentcollide(physent *d, physent *o, const vec &dir)
    {
        if(d->type == ENT_AI)
        {
            if(dir.z > 0) stackmonster((monster *)d, o);
        }
    }

    void msgsound(int n, physent *d)
    {
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

    int numdynents()
    {
        return players.length() + monsters.length();
    }

    dynent *iterdynents(int i)
    {
        if(i<players.length()) return players[i];
        i -= players.length();
        if(i<monsters.length()) return (dynent *)monsters[i];
        i -= monsters.length();
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
        if(!teamcolortext || !m_teammode || !validteam(d->team)) return colorname(d, NULL, alt);
        return colorname(d, NULL, alt, teamtextcode[d->team]);
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

    void suicide(physent *d)
    {
        if(d==self || (d->type==ENT_PLAYER && ((gameent *)d)->ai))
        {
            if(d->state!=CS_ALIVE) return;
            gameent *pl = (gameent *)d;
            if(!m_mp(gamemode)) kill(pl, pl, -1);
            else
            {
                int seq = (pl->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
                if(pl->suicided!=seq) { addmsg(N_SUICIDE, "rc", pl); pl->suicided = seq; }
            }
        }
        else if(d->type==ENT_AI) suicidemonster((monster *)d);
    }
    ICOMMAND(suicide, "", (), suicide(self));

    bool needminimap() { return m_ctf; }

    float abovegameplayhud(int w, int h)
    {
        switch(hudplayer()->state)
        {
            case CS_EDITING:
            case CS_SPECTATOR:
                return 1;
            default:
                return 1650.0f/1800.0f;
        }
    }

    float clipconsole(float w, float h)
    {
        if(cmode) return cmode->clipconsole(w, h);
        return 0;
    }

    VARP(allycrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 400, 1000);

    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 3: return "data/interface/crosshair/ally.png";
            case 2: return "data/interface/crosshair/default_hit.png";
            case 1: return "data/interface/crosshair/default.png";
            default: return "data/interface/crosshair/edit.png";
        }
    }

    int selectcrosshair(vec &col)
    {
        gameent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_DEAD || UI::uivisible("scoreboard") || intermission) return -1;

        if(d->state!=CS_ALIVE) return 0;

        int crosshair = 1;
        if(d->lasthit && lastmillis - d->lasthit < hitcrosshair) crosshair = 2;
        else if(allycrosshair)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);
            if(o && o->type==ENT_PLAYER && isally(((gameent *)o), d))
            {
                crosshair = 3;
                if(m_teammode) col = vec::hexcolor(teamtextcolor[d->team]);
            }
        }
        if(d->gunwait) col.mul(0.5f);
        return crosshair;
    }

    int maxsoundradius(int n)
    {
        switch(n)
        {
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
            case S_FOOTSTEP_SOFT:
            case S_FOOTSTEP_METAL:
            case S_FOOTSTEP_WOOD:
            case S_FOOTSTEP_WATER:
                return 300;

            case S_BOUNCE_CARTRIDGE_SG:
            case S_BOUNCE_CARTRIDGE_SMG:
            case S_BOUNCE_CARTRIDGE_RAILGUN:
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
    ICOMMAND(servinfoicon, "i", (int *i),
        GETSERVINFO(*i, si,
        {
            int mm = si->attr.inrange(2) ? si->attr[2] : MM_INVALID;
            result(si->maxplayers > 0 && si->numplayers >= si->maxplayers ? "server_full" : mastermodeicon(mm, "server_unknown"));
        }));

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *gameconfig() { return "config/game.cfg"; }
    const char *savedconfig() { return "config/saved.cfg"; }
    const char *defaultconfig() { return "config/default.cfg"; }
    const char *autoexec() { return "config/autoexec.cfg"; }
    const char *savedservers() { return "config/server/saved_servers.cfg"; }

    void loadconfigs()
    {
        execfile("config/server/auth.cfg", false);
    }

    bool clientoption(const char *arg) { return false; }
}

