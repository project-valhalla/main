#include "game.h"

namespace entities
{
    using namespace game;

    int extraentinfosize() { return 0; }       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity& e, char* buf)   // write any additional data to disk (except for ET_ ents)
    {
    }

    void readent(entity& e, char* buf, int ver)     // read from disk, and init
    {
    }

#ifndef STANDALONE
    vector<extentity*> ents;

    vector<extentity*>& getents() { return ents; }

    bool mayattach(extentity& e) { return false; }
    bool attachent(extentity& e, extentity& a) { return false; }

    const char* entmodel(const entity& e)
    {
        if (e.type == TELEPORT || e.type == TRIGGER)
        {
            if (e.attr2 > 0) return mapmodelname(e.attr2);
            if (e.attr2 < 0) return NULL;
        }
        return e.type < MAXENTTYPES ? gentities[e.type].file : NULL;
    }

    bool canspawnitem(int type)
    {
        if (!validitem(type) || m_noitems(mutators))
        {
            return false;
        }

        switch (type)
        {
            case I_AMMO_SG: case I_AMMO_SMG: case I_AMMO_PULSE: case I_AMMO_RL: case I_AMMO_RAIL: case I_AMMO_GRENADE:
            {
                if (m_insta(mutators) || m_tactics(mutators) || m_voosh(mutators))
                {
                    return false;
                }
                break;
            }

            case I_YELLOWSHIELD: case I_REDSHIELD:
            {
                if (m_insta(mutators) || m_effic(mutators))
                {
                    return false;
                }
                break;
            }

            case I_HEALTH:
            {
                if (m_insta(mutators) || m_effic(mutators) || m_vampire(mutators))
                {
                    return false;
                }
                break;
            }

            case I_MEGAHEALTH: case I_ULTRAHEALTH:
            {
                if (m_insta(mutators) || m_vampire(mutators))
                {
                    return false;
                }
                break;
            }

            case I_DDAMAGE: case I_ARMOR: case I_INFINITEAMMO:
            {
                if (m_insta(mutators) || m_nopowerups(mutators))
                {
                    return false;
                }
                break;
            }

            case I_HASTE: case I_AGILITY: case I_INVULNERABILITY:
            {
                if (m_nopowerups(mutators))
                {
                    return false;
                }
                break;
            }

            default: break;
        }
        return true;
    }

    bool allowpickup()
    {
        return !((!m_infection && !m_betrayal && betweenrounds) || (m_hunt && hunterchosen && betweenrounds));
    }

    void preloadentities()
    {
        loopi(MAXENTTYPES)
        {
            if (!canspawnitem(i))
            {
                continue;
            }
            const char* mdl = gentities[i].file;
            if (!mdl)
            {
                continue;
            }
            preloadmodel(mdl);
        }
        loopv(ents)
        {
            extentity& e = *ents[i];
            switch (e.type)
            {
                case TELEPORT:
                case TRIGGER:
                {
                    if (e.attr2 > 0)
                    {
                        preloadmodel(mapmodelname(e.attr2));
                    }
                    break;
                }

                case JUMPPAD:
                {
                    if (e.attr4 > 0)
                    {
                        preloadmapsound(e.attr4);
                    }
                    break;
                }
            }
        }
    }

    extentity* findclosest(int type, const vec position)
    {
        extentity* closest = NULL;
        if (!ents.empty() && canspawnitem(type))
        {
            float distance = 1e16f;
            loopv(ents)
            {
                extentity* entity = ents[i];
                if (!entity->spawned() || (type >= 0 && ents[i]->type != type))
                {
                    continue;
                }
                float entitydistance = entity->o.dist(position);
                if (entitydistance < distance)
                {
                    distance = entitydistance;
                    closest = entity;
                }
            }
        }
        return closest;
    }

    extentity* closest = NULL;
    vec lastposition;

    void searchentities()
    {
        gameent* hud = followingplayer(self);
        if (lowhealthscreen && hud->haslowhealth())
        {
            /* If the low-health warning feature is enabled,
             * find the closest health pack to help guide the player.
             */
            static const int movethreshold = 128;
            bool shouldsearch = lastposition.dist(hud->o) >= movethreshold;
            if (shouldsearch)
            {
                lastposition = hud->o;
                closest = findclosest(I_HEALTH, lastposition);
            }
            if (closest)
            {
                particle_hud_mark(closest->o, 3, 1, PART_GAME_ICONS, 1, 0x00FF3F, 1.8f);
            }
        }
        else if (closest)
        {
            closest = NULL;
        }
    }

    void markentity(extentity& entity)
    {
        if (m_noitems(mutators) || (!validitem(entity.type) && entity.type != TRIGGER))
        {
            return;
        }

        if (m_story)
        {
            if (entity.type == TRIGGER && entity.isactive() && entity.attr5 == Trigger_Interest)
            {
                particle_hud_mark(entity.o, 1, 2, PART_GAME_ICONS, 1, 0x00FF3F, 4.0f);
            }
        }
        else
        {
            if (canspawnitem(entity.type) && entity.spawned())
            {
                if (entity.type >= I_DDAMAGE && entity.type <= I_INVULNERABILITY)
                {
                    particle_hud_mark(entity.o, 0, 2, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
                }
                else if (entity.type == I_ULTRAHEALTH)
                {
                    particle_hud_mark(entity.o, 3, 1, PART_GAME_ICONS, 1, 0xFF9796, 2.0f);
                }
            }
        }
    }

    void renderentities()
    {
        if (ents.empty())
        {
            return;
        }

        searchentities();
        gameent* hud = followingplayer(self);
        loopv(ents)
        {
            extentity& e = *ents[i];
            if (!e.isactive())
            {
                continue;
            }
            markentity(e);
            const int revs = 10;
            switch (e.type)
            {
                case TELEPORT:
                case TRIGGER:
                {
                    if (e.attr2 < 0 || (e.type == TRIGGER && !m_story))
                    {
                        continue;
                    }
                    break;
                }

                default:
                {
                    if ((!editmode && !e.spawned()) || !validitem(e.type))
                    {
                        continue;
                    }
                    break;
                }
            }
            const char* mdlname = entmodel(e);
            if (mdlname)
            {
                vec p = e.o;
                p.z += 1 + sinf(lastmillis / 100.0 + e.o.x + e.o.y) / 20;
                float trans = 1;
                if (validitem(e.type) && (!e.spawned() || !hud->canpickup(e.type)))
                {
                    trans = 0.5f;
                }
                float progress = min((lastmillis - e.lastspawn) / 1000.0f, 1.0f);
                float size = ease::outelastic(progress);
                rendermodel(mdlname, ANIM_MAPMODEL | ANIM_LOOP, p, lastmillis / (float)revs, 0, 0, MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, NULL, NULL, 0, 0, size, vec4(1, 1, 1, trans));
            }
        }
    }

    void addammo(int type, int& v, bool local)
    {
        itemstat& is = itemstats[type - I_AMMO_SG];
        v += is.add;
        if (v > is.max) v = is.max;
        if (local) msgsound(is.sound);
    }

    void repammo(gameent* d, int type, bool local)
    {
        addammo(type, d->ammo[type - I_AMMO_SG + GUN_SCATTER], local);
    }

    /* This function is called once the server acknowledges that you really
     * picked up the item (in multiplayer someone may grab it before you).
     */
    void dopickupeffects(const int n, gameent* d)
    {
        if (!d || !ents.inrange(n) || !validitem(ents[n]->type))
        {
            return;
        }

        if (ents[n]->spawned())
        {
            ents[n]->clearspawned();
        }
        const int type = ents[n]->type;
        game::autoswitchweapon(d, type);
        d->pickup(type);
        itemstat& is = itemstats[type - I_AMMO_SG];
        gameent* hud = followingplayer(self);
        playsound(is.sound, NULL, d == hud ? NULL : &d->o, NULL, 0, 0, 0, -1, 0, 1800);
        if (type >= I_DDAMAGE && type <= I_INVULNERABILITY)
        {
            if (d == hud)
            {
                conoutf(CON_GAMEINFO, "\f2%s power-up obtained", gentities[type].prettyname);
                if (validsound(is.announcersound))
                {
                    announcer::playannouncement(is.announcersound);
                }
            }
            else
            {
                conoutf(CON_GAMEINFO, "%s \fs\f2obtained the %s power-up\fr", colorname(d), gentities[type].prettyname);
                playsound(S_POWERUP);
            }
        }
        dohudpickupeffects(type, d);
    }

    void dohudpickupeffects(const int type, gameent* d, bool shouldCheck)
    {
        gameent* hud = followingplayer(self);
        if (d != hud)
        {
            return;
        }

        addscreenflash(ENTITY_COLLECT_FLASH);
        if (shouldCheck)
        {
            game::checkentity(type);
            d->lastpickupmillis = lastmillis;
        }
    }

    // These following functions are called when the client touches the entity.

    void playentitysound(const int fallback, const int mapsound, const vec &o)
    {
        int sound = fallback, flags = 0;
        if(mapsound > 0)
        {
            sound = mapsound;
            flags = SND_MAP;
        }
        playsound(sound, NULL, o.iszero() ? NULL : &o, NULL, flags);
    }

    void teleportparticleeffects(gameent *d, vec p)
    {
        if(d==followingplayer(self)) particle_splash(PART_SPARK2, 250, 500, p, getplayercolor(d, d->team), 0.50f, 800, -5);
        else particle_splash(PART_SPARK2, 250, 200, p, getplayercolor(d, d->team), 0.50f, 250, 5);
        adddynlight(p, 100, vec(0.50f, 1.0f, 1.5f), 500, 100);
    }

    void teleporteffects(gameent *d, int tp, int td, bool local)
    {
        if(d->state!=CS_ALIVE) return;
        if(ents.inrange(tp) && ents[tp]->type == TELEPORT)
        {
            extentity &e = *ents[tp];
            if(e.attr4 >= 0)
            {
                gameent* hud = followingplayer(self);
                playentitysound(S_TELEPORT, e.attr4, d == hud ? vec(0, 0, 0) : e.o);
                if(ents.inrange(td) && ents[td]->type == TELEDEST)
                {
                    if (d != hud)
                    {
                        playsound(S_TELEDEST, NULL, &ents[td]->o);
                    }
                    else
                    {
                        addscreenflash(ENTITY_TELEPORT_FLASH);
                        camera::camera.addevent(d, camera::CameraEvent_Teleport, 500);
                    }
                    teleportparticleeffects(d, ents[td]->o);
                }
                teleportparticleeffects(d, d->o);
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
                 playentitysound(S_JUMPPAD, e.attr4, d == followingplayer(self) ? vec(0, 0, 0) : e.o);
            }
            sway.addevent(d, SwayEvent_Land, 250, -2);
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

    int respawnent = -1;

    void trypickup(int n, gameent *d)
    {
        switch(ents[n]->type)
        {
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

            case TRIGGER:
            {
                if (d->lastpickup == ents[n]->type && lastmillis - d->lastpickupmillis < 100)
                {
                    break;
                }
                //if(ents[n]->attr5 && lastmillis - ents[n]->lasttrigger <= ents[n]->attr5) break;
                defformatstring(hookname, "trigger_%d", ents[n]->attr1);
                if (identexists(hookname))
                {
                    execident(hookname);
                }
                gameent* hud = followingplayer(self);
                if (ents[n]->attr4 >= 0)
                {
                    playentitysound(S_TRIGGER, ents[n]->attr4, d == hud ? vec(0, 0, 0) : ents[n]->o);
                }
                int triggertype = ents[n]->attr5;
                if (triggertype == Trigger_Item)
                {
                    dohudpickupeffects(n, d);
                }
                else if (triggertype == Trigger_RespawnPoint)
                {
                    respawnent = n;
                }
                ents[n]->setactivity(false);
                d->lastpickup = ents[n]->type;
                break;
            }

            default:
            {
                if (d->canpickup(ents[n]->type) && allowpickup())
                {
                    addmsg(N_ITEMPICKUP, "rci", d, n);
                    ents[n]->clearspawned(); // Even if someone else gets it first.
                }
                break;
            }
        }
    }

    bool ishovered(int& entity)
    {
        int orient = 0;
        const float maxDistance = 1e16f;
        const float distance = rayent(camera1->o, camdir, maxDistance, RAY_ENTS | RAY_SKIPFIRST, 0, orient, entity);
        return distance < maxDistance && entity > -1;
    }

    FVARP(itemhoverdistance, 0, 100.0f, 500.0f);
    int hoveredweapon = -1;

    void checkhovereditem(gameent* d)
    {
        if (!itemhoverdistance || m_noitems(mutators) || d->state != CS_ALIVE || d != followingplayer(self))
        {
            return;
        }

        int id = -1;
        if (ishovered(id))
        {
            extentity* entity = NULL;
            if (ents.inrange(id))
            {
                entity = ents[id];
            }
            if (entity && entity->spawned() && validitem(entity->type))
            {
                const bool isClose = camera1->o.dist(entity->o) <= itemhoverdistance;
                if (isClose)
                {
                    if (entity->type >= I_AMMO_SG && entity->type <= I_AMMO_GRENADE)
                    {
                        if (!validgun(hoveredweapon))
                        {
                            hoveredweapon = itemstats[entity->type - I_AMMO_SG].info;
                        }
                        return;
                    }
                }
            }
        }
        // Remove valid hover information.
        if (validgun(hoveredweapon))
        {
            hoveredweapon = -1;
        }
    }
    ICOMMAND(gethoverweapon, "", (), intret(hoveredweapon));

    void checkitems(gameent *d)
    {
        if (d->state != CS_ALIVE && d->state != CS_SPECTATOR)
        {
            return;
        }

        checkhovereditem(d);
        vec o = d->feetpos();
        loopv(ents)
        {
            extentity &e = *ents[i];
            if (e.type == NOTUSED)
            {
                continue;
            }
            if (!e.isactive())
            {
                continue;
            }
            float dist = e.o.dist(o);
            if(e.type == TRIGGER && m_story)
            {
                const int radius = e.attr3 ? e.attr3 : ENTITY_COLLECT_RADIUS;
                if (dist < radius)
                {
                    trypickup(i, d);
                }
                continue;
            }
            if (d->state == CS_SPECTATOR && e.type != TELEPORT)
            {
                continue;
            }
            if (!e.spawned() && e.type != TELEPORT && e.type != JUMPPAD)
            {
                continue;
            }
            const int radius = e.type == TELEPORT ? ENTITY_TELEPORT_RADIUS : ENTITY_COLLECT_RADIUS;
            if (dist < radius)
            {
                trypickup(i, d);
            }
        }
    }

    void updatepowerups(int time, gameent* d)
    {
        const int sound = d->role == ROLE_BERSERKER ? S_BERSERKER_LOOP : (S_LOOP_DAMAGE + d->poweruptype - 1);
        d->playchannelsound(Chan_PowerUp, sound, 200, true);

        if (m_berserker && d->role == ROLE_BERSERKER && !d->powerupmillis)
        {
            return;
        }

        if ((d->powerupmillis -= time) <= 0)
        {
            d->powerupmillis = 0;
            playsound(S_TIMEOUT_DAMAGE + d->poweruptype - 1, d);
            d->poweruptype = PU_NONE;
            if (d->role != ROLE_BERSERKER)
            {
                d->stopchannelsound(Chan_PowerUp, 500);
            }
        }
    }

    void putitems(packetbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, N_ITEMLIST);
        loopv(ents) if(canspawnitem(ents[i]->type))
        {
            putint(p, i);
            putint(p, ents[i]->type);
        }
        putint(p, -1);
    }

    void resetspawns()
    { 
        loopv(ents)
        {
            ents[i]->clearspawned();
            if (ents[i]->type == TRIGGER && ents[i]->attr5 == Trigger_Interest)
            {
                continue;
            }
            ents[i]->setactivity(true);
        }
    }

    void spawnitems(bool force)
    {
        loopv(ents) if(canspawnitem(ents[i]->type))
        {
            ents[i]->setspawned(force || !server::delayspawn(ents[i]->type));
            ents[i]->lastspawn = lastmillis;
        }
    }

    static void spawneffect(extentity *e)
    {
        int spawncolor = 0x00E463;
        particle_splash(PART_SPARK, 20, 100, e->o, spawncolor, 1.0f, 100, 60);
        particle_flare(e->o, e->o, 500, PART_EXPLODE1, 0x83E550, 1.0f, NULL, 16.0f);
        adddynlight(e->o, 116, vec::hexcolor(spawncolor), 500, 75, DL_EXPAND | L_NOSHADOW);
        playsound(S_ITEM_SPAWN, NULL, &e->o, NULL, 0, 0, 0, -1, 0, 1500);
        if (e->type >= I_DDAMAGE && e->type <= I_INVULNERABILITY)
        {  
            conoutf(CON_GAMEINFO, "\f2%s power-up available!", gentities[e->type].prettyname);
            playsound(S_POWERUP_SPAWN);
        }
    }

    void setspawn(int i, bool shouldspawn, bool isforced)
    {
        if (ents.inrange(i))
        {
            extentity* e = ents[i];
            e->setspawned(shouldspawn);
            if (!isforced)
            {
                spawneffect(e);
            }
            e->lastspawn = lastmillis;
        }
    }

    extentity *newentity() { return new gameentity(); }
    void deleteentity(extentity *e) { delete (gameentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }

    void animatemapmodel(const extentity &e, int &anim, int &basetime)
    {
        switch (e.triggerstate)
        {
            case TriggerState_Reset:
            {
                anim = ANIM_TRIGGER | ANIM_START;
                break;
            }

            case TriggerState_Triggering:
            {
                anim = ANIM_TRIGGER;
                basetime = e.lasttrigger;
                break;
            }

            case TriggerState_Triggered:
            {
                anim = ANIM_TRIGGER | ANIM_END;
                break;
            }

            case TriggerState_Resetting:
            {
                anim = ANIM_TRIGGER | ANIM_REVERSE;
                basetime = e.lasttrigger;
                break;
            }

            default: break;
        }
    }

    void fixentity(extentity &e)
    {
        switch (e.type)
        {
            case FLAG:
            {
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
                // fall through
            }
            case TELEDEST:
            {
                e.attr3 = e.attr2;
                e.attr2 = e.attr1;
                e.attr1 = (int)self->yaw;
                break;
            }

            case TARGET:
            {
                e.attr2 = e.attr1;
                break;
            }

        }
    }

    void entradius(extentity &e, bool color)
    {
        switch (e.type)
        {
            case TELEPORT:
            {
                loopv(ents) if (ents[i]->type == TELEDEST && e.attr1 == ents[i]->attr2)
                {
                    renderentarrow(e, vec(ents[i]->o).sub(e.o).normalize(), e.o.dist(ents[i]->o));
                    break;
                }
                break;
            }

            case JUMPPAD:
            {
                renderentarrow(e, vec((int)(char)e.attr3 * 10.0f, (int)(char)e.attr2 * 10.0f, e.attr1 * 12.5f).normalize(), 4);
                break;
            }

            case FLAG:
            case TELEDEST:
            {
                vec dir;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                break;
            }

            case TRIGGER:
            {
                renderentsphere(e, e.attr3);
                break;
            }

            case TARGET:
            {
                vec dir;
                vecfromyawpitch(e.attr2, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                break;
            }
        }
    }

    bool printent(extentity &e, char *buf, int len)
    {
        return false;
    }

    const char *entnameinfo(entity &e)
    {
        return e.type < MAXENTTYPES ? gentities[e.type].prettyname : "";
    }

    const char *entname(int type)
    {
        return type >= 0 && type < MAXENTTYPES ? gentities[type].name : "";
    }

    static inline void cleartriggerflags(extentity* entity)
    {
        if (entity)
        {
            entity->flags &= ~(EF_ANIM | EF_NOVIS | EF_NOSHADOW | EF_NOCOLLIDE);
            entity->lasttrigger = 0;
            entity->triggerstate = TriggerState_Null;
        }
    }

    void resettriggers()
    {
        loopv(ents)
        {
            extentity* e = ents[i];
            if (e->type != ET_MAPMODEL) continue;
            cleartriggerflags(e);
        }
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        cleartriggerflags(&e);
        e.setactivity(true);
        if (local)
        {
            addmsg(N_EDITENT, "rii3ii5", i, (int)(e.o.x * DMF), (int)(e.o.y * DMF), (int)(e.o.z * DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
        }
        if (canspawnitem(e.type) && !e.spawned())
        {
            e.lastspawn = lastmillis;
        }
    }

    float dropheight(entity &e)
    {
        if(e.type==FLAG) return 0.0f;
        return 4.0f;
    }

    void triggertoggle(int id, int id2 = -1)
    {
        loopv(ents)
        {
            extentity* entity = ents[i];
            if (entity->type != TRIGGER)
            {
                continue;
            }
            if (entity->attr1 == id || (id2 >= 0 && entity->attr1 == id2))
            {
                entity->setactivity(entity->isactive() ? false : true);
            }
        }
    }
    ICOMMAND(triggertoggle, "i", (int* id), triggertoggle(*id));
    ICOMMAND(triggerswap, "ii", (int* id, int* id2), triggertoggle(*id, *id2));

    void triggermapmodel(int id, int state, int sound = -1)
    {
        extentity* entity = NULL;
        if (ents.inrange(id))
        {
            entity = ents[id];
        }
        if (!entity || !m_story || entity->type != ET_MAPMODEL)
        {
            return;
        }

        cleartriggerflags(entity);
        if (state > TriggerState_Null)
        {
            entity->flags |= EF_ANIM;
        }
        if (state == TriggerState_Triggering || state == TriggerState_Triggered)
        {
            entity->flags |= EF_NOCOLLIDE;
        }
        entity->triggerstate = state;
        entity->lasttrigger = lastmillis;
        if (validsound(sound))
        {
            playentitysound(S_TRIGGER, sound, entity->o);
        }
    }
    ICOMMAND(triggermapmodel, "iib", (int* id, int* state, int* sound), triggermapmodel(*id, *state, *sound));
#endif
}

