#include "game.h"

namespace game
{
    bool intermission = false, betweenrounds = false;
    bool infection = false;
    int maptime = 0, maprealtime = 0, maplimit = -1;
    int lastspawnattempt = 0, lastheadshot = 0;

    gameent *player1 = NULL;         // our client
    vector<gameent *> players;       // other clients
    vector<gameent *> bestplayers;
    vector<int> bestteams;

    void taunt(gameent *d)
    {
        if(d->state!=CS_ALIVE || lastmillis-d->lasttaunt<1000) return;
        d->lasttaunt = lastmillis;
        addmsg(N_ANIMATION, "rci", d, ANIM_TAUNT);
        msgsound(d->tauntsound(), d);
    }
    ICOMMAND(taunt, "", (), taunt(player1));

    int following = -1;

    VARFP(specmode, 0, 0, 2,
    {
        if(!specmode) stopfollowing();
        else if(following < 0) nextfollow();
    });

    gameent *followingplayer()
    {
        if(player1->state!=CS_SPECTATOR || following<0) return NULL;
        gameent *target = getclient(following);
        if(target && target->state!=CS_SPECTATOR) return target;
        return NULL;
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
            if(player1->state != CS_SPECTATOR) return;
            cn = parseplayer(arg);
            if(cn == player1->clientnum) cn = -1;
        }
        if(cn < 0 && (following < 0 || specmode)) return;
        following = cn;
    }
    COMMAND(follow, "s");

    void nextfollow(int dir)
    {
        if(player1->state!=CS_SPECTATOR) return;
        int cur = following >= 0 ? following : (dir < 0 ? clients.length() - 1 : 0);
        loopv(clients)
        {
            cur = (cur + dir + clients.length()) % clients.length();
            if(clients[cur] && canfollow(player1, clients[cur]))
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
        if(player1->state != CS_SPECTATOR)
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
    }

    gameent *spawnstate(gameent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode, mutators, forceweapon);
        return d;
    }

    void respawnself()
    {
        if(ispaused()) return;
        if(m_mp(gamemode))
        {
            int seq = (player1->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
            if(player1->respawned!=seq) { addmsg(N_TRYSPAWN, "rc", player1); player1->respawned = seq; }
        }
        else
        {
            spawnplayer(player1);
            showscores(false);
            if(cmode) cmode->respawned(player1);
        }
    }

    gameent *pointatplayer()
    {
        loopv(players) if(players[i] != player1 && intersect(players[i], player1->o, worldpos)) return players[i];
        return NULL;
    }

    gameent *hudplayer()
    {
        if(thirdperson || specmode > 1) return player1;
        gameent *target = followingplayer();
        return target ? target : player1;
    }

    void setupcamera()
    {
        gameent *target = followingplayer();
        if(target)
        {
            player1->yaw = target->yaw;
            player1->pitch = target->state==CS_DEAD ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    bool detachcamera()
    {
        gameent *d = followingplayer();
        if(d) return specmode > 1 || d->state == CS_DEAD;
        return player1->state == CS_DEAD || intermission;
    }

    bool collidecamera()
    {
        switch(player1->state)
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
            if(d == player1 || d->ai) continue;

            if(d->state==CS_DEAD && d->ragdoll) moveragdoll(d);
            else if(!intermission && d->state==CS_ALIVE)
            {
                if(lastmillis - d->lastaction >= d->gunwait) d->gunwait = 0;
                if(d->haspowerups())
                    entities::updatepowerups(curtime, d);
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

    VARP(deathscore, 0, 1, 1);
    int waterchan = -1;

    void updateworld()        // main game update loop
    {
        if(!maptime) { maptime = lastmillis; maprealtime = totalmillis; return; }
        if(!curtime) { gets2c(); if(player1->clientnum>=0) c2sinfo(); return; }

        physicsframe();
        ai::navigate();
        if(player1->state == CS_ALIVE && player1->haspowerups())
            entities::updatepowerups(curtime, player1);
        updateweapons(curtime);
        otherplayers(curtime);
        ai::update();
        moveragdolls();
        gets2c();
        if(connected)
        {
            if(player1->state == CS_DEAD)
            {
                if(player1->ragdoll) moveragdoll(player1);
                else if(lastmillis-player1->lastpain<2000)
                {
                    player1->move = player1->strafe = 0;
                    moveplayer(player1, 10, true);
                }
                if(deathscore && lastmillis-player1->lastpain>950 && lastmillis-player1->lastpain<=1000) showscores(true);
            }
            else if(!intermission)
            {
                if(player1->ragdoll) cleanragdoll(player1);
                crouchplayer(player1, 10, true);
                moveplayer(player1, 10, true);
                swayhudgun(curtime);
                entities::checkitems(player1);
                if(cmode) cmode->checkitems(player1);
            }
        }
        int mat = lookupmaterial(camera1->o);
        if(player1->state!=CS_EDITING && mat&MAT_WATER) waterchan = playsound(S_WATER, NULL, NULL, NULL, 0, -1, 200, waterchan);
        else
        {
            if(waterchan >= 0)
            {
                stopsound(S_WATER, waterchan, 500);
                waterchan = -1;
            }
        }
        if(player1->clientnum>=0) c2sinfo();   // do this last, to reduce the effective frame lag
    }

    void spawnplayer(gameent *d)   // place at random spawn
    {
        if(cmode) cmode->pickspawn(d);
        else findplayerspawn(d, -1, m_teammode ? d->team : 0);
        spawnstate(d);
        if(d==player1)
        {
            if(editmode) d->state = CS_EDITING;
            else if(d->state != CS_SPECTATOR) d->state = CS_ALIVE;
        }
        else d->state = CS_ALIVE;
        checkfollow();
    }

    void respawn()
    {
        if(player1->state==CS_DEAD)
        {
            player1->attacking = ACT_IDLE;
            int wait = cmode ? cmode->respawnwait(player1) : 0;
            if(wait>0)
            {
                lastspawnattempt = lastmillis;
                //conoutf(CON_GAMEINFO, "\f2you must wait %d second%s before respawn!", wait, wait!=1 ? "s" : "");
                return;
            }
            respawnself();
        }
    }

    // inputs

    void checkaction(int act)
    {
        if(act == ACT_SECONDARY && guns[player1->gunselect].attacks[ACT_PRIMARY] == guns[player1->gunselect].attacks[ACT_SECONDARY])
        {
            zoom = !zoom? 1: -1;
            return;
        }
        else if(act > ACT_MELEE && player1->lastact > ACT_MELEE && player1->lastact != act && guns[player1->gunselect].attacks[ACT_COMBO] >= 0)
            act = ACT_COMBO;
        doaction(act);
    }

    void doaction(int act)
    {
        if(!connected || intermission) return;
        if((player1->attacking = act)) respawn();
        player1->lastact = act;
    }

    ICOMMAND(primary, "D", (int *down), checkaction(*down ? ACT_PRIMARY : ACT_IDLE));
    ICOMMAND(secondary, "D", (int *down), checkaction(*down ? ACT_SECONDARY : ACT_IDLE));
    ICOMMAND(melee, "D", (int *down), doaction(*down ? ACT_MELEE : ACT_IDLE));

    VARF(primaryweapon, -1, -1, 5, addmsg(N_SETWEAPONS, "riii", player1->clientnum, primaryweapon, player1->secondary));
    VARF(secondaryweapon, -1, -1, 5, addmsg(N_SETWEAPONS, "riii", player1->clientnum, player1->primary, secondaryweapon));

    void useitem()
    {
        if(!player1->item) return;
        addmsg(N_USEITEM, "rc", player1);
    }
    ICOMMAND(useitem, "", (), { useitem(); });

    bool canjump()
    {
        if(!connected || intermission) return false;
        respawn();
        return player1->state!=CS_DEAD;
    }

    bool cancrouch()
    {
        if(!connected || intermission) return false;
        return player1->state!=CS_DEAD;
    }

    bool allowmove(physent *d)
    {
        if(d->type!=ENT_PLAYER) return true;
        return !intermission && !(gore && ((gameent *)d)->state==CS_DEAD && ((gameent *)d)->health<=-50);
    }

    bool isally(gameent *a, gameent *b)
    {
        return isteam(a->team, b->team) || (m_infection && ((a->zombie && b->zombie) ||
                      (!a->zombie && !b->zombie)));
    }

    VARP(hitsound, 0, 0, 2);
    VARP(playheadshotsound, 0, 1, 2);

    void damaged(int damage, vec &p, gameent *d, gameent *actor, int atk, int flags, bool local)
    {
        if((d->state!=CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING) || intermission || server::betweenrounds) return;

        if((m_headhunter(mutators) && !(flags & HIT_HEAD)) || (!selfdam && d==actor) ||
           (!teamdam && isally(d, actor))) damage = 0;

        if(damage <= 0) return;

        if(local) damage = d->dodamage(damage);

        ai::damaged(d, actor);

        gameent *h = hudplayer();

        if(h!=player1 && actor==h && d!=actor)
        {
            if(hitsound && actor->lasthit != lastmillis)
                playsound(isally(d, actor) ? S_HIT_ALLY : (hitsound == 1 ? S_HIT1 : S_HIT2));
        }
        if(d!=actor) actor->lasthit = lastmillis;
        if(d->invulnmillis && !actor->invulnmillis) playsound(S_INVULNERABILITY_ACTION, d);
        if(!d->invulnmillis || (d->invulnmillis && actor->invulnmillis))
        {
            if(d==h) damagecompass(damage, actor->o);
            damageeffect(damage, d, p, atk, d!=h);
            if(flags & HIT_HEAD)
            {
                d->headless = true;
                if(!isally(d, actor) && !m_headhunter(mutators))
                {
                    if(actor==h)
                    {
                        if((playheadshotsound == 1 && attacks[atk].bonusdam) || (playheadshotsound > 1 && lastmillis-lastheadshot > 1000))
                            playsound(attacks[atk].action!=ACT_MELEE? S_ANNOUNCER_HEADSHOT: S_ANNOUNCER_FACE_PUNCH, NULL, NULL, NULL, SND_ANNOUNCER);
                        lastheadshot = lastmillis;
                    }
                    playsound(S_HEAD_HIT, NULL, &d->o);
                }
            }
            else d->headless = false;
        }
        if(d->health<=0) { if(local) killed(d, actor, NULL); }
    }

    VARP(gore, 0, 1, 1);

    void deathstate(gameent *d, bool restore)
    {
        bool gib = gore && d->health<=-50;
        if(d->state==CS_ALIVE)
        {
            stopownersounds(d);
            if(!gib) playsound(d->diesound(), d, &d->o);
        }
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        if(!restore)
        {
            if(gib) gibeffect(max(-d->health, 0), d->vel, d);
            d->deaths++;
        }
        if(d==player1)
        {
            //if(deathscore) showscores(true);
            disablezoom();
            d->attacking = ACT_IDLE;
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
        if(d->juggernautchan >= 0)
        {
            stopsound(S_JUGGERNAUT_LOOP, d->juggernautchan, 1200);
            d->juggernautchan = -1;
        }
    }

    VARP(teamcolorfrags, 0, 1, 1);
    VARP(killsound, 0, 0, 2);

    void dead(gameent *d, gameent *actor)
    {
        deathstate(d);
        ai::killed(d, actor);
    }

    void juggernauteffect(gameent *d)
    {
        msgsound(S_JUGGERNAUT, d);
        if(d!=hudplayer() || isthirdperson()) particle_splash(PART_SPARK1, 35, 150, d->feetpos(), 0xFF80FF, 6.0f, 80, -1);
        adddynlight(d->o, 70, vec(2, 1, 2), 600, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
    }

    void killed(gameent *d, gameent *actor, int flags)
    {
        if(d->state==CS_EDITING)
        {
            d->editstate = CS_DEAD;
            d->deaths++;
            if(d!=player1) d->resetinterp();
            return;
        }
        else if((d->state!=CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING) || intermission) return;
        gameent *h = followingplayer();
        if(!h) h = player1;
        int contype = d==h || actor==h ? CON_FRAG_SELF : CON_FRAG_OTHER;
        const char *dname = "", *aname = "", *dtype = "";
        if(m_teammode && teamcolorfrags)
        {
            dname = teamcolorname(d, "you");
            if(actor) aname = teamcolorname(actor, "you");
        }
        else
        {
            dname = colorname(d, NULL, "you");
            if(actor) aname = colorname(actor, NULL, "you");
        }
        if(m_infection && d->zombie) dtype = "(zombie)";
        if(m_juggernaut && d->juggernaut) dtype = "(juggernaut)";
        if(d==actor)
        {
            if(!d->juggernaut) conoutf(contype, "%s \f2suicided%s", dname, d==player1 ? "!" : "");
            else conoutf(contype, "%s \f2(juggernaut) died", dname);
            if(d==hudplayer() && killsound) playsound(S_SUICIDE);
        }
        else if(isally(d, actor))
        {
            contype |= CON_TEAMKILL;
            if(actor==player1) conoutf(contype, "%s \f2killed a \f6teammate\f2 (\ff%s\f2)", aname, dname);
            else if(d==player1) conoutf(contype, "%s \f2got killed by a \f6teammate\f2 (\ff%s\f2)", dname, aname);
            else conoutf(contype, "%s \f2killed a \f6teammate\f2 (\ff%s\f2)", aname, dname);
            if(actor==hudplayer() && killsound) playsound(S_KILL_ALLY);
        }
        else
        {
            if(flags&K_TELEFRAG) conoutf(CON_GAMEINFO, "%s \f2teleported into \ff%s \f2%s", aname, dname, dtype);
            else if(flags&K_STOMP) conoutf(CON_GAMEINFO, "%s \f2got stomped by \ff%s \f2%s", dname, aname, dtype);
            else
            {
                if(d==player1) conoutf(contype, "%s \f2got killed by \ff%s", dname, aname);
                else conoutf(contype, "%s \f2killed \ff%s \f2%s", aname, dname, dtype);
            }
            if(m_juggernaut && flags&K_JUGGERNAUT)
            {
                conoutf(CON_GAMEINFO, "%s \f2became the juggernaut", aname);
                if(actor==player1) playsound(S_ANNOUNCER_JUGGERNAUT, NULL, NULL, NULL, SND_ANNOUNCER);
                juggernauteffect(d);
            }
            if(flags&K_FIRST)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_FIRST_BLOOD, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2drew first blood", aname);
                if(actor->aitype==AI_BOT) taunt(actor);
            }
            if(flags&K_DOUBLE)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_DOUBLE_KILL, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2scored a double kill", aname);
            }
            else if(flags&K_MULTI)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_MULTI_KILL, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2scored a multi kill", aname);
            }
            if(flags&K_SPREE)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_KILLING_SPREE, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2%s on a killing spree", aname, actor==player1 ? "are" : "is");
                if(actor->aitype==AI_BOT) taunt(actor);
            }
            else if(flags&K_UNSTOPPABLE)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_UNSTOPPABLE, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2%s unstoppable!", aname, actor==player1 ? "are" : "is");
                if(actor->aitype==AI_BOT) taunt(actor);
            }
            if(flags&K_REVENGE)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_REVENGE, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2took %s revenge on \ff%s", aname, actor==player1? "your": "their", dname);
            }
            if(flags&K_HEADSHOT)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_HEAD_HUNTER, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2%s a head hunter", aname, actor==player1 ? "are" : "is");
            }
            if(flags&K_ZOMBIE)
            {
                if(actor==hudplayer()) playsound(S_ANNOUNCER_ANNIHILATOR, NULL, NULL, NULL, SND_ANNOUNCER);
                conoutf(CON_GAMEINFO, "%s \f2%s a zombie annihilator", aname, actor==player1 ? "are" : "is");
                if(actor->aitype==AI_BOT) taunt(actor);
            }
            if(actor->aitype==AI_BOT && d->aitype != AI_BOT && actor->skill < 80 && d->frags >= actor->frags)
                taunt(actor);
            if(actor==hudplayer() && killsound) playsound(killsound == 1 ? S_KILL1 : S_KILL2);
        }
        dead(d, actor);
    }

    void timeupdate(int secs)
    {
        if(secs > 0)
        {
            maplimit = lastmillis + secs*1000;
        }
        else
        {
            intermission = true;
            player1->attacking = ACT_IDLE;
            if(cmode) cmode->gameover();
            conoutf(CON_GAMEINFO, "\f2intermission:");
            conoutf(CON_GAMEINFO, "\f2game has ended!");
            if(m_ctf) conoutf(CON_GAMEINFO, "\f2player frags: %d, flags: %d, deaths: %d", player1->frags, player1->flags, player1->deaths);
            else conoutf(CON_GAMEINFO, "\f2player frags: %d, deaths: %d", player1->frags, player1->deaths);
            int accuracy = (player1->totaldamage*100)/max(player1->totalshots, 1);
            conoutf(CON_GAMEINFO, "\f2player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);

            bestteams.shrink(0);
            bestplayers.shrink(0);
            if(m_teammode) getbestteams(bestteams);
            else getbestplayers(bestplayers);

            if(validteam(player1->team) ? bestteams.htfind(player1->team)>=0 : bestplayers.find(player1)>=0)
            {
                playsound(S_INTERMISSION_WIN);
                playsound(S_ANNOUNCER_WIN, NULL, NULL, NULL, SND_ANNOUNCER);
            }
            else playsound(S_INTERMISSION);

            showscores(true);
            disablezoom();
            stopmusic(8000);

            execident("intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(player1->frags));
    ICOMMAND(getflags, "", (), intret(player1->flags));
    ICOMMAND(getdeaths, "", (), intret(player1->deaths));
    ICOMMAND(getaccuracy, "", (), intret((player1->totaldamage*100)/max(player1->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(player1->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(player1->totalshots));

    vector<gameent *> clients;

    gameent *newclient(int cn)   // ensure valid entity
    {
        if(cn < 0 || cn > max(0xFF, MAXCLIENTS + MAXBOTS))
        {
            neterr("clientnum", false);
            return NULL;
        }

        if(cn == player1->clientnum) return player1;

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
        if(cn == player1->clientnum) return player1;
        return clients.inrange(cn) ? clients[cn] : NULL;
    }

    void clientdisconnected(int cn, bool notify)
    {
        if(!clients.inrange(cn)) return;
        unignore(cn);
        gameent *d = clients[cn];
        if(d)
        {
            if(notify && d->name[0]) conoutf("%s \f4left the game", colorname(d));
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
        player1 = spawnstate(new gameent);
        filtertext(player1->name, "player", false, false, true, false, MAXNAMELEN);
        players.add(player1);
    }

    VARP(showmodeinfo, 0, 1, 1);

    void startgame()
    {
        clearprojectiles();
        clearbouncers();
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

        conoutf(CON_GAMEINFO, "%s on %s", server::modeprettyname(gamemode), getclientmap());

        if(mutators != 0) loopi(NUMMUTATORS)
        {
           if(mutators & mutator[i].flags) conoutf(CON_GAMEINFO, "%s", mutator[i].info);
        }
        const char *info = m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
        if(showmodeinfo && info) conoutf(CON_GAMEINFO, "\f0%s", info);

        syncplayer();

        showscores(false);
        disablezoom();

        execident("mapstart");
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);

        if(!m_mp(gamemode)) spawnplayer(player1);
        else findplayerspawn(player1, -1, m_teammode ? player1->team : 0);
        entities::resetspawns();
        copystring(clientmap, name ? name : "");

        sendmapinfo();
    }

    vector<char *> tips;
    ICOMMAND(registertip, "s", (char *tip), { tips.add(newstring(tip)); });

    const char *getmapinfo()
    {
       static char info[1000];
       info[0] = '\0';
       strcat(info, gamemodes[gamemode - STARTGAMEMODE].info);
       /*if(mutators != 0) loopi(NUMMUTATORS)
       {
           if(mutators & mutator[i].flags)
           {
               strcat(info, "\n");
               strcat(info, mutator[i].info);
           }
       }
       else*/ if(!tips.empty())
       {
            strcat(info, "\n\n");
            strcat(info, tips[rnd(tips.length())]);
       }
       return showmodeinfo && m_valid(gamemode) ? info : NULL;
    }

    const char *getscreenshotinfo()
    {
        return server::modename(gamemode, NULL);
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
    {
        if(d->state>CS_DEAD) return;
        gameent *e = (gameent *)d;
        vec o = d->state==CS_DEAD || (d==hudplayer() && !isthirdperson()) ? d->feetpos() : d->o;
        if     (waterlevel>0)
        {
            if(material!=MAT_LAVA)
            {
                playsound(S_SPLASHOUT, NULL, d==player1 ? NULL : &d->o);
                particle_splash(PART_WATER, 120, 100, o, 0xFFFFFF, 0.10f, 200, 5);
                particle_splash(PART_WATER, 130, 150, o, 0xFFFFFF, 0.09f+rndscale(0.14f), 300, 1);
                particle_splash(PART_STEAM, 10, 100, o, 0xFFFFFF, 0.80f);
                particle_splash(PART_STEAM, 20, 100, o, 0xFFFFFF, 1.0f, 100, 0);
                particle_splash(PART_STEAM, 30, 80, o, 0xFFFFFF, 0.70f, 110, -1);
                particle_splash(PART_STEAM, 40, 100, o, 0xFFFFFF, 0.30f, 120, 1);
            }
        }
        else if(waterlevel<0)
        {
            playsound(material==MAT_LAVA ? S_BURN : S_SPLASHIN, NULL, d==player1 ? NULL : &d->o);
            if(material&MAT_WATER)
            {
                playsound(S_SPLASHOUT, NULL, d==player1 ? NULL : &d->o);
                particle_splash(PART_WATER, 200, 100, o, 0xFFFFFF, 0.01+rndscale(0.10f));
                particle_splash(PART_WATER, 180, 130, o, 0xFFFFFF, 0.11f+rndscale(0.17f), 400, 2);
                particle_splash(PART_WATER, 160, 150, o, 0xFFFFFF, 0.01+rndscale(0.12f), 300, 1);
                particle_splash(PART_STEAM, 10, 120, o, 0xFFFFFF, 1.0f);
                particle_splash(PART_STEAM, 30, 100, o, 0xFFFFFF, 2.5f, 100, -1);
                particle_splash(PART_STEAM, 40, 80, o, 0xFFFFFF, 1.0f, 150, -1);
            }
        }
        if     (floorlevel>0)
        {
            if(d==player1 || d->type!=ENT_PLAYER || ((gameent *)d)->ai)
            {
                if(!e->timeinair) msgsound(S_JUMP1, d);
                else msgsound(e->zombie ? S_JUMP2 : S_JUMP3, d);
            }
        }
        else if(floorlevel<0)
        {
            if(d==player1 || d->type!=ENT_PLAYER || ((gameent *)d)->ai)
            {
                if(d->state==CS_ALIVE)
                {
                    e->landmillis = lastmillis;
                    addmsg(N_ANIMATION, "rci", e, ANIM_CROUCH);
                    if(lookupmaterial(d->feetpos())&MAT_WATER)
                    {
                        vec feet = d->feetpos();
                        feet.z += 5.2f;
                        particle_splash(PART_WATER, 130, 250, feet, 0xFFFFFF, 0.04f, 250, 4);
                        particle_splash(PART_WATER, 200, 200, feet, 0xFFFFFF, 0.08f, 300, 3);
                        particle_splash(PART_WATER, 50, 100, feet, 0xFFFFFF, 2.0f, 150, 5);
                        particle_splash(PART_STEAM, 50, 120, feet, 0xFFFFFF, 2.8f, 80, 50);
                        msgsound(S_LAND_WATER, d);
                    }
                    else msgsound(S_LAND, d);
                    specialattack(e, ATK_STOMP, d->feetpos(), worldpos); // really ugly code, it does nothing for hacks... wrote to test the stomp attack concept
                }
                else msgsound(S_CORPSE, d);
            }
        }
    }

    VARP(footstepssound, 0, 1, 1);

    void footsteps(physent *d)
    {
        if(!footstepssound || (!d->inwater && d->crouching && d->crouched()) || d->blocked) return;
        bool moving = d->move || d->strafe;
        gameent *pl = (gameent *)d;
        if(d->physstate>=PHYS_SLOPE && moving)
        {
            int snd = S_FOOTSTEP;
            if(d->inwater) snd = S_SWIM;
            else if(lookupmaterial(d->feetpos())&MAT_WATER) snd = S_FOOTSTEP_WATER;
            if(lastmillis-pl->lastfootstep < (d->vel.magnitude()*450/d->vel.magnitude())) return;
            else playsound(snd, d, &d->o, NULL, 0, 0, 0, -1, 200);
        }
        pl->lastfootstep = lastmillis;
    }

    void dynentcollide(physent *d, physent *o, const vec &dir)
    {
    }

    void msgsound(int n, physent *d)
    {
        if(!d || d == player1)
        {
            addmsg(N_SOUND, "ci", d, n);
            playsound(n, camera1);
        }
        else
        {
            if(d->type==ENT_PLAYER && ((gameent *)d)->ai)
                addmsg(N_SOUND, "ci", d, n);
            playsound(n, d);
        }
    }

    int numdynents() { return players.length(); }

    dynent *iterdynents(int i)
    {
        if(i<players.length()) return players[i];
        return NULL;
    }

    bool duplicatename(gameent *d, const char *name = NULL, const char *alt = NULL)
    {
        if(!name) name = d->name;
        if(alt && d != player1 && !strcmp(name, alt)) return true;
        loopv(players) if(d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    const char *colorname(gameent *d, const char *name, const char * alt, const char *color)
    {
        if(!name) name = alt && d == player1 ? alt : d->name;
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

    void damage(physent *d)
    {
        if(d==player1 || (d->type==ENT_PLAYER && ((gameent *)d)->ai))
        {
            if(d->state!=CS_ALIVE) return;
            gameent *e = (gameent *)d;
            if(lastmillis-e->lastdamage <= 1000) return;
            int damage = ENV_DAM;
            if(e->invulnmillis) damage = 0;
            if(!m_mp(gamemode)) damaged(damage, e->o, e, e, -1);
            else addmsg(N_HURTPLAYER, "rci", e, damage);
            if(damage > 0)
            {
                damageeffect(damage, e, e->o, -1);
                //msgsound(S_DAMAGE, e);
            }
            e->lastdamage = lastmillis;
        }
    }

    void suicide(physent *d)
    {
        if(d==player1 || (d->type==ENT_PLAYER && ((gameent *)d)->ai))
        {
            if(d->state!=CS_ALIVE) return;
            gameent *pl = (gameent *)d;
            if(pl->invulnmillis)
            {
                addmsg(N_SUICIDE, "rc", pl);
                return;
            }
            if(!m_mp(gamemode))
                killed(pl, pl, NULL);
            else
            {
                int seq = (pl->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
                if(pl->suicided!=seq) { addmsg(N_SUICIDE, "rc", pl); pl->suicided = seq; }
            }
        }
    }
    ICOMMAND(suicide, "", (), suicide(player1));

    bool needminimap() { return m_ctf; }

    void drawicon(int icon, float x, float y, float sz)
    {
        settexture(iconnames[icon]);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0, 0);
        gle::attribf(x+sz, y);   gle::attribf(1, 0);
        gle::attribf(x,   y+sz); gle::attribf(0, 1);
        gle::attribf(x+sz, y+sz); gle::attribf(1, 1);
        gle::end();
    }

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

    void drawhudicons(gameent *d, int w, int h)
    {
        pushhudscale(2);
        float x = 1800*w/h*0.5f-HICON_SIZE/2, y = 1800*0.95f-HICON_SIZE/2;

        if(d->state!=CS_DEAD)
        {
            draw_textf("%s%d", (x-1.0*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, y/2, (d->health >= d->maxhealth/2 ? "\ff" :
                       (d->health<= d->maxhealth/4 ? "\f3" : "\f6")), d->health);
            draw_textf("%d", (x+0.9*HICON_STEP - HICON_SIZE - HICON_SPACE)/2, y/2, d->shield);
            draw_textf("%d", (x+2.4*HICON_STEP - HICON_SIZE - HICON_SPACE)/2, y/2, d->ammo[d->gunselect]);
            if(!d->juggernaut)
            {
                if(d->damagemillis) draw_textf("%d", (x-3.0*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, y/2, d->damagemillis/1000);
                if(d->armourmillis) draw_textf("%d", (x-2.4*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, y/2, d->armourmillis/1000);
                if(d->hastemillis) draw_textf("%d", (x-1.8*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, y/2, d->hastemillis/1000);
                if(d->ammomillis) draw_textf("%d", (x-2.4*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, y/2, d->ammomillis/1000);
            }
            if(d->invulnmillis) draw_textf("%d", (x-3.0*HICON_STEP + HICON_SIZE + HICON_SPACE)/2, y/2, d->invulnmillis/1000);
        }

        pophudmatrix();
        resethudshader();

        if(d->state!=CS_DEAD)
        {
            drawicon(HICON_HEALTH, x-1.0*HICON_STEP, y);
            drawicon(HICON_SHIELD, x+1.0*HICON_STEP, y);
            drawicon(HICON_SG+d->gunselect, x+2.5*HICON_STEP, y);
            if(!d->juggernaut)
            {
                if(d->damagemillis) drawicon(HICON_DDAMAGE, x-3.0*HICON_STEP, y);
                if(d->armourmillis) drawicon(HICON_ARMOUR, x-2.4*HICON_STEP, y);
                if(d->hastemillis) drawicon(HICON_HASTE, x-1.8*HICON_STEP, y);
                if(d->ammomillis) drawicon(HICON_UAMMO, x-2.4*HICON_STEP, y);
            }
            if(d->item == 1 || d->invulnmillis) drawicon(HICON_INVULNERABILITY, x-3.0*HICON_STEP, y);

        }
    }

    void gameplayhud(int w, int h)
    {
        pushhudscale(h/1800.0f);

        if(player1->state==CS_SPECTATOR)
        {
            float pw, ph, tw, th, fw, fh;
            text_boundsf("  ", pw, ph);
            text_boundsf("\f2SPECTATOR", tw, th);
            th = max(th, ph);
            gameent *f = followingplayer();
            text_boundsf(f ? colorname(f) : " ", fw, fh);
            fh = max(fh, ph);
            draw_text("\f2SPECTATOR", w*1800/h - tw - pw, 1650 - th - fh);
            if(f)
            {
                int color = f->state!=CS_DEAD ? 0xFFFFFF : 0x606060;
                if(f->privilege)
                {
                    color = f->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(f->state==CS_DEAD) color = (color>>1)&0x7F7F7F;
                }
                draw_text(colorname(f), w*1800/h - fw - pw, 1650 - fh, (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
            }
            resethudshader();
        }

        gameent *d = hudplayer();
        if(d->state!=CS_EDITING)
        {
            if(d->state!=CS_SPECTATOR) drawhudicons(d, w, h);
            if(cmode) cmode->drawhud(d, w, h);
        }

        pophudmatrix();
    }

    float clipconsole(float w, float h)
    {
        if(cmode) return cmode->clipconsole(w, h);
        return 0;
    }

    VARP(teamcrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 425, 1000);

    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 2: return "data/interface/crosshair/default_hit.png";
            case 1: return "data/interface/crosshair/teammate.png";
            default: return "data/interface/crosshair/default.png";
        }
    }

    int selectcrosshair(vec &col)
    {
        gameent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_DEAD || UI::uivisible("scoreboard") || intermission) return -1;

        if(d->state!=CS_ALIVE) return 0;

        int crosshair = 0;
        if(d->lasthit && lastmillis - d->lasthit < hitcrosshair) crosshair = 2;
        else if(teamcrosshair && m_teammode)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);
            if(o && o->type==ENT_PLAYER && validteam(d->team) && ((gameent *)o)->team == d->team)
            {
                crosshair = 1;

                col = vec::hexcolor(teamtextcolor[d->team]);
            }
        }

#if 0
        if(crosshair!=1 && !editmode)
        {
            if(d->health<=25) { r = 1.0f; g = b = 0; }
            else if(d->health<=50) { r = 1.0f; g = 0.5f; b = 0; }
        }
#endif
        if(d->gunwait) col.mul(0.5f);
        return crosshair;
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
            result(si->maxplayers > 0 && si->numplayers >= si->maxplayers ? "serverfull" : mastermodeicon(mm, "serverunk"));
        }));

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *gameconfig() { return "data/config/game.cfg"; }
    const char *savedconfig() { return "data/config/saved.cfg"; }
    const char *restoreconfig() { return "data/config/restore.cfg"; }
    const char *defaultconfig() { return "data/config/default.cfg"; }
    const char *autoexec() { return "data/config/autoexec.cfg"; }
    const char *savedservers() { return "data/config/servers.cfg"; }

    void loadconfigs()
    {
        execfile("data/config/auth.cfg", false);
    }

    bool clientoption(const char *arg) { return false; }
}

