#include "game.h"

namespace entities
{
    using namespace game;

    int extraentinfosize() { return 0; }       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
    {
    }

    void readent(entity &e, char *buf, int ver)     // read from disk, and init
    {
    }

#ifndef STANDALONE
    vector<extentity *> ents;

    vector<extentity *> &getents() { return ents; }

    bool mayattach(extentity &e) { return false; }
    bool attachent(extentity &e, extentity &a) { return false; }

    const char *itemname(int i)
    {
        return NULL;
#if 0
        int t = ents[i]->type;
        if(!validitem(t)) return NULL;
        return itemstats[t-I_FIRST].name;
#endif
    }

    int itemicon(int i)
    {
        return -1;
#if 0
        int t = ents[i]->type;
        if(!validitem(t)) return -1;
        return itemstats[t-I_FIRST].icon;
#endif
    }

    const char *entmdlname(int type)
    {
        static const char * const entmdlnames[MAXENTTYPES] =
        {
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            "item/teleport", NULL, NULL,
            NULL,

            "item/ammo/shells", "item/ammo/bullets", "item/ammo/pulse", "item/ammo/rockets", "item/ammo/rrounds",
            "item/health", "item/shield/yellow", "item/shield/red",
            "item/health/super", "item/health/mega", "item/ddamage", "item/haste", "item/armor", "item/ammo/item", "", "", "item/ankh"
        };
        return entmdlnames[type];
    }

    const char *entmodel(const entity &e)
    {
        if(e.type == TELEPORT)
        {
            if(e.attr2 > 0) return mapmodelname(e.attr2);
            if(e.attr2 < 0) return NULL;
        }
        return e.type < MAXENTTYPES ? entmdlname(e.type) : NULL;
    }

    void preloadentities()
    {
        loopi(MAXENTTYPES)
        {
            if(!server::canspawnitem(i)) continue;
            const char *mdl = entmdlname(i);
            if(!mdl) continue;
            preloadmodel(mdl);
        }
        loopv(ents)
        {
            extentity &e = *ents[i];
            switch(e.type)
            {
                case TELEPORT:
                    if(e.attr2 > 0) preloadmodel(mapmodelname(e.attr2));
                case JUMPPAD:
                    if(e.attr4 > 0) preloadmapsound(e.attr4);
                    break;
            }
        }
    }

