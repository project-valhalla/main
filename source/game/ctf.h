#ifndef PARSEMESSAGES

#ifdef SERVMODE
VAR(ctftkpenalty, 0, 1, 1);

struct ctfservmode : servmode
#else
struct ctfclientmode : clientmode
#endif
{
    static const int MAXFLAGS = 20;
    static const int FLAGRADIUS = 16;
    static const int FLAGLIMIT = 10;
    static const int RESPAWNSECS = 5;

    struct flag
    {
        int id, version;
        vec droploc, spawnloc;
        int team, droptime, owntime, chan;
#ifdef SERVMODE
        int owner, dropcount, dropper;
#else
        gameent *owner, *prevowner;
        float dropangle, spawnangle;
        vec interploc;
        float interpangle;
        int interptime;
#endif

        flag() : id(-1) { reset(); }

        void reset()
        {
            version = 0;
            droploc = spawnloc = vec(0, 0, 0);
#ifdef SERVMODE
            dropcount = 0;
            owner = dropper = -1;
            owntime = 0;
#else
            if(id >= 0) loopv(players) players[i]->flagpickup &= ~(1<<id);
            owner = prevowner = NULL;
            dropangle = spawnangle = 0;
            interploc = vec(0, 0, 0);
            interpangle = 0;
            interptime = 0;
#endif
            team = 0;
            droptime = owntime = 0;
            chan = -1;
        }

#ifndef SERVMODE
        vec pos() const
        {
            if(owner) return vec(owner->o).sub(owner->eyeheight);
            if(droptime) return droploc;
            return spawnloc;
        }
#endif
    };

    vector<flag> flags;
    int scores[MAXTEAMS];

    void resetflags()
    {
        flags.shrink(0);
        loopk(MAXTEAMS) scores[k] = 0;
    }

#ifdef SERVMODE
    bool addflag(int i, const vec &o, int team)
#else
    bool addflag(int i, const vec &o, int team)
#endif
    {
        if(i<0 || i>=MAXFLAGS) return false;
        while(flags.length()<=i) flags.add();
        flag &f = flags[i];
        f.id = i;
        f.reset();
        f.team = team;
        f.spawnloc = o;
        return true;
    }

#ifdef SERVMODE
    void ownflag(int i, int owner, int owntime)
#else
    void ownflag(int i, gameent *owner, int owntime)
#endif
    {
        flag &f = flags[i];
        f.owner = owner;
        f.owntime = owntime;
#ifdef SERVMODE
        if(owner == f.dropper) { if(f.dropcount < INT_MAX) f.dropcount++; }
        else f.dropcount = 0;
        f.dropper = -1;
#else
        loopv(players) players[i]->flagpickup &= ~(1<<f.id);
#endif
    }

#ifdef SERVMODE
    void dropflag(int i, const vec &o, int droptime, int dropper = -1, bool penalty = false)
#else
    void dropflag(int i, const vec &o, float yaw, int droptime)
#endif
    {
        flag &f = flags[i];
        f.droploc = o;
        f.droptime = droptime;
#ifdef SERVMODE
        if(dropper < 0) f.dropcount = 0;
        else if(penalty) f.dropcount = INT_MAX;
        f.dropper = dropper;
        f.owner = -1;
#else
        loopv(players) players[i]->flagpickup &= ~(1<<f.id);
        f.prevowner = f.owner;
        f.owner = NULL;
        f.dropangle = yaw;
#endif
    }

#ifdef SERVMODE
    void returnflag(int i)
#else
    void returnflag(int i)
#endif
    {
        flag &f = flags[i];
        f.droptime = 0;
#ifdef SERVMODE
        f.dropcount = 0;
        f.owner = f.dropper = -1;
#else
        loopv(players) players[i]->flagpickup &= ~(1<<f.id);
        f.prevowner = f.owner;
        f.owner = NULL;
#endif
    }

    int totalscore(int team)
    {
        return validteam(team) ? scores[team-1] : 0;
    }

    int setscore(int team, int score)
    {
        if(validteam(team)) return scores[team-1] = score;
        return 0;
    }

    int addscore(int team, int score)
    {
        if(validteam(team)) return scores[team-1] += score;
        return 0;
    }

    bool hidefrags() { return true; }

    int getteamscore(int team)
    {
        return totalscore(team);
    }

    void getteamscores(vector<teamscore> &tscores)
    {
        loopk(MAXTEAMS) if(scores[k]) tscores.add(teamscore(k+1, scores[k]));
    }

#ifdef SERVMODE
    static const int RESETFLAGTIME = 10000;

    bool notgotflags;

    ctfservmode() : notgotflags(false) {}

    void reset(bool empty)
    {
        resetflags();
        notgotflags = !empty;
    }

    void cleanup()
    {
        reset(false);
    }

    void setup()
    {
        reset(false);
        if(notgotitems || ments.empty()) return;
        loopv(ments)
        {
            entity &e = ments[i];
            if(e.type != FLAG || !validteam(e.attr2)) continue;
            if(!addflag(flags.length(), e.o, e.attr2)) break;
        }
        notgotflags = false;
    }

    void newmap()
    {
        reset(true);
    }

    void dropflag(clientinfo *ci, clientinfo *dropper = NULL)
    {
        if(notgotflags) return;
        loopv(flags) if(flags[i].owner==ci->clientnum)
        {
            flag &f = flags[i];
            ivec o(vec(ci->state.o).mul(DMF));
            sendf(-1, 1, "ri7", N_DROPFLAG, ci->clientnum, i, ++f.version, o.x, o.y, o.z);
            dropflag(i, vec(o).div(DMF), lastmillis, dropper ? dropper->clientnum : ci->clientnum, dropper && dropper!=ci);
        }
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        dropflag(ci);
        loopv(flags) if(flags[i].dropper == ci->clientnum) { flags[i].dropper = -1; flags[i].dropcount = 0; }
    }

    void died(clientinfo *ci, clientinfo *actor)
    {
        dropflag(ci, ctftkpenalty && actor && actor != ci && sameteam(actor->team, ci->team) ? actor : NULL);
        loopv(flags) if(flags[i].dropper == ci->clientnum) { flags[i].dropper = -1; flags[i].dropcount = 0; }
    }

    bool canspawn(clientinfo *ci, bool connecting)
    {
        return connecting || !ci->state.lastdeath || gamemillis+curtime-ci->state.lastdeath >= RESPAWNSECS*1000;
    }

    bool canchangeteam(clientinfo *ci, int oldteam, int newteam)
    {
        return true;
    }

    void changeteam(clientinfo *ci, int oldteam, int newteam)
    {
        dropflag(ci);
    }

    void scoreflag(clientinfo *ci, int goal, int relay = -1)
    {
        returnflag(relay >= 0 ? relay : goal);
        ci->state.flags++;
        int team = ci->team, score = addscore(team, 1);
        sendf(-1, 1, "ri9", N_SCOREFLAG, ci->clientnum, relay, relay >= 0 ? ++flags[relay].version : -1, goal, ++flags[goal].version, team, score, ci->state.flags);
        if(gamescorelimit && score >= gamescorelimit)
        {
            if(checkovertime()) return;
            startintermission();
            defformatstring(win, "%s%s \fs\f2team reached the score limit\fr", teamtextcode[team], teamnames[team]);
            sendf(-1, 1, "ri2s", N_ANNOUNCE, NULL, win);
        }
    }

    void takeflag(clientinfo *ci, int i, int version)
    {
        if(notgotflags || !flags.inrange(i) || ci->state.state!=CS_ALIVE || !ci->team) return;
        flag &f = flags[i];
        if(!validteam(f.team) || f.owner>=0 || f.version != version || (f.droptime && f.dropper == ci->clientnum && f.dropcount >= 1)) return;
        if(f.team!=ci->team)
        {
            loopvj(flags) if(flags[j].owner==ci->clientnum) return;
            ownflag(i, ci->clientnum, lastmillis);
            sendf(-1, 1, "ri4", N_TAKEFLAG, ci->clientnum, i, ++f.version);
        }
        else if(f.droptime)
        {
            returnflag(i);
            sendf(-1, 1, "ri4", N_RETURNFLAG, ci->clientnum, i, ++f.version);
        }
        else
        {
            loopvj(flags) if(flags[j].owner==ci->clientnum) { scoreflag(ci, i, j); break; }
        }
    }

    void update()
    {
        if(gamemillis>=gamelimit || notgotflags) return;
        loopv(flags)
        {
            flag &f = flags[i];
            if(f.owner<0 && f.droptime && lastmillis - f.droptime >= RESETFLAGTIME)
            {
                returnflag(i);
                sendf(-1, 1, "ri3", N_RESETFLAG, i, ++f.version);
            }
        }
    }

    void initclient(clientinfo *ci, packetbuf &p, bool connecting)
    {
        putint(p, N_INITFLAGS);
        loopk(2) putint(p, scores[k]);
        putint(p, flags.length());
        loopv(flags)
        {
            flag &f = flags[i];
            putint(p, f.version);
            putint(p, f.owner);
            if(f.owner<0)
            {
                putint(p, f.droptime ? 1 : 0);
                if(f.droptime)
                {
                    putint(p, int(f.droploc.x*DMF));
                    putint(p, int(f.droploc.y*DMF));
                    putint(p, int(f.droploc.z*DMF));
                }
            }
        }
    }

    void parseflags(ucharbuf &p, bool commit)
    {
        int numflags = getint(p);
        loopi(numflags)
        {
            int team = getint(p);
            vec o;
            loopk(3) o[k] = max(getint(p)/DMF, 0.0f);
            if(p.overread()) break;
            if(commit && notgotflags)
            {
                addflag(i, o, team);
            }
        }
        if(commit && notgotflags)
        {
            notgotflags = false;
        }
    }
};
#else
    #define FLAGCENTER 3.5f
    #define FLAGFLOAT 7

    void preload()
    {
        preloadmodel("item/flag/rojo");
        preloadmodel("item/flag/azul");
        for(int i = S_FLAGPICKUP; i <= S_FLAGFAIL; i++) preloadsound(i);
    }

    void drawblip(gameent *d, float x, float y, float s, const vec &pos, bool flagblip)
    {
        float scale = calcradarscale();
        vec dir = d->o;
        dir.sub(pos).div(scale);
        float size = flagblip ? 0.1f : 0.05f,
              xoffset = flagblip ? -2*(3/32.0f)*size : -size,
              yoffset = flagblip ? -2*(1 - 3/32.0f)*size : -size,
              dist = dir.magnitude2(), maxdist = 1 - 0.05f - 0.05f;
        if(dist >= maxdist) dir.mul(maxdist/dist);
        dir.rotate_around_z(camera1->yaw*-RAD);
        drawradar(x + s*0.5f*(1.0f + dir.x + xoffset), y + s*0.5f*(1.0f + dir.y + yoffset), size*s);
    }

    void drawblip(gameent *d, float x, float y, float s, int i, bool flagblip)
    {
        flag &f = flags[i];
        setbliptex(f.team, flagblip ? "_flag" : "");
        drawblip(d, x, y, s, flagblip ? (f.owner ? f.owner->o : (f.droptime ? f.droploc : f.spawnloc)) : f.spawnloc, flagblip);
    }

    float clipconsole(float w, float h)
    {
        return (h*(1 + 1 + 10))/(4*10);
    }

    void removeplayer(gameent *d)
    {
        loopv(flags) if(flags[i].owner == d)
        {
            flag &f = flags[i];
            f.interploc.x = -1;
            f.interptime = 0;
            dropflag(i, f.owner->o, f.owner->yaw, 1);
        }
    }

    vec interpflagpos(flag &f, float &angle)
    {
        vec pos = f.owner ? vec(f.owner->abovehead()).addz(1) : (f.droptime ? f.droploc : f.spawnloc);
        if(f.owner) angle = f.owner->yaw;
        else { angle = f.droptime ? f.dropangle : f.spawnangle; pos.addz(FLAGFLOAT); }
        if(pos.x < 0) return pos;
        pos.addz(FLAGCENTER);
        if(f.interptime && f.interploc.x >= 0)
        {
            float t = min((lastmillis - f.interptime)/500.0f, 1.0f);
            pos.lerp(f.interploc, pos, t);
            angle += (1-t)*(f.interpangle - angle);
        }
        return pos;
    }

    vec interpflagpos(flag &f) { float angle; return interpflagpos(f, angle); }

    void rendergame()
    {
        loopv(flags)
        {
            flag &f = flags[i];
            if(f.owner)
            {
                if(!f.owner->holdingflag)
                {
                    f.owner->holdingflag = f.team;
                }
            }
            else if(f.prevowner)
            {
                if(f.prevowner->holdingflag)
                {
                    f.prevowner->holdingflag = 0;
                    f.prevowner = NULL;
                }
            }
            if(!f.owner && f.droptime && f.droploc.x < 0) continue;
            const char *flagname = f.team==1 ? "item/flag/azul" : "item/flag/rojo";
            float angle;
            vec pos = interpflagpos(f, angle);
            rendermodel(flagname, ANIM_MAPMODEL|ANIM_LOOP, pos, angle, 0, 0, MDL_CULL_VFC | MDL_CULL_OCCLUDED);

            vec lightcolor = vec::hexcolor(teameffectcolor[f.team]);
            addgamelight(pos, lightcolor.mul(255.f * (0.625f - 0.375f * cos(2 * PI * lastmillis / 1000.f))), 32);
            if (self && self->state != CS_EDITING)
            {
                f.chan = playsound(S_FLAGLOOP, NULL, f.owner == self ? NULL : &pos, NULL, 0, -1, 500, f.chan, 200);
            }
            else
            {
                stopsound(S_FLAGLOOP, f.chan);
                f.chan = -1;
            }

            gameent *hud = followingplayer(self);
            if(hud->holdingflag && f.team == hud->team)
            {
                vec base = f.spawnloc;
                particle_hud_text(base, "GOAL", PART_TEXT, 1, teamtextcolor[hud->team], 4.0f, "wide");
            }
            else if(f.owner)
            {
                if(lastmillis%1000 >= 500) continue;
            }
            else if(f.droptime && (f.droploc.x < 0 || lastmillis%300 >= 150)) continue;
            particle_hud_mark(pos, f.team == 1 ? 1 : 0, 0, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
        }
    }

    void setup()
    {
        resetflags();
        loopv(entities::ents)
        {
            extentity *e = entities::ents[i];
            if(e->type!=FLAG || !validteam(e->attr2)) continue;
            int index = flags.length();
            if(!addflag(index, e->o, e->attr2)) continue;
            flags[index].spawnangle = e->attr1;
        }
    }

    void senditems(packetbuf &p)
    {
        putint(p, N_INITFLAGS);
        putint(p, flags.length());
        loopv(flags)
        {
            flag &f = flags[i];
            putint(p, f.team);
            loopk(3) putint(p, int(f.spawnloc[k]*DMF));
        }
    }

    void parseflags(ucharbuf &p, bool commit)
    {
        loopk(2)
        {
            int score = getint(p);
            if(commit) scores[k] = score;
        }
        int numflags = getint(p);
        loopi(numflags)
        {
            int version = getint(p), owner = getint(p), dropped = 0;
            vec droploc(0, 0, 0);
            if(owner<0)
            {
                dropped = getint(p);
                if(dropped) loopk(3) droploc[k] = getint(p)/DMF;
            }
            if(p.overread()) break;
            if(commit && flags.inrange(i))
            {
                flag &f = flags[i];
                f.version = version;
                f.owner = owner>=0 ? (owner==self->clientnum ? self : newclient(owner)) : NULL;
                f.droptime = dropped;
                f.droploc = dropped ? droploc : f.spawnloc;
                f.interptime = 0;

                if(dropped && !droptofloor(f.droploc.addz(4), 4, 0)) f.droploc = vec(-1, -1, -1);
            }
        }
    }

    void trydropflag()
    {
        if(!m_ctf) return;
        loopv(flags) if(flags[i].owner == self)
        {
            addmsg(N_TRYDROPFLAG, "rc", self);
            return;
        }
    }

    const char *teamcolorflag(flag &f)
    {
        return teamcolor("", "\fs\f2's flag\fr", f.team, "\fs\f2a flag\fr");
    }

    void dropflag(gameent *d, int i, int version, const vec &droploc)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.version = version;
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = lastmillis;
        dropflag(i, droploc, d->yaw, 1);
        d->flagpickup |= 1<<f.id;
        if(!droptofloor(f.droploc.addz(4), 4, 0))
        {
            f.droploc = vec(-1, -1, -1);
            f.interptime = 0;
        }
        conoutf(CON_GAMEINFO, "%s \fs\f2dropped\fr %s", teamcolorname(d), teamcolorflag(f));
        playsound(S_FLAGDROP);
    }

    void flagexplosion(int i, int team, const vec &loc)
    {
        int fcolor = teameffectcolor[team];
        particle_fireball(loc, 30, PART_EXPLOSION1, -1, fcolor, 4.8f);
        particle_splash(PART_SPARK, 150, 300, loc, fcolor, 0.24f);
        vec lightcolor = vec::hexcolor(fcolor);
        adddynlight(loc, 35, lightcolor, 900, 100);
    }

    void flageffect(int i, int team, const vec &from, const vec &to)
    {
        if(from.x >= 0)
        {
            flagexplosion(i, team, from);
        }
        if(from == to) return;
        if(to.x >= 0)
        {
            flagexplosion(i, team, to);
        }
        if(from.x >= 0 && to.x >= 0)
        {
            particle_flare(from, to, 600, PART_LIGHTNING, team==1 ? 0x2222FF : 0xFF2222, 1.0f);
        }
    }

    void returnflag(gameent *d, int i, int version)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.version = version;
        flageffect(i, f.team, interpflagpos(f), vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER));
        f.interptime = 0;
        returnflag(i);
        conoutf(CON_GAMEINFO, "%s \fs\f2returned\fr %s", teamcolorname(d), teamcolorflag(f));
        playsound(S_FLAGRETURN);
        entities::dohudpickupeffects(FLAG, d, false);
    }

    void resetflag(int i, int version)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.version = version;
        flageffect(i, f.team, interpflagpos(f), vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER));
        f.interptime = 0;
        returnflag(i);
        conoutf(CON_GAMEINFO, "%s \fs\f2reset\fr", teamcolorflag(f));
        playsound(S_FLAGRESET);
    }

    void scoreflag(gameent *d, int relay, int relayversion, int goal, int goalversion, int team, int score, int dflags)
    {
        setscore(team, score);
        if(flags.inrange(goal))
        {
            flag &f = flags[goal];
            f.version = goalversion;
            if(relay >= 0)
            {
                flags[relay].version = relayversion;
                flageffect(goal, team, vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER), vec(flags[relay].spawnloc).addz(FLAGFLOAT+FLAGCENTER));
            }
            else flageffect(goal, team, interpflagpos(f), vec(f.spawnloc).addz(FLAGFLOAT+FLAGCENTER));
            f.interptime = 0;
            returnflag(relay >= 0 ? relay : goal);
            d->flagpickup &= ~(1<<f.id);
            if(d->feetpos().dist(f.spawnloc) < FLAGRADIUS) d->flagpickup |= 1<<f.id;
        }
        if(d!=self) particle_textcopy(d->abovehead(), tempformatstring("%d", score), PART_TEXT, 2000, 0x32FF64, 4.0f, -8);
        d->flags = dflags;
        conoutf(CON_GAMEINFO, "%s \fs\f2scored for\fr %s", teamcolorname(d), teamcolor("\fs\f2team\fr ", "", team, "\fs\f2a team\fr"));
        playsound(team==self->team ? S_FLAGSCORE : S_FLAGFAIL);
        if(d->aitype==AI_BOT) taunt(d);
        entities::dohudpickupeffects(FLAG, d, false);
    }

    void takeflag(gameent *d, int i, int version)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.version = version;
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = lastmillis;
        if(f.droptime) conoutf(CON_GAMEINFO, "%s \fs\f2picked up\fr %s", teamcolorname(d), teamcolorflag(f));
        else conoutf(CON_GAMEINFO, "%s \fs\f2stole\fr %s", teamcolorname(d), teamcolorflag(f));
        ownflag(i, d, lastmillis);
        playsound(S_FLAGPICKUP);
        entities::dohudpickupeffects(FLAG, d);
    }

    void checkitems(gameent *d)
    {
        if(d->state!=CS_ALIVE) return;
        vec o = d->feetpos();
        loopv(flags)
        {
            flag &f = flags[i];
            if(!validteam(f.team) || f.owner || (f.droptime && f.droploc.x<0)) continue;
            const vec &loc = f.droptime ? f.droploc : f.spawnloc;
            if(o.dist(loc) < FLAGRADIUS)
            {
                if(d->flagpickup&(1<<f.id)) continue;
                //if((lookupmaterial(o)&MATF_CLIP) != MAT_GAMECLIP && (lookupmaterial(loc)&MATF_CLIP) != MAT_GAMECLIP)
                    addmsg(N_TAKEFLAG, "rcii", d, i, f.version);
                d->flagpickup |= 1<<f.id;
            }
            else d->flagpickup &= ~(1<<f.id);
       }
    }

    void respawned(gameent *d)
    {
        vec o = d->feetpos();
        d->flagpickup = 0;
        loopv(flags)
        {
            flag &f = flags[i];
            if(!validteam(f.team) || f.owner || (f.droptime && f.droploc.x<0)) continue;
            if(o.dist(f.droptime ? f.droploc : f.spawnloc) < FLAGRADIUS) d->flagpickup |= 1<<f.id;
       }
    }

    int respawnwait()
    {
        return RESPAWNSECS * 1000;
    }

    bool aihomerun(gameent *d, ai::aistate &b)
    {
        vec pos = d->feetpos();
        loopk(2)
        {
            int goal = -1;
            loopv(flags)
            {
                flag &g = flags[i];
                if(g.team == d->team && (k || (!g.owner && !g.droptime)) &&
                    (!flags.inrange(goal) || g.pos().squaredist(pos) < flags[goal].pos().squaredist(pos)))
                {
                    goal = i;
                }
            }
            if(flags.inrange(goal) && ai::makeroute(d, b, flags[goal].pos()))
            {
                d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_AFFINITY, goal);
                return true;
            }
        }
        if(b.type == ai::AI_S_INTEREST && b.targtype == ai::AI_T_NODE) return true; // we already did this..
        if(randomnode(d, b, ai::SIGHTMIN, 1e16f))
        {
            d->ai->switchstate(b, ai::AI_S_INTEREST, ai::AI_T_NODE, d->ai->route[0]);
            return true;
        }
        return false;
    }

    bool aicheck(gameent *d, ai::aistate &b)
    {
        static vector<int> takenflags;
        takenflags.setsize(0);
        loopv(flags)
        {
            flag &g = flags[i];
            if(g.owner == d) return aihomerun(d, b);
            else if(g.team == d->team && ((g.owner && g.team != g.owner->team) || g.droptime))
                takenflags.add(i);
        }
        if(!ai::badhealth(d) && !takenflags.empty())
        {
            int flag = takenflags.length() > 2 ? rnd(takenflags.length()) : 0;
            d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_AFFINITY, takenflags[flag]);
            return true;
        }
        return false;
    }

    void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests)
    {
        vec pos = d->feetpos();
        loopvj(flags)
        {
            flag &f = flags[j];
            if(f.owner != d)
            {
                static vector<int> targets; // build a list of others who are interested in this
                targets.setsize(0);
                bool home = f.team == d->team;
                ai::checkothers(targets, d, home ? ai::AI_S_DEFEND : ai::AI_S_PURSUE, ai::AI_T_AFFINITY, j, true);
                gameent *e = NULL;
                loopi(numdynents()) if((e = (gameent *)iterdynents(i)) && !e->ai && e->state == CS_ALIVE && sameteam(d->team, e->team))
                { // try to guess what non ai are doing
                    vec ep = e->feetpos();
                    if(targets.find(e->clientnum) < 0 && (ep.squaredist(f.pos()) <= (FLAGRADIUS*FLAGRADIUS*4) || f.owner == e))
                        targets.add(e->clientnum);
                }
                if(home)
                {
                    bool guard = false;
                    if((f.owner && f.team != f.owner->team) || f.droptime || targets.empty()) guard = true;
#if 0
                    else if(d->hasammo(d->ai->weappref))
                    { // see if we can relieve someone who only has a piece of crap
                        gameent *t;
                        loopvk(targets) if((t = getclient(targets[k])))
                        {
                            if((t->ai && !t->hasammo(t->ai->weappref)) || (!t->ai && t->gunselect == GUN_MELEE))
                            {
                                guard = true;
                                break;
                            }
                        }
                    }
#endif
                    if(guard)
                    { // defend the flag
                        ai::interest &n = interests.add();
                        n.state = ai::AI_S_DEFEND;
                        n.node = ai::closestwaypoint(f.pos(), ai::SIGHTMIN, true);
                        n.target = j;
                        n.targtype = ai::AI_T_AFFINITY;
                        n.score = pos.squaredist(f.pos())/100.f;
                    }
                }
                else
                {
                    if(targets.empty())
                    { // attack the flag
                        ai::interest &n = interests.add();
                        n.state = ai::AI_S_PURSUE;
                        n.node = ai::closestwaypoint(f.pos(), ai::SIGHTMIN, true);
                        n.target = j;
                        n.targtype = ai::AI_T_AFFINITY;
                        n.score = pos.squaredist(f.pos());
                    }
                    else
                    { // help by defending the attacker
                        gameent *t;
                        loopvk(targets) if((t = getclient(targets[k])))
                        {
                            ai::interest &n = interests.add();
                            n.state = ai::AI_S_DEFEND;
                            n.node = t->lastnode;
                            n.target = t->clientnum;
                            n.targtype = ai::AI_T_PLAYER;
                            n.score = d->o.squaredist(t->o);
                        }
                    }
                }
            }
        }
    }

    bool aidefend(gameent *d, ai::aistate &b)
    {
        loopv(flags)
        {
            flag &g = flags[i];
            if(g.owner == d) return aihomerun(d, b);
        }
        if(flags.inrange(b.target))
        {
            flag &f = flags[b.target];
            if(f.droptime) return ai::makeroute(d, b, f.pos());
            if(f.owner) return ai::violence(d, b, f.owner, 4);
            int walk = 0;
            if(lastmillis-b.millis >= (201-d->skill)*33)
            {
                static vector<int> targets; // build a list of others who are interested in this
                targets.setsize(0);
                ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, b.target, true);
                gameent *e = NULL;
                loopi(numdynents()) if((e = (gameent *)iterdynents(i)) && !e->ai && e->state == CS_ALIVE && sameteam(d->team, e->team))
                { // try to guess what non ai are doing
                    vec ep = e->feetpos();
                    if(targets.find(e->clientnum) < 0 && (ep.squaredist(f.pos()) <= (FLAGRADIUS*FLAGRADIUS*4) || f.owner == e))
                        targets.add(e->clientnum);
                }
                if(!targets.empty())
                {
                    d->ai->trywipe = true; // re-evaluate so as not to herd
                    return true;
                }
                else
                {
                    walk = 2;
                    b.millis = lastmillis;
                }
            }
            vec pos = d->feetpos();
            float mindist = float(FLAGRADIUS*FLAGRADIUS*8);
            loopv(flags)
            { // get out of the way of the returnee!
                flag &g = flags[i];
                if(pos.squaredist(g.pos()) <= mindist)
                {
                    if(g.owner && g.owner->team == d->team) walk = 1;
                    if(g.droptime && ai::makeroute(d, b, g.pos())) return true;
                }
            }
            return ai::defend(d, b, f.pos(), float(FLAGRADIUS*2), float(FLAGRADIUS*(2+(walk*2))), walk);
        }
        return false;
    }

    bool aipursue(gameent *d, ai::aistate &b)
    {
        if(flags.inrange(b.target))
        {
            flag &f = flags[b.target];
            if(f.owner == d) return aihomerun(d, b);
            if(f.team == d->team)
            {
                if(f.droptime) return ai::makeroute(d, b, f.pos());
                if(f.owner) return ai::violence(d, b, f.owner, 4);
                loopv(flags)
                {
                    flag &g = flags[i];
                    if(g.owner == d) return ai::makeroute(d, b, f.pos());
                }
            }
            else
            {
                if(f.owner) return ai::violence(d, b, f.owner, 4);
                return ai::makeroute(d, b, f.pos());
            }
        }
        return false;
    }
};

