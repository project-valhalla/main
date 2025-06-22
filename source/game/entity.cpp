#include "game.h"
#include "event.h"
#include "query.h"

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

    vector<int> proximity_triggers; // triggers in proximity of the player, used for "Distance" events

    // search ents by type and label and returns a list of ids
    ICOMMAND(entquery, "ss", (char *type, char *query),
    {
        vector<extentity *>& ents = entities::getents();
        int type_i = -1;
        if(type[0])
        {
            if((type_i = findenttype(type)) == ET_EMPTY)
            {
                result("");
                return;
            }
        }

        vector<char> buf;
        string id;
        int num_ids = 0;
        loopv(ents)
        {
            if(
                (type_i < 0 || type_i == ents[i]->type) &&
                (query::match(query, ents[i]->label))
            )
            {
                if(num_ids++) buf.add(' ');
                formatstring(id, "%d", i);
                buf.put(id, strlen(id));
            }
        }
        buf.add('\0');
        result(buf.getbuf());
    })

    // clear the list of triggers in proximity (when starting a new map)
    void clearProximityTriggers()
    {
        proximity_triggers.setsize(0);
    }
    // emit "Distance" events for all triggers in proximity (called when the player dies)
    void emitDistanceEvents()
    {
        loopvrev(proximity_triggers)
        {
            event::emit<event::Trigger>(proximity_triggers[i], event::Distance);
            proximity_triggers.remove(i);
        }
    }

    // Events.
    void onPlayerDeath(const gameent *d, const gameent *actor)
    {
        if (d != self)
        {
            return;
        }
        emitDistanceEvents();
    }

    void onPlayerSpectate(const gameent *d)
    {
        if (d != self)
        {
            return;
        }
        emitDistanceEvents();
    }

    void onPlayerUnspectate(const gameent *d) {}

    void onMapStart()
    {
        clearProximityTriggers();
    }

    int respawnent = -1;

    void setRespawnPoint(int id)
    {
        respawnent = ents.inrange(id) ? id : -1;
    }
    ICOMMAND(setrespawnpoint, "i", (int *id), setRespawnPoint(*id));

    // returns whether or not a trigger is enabled
    int getTriggerState(int id)
    {
        if(!ents.inrange(id)) return 0;
        return ents[id]->isactive() ? 1 : 0;
    }
    // enables or disables a trigger
    void setTriggerState(int id, int state)
    {
        if(!ents.inrange(id)) return;
        ents[id]->setactivity(state != 0 ? true : false);
    }
    ICOMMAND(triggerstate, "iiN", (int *id, int *state, int *numargs),
    {
        if(*numargs > 1)
        {
            setTriggerState(*id, *state);
        }
        else
        {
            intret(getTriggerState(*id));
        }
    });

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
        if (m_story)
        {
            return;
        }

        gameent* hud = followingplayer(self);
        if (lowhealthscreen && hud->haslowhealth())
        {
            /* If the low-health warning feature is enabled,
             * find the closest health pack to help guide the player.
             */
            const int moveThreshold = 128;
            const bool shouldSearch = lastposition.dist(hud->o) >= moveThreshold;
            if (shouldSearch)
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
            if (entity.type == TRIGGER && entity.isactive() && entity.attr5 == TriggerType::Marker)
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
        if (local) sendsound(is.sound);
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

        const int collectFlash = 80;
        addscreenflash(collectFlash);
        if (shouldCheck)
        {
            game::checkentity(type);
            d->lastpickupmillis = lastmillis;
        }
    }
    ICOMMAND(triggerpickupeffects, "b", (int *cn), {
        gameent *d = *cn < 0 ? self : getclient(*cn);
        if(!d) return;
        dohudpickupeffects(TRIGGER, d);
    });

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
    ICOMMAND(triggersound, "iiiN", (int *ent_id, int *sound_id, int *cn, int *numargs),
    {
        if(!ents.inrange(*ent_id) || ents[*ent_id]->type != TRIGGER) return;
        gameent *hud = followingplayer(self);
        gameent *d = *numargs < 3 ? hud : getclient(*cn);
        playentitysound(S_TRIGGER, *sound_id, d && d == hud ? vec(0, 0, 0) : ents[*ent_id]->o);
    });

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
                        const int teleportFlash = 150;
                        addscreenflash(teleportFlash);
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
                event::emit<event::Trigger>(n, event::Proximity);
                int triggertype = ents[n]->attr5;
                
                if (triggertype == TriggerType::Usable && self->interacting)
                {
                    ents[n]->setactivity(false);
                    self->interacting = false;
                    event::emit<event::Trigger>(n, event::Use);
                }
                else if (triggertype == TriggerType::Item || triggertype == TriggerType::Marker)
                {
                    // disable the item once the player reaches it
                    ents[n]->setactivity(false);
                }

                // add ent id to the list of triggers in proximity
                if(proximity_triggers.find(n) < 0) proximity_triggers.add(n);

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
    int hoveredweapon = GUN_INVALID;

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
            hoveredweapon = GUN_INVALID;
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
        // check if the player moved away from a trigger in proximity
        if(m_story && d == self) loopvrev(proximity_triggers)
        {
            const int id = proximity_triggers[i];
            if(!ents.inrange(id) || ents[id]->type != TRIGGER) continue;
            const extentity& e = *ents[id];
            const float dist = e.o.dist(o);
            const int radius = e.attr3 ? e.attr3 : ENTITY_COLLECT_RADIUS;
            const int exit_radius = max(0, max(radius, e.attr4 ? e.attr4 : ENTITY_COLLECT_RADIUS));
            if(dist > exit_radius)
            {
                event::emit<event::Trigger>(id, event::Distance);
                proximity_triggers.remove(i);
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
            if (ents[i]->type == TRIGGER && ents[i]->attr5 == TriggerType::Marker)
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
            case TriggerState::Reset:
            {
                anim = ANIM_TRIGGER | ANIM_START;
                break;
            }

            case TriggerState::Triggering:
            {
                anim = ANIM_TRIGGER;
                basetime = e.lasttrigger;
                break;
            }

            case TriggerState::Triggered:
            {
                anim = ANIM_TRIGGER | ANIM_END;
                break;
            }

            case TriggerState::Resetting:
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
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
                // fall through
            case TELEDEST:
                e.attr3 = e.attr2;
                e.attr2 = e.attr1;
                e.attr1 = (int)self->yaw;
                break;

            case TARGET:
                e.attr2 = e.attr1;
                break;
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
                vec dir;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                renderentarrow(e, dir, 4);
                gle::color(bvec4(0x7F, 0xFF, 0xD4, 0xFF));
                renderentsphere(e, e.attr3);
                if(e.attr3 > 0 && e.attr4 > e.attr3)
                {
                    gle::color(bvec4(0xFF, 0xA0, 0x7A, 0xFF));
                    renderentsphere(e, e.attr4);
                }
                gle::color(bvec4(0xFF, 0xFF, 0xFF, 0xFF));
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
            entity->triggerstate = TriggerState::Null;
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
    void editentlabel(int i, bool local)
    {
        extentity& e = *ents[i];
        if(local)
        {
            addmsg(N_EDITENTLABEL, "ris", i, e.label ? e.label : "");
        }
    }

    float dropheight(entity &e)
    {
        if(e.type==FLAG) return 0.0f;
        return 4.0f;
    }

    // sets the trigger state of a map model
    void triggermapmodel(int id, int state, int sound = S_INVALID)
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
        if (state > TriggerState::Null)
        {
            entity->flags |= EF_ANIM;
        }
        if (state == TriggerState::Triggering || state == TriggerState::Triggered)
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
    ICOMMAND(mapmodeltriggerstate, "i", (int *id),
    {
        extentity* entity = ents.inrange(*id) ? ents[*id] : nullptr;
        intret(entity && entity->type == ET_MAPMODEL ? entity->triggerstate : 0);
    });
#endif
}