    void renderentities()
    {
        loopv(ents)
        {
            extentity &e = *ents[i];
            int revs = 10;
            switch(e.type)
            {
                case TELEPORT:
                    if(e.attr2 < 0) continue;
                    break;
                default:
                    if(!e.spawned() || !validitem(e.type)) continue;
                    break;
            }
            const char *mdlname = entmodel(e);
            if(mdlname)
            {
                vec p = e.o;
                p.z += 1+sinf(lastmillis/100.0+e.o.x+e.o.y)/20;
                rendermodel(mdlname, ANIM_MAPMODEL|ANIM_LOOP, p, lastmillis/(float)revs, 0, 0, MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
            }
        }
    }

    void addammo(int type, int &v, bool local)
    {
        itemstat &is = itemstats[type-I_AMMO_SG];
        v += is.add;
        if(v>is.max) v = is.max;
        if(local) msgsound(is.sound);
    }
    void repammo(gameent *d, int type, bool local)
    {
        addammo(type, d->ammo[type-I_AMMO_SG+GUN_SG], local);
    }

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void pickupeffects(int n, gameent *d)
    {
        if(!ents.inrange(n)) return;
        int type = ents[n]->type;
        if(!validitem(type)) return;
        ents[n]->clearspawned();
        if(!d) return;
        /*
        itemstat &is = itemstats[type-I_HEALTH];
        if(d!=player1 || isthirdperson())
        {
            //particle_text(d->abovehead(), is.name, PART_TEXT, 2000, 0xFFC864, 4.0f, -8);
            particle_icon(d->abovehead(), is.icon%4, is.icon/4, PART_HUD_ICON_GREY, 2000, 0xFFFFFF, 2.0f, -8);
        }
        */
        playsound(itemstats[type-I_AMMO_SG].sound, NULL, d!=player1 ? &d->o : NULL, NULL, 0, 0, 0, -1, 0, 1800);
        d->pickup(type);
        int asnd = -1;
        switch(type)
        {
            case I_DDAMAGE: asnd = S_ANNOUNCER_DDAMAGE; break;
            case I_HASTE: asnd = S_ANNOUNCER_HASTE; break;
            case I_ARMOUR: asnd = S_ANNOUNCER_ARMOUR; break;
            case I_UAMMO: asnd = S_ANNOUNCER_UAMMO; break;
            default: asnd = -1; break;
        }
        if(d==hudplayer() && asnd >= 0) playsound(asnd, NULL, NULL, NULL, SND_ANNOUNCER);
    }

    // these functions are called when the client touches the item

    void tpeffects(vec p)
    {
        particle_splash(PART_SPARK2, 50, 200, p, 0x89E5FB, 0.20f, 250, 8);
        particle_fireball(p, 10.0f, PART_PULSE_BURST, 500, 0x89E5FB, 1.0f);
        adddynlight(p, 100, vec(0.50f, 1.0f, 1.4f), 500, 100);
    }

    void teleporteffects(gameent *d, int tp, int td, bool local)
    {
        if(d->state!=CS_ALIVE) return;
        if(ents.inrange(tp) && ents[tp]->type == TELEPORT)
        {
            extentity &e = *ents[tp];
            if(e.attr4 >= 0)
            {
                int snd = S_TELEPORT, flags = 0;
                if(e.attr4 > 0) { snd = e.attr4; flags = SND_MAP; }
                if(d==hudplayer() && !isthirdperson())
                {
                    playsound(snd, NULL, NULL, NULL, flags);
                    if(ents.inrange(td) && ents[td]->type == TELEDEST)
                        adddynlight(ents[td]->o, 100, vec(0.50f, 1.0f, 1.4f), 800, 100);
                }
                else
                {
                    playsound(snd, NULL, &e.o, NULL, flags);
                    tpeffects(d->o);
                    if(ents.inrange(td) && ents[td]->type == TELEDEST)
                    {
                        playsound(S_TELEDEST, NULL, &ents[td]->o, NULL, flags);
                        tpeffects(ents[td]->o);
                    }
                }
            }
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(32, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_TELEPORT);
            putint(p, d->clientnum);
            putint(p, tp);
            putint(p, td);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void jumppadeffects(gameent *d, int jp, bool local)
    {
        if(d->state!=CS_ALIVE) return;
        if(ents.inrange(jp) && ents[jp]->type == JUMPPAD)
        {
            extentity &e = *ents[jp];
            if(e.attr4 >= 0)
            {
                int snd = S_JUMPPAD, flags = 0;
                if(e.attr4 > 0) { snd = e.attr4; flags = SND_MAP; }
                if(d == player1) playsound(snd, NULL, NULL, NULL, flags);
                else playsound(snd, NULL, &e.o, NULL, flags);
            }
        }
        if(local && d->clientnum >= 0)
        {
            sendposition(d);
            packetbuf p(16, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_JUMPPAD);
            putint(p, d->clientnum);
            putint(p, jp);
            sendclientpacket(p.finalize(), 0);
            flushclient();
        }
    }

    void teleport(int n, gameent *d)     // also used by monsters
    {
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(TELEDEST, e+1);
            if(e==beenhere || e<0) { conoutf(CON_WARN, "no teleport destination for tag %d", tag); return; }
            if(beenhere<0) beenhere = e;
            if(ents[e]->attr2==tag)
            {
                teleporteffects(d, n, e, true);
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                if(ents[e]->attr3 > 0)
                {
                    vec dir;
                    vecfromyawpitch(d->yaw, 0, 1, 0, dir);
                    float speed = d->vel.magnitude2();
                    d->vel.x = dir.x*speed;
                    d->vel.y = dir.y*speed;
                }
                else d->vel = vec(0, 0, 0);
                entinmap(d);
                updatedynentcache(d);
                ai::inferwaypoints(d, ents[n]->o, ents[e]->o, 16.f);
                break;
            }
        }
    }

    VARR(teleteam, 0, 1, 1);

    void trypickup(int n, gameent *d)
    {
        switch(ents[n]->type)
        {
            default:
                if(d->canpickup(ents[n]->type))
                {
                    addmsg(N_ITEMPICKUP, "rci", d, n);
                    ents[n]->clearspawned(); // even if someone else gets it first
                }
                break;

            case TELEPORT:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<500) break;
                if(!teleteam && m_teammode) break;
                if(ents[n]->attr3 > 0)
                {
                    defformatstring(hookname, "can_teleport_%d", ents[n]->attr3);
                    if(!execidentbool(hookname, true)) break;
                }
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                teleport(n, d);
                break;
            }

            case JUMPPAD:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<300) break;
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                jumppadeffects(d, n, true);
                if(d->ai) d->ai->becareful = true;
                d->falling = vec(0, 0, 0);
                d->physstate = PHYS_FALL;
                d->timeinair = 1;
                d->vel = vec(ents[n]->attr3*10.0f, ents[n]->attr2*10.0f, ents[n]->attr1*12.5f);
                break;
            }
        }
    }

    void checkitems(gameent *d)
    {
        if(d->state!=CS_ALIVE && d->state!=CS_SPECTATOR) return;
        vec o = d->feetpos();
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type!=TELEPORT && d->state!=CS_ALIVE) continue;
            if(e.type==NOTUSED) continue;
            if(!e.spawned() && e.type!=TELEPORT && e.type!=JUMPPAD) continue;
            float dist = e.o.dist(o);
            if(dist<(e.type==TELEPORT ? 16 : 12)) trypickup(i, d);

            // really ugly code, it does nothing for hacks... wrote to test the telefrag concept
            if(d->state!=CS_ALIVE) continue;
            if(e.type==TELEPORT && lastmillis-d->lastpickupmillis<=50)
            {
                vec v;
                if(d->aitype==AI_BOT) v = d->ai->target;
                else v = worldpos;
                specialattack(d, ATK_TELEPORT, d->o, v);
            }
        }
    }

    void updatepowerups(int time, gameent *d)
    {
        bool hud = (d == hudplayer());
        if(d->juggernaut)
        {
            d->stoppowerupsound();
            d->juggernautchan = playsound(S_JUGGERNAUT_LOOP, NULL, hud ? NULL : &d->o, NULL, 0, -1, 1400, d->juggernautchan, 500);
            adddynlight(d->abovehead(), 30, vec(1, 0.50f, 1), 1, 0, DL_FLASH|L_NOSHADOW);
            return;
        }
        if(d->damagemillis)
        {
            d->ddamagechan = playsound(S_DDAMAGE_LOOP, NULL, hud ? NULL : &d->o, NULL, 0, -1, 500, d->ddamagechan, 200);
            adddynlight(d->abovehead(), 30, vec(1, 0.50f, 0.50f), 1, 0, DL_FLASH|L_NOSHADOW);
            particle_icon(d->abovehead(), HICON_DDAMAGE%5, HICON_DDAMAGE/5, PART_HUD_ICON, 1, 0xFFFFFF, 3.0f, NULL);
            if((d->damagemillis -= time)<=0)
            {
                d->damagemillis = 0;
                stopsound(S_DDAMAGE_LOOP, d->ddamagechan);
                playsound(S_DDAMAGE_TIMEOUT, d);
            }
        }
        if(d->hastemillis)
        {
            d->hastechan = playsound(S_HASTE_LOOP, NULL, hud ? NULL : &d->o, NULL, 0, -1, 500, d->hastechan, 200);
            adddynlight(d->abovehead(), 30, vec(0.50f, 1, 0.50f), 1, 0, DL_FLASH|L_NOSHADOW);
            particle_icon(d->abovehead(), HICON_HASTE%5, HICON_HASTE/5, PART_HUD_ICON, 1, 0xFFFFFF, 3.0f, NULL);
            if((d->hastemillis -= time)<=0)
            {
                d->hastemillis = 0;
                stopsound(S_HASTE_LOOP, d->hastechan);
                playsound(S_HASTE_TIMEOUT, d);
            }
            if((d->move || d->strafe) || d->jumping)
            {
                //regular_particle_flame(PART_FLAME, d->feetpos(), 1.5f, 1, 0x309020, 4, 1.0f, 100.0f, 300.0f);
                //adddynlight(d->o, 20, vec(1, 2, 1), 1, 20, DL_FLASH, 0, vec(0, 0, 0), d);
            }
        }
        if(d->armourmillis)
        {
            d->armourchan = playsound(S_ARMOUR_LOOP, NULL, hud ? NULL : &d->o, NULL, 0, -1, 500, d->armourchan, 200);
            adddynlight(d->abovehead(), 30, vec(0.50f, 0.50f, 1), 1, 0, DL_FLASH|L_NOSHADOW);
            particle_icon(d->abovehead(), HICON_ARMOUR%5, HICON_ARMOUR/5, PART_HUD_ICON, 1, 0xFFFFFF, 3.0f, NULL);
            if((d->armourmillis -= time)<=0)
            {
                d->armourmillis = 0;
                stopsound(S_ARMOUR_LOOP, d->armourchan);
                playsound(S_ARMOUR_TIMEOUT, d);
            }
        }
        if(d->ammomillis)
        {
            d->ammochan = playsound(S_UAMMO_LOOP, NULL, hud ? NULL : &d->o, NULL, 0, -1, 1200, d->ammochan, 500);
            adddynlight(d->abovehead(), 30, vec(1, 1, 1), 1, 0, DL_FLASH|L_NOSHADOW);
            particle_icon(d->abovehead(), HICON_UAMMO%5, HICON_UAMMO/5, PART_HUD_ICON, 1, 0xFFFFFF, 3.0f, NULL);
            if((d->ammomillis -= time)<=0)
            {
                d->ammomillis = 0;
                stopsound(S_UAMMO_LOOP, d->ammochan);
                playsound(S_UAMMO_TIMEOUT, d);
            }
        }
        if(d->invulnmillis)
        {
            d->invulnchan = playsound(S_INVULNERABILITY_LOOP, NULL, hud ? NULL : &d->o, NULL, 0, -1, 1200, d->invulnchan, 500);
            adddynlight(d->abovehead(), 30, vec(1, 1, 0.50f), 1, 0, DL_FLASH|L_NOSHADOW);
            particle_icon(d->abovehead(), HICON_INVULNERABILITY%5, HICON_INVULNERABILITY/5, PART_HUD_ICON, 1, 0xFFFFFF, 3.0f, NULL);
            if((d->invulnmillis -= time)<=0)
            {
                d->invulnmillis = 0;
                stopsound(S_INVULNERABILITY_LOOP, d->invulnchan);
                playsound(S_INVULNERABILITY_TIMEOUT, d);
            }
        }
    }

    void putitems(packetbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, N_ITEMLIST);
        loopv(ents) if(validitem(ents[i]->type) && server::canspawnitem(ents[i]->type))
        {
            putint(p, i);
            putint(p, ents[i]->type);
        }
        putint(p, -1);
    }

    void resetspawns() { loopv(ents) ents[i]->clearspawned(); }

    void spawnitems(bool force)
    {
        loopv(ents) if(validitem(ents[i]->type) && server::canspawnitem(ents[i]->type))
        {
            ents[i]->setspawned(force || !server::delayspawn(ents[i]->type));
        }
    }

    void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->setspawned(on); }

    extentity *newentity() { return new gameentity(); }
    void deleteentity(extentity *e) { delete (gameentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }

    void animatemapmodel(const extentity &e, int &anim, int &basetime)
    {
    }

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case FLAG:
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
            case TELEDEST:
                e.attr3 = e.attr2;
                e.attr2 = e.attr1;
                e.attr1 = (int)player1->yaw;
                break;
        }
    }

    void entradius(extentity &e, bool color)
    {
        switch(e.type)
        {
            case TELEPORT:
                loopv(ents) if(ents[i]->type == TELEDEST && e.attr1==ents[i]->attr2)
                {
                    renderentarrow(e, vec(ents[i]->o).sub(e.o).normalize(), e.o.dist(ents[i]->o));
                    break;
                }
                break;

            case JUMPPAD:
                renderentarrow(e, vec((int)(char)e.attr3*10.0f, (int)(char)e.attr2*10.0f, e.attr1*12.5f).normalize(), 4);
                break;

            case FLAG:
            case TELEDEST:
            {
                vec dir;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                break;
            }
        }
    }

    bool printent(extentity &e, char *buf, int len)
    {
        return false;
    }

    const char *entnameinfo(entity &e) { return ""; }
    const char *entname(int i)
    {
        static const char * const entnames[MAXENTTYPES] =
        {
            "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight", "decal",
            "teleport", "teledest", "jumppad",
            "flag",
            "sg", "smg", "pulse", "rl", "rail",
            "health", "shield1", "shield2",
            "health_super", "health_mega", "doubledamage", "haste", "armor", "unlimited_ammo", "?", "!", "invulnerability"
        };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        //e.flags = 0;
        if(local) addmsg(N_EDITENT, "rii3ii5", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
    }

    float dropheight(entity &e)
    {
        if(e.type==FLAG) return 0.0f;
        return 4.0f;
    }
#endif
}