extern ctfclientmode ctfmode;
ICOMMAND(dropflag, "", (), { ctfmode.trydropflag(); });
ICOMMAND(numflags, "", (), { intret(ctfmode.flags.length()); });
ICOMMAND(flagbase, "i", (int *n), { result(ctfmode.flags.inrange(*n) ? tempformatstring("%f %f %f", ctfmode.flags[*n].spawnloc.x, ctfmode.flags[*n].spawnloc.y, ctfmode.flags[*n].spawnloc.z) : "0 0 0"); });
ICOMMAND(flagteam, "i", (int *n), { intret(ctfmode.flags.inrange(*n) ? ctfmode.flags[*n].team : 0); });
// 0 = at base, 1 = stolen, 2 = dropped, 3 = fallen under the map
ICOMMAND(flagstate, "i", (int *n),
{
    if(!ctfmode.flags.inrange(*n)) return intret(0);
    intret(ctfmode.flags[*n].owner ? 1 : (ctfmode.flags[*n].droptime ? (ctfmode.flags[*n].droploc.x < 0 ? 3 : 2) : 0));
});
ICOMMAND(flagdroploc, "i", (int *n), { result(ctfmode.flags.inrange(*n) ? tempformatstring("%f %f %f", ctfmode.flags[*n].droploc.x, ctfmode.flags[*n].droploc.y, ctfmode.flags[*n].droploc.z) : "0 0 0"); });
ICOMMAND(flagowner, "i", (int *n), { intret(ctfmode.flags.inrange(*n) ? ctfmode.flags[*n].owner ? ctfmode.flags[*n].owner->clientnum : -1 : -1); });

