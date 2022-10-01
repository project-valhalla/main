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

            "item/ammo/shotgun", "item/ammo/smg", "item/ammo/pulse", "item/ammo/rocket", "item/ammo/railgun",
            "item/health", "item/shield/yellow", "item/shield/red",
            "item/health/super", "item/health/mega", "item/doubledamage", "item/haste", "item/armour", "item/ammo/unlimited", "item/agility", "item/invulnerability"
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

    bool canspawnitem(int type)
    {
        if(!validitem(type)) return false;
        switch(type)
        {
            case I_AMMO_SG: case I_AMMO_SMG: case I_AMMO_PULSE: case I_AMMO_RL: case I_AMMO_RAIL:
            case I_YELLOWSHIELD: case I_REDSHIELD:
                if(!m_classic(mutators) || m_noitems(mutators)) return false;
                    break;
            case I_HEALTH:
                if(!m_classic(mutators) || m_vampire(mutators) || m_noitems(mutators)) return false;
                break;
            case I_SUPERHEALTH: case I_MEGAHEALTH:
                    if(m_insta(mutators) || m_vampire(mutators) || m_noitems(mutators)) return false;
                    break;
                case I_DDAMAGE: case I_ARMOUR: case I_UAMMO:
                    if(m_insta(mutators) || m_noitems(mutators) || m_nopowerups(mutators)) return false;
                    break;
                case I_HASTE: case I_AGILITY: case I_INVULNERABILITY:
                    if(m_noitems(mutators) || m_nopowerups(mutators)) return false;
                break;
        }
        return true;
    }

    void preloadentities()
    {
        loopi(MAXENTTYPES)
        {
            if(!canspawnitem(i)) continue;
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

    VARP(itemtrans, 0, 1, 1);

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
                    if((!editmode && !e.spawned()) || !validitem(e.type)) continue;
                    break;
            }
            const char *mdlname = entmodel(e);
            if(mdlname)
            {
                vec p = e.o;
                p.z += 1+sinf(lastmillis/100.0+e.o.x+e.o.y)/20;
                float trans = 1;
                if(itemtrans && validitem(e.type) && !e.spawned()) trans = 0.5f;
                rendermodel(mdlname, ANIM_MAPMODEL|ANIM_LOOP, p, lastmillis/(float)revs, 0, 0, MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, NULL, NULL, 0, 0, 1, vec4(1, 1, 1, trans));
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
        gameent *h = followingplayer(self);
        playsound(itemstats[type-I_AMMO_SG].sound, NULL, d != h ? &d->o : NULL, NULL, 0, 0, 0, -1, 0, 1800);
        d->pickup(type);
        const char *itemname = "";
        int announcersound = -1;
        switch(type)
        {
            case I_DDAMAGE:
                itemname = "Double Damage";
                announcersound = S_ANNOUNCER_DDAMAGE;
                break;

            case I_HASTE:
                itemname = "Haste";
                announcersound = S_ANNOUNCER_HASTE;
                break;

            case I_ARMOUR:
                itemname = "Armour";
                announcersound = S_ANNOUNCER_ARMOUR;
                break;

            case I_UAMMO:
                itemname = "Unlimited Ammo";
                announcersound = S_ANNOUNCER_UAMMO;
                break;

            case I_AGILITY:
                itemname = "Agility";
                announcersound = S_ANNOUNCER_AGILITY;
                break;

            case I_INVULNERABILITY:
                itemname = "Invulnerability";
                announcersound = S_ANNOUNCER_INVULNERABILITY;
                break;

            default: return;
        }
        if(d == self) conoutf(CON_GAMEINFO, "\f2%s obtained", itemname);
        else conoutf(CON_GAMEINFO, "%s \f2obtained the %s power-up", colorname(d), itemname);
        playsound(d == h ? announcersound : S_POWERUP, NULL, NULL, NULL, SND_ANNOUNCER);
    }

    // these functions are called when the client touches the item

    void tpeffects(vec p)
    {
        particle_splash(PART_SPARK2, 50, 200, p, 0x89E5FB, 0.20f, 250, 8);
        particle_fireball(p, 10.0f, PART_EXPLOSION2, 500, 0x89E5FB, 1.0f);
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
                if(d == self) playsound(snd, NULL, NULL, NULL, flags);
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
    VARP(autoswitch, 0, 1, 1);

    void trypickup(int n, gameent *d)
    {
        switch(ents[n]->type)
        {
            default:
                if(d->canpickup(ents[n]->type))
                {
                    addmsg(N_ITEMPICKUP, "rci", d, n);
                    ents[n]->clearspawned(); // even if someone else gets it first
                    // first time you pick up a weapon you switch to it automatically
                    if(d->aitype == AI_BOT || !autoswitch || (ents[n]->type < I_AMMO_SG || ents[n]->type > I_AMMO_RAIL)) break;
                    itemstat &is = itemstats[ents[n]->type-I_AMMO_SG];
                    if(d->gunselect != is.info && !d->ammo[is.info]) gunselect(is.info, d);
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
        }
    }

    void updatepowerups(int time, gameent *d)
    {
        d->powerupsound = S_LOOP_DAMAGE + d->poweruptype-1;
        gameent *hud = followingplayer(self);
        d->powerupchan = playsound(d->powerupsound, NULL, d==hud ? NULL : &d->o, NULL, 0, -1, 200, d->powerupchan);
        if((d->powerupmillis -= time)<=0)
        {
            d->powerupmillis = 0;
            playsound(S_TIMEOUT_DAMAGE + d->poweruptype-1, d);
            d->poweruptype = PU_NONE;
            d->stoppowerupsound();
        }
    }

    void putitems(packetbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, N_ITEMLIST);
        loopv(ents) if(validitem(ents[i]->type) && canspawnitem(ents[i]->type))
        {
            putint(p, i);
            putint(p, ents[i]->type);
        }
        putint(p, -1);
    }

    void resetspawns() { loopv(ents) ents[i]->clearspawned(); }

    void spawnitems(bool force)
    {
        loopv(ents) if(validitem(ents[i]->type) && canspawnitem(ents[i]->type))
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
                e.attr1 = (int)self->yaw;
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
            "shotgun", "smg", "pulse", "rocket", "railgun",
            "health", "light_shield", "heavy_shield",
            "superhealth", "megahealth", "double_damage", "haste", "armour", "unlimited_ammo", "agility", "invulnerability"
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

