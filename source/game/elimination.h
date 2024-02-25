#ifndef PARSEMESSAGES
#ifdef SERVMODE

struct eliminationservmode : servmode
#else
struct eliminationclientmode : clientmode
#endif
{
    struct score
    {
        int team, total;
    };

    vector<score> scores;

    score *lookupscore(int team)
    {
        loopv(scores)
        {
            if(sameteam(scores[i].team, team)) return &scores[i];
        }
        return NULL;
    }
    score &makescore(int team)
    {
        score *sc = lookupscore(team);
        if(!sc)
        {
            score &add = scores.add();
            add.total = 0;
            add.team = team;
            return add;
        }
        else return *sc;
    }
    int getteamscore(int team)
    {
        score *sc = lookupscore(team);
        if(sc) return sc->total;
        else return 0;
    }
    void getteamscores(vector<teamscore> &teamscores)
    {
        loopv(scores) teamscores.add(teamscore(scores[i].team, scores[i].total));
    }

    void setup()
    {
#ifdef SERVMODE
        betweenrounds = false;
#endif
        scores.setsize(0);
    }
    void cleanup()
    {
    }
    bool hidefrags()
    {
        return true;
    }
#ifdef SERVMODE
    void roundscore(int winner)
    {
        if(!winner) return;
        score &sc = makescore(winner);
        sc.total++;
        sendf(-1, 1, "ri3", N_ROUNDSCORE, sc.total, sc.team);
        if(gamescorelimit && sc.total >= gamescorelimit)
        {
            if(checkovertime()) return;
            startintermission();
            defformatstring(win, "%s%s \fs\f2team reached the score limit\fr", teamtextcode[winner], teamnames[winner]);
            sendf(-1, 1, "ri2s", N_ANNOUNCE, NULL, win);
        }
        else
        {
            defformatstring(win, "%s%s \f2team won the round", teamtextcode[winner], teamnames[winner]);
            sendf(-1, 1, "ri2s", N_ANNOUNCE, S_ROUND, win);
        }
    }

    struct winstate
    {
        bool over;
        int winner;
    };
    const winstate winningteam()
    {
        winstate won = { false, NULL };
        int aliveteam = NULL;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_ALIVE)
            {
                if(aliveteam)
                {
                    if(!sameteam(aliveteam, ci->team)) return won;
                }
                else aliveteam = ci->team;
            }
        }
        won.over = true;
        won.winner = aliveteam;
        return won;
    }
    virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting)
    {
        if(!connecting) return;
        loopv(scores)
        {
            score &sc = scores[i];
            putint(p, N_ROUNDSCORE);
            putint(p, sc.total);
            putint(p, sc.team);
        }
    }
    void entergame(clientinfo *ci)
    {
        checkround = true;
    }
    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        checkround = true;
    }
    void died(clientinfo *victim, clientinfo *actor)
    {
        checkround = true;
    }
    bool canspawn(clientinfo *ci, bool connecting)
    {
        const int numplayers = numclients(-1, true, false);
        return numplayers <= 1 || (numplayers <= 2 && connecting) || (numplayers <= 1 && ci->state.aitype==AI_BOT);
    }
    bool canchangeteam(clientinfo *ci, int oldteam, int newteam)
    {
        return true;
        // only allow two teams?
    }
    void update()
    {
        if(!checkround) return;
        checkround = false;
        if(betweenrounds) return;
        winstate won = winningteam();
        if(won.over)
        {
            roundscore(won.winner);
            endround();
        }
    }
#else

    bool canfollow(gameent *s, gameent *f)
    {
        if(s->state!= CS_SPECTATOR || f->state == CS_DEAD) return false;
        if(sameteam(s->team, f->team)) return true;
        loopv(players)
        { // if any living players are on your team, you can't spectate an opponent
            gameent *p = players[i];
            if(p->state == CS_ALIVE && sameteam(p->team, s->team)) return false;
        }
        return true;
    }

#endif

};
#elif SERVMODE
#else
case N_ROUNDSCORE:
{
    int score = getint(p),
        team  = getint(p);
    if(p.overread() || !team) break;
    eliminationmode.makescore(team).total = score;
    break;
}
#endif