#endif

#elif SERVMODE

case N_TRYDROPFLAG:
{
    if((ci->state.state!=CS_SPECTATOR || ci->local || ci->privilege) && cq && smode==&ctfmode) ctfmode.dropflag(cq);
    break;
}

case N_TAKEFLAG:
{
    int flag = getint(p), version = getint(p);
    if((ci->state.state!=CS_SPECTATOR || ci->local || ci->privilege) && cq && smode==&ctfmode) ctfmode.takeflag(cq, flag, version);
    break;
}

case N_INITFLAGS:
    if(smode==&ctfmode) ctfmode.parseflags(p, (ci->state.state!=CS_SPECTATOR || ci->privilege || ci->local) && !strcmp(ci->clientmap, smapname));
    break;

#else

case N_INITFLAGS:
{
    ctfmode.parseflags(p, m_ctf);
    break;
}

case N_DROPFLAG:
{
    int ocn = getint(p), flag = getint(p), version = getint(p);
    vec droploc;
    loopk(3) droploc[k] = getint(p)/DMF;
    gameent *o = ocn==self->clientnum ? self : newclient(ocn);
    if(o && m_ctf) ctfmode.dropflag(o, flag, version, droploc);
    break;
}

case N_SCOREFLAG:
{
    int ocn = getint(p), relayflag = getint(p), relayversion = getint(p), goalflag = getint(p), goalversion = getint(p), team = getint(p), score = getint(p), oflags = getint(p);
    gameent *o = ocn==self->clientnum ? self : newclient(ocn);
    if(o && m_ctf) ctfmode.scoreflag(o, relayflag, relayversion, goalflag, goalversion, team, score, oflags);
    break;
}

case N_RETURNFLAG:
{
    int ocn = getint(p), flag = getint(p), version = getint(p);
    gameent *o = ocn==self->clientnum ? self : newclient(ocn);
    if(o && m_ctf) ctfmode.returnflag(o, flag, version);
    break;
}

case N_TAKEFLAG:
{
    int ocn = getint(p), flag = getint(p), version = getint(p);
    gameent *o = ocn==self->clientnum ? self : newclient(ocn);
    if(o && m_ctf) ctfmode.takeflag(o, flag, version);
    break;
}

case N_RESETFLAG:
{
    int flag = getint(p), version = getint(p);
    if(m_ctf) ctfmode.resetflag(flag, version);
    break;
}

#endif

