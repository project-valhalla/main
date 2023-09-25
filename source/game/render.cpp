#include "game.h"

extern int maxparticletextdistance;

namespace game
{
    VARP(ragdoll, 0, 1, 1);
    VARP(ragdollmillis, 0, 10000, 300000);
    VARP(ragdollfade, 0, 400, 5000);
    VARP(forceplayermodels, 0, 0, 1);
    VARP(hidedead, 0, 0, 1);

    extern int playermodel;

    vector<gameent *> ragdolls;

    void saveragdoll(gameent *d)
    {
        if(!d->ragdoll || !ragdollmillis || (!ragdollfade && lastmillis > d->lastpain + ragdollmillis)) return;
        gameent *r = new gameent(*d);
        r->lastupdate = ragdollfade && lastmillis > d->lastpain + max(ragdollmillis - ragdollfade, 0) ? lastmillis - max(ragdollmillis - ragdollfade, 0) : d->lastpain;
        r->edit = NULL;
        r->ai = NULL;
        if(d==self) r->playermodel = playermodel;
        ragdolls.add(r);
        d->ragdoll = NULL;
    }

    void clearragdolls()
    {
        ragdolls.deletecontents();
    }

    void moveragdolls()
    {
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            if(lastmillis > d->lastupdate + ragdollmillis)
            {
                delete ragdolls.remove(i--);
                continue;
            }
            moveragdoll(d);
        }
    }

    static const int playercolors[] =
    {
        0xA12020,
        0xA15B28,
        0xB39D52,
        0x3E752F,
        0x3F748C,
        0x214C85,
        0xB3668C,
        0x523678,
        0xB3ADA3
    };

    static const int playercolorsblue[] =
    {
        0x27508A,
        0x3F748C,
        0x3B3B80,
        0x5364B5
    };

    static const int playercolorsred[] =
    {
        0xAC2C2A,
        0x992417,
        0x802438,
        0xA3435B
    };

    extern void changedplayercolor();
    VARFP(playercolor, 0, 4, sizeof(playercolors)/sizeof(playercolors[0])-1, changedplayercolor());
    VARFP(playercolorblue, 0, 0, sizeof(playercolorsblue)/sizeof(playercolorsblue[0])-1, changedplayercolor());
    VARFP(playercolorred, 0, 0, sizeof(playercolorsred)/sizeof(playercolorsred[0])-1, changedplayercolor());

    static const playermodelinfo playermodels[2] =
    {
        { "player/bones",  "player/bones/arm", true, S_PAIN_MALE,   S_DIE_MALE,   S_TAUNT_MALE,   },
        { "player/bonnie", "player/bones/arm", true, S_PAIN_FEMALE, S_DIE_FEMALE, S_TAUNT_FEMALE  }
    };

    extern void changedplayermodel();
    VARFP(playermodel, 0, 0, sizeof(playermodels)/sizeof(playermodels[0])-1, changedplayermodel());

    int chooserandomplayermodel(int seed)
    {
        return (seed&0xFFFF)%(sizeof(playermodels)/sizeof(playermodels[0]));
    }

    int getplayermodel(gameent *d)
    {
        return d==self || forceplayermodels ? playermodel : d->playermodel;
    }

    const playermodelinfo *getplayermodelinfo(int n)
    {
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0])) return NULL;
        return &playermodels[n];
    }

    const playermodelinfo &getplayermodelinfo(gameent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(playermodel);
        if(!mdl) mdl = getplayermodelinfo(playermodel);
        return *mdl;
    }

    int getplayercolor(int team, int color)
    {
        #define GETPLAYERCOLOR(playercolors) \
            return playercolors[color%(sizeof(playercolors)/sizeof(playercolors[0]))];
        switch(team)
        {
            case 1: GETPLAYERCOLOR(playercolorsblue)
            case 2: GETPLAYERCOLOR(playercolorsred)
            default: GETPLAYERCOLOR(playercolors)
        }
    }

    ICOMMAND(getplayercolor, "ii", (int *color, int *team), intret(getplayercolor(*team, *color)));

    int getplayercolor(gameent *d, int team)
    {
        if(d==self) switch(team)
        {
            case 1: return getplayercolor(1, playercolorblue);
            case 2: return getplayercolor(2, playercolorred);
            default: return getplayercolor(0, playercolor);
        }
        else return getplayercolor(team, (d->playercolor>>(5*team))&0x1F);
    }

    void changedplayermodel()
    {
        if(self->clientnum < 0) self->playermodel = playermodel;
        if(self->ragdoll) cleanragdoll(self);
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            if(!d->ragdoll) continue;
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl) continue;
            }
            cleanragdoll(d);
        }
        loopv(players)
        {
            gameent *d = players[i];
            if(d == self || !d->ragdoll) continue;
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl) continue;
            }
            cleanragdoll(d);
        }
    }

    void changedplayercolor()
    {
        if(self->clientnum < 0) self->playercolor = playercolor | (playercolorblue<<5) | (playercolorred<<10);
    }

    void syncplayer()
    {
        if(self->playermodel != playermodel)
        {
            self->playermodel = playermodel;
            addmsg(N_SWITCHMODEL, "ri", self->playermodel);
        }

        int col = playercolor | (playercolorblue<<5) | (playercolorred<<10);
        if(self->playercolor != col)
        {
            self->playercolor = col;
            addmsg(N_SWITCHCOLOR, "ri", self->playercolor);
        }
    }

    void preloadplayermodel()
    {
        loopi(sizeof(playermodels)/sizeof(playermodels[0]))
        {
            const playermodelinfo *mdl = getplayermodelinfo(i);
            if(!mdl) break;
            if(i != playermodel && (!multiplayer(false) || forceplayermodels)) continue;
            preloadmodel(mdl->directory);
            preloadmodel(zombies[playermodel].directory);
        }
    }

    int numanims() { return NUMANIMS; }

    void findanims(const char *pattern, vector<int> &anims)
    {
        loopi(sizeof(animnames)/sizeof(animnames[0])) if(matchanim(animnames[i], pattern)) anims.add(i);
    }

    VAR(animoverride, -1, 0, NUMANIMS-1);
    VAR(testanims, 0, 0, 1);
    VAR(testpitch, -90, 0, 90);

    void renderplayer(gameent *d, const playermodelinfo &playermodel, int color, int team, float fade, int flags = 0, bool mainpass = true)
    {
        if(gore && d->gibbed()) return;
        int lastaction = d->lastaction, anim = ANIM_IDLE|ANIM_LOOP, attack = 0, delay = 0;
        if(d->state==CS_ALIVE)
        {
            if(d->lastattack >= 0)
            {
                attack = attacks[d->lastattack].anim;
                delay = attacks[d->lastattack].attackdelay+50;
            }
            if(d->lasttaunt && lastmillis-d->lasttaunt<1000 && lastmillis-d->lastaction>delay)
            {
                lastaction = d->lasttaunt;
                anim = attack = ANIM_TAUNT;
                delay = 1000;
            }
            if(d->lastswitch && lastmillis-d->lastswitch <= 600)
            {
                lastaction = d->lastswitch;
                anim = attack = ANIM_SWITCH;
                delay = 600;
            }
        }
        modelattach a[5];
        int ai = 0;
        if(guns[d->gunselect].worldmodel)
        {
            int vanim = ANIM_VWEP_IDLE|ANIM_LOOP, vtime = 0;
            if(lastaction && d->lastattack >= 0 && attacks[d->lastattack].gun==d->gunselect && lastmillis < lastaction + delay)
            {
                vanim = attacks[d->lastattack].vwepanim;
                vtime = lastaction;
            }
            a[ai++] = modelattach("tag_weapon", guns[d->gunselect].worldmodel, vanim, vtime);
        }
        if(mainpass && !(flags&MDL_ONLYSHADOW))
        {
            d->muzzle = vec(-1, -1, -1);
            if(guns[d->gunselect].worldmodel) a[ai++] = modelattach("tag_muzzle", &d->muzzle);
        }
        const char *playermodelfile = !d->zombie ? playermodel.directory : zombies[d->playermodel].directory;
        float yaw = testanims && d==self ? 0 : d->yaw,
              pitch = testpitch && d==self ? testpitch : d->pitch;
        vec o = d->feetpos();
        int basetime = 0;
        if(animoverride) anim = (animoverride<0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
        else if(d->state==CS_DEAD)
        {
            anim = ANIM_DYING|ANIM_NOPITCH;
            basetime = d->lastpain;
            if(ragdoll && playermodel.ragdoll) anim |= ANIM_RAGDOLL;
            else if(lastmillis-basetime>1000) anim = ANIM_DEAD|ANIM_LOOP|ANIM_NOPITCH;
        }
        else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) anim = ANIM_EDIT|ANIM_LOOP;
        else if(d->state==CS_LAGGED)                            anim = ANIM_LAG|ANIM_NOPITCH|ANIM_LOOP;
        else
        {
            if(lastmillis-d->lastpain < 300)
            {
                anim = ANIM_PAIN;
                basetime = d->lastpain;
            }
            else if(d->lastpain < lastaction && lastmillis-lastaction < delay)
            {
                anim = attack;
                basetime = lastaction;
            }

            if(d->inwater && d->physstate<=PHYS_FALL) anim |= (((game::allowmove(d) && (d->move || d->strafe)) || d->vel.z+d->falling.z>0 ? ANIM_SWIM : ANIM_SINK)|ANIM_LOOP)<<ANIM_SECONDARY;
            else
            {
                static const int dirs[9] =
                {
                    ANIM_RUN_SE, ANIM_RUN_S, ANIM_RUN_SW,
                    ANIM_RUN_E,  0,          ANIM_RUN_W,
                    ANIM_RUN_NE, ANIM_RUN_N, ANIM_RUN_NW
                };
                int dir = dirs[(d->move+1)*3 + (d->strafe+1)];
                if(d->timeinair>100) anim |= ((dir && game::allowmove(d) ? dir+ANIM_JUMP_N-ANIM_RUN_N : ANIM_JUMP) | ANIM_END) << ANIM_SECONDARY;
                else if(dir && game::allowmove(d)) anim |= (dir | ANIM_LOOP) << ANIM_SECONDARY;
            }

            if(d->crouching) switch((anim>>ANIM_SECONDARY)&ANIM_INDEX)
            {
                case ANIM_IDLE: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH<<ANIM_SECONDARY; break;
                case ANIM_JUMP: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH_JUMP<<ANIM_SECONDARY; break;
                case ANIM_SWIM: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH_SWIM<<ANIM_SECONDARY; break;
                case ANIM_SINK: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH_SINK<<ANIM_SECONDARY; break;
                case 0: anim |= (ANIM_CROUCH|ANIM_LOOP)<<ANIM_SECONDARY; break;
                case ANIM_RUN_N: case ANIM_RUN_NE: case ANIM_RUN_E: case ANIM_RUN_SE: case ANIM_RUN_S: case ANIM_RUN_SW: case ANIM_RUN_W: case ANIM_RUN_NW:
                    anim += (ANIM_CROUCH_N - ANIM_RUN_N) << ANIM_SECONDARY;
                    break;
                case ANIM_JUMP_N: case ANIM_JUMP_NE: case ANIM_JUMP_E: case ANIM_JUMP_SE: case ANIM_JUMP_S: case ANIM_JUMP_SW: case ANIM_JUMP_W: case ANIM_JUMP_NW:
                    anim += (ANIM_CROUCH_JUMP_N - ANIM_JUMP_N) << ANIM_SECONDARY;
                    break;
            }

            if((anim&ANIM_INDEX)==ANIM_IDLE && (anim>>ANIM_SECONDARY)&ANIM_INDEX) anim >>= ANIM_SECONDARY;
        }
        if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX)) anim |= (ANIM_IDLE|ANIM_LOOP)<<ANIM_SECONDARY;
        if(d!=self) flags |= MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY;
        if(d->type==ENT_PLAYER) flags |= MDL_FULLBRIGHT;
        else flags |= MDL_CULL_DIST;
        if(!mainpass) flags &= ~(MDL_FULLBRIGHT | MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY | MDL_CULL_DIST);
        float trans = 1;
        if(d->state == CS_LAGGED) trans = 0.5f;
        else if(d->state == CS_ALIVE && camera1->o.dist(d->o) < d->radius) trans = 0.1f;
        else flags &= ~(MDL_NOSHADOW);
        rendermodel(playermodelfile, anim, o, yaw, pitch, 0, flags, d, a[0].tag ? a : NULL, basetime, 0, fade, vec4(vec::hexcolor(color), trans));
    }

    static inline void renderplayer(gameent *d, float fade = 1, int flags = 0)
    {
        int team = m_teammode && validteam(d->team) ? d->team : 0;
        renderplayer(d, getplayermodelinfo(d), getplayercolor(d, team), team, fade, flags);
    }

    void rendergame()
    {
        ai::render();

        bool third = isthirdperson();
        gameent *f = followingplayer(), *exclude = third ? NULL : f;
        loopv(players)
        {
            gameent *d = players[i];
            if(d == self || d->state==CS_SPECTATOR || d->state==CS_SPAWNING || d->lifesequence < 0 || d == exclude || (d->state==CS_DEAD && hidedead)) continue;
            renderplayer(d);
            copystring(d->info, colorname(d));
            if(d->state!=CS_DEAD)
            {
                int team = m_teammode && validteam(d->team) ? d->team : 0;
                gameent *hud = followingplayer(self);
                if(isally(hud, d) && hud->o.dist(d->o) > maxparticletextdistance)
                {
                    particle_icon(d->abovehead(), 1, 3, PART_GAME_ICONS, 1, 0xFFFFFF, 3.0f, 0);
                }
                else particle_text(d->abovehead(), d->info, PART_TEXT, 1, teamtextcolor[team], 2.0f);
            }
        }
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            float fade = 1.0f;
            if(ragdollmillis && ragdollfade)
                fade -= clamp(float(lastmillis - (d->lastupdate + max(ragdollmillis - ragdollfade, 0)))/min(ragdollmillis, ragdollfade), 0.0f, 1.0f);
            renderplayer(d, fade);
        }
        if(exclude)
            renderplayer(exclude, 1, MDL_ONLYSHADOW);
        else if(!f && (self->state==CS_ALIVE || (self->state==CS_EDITING && third) || (self->state==CS_DEAD && !hidedead)))
            renderplayer(self, 1, third ? 0 : MDL_ONLYSHADOW);
        entities::renderentities();
        renderbouncers();
        renderprojectiles();
        if(cmode) cmode->rendergame();
    }

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);

    FVAR(swaystep, 1, 33.5f, 100);
    FVAR(swayside, 0, 0.05f, 1);
    FVAR(swayup, -1, 0.11f, 1);
    FVAR(swayrollfactor, 1, 4.2f, 30);
    FVAR(swaydecay, 0.1f, 0.996f, 0.9999f);
    FVAR(swayinertia, 0.0f, 0.04f, 1.0f);
    FVAR(swaymaxinertia, 0.0f, 15.0f, 1000.0f);

    float swayfade = 0, swayspeed = 0, swaydist = 0, swayyaw = 0, swaypitch = 0;
    vec swaydir(0, 0, 0);

    static void updatesway(gameent* d, vec& sway, int curtime)
    {
        vec sidedir = vec((d->yaw + 90) * RAD, 0.0f),
            translation = vec(0, 0, 0);

        float steplen = swaystep, steps = swaydist / steplen * M_PI;

        // magic floats to generate the animation cycle
        float f1 = sinf(steps * 2.0f) + 1,
              f2 = (f1 * f1 * 0.25f) - 0.5f;

        float rotyaw = 0, rotpitch = 0;

        rotyaw += swayside * f2 * 24.0f;
        rotpitch += swayup * f1 * -10.0f;

        // "Look-around" animation.
        static int lastsway = 0;
        static vec2 lastcamera = vec2(camera1->yaw, camera1->pitch);
        static vec2 cameravel = vec2(0, 0);

        if (lastmillis != lastsway) // prevent running the inertia math multiple times in the same frame
        {
            vec2 curcamera = vec2(camera1->yaw, camera1->pitch);
            vec2 camerarot = vec2(lastcamera).sub(curcamera);

            if (camerarot.x > 180.0f) camerarot.x -= 360.0f;
            else if (camerarot.x < -180.0f) camerarot.x += 360.0f;

            cameravel.mul(powf(swaydecay, curtime));
            cameravel.add(vec2(camerarot).mul(swayinertia));
            cameravel.clamp(-swaymaxinertia, swaymaxinertia);

            lastcamera = curcamera;
            lastsway = lastmillis;
        }
        translation.add(sidedir.mul(cameravel.x * 0.06f));
        translation.z += cameravel.y * 0.045f; // translation vector for the calculations.
        sway.add(translation); // add the translation to the swaydir vector, where the weapon model is at
        swayyaw += rotyaw;
        swaypitch += rotpitch;
    }

    void swayhudgun(int curtime)
    {
        gameent *d = hudplayer();
        if(d->state != CS_SPECTATOR)
        {
            if(d->physstate >= PHYS_SLOPE)
            {
                swayspeed = min(sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y), d->speed);
                swaydist += swayspeed*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade = 1;
            }
            else if(swayfade > 0)
            {
                swaydist += swayspeed*swayfade*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade -= 0.5f*(curtime*d->speed)/(swaystep*1000.0f);
            }

            float k = pow(0.7f, curtime/10.0f);
            swaydir.mul(k);
            vec vel(d->vel);
            vel.add(d->falling);
            swaydir.add(vec(vel).mul((1-k)/(8*max(vel.magnitude(), d->speed))));
        }
    }

    struct hudent : dynent
    {
        hudent() { type = ENT_CAMERA; }
    } guninterp;

    void drawhudmodel(gameent *d, int anim, int basetime)
    {
        const char *file = guns[d->gunselect].model;
        if(!file) return;

        vec sway;
        vecfromyawpitch(d->yaw, 0, 0, 1, sway);
        float steps = swaydist/swaystep*M_PI;
        sway.mul(swayside*cosf(steps));
        sway.z = swayup*(fabs(sinf(steps)) - 1);
        updatesway(d, sway, curtime);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway) sway = d->o;

        const playermodelinfo &playermodel = getplayermodelinfo(d);
        int team = m_teammode && validteam(d->team) ? d->team : 0, color = getplayercolor(d, team);
        defformatstring(gunname, "%s/%s", playermodel.armdirectory, file);
        modelattach a[2];
        d->muzzle = vec(-1, -1, -1);
        a[0] = modelattach("tag_muzzle", &d->muzzle);
        float swayroll = d->roll * swayrollfactor;
        rendermodel(gunname, anim, sway, d->yaw, d->pitch, swayroll, MDL_NOBATCH, NULL, a, basetime, 0, 1, vec4(vec::hexcolor(color), 1));
        if(d->muzzle.x >= 0) d->muzzle = calcavatarpos(d->muzzle, 12);
    }

    void drawhudgun()
    {
        gameent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_EDITING || !hudgun || editmode)
        {
            d->muzzle = self->muzzle = vec(-1, -1, -1);
            return;
        }

        int anim = ANIM_GUN_IDLE|ANIM_LOOP, basetime = 0;
        bool animateattack = d->lastattack == ATK_MELEE || attacks[d->lastattack].gun == d->gunselect;
        if(animateattack && d->lastaction && d->lastattack >= 0 && lastmillis - d->lastaction < attacks[d->lastattack].attackdelay)
        {
            if(anim >= 0) anim = attacks[d->lastattack].hudanim;
            basetime = d->lastaction;
        }
        if(lastmillis - d->lastswitch <= 600)
        {
            if(anim >= 0) anim = ANIM_GUN_SWITCH;
            basetime = d->lastswitch;
        }
        drawhudmodel(d, anim, basetime);
    }

    void renderavatar()
    {
        drawhudgun();
    }

    void renderplayerpreview(int model, int color, int team, int weap)
    {
        static gameent *previewent = NULL;
        if(!previewent)
        {
            previewent = new gameent;
            loopi(NUMGUNS) previewent->ammo[i] = 1;
        }
        float height = previewent->eyeheight + previewent->aboveeye,
              zrad = height/2;
        vec2 xyrad = vec2(previewent->xradius, previewent->yradius).max(height/4);
        previewent->o = calcmodelpreviewpos(vec(xyrad, zrad), previewent->yaw).addz(previewent->eyeheight - zrad);
        previewent->gunselect = validgun(weap) ? weap : GUN_RAIL;
        const playermodelinfo *mdlinfo = getplayermodelinfo(model);
        if(!mdlinfo) return;
        renderplayer(previewent, *mdlinfo, getplayercolor(team, color), team, 1, 0, false);
    }

    vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d)
    {
        if(d->muzzle.x >= 0) return d->muzzle;
        vec offset(from);
        if(d!=hudplayer() || isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            offset.z += (d->aboveeye + d->eyeheight)*0.75f - d->eyeheight;
            vecfromyawpitch(d->yaw, 0, 0, -1, right);
            offset.add(right.mul(0.5f*d->radius));
            offset.add(front);
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if(hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        else offset.sub(vec(camup).mul(0.8f));
        return offset;
    }

    void preloadweapons()
    {
        const playermodelinfo &playermodel = getplayermodelinfo(self);
        loopi(NUMGUNS)
        {
            const char *file = guns[i].model;
            if(!file) continue;
            string fname;
            formatstring(fname, "%s/%s", playermodel.armdirectory, file);
            preloadmodel(fname);
            if(guns[i].worldmodel) preloadmodel(guns[i].worldmodel);
        }
    }

    void preloadsounds()
    {
        for(int i = S_JUMP1; i <= S_ANNOUNCER_INVULNERABILITY; i++) preloadsound(i);
        for(int i = S_HIT; i <= S_INTERMISSION_WIN; i++) preloadsound(i);
    }

    void preload()
    {
        preloadweapons();
        preloadbouncers();
        preloadplayermodel();
        preloadsounds();
        entities::preloadentities();
    }

}

