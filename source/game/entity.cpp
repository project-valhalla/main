/*
 * =====================================================================
 * entity.cpp
 * Handles game entities and their effects.
 * =====================================================================
 *
 * This file handles game entity logic:
 * - Collision detection with game-specific entities:
 *     - Players touching pickups.
 *     - Actors using teleporters or jump pads.
 * - Trigger event handling and activation.
 * - Pickup/use effects.
 * - Rendering and preloading the entity models.
 * - Communication with the server:
 *     - Reporting available pickups.
 *     - Sending information about clients attempting pickups.
 *     - Tracking spawn states and timers for pickups.
 * - Other game-specific entity behaviours.
 *
 * =====================================================================
 */

#include "game.h"
#include "event.h"
#include "query.h"

namespace entities
{
    using namespace game;

    /*
        Size in bytes of what the 2 methods below read/write.
        Can be skipped by other games.
    */
    int getExtraInfoSize()
    {
        return 0;
    }

    /*
        Write any additional data to disk.
        Note: currently, entities with the "ET_" prefix (engine entities) are ignored.
    */
    void write(const entity& entity, const char* buf)
    {
    }

    // Read from disk, and initialise the entity.
    void read(const entity& entity, const char* buf, const int version)
    {
    }

#ifndef STANDALONE

    // Preload all the default entity models.
    void preload()
    {
        for (int i = 0; i < MAXENTTYPES; i++)
        {
            const char* model = gentities[i].file;
            if (!model)
            {
                continue;
            }
            preloadmodel(model);
        }
    }

    // Preload the map models used by the game entities.
    void preloadWorld()
    {
        loopv(ents)
        {
            const extentity& entity = *ents[i];
            switch (entity.type)
            {
                case TELEPORT:
                case TRIGGER:
                    if (entity.attr2 > 0)
                    {
                        preloadmodel(mapmodelname(entity.attr2));
                    }
                    break;

                case JUMPPAD:
                    if (entity.attr4 > 0)
                    {
                        preloadmapsound(entity.attr4);
                    }
                    break;
            }
        }
    }

    vector<extentity*> ents;

    vector<extentity*>& getents()
    {
        return ents;
    }

    bool isAttachable(const extentity& e)
    { 
        return false;
    }

    bool shouldAttach(const extentity& entity, const extentity& attached)
    {
        return false;
    }

    /*
        Returns the model name associated with an entity.
        For certain entity types, a map model is used when the second attribute (`attr2`) is greater than 0.
        If `attr2` is less than 0, the entity returns no model.
    */
    const char* getModel(const entity& e)
    {
        if (e.type == TELEPORT || e.type == TRIGGER)
        {
            if (e.attr2 > 0)
            {
                return mapmodelname(e.attr2);
            }
            if (e.attr2 < 0)
            {
                return nullptr;
            }
        }
        return e.type < MAXENTTYPES ? gentities[e.type].file : nullptr;
    }

    // Checks whether a pickable entity (item) can spawn in a certain game mode.
    static bool canSpawnItem(const int type)
    {
        if (!validitem(type) || m_noitems(mutators))
        {
            return false;
        }
        switch (type)
        {
            case I_AMMO_SG: case I_AMMO_SMG: case I_AMMO_PULSE: case I_AMMO_RL: case I_AMMO_RAIL: case I_AMMO_GRENADE:
                if (m_insta(mutators) || m_tactics(mutators) || m_voosh(mutators))
                {
                    return false;
                }
                break;

            case I_YELLOWSHIELD: case I_REDSHIELD:
                if (m_insta(mutators) || m_effic(mutators))
                {
                    return false;
                }
                break;

            case I_HEALTH:
                if (m_insta(mutators) || m_effic(mutators) || m_vampire(mutators))
                {
                    return false;
                }
                break;

            case I_MEGAHEALTH: case I_ULTRAHEALTH:
                if (m_insta(mutators) || m_vampire(mutators))
                {
                    return false;
                }
                break;

            case I_DDAMAGE: case I_ARMOR: case I_INFINITEAMMO:
                if (m_insta(mutators) || m_nopowerups(mutators))
                {
                    return false;
                }
                break;

            case I_HASTE: case I_AGILITY: case I_INVULNERABILITY:
                if (m_nopowerups(mutators))
                {
                    return false;
                }
                break;

            default:
                break;
        }
        return true;
    }

    // Determines whether the game currently allows item entities to be picked up.
    static bool isPickupAllowed()
    {
        if
        (
            !((!m_infection && !m_betrayal && betweenrounds) ||
            (m_hunt && hunterchosen && betweenrounds))
        )
        {
            return true;
        }
        return false;
    }

    // Find the closest entity by type.
    static extentity* findClosest(const int type, const vec position)
    {
        extentity* closest = nullptr;
        if (!ents.empty() && canSpawnItem(type))
        {
            float distance = 1e16f;
            loopv(ents)
            {
                extentity* entity = ents[i];
                if (!entity->spawned() || (type >= 0 && ents[i]->type != type))
                {
                    continue;
                }
                float entityDistance = entity->o.dist(position);
                if (entityDistance < distance)
                {
                    distance = entityDistance;
                    closest = entity;
                }
            }
        }
        return closest;
    }

    // Search entities based on need.
    static void search()
    {
        if (m_story)
        {
            return;
        }
        gameent* hudPlayer = followingplayer(self);

        /*
            If the low-health warning feature is enabled,
            find the closest health pack to help guide the player.
        */
        static extentity* closest = nullptr;
        if (lowhealthscreen && hudPlayer->haslowhealth())
        {
            static vec lastPosition;
            const int moveThreshold = 128;
            const bool shouldSearch = lastPosition.dist(hudPlayer->o) >= moveThreshold;
            if (shouldSearch)
            {
                lastPosition = hudPlayer->o;
                closest = findClosest(I_HEALTH, lastPosition);
            }
            if (closest)
            {
                particle_hud_mark(closest->o, 3, 1, PART_GAME_ICONS, 1, 0x00FF3F, 1.8f);
            }
        }
        else if (closest)
        {
            closest = nullptr;
        }
    }

    // Mark entities on the HUD based on need.
    static void mark(const extentity& entity)
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
            if (canSpawnItem(entity.type) && entity.spawned())
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

    // Render game entity models and manage simple effects.
    void render()
    {
        if (ents.empty())
        {
            return;
        }
        search();
        gameent* hudPlayer = followingplayer(self);
        loopv(ents)
        {
            const extentity& entity = *ents[i];
            if (!entity.isactive())
            {
                continue;
            }
            mark(entity);
            const int revolutions = 10;
            switch (entity.type)
            {
                case TELEPORT:
                case TRIGGER:
                    if (entity.attr2 < 0 || (entity.type == TRIGGER && !m_story))
                    {
                        continue;
                    }
                    break;

                default:
                    if ((!editmode && !entity.spawned()) || !validitem(entity.type))
                    {
                        continue;
                    }
                    break;
            }
            const char* modelName = getModel(entity);
            if (modelName)
            {
                const vec position = vec(entity.o).addz(1 + sinf(lastmillis / 100.0 + entity.o.x + entity.o.y) / 20);
                float trans = 1;
                if
                    (validitem(entity.type) &&
                    (!entity.spawned() || !hudPlayer->canpickup(entity.type))
                )
                {
                    trans = 0.5f;
                }
                const float progress = min((lastmillis - entity.lastspawn) / 1000.0f, 1.0f);
                const float size = ease::outelastic(progress);
                rendermodel
                (
                    modelName, ANIM_MAPMODEL | ANIM_LOOP, position, lastmillis / static_cast<float>(revolutions),
                    0, 0, MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, nullptr, nullptr, 0, 0, size, vec4(1, 1, 1, trans)
                );
            }
        }
    }

    /*
        This function is called once the server acknowledges that you really
        picked up the item (in multiplayer someone may grab it before you).
    */
    void doPickupEffects(const int id, gameent* player)
    {
        if (!player || !ents.inrange(id) || !validitem(ents[id]->type))
        {
            return;
        }
        if (ents[id]->spawned())
        {
            ents[id]->clearspawned();
        }
        const int type = ents[id]->type;
        game::autoswitchweapon(player, type);
        player->pickup(type);
        itemstat& itemInfo = itemstats[type - I_AMMO_SG];
        gameent* hud = followingplayer(self);
        playsound(itemInfo.sound, nullptr, player == hud ? nullptr : &player->o, nullptr, 0, 0, 0, -1, 0, 1800);
        if (type >= I_DDAMAGE && type <= I_INVULNERABILITY)
        {
            if (player == hud)
            {
                conoutf(CON_GAMEINFO, "\f2%s power-up obtained", gentities[type].prettyname);
                if (validsound(itemInfo.announcersound))
                {
                    announcer::playannouncement(itemInfo.announcersound);
                }
            }
            else
            {
                conoutf(CON_GAMEINFO, "%s \fs\f2obtained the %s power-up\fr", colorname(player), gentities[type].prettyname);
                playsound(S_POWERUP);
            }
        }
        doHudPickupEffects(type, player);
    }

    // Manage effects when you pick a pickable entity (item) up. 
    void doHudPickupEffects(const int type, gameent* player, const bool shouldCheck)
    {
        gameent* hud = followingplayer(self);
        if (player != hud)
        {
            return;
        }
        const int collectFlash = 80;
        addscreenflash(collectFlash);
        if (shouldCheck)
        {
            game::checkentity(type);
            player->lastpickupmillis = lastmillis;
        }
    }
    ICOMMAND(triggerpickupeffects, "b", (int *cn),
    {
        gameent* player = *cn < 0 ? self : getclient(*cn);
        if (!player)
        {
            return;
        }
        doHudPickupEffects(TRIGGER, player);
    });

    // These following functions are called when the client touches the entity.
    static void playSound(const int fallback, const int mapSound, const vec& o)
    {
        int sound = fallback, flags = 0;
        if (mapSound > 0)
        {
            sound = mapSound;
            flags = SND_MAP;
        }
        playsound(sound, nullptr, o.iszero() ? nullptr : &o, nullptr, flags);
    }
    ICOMMAND(triggersound, "iiiN", (int* entityId, int* soundId, int* cn, int* numargs),
    {
        if (!ents.inrange(*entityId))
        {
            return;
        }
        const gameent* hudPlayer = followingplayer(self);
        const gameent* player = *numargs < 3 ? hudPlayer : getclient(*cn);
        playSound(S_TRIGGER, *soundId, player && player == hudPlayer ? vec(0, 0, 0) : ents[*entityId]->o);
    });

    static void doTeleportParticleEffects(const gameent* player, const vec p)
    {
        if (player == followingplayer(self))
        {
            particle_splash(PART_SPARK2, 250, 500, p, getplayercolor(player, player->team), 0.50f, 800, -5);
        }
        else
        {
            particle_splash(PART_SPARK2, 250, 200, p, getplayercolor(player, player->team), 0.50f, 250, 5);
        }
        adddynlight(p, 100, vec(0.50f, 1.0f, 1.5f), 500, 100);
    }

    void doEntityEffects(const gameent* player, const int sourceEntityId, const bool isLocal, const int targetEntityId)
    {
        if (player->state != CS_ALIVE || !ents.inrange(sourceEntityId))
        {
            return;
        }
        extentity& entity = *ents[sourceEntityId];
        switch (entity.type)
        {
            case TELEPORT:
                if (entity.attr4 >= 0)
                {
                    const gameent* hud = followingplayer(self);
                    playSound(S_TELEPORT, entity.attr4, player == hud ? vec(0, 0, 0) : entity.o);
                    if (ents.inrange(targetEntityId) && ents[targetEntityId]->type == TELEDEST)
                    {
                        if (player != hud)
                        {
                            playsound(S_TELEDEST, nullptr, &ents[targetEntityId]->o);
                        }
                        else
                        {
                            const int teleportFlash = 150;
                            addscreenflash(teleportFlash);
                            camera::camera.addevent(player, camera::CameraEvent_Teleport, 500);
                        }
                        doTeleportParticleEffects(player, ents[targetEntityId]->o);
                    }
                    doTeleportParticleEffects(player, player->o);
                }
                if (isLocal && player->clientnum >= 0)
                {
                    sendposition(player);
                    packetbuf p(32, ENET_PACKET_FLAG_RELIABLE);
                    putint(p, N_TELEPORT);
                    putint(p, player->clientnum);
                    putint(p, sourceEntityId);
                    putint(p, targetEntityId);
                    sendclientpacket(p.finalize(), 0);
                    flushclient();
                }
                break;

            case JUMPPAD:
                if (entity.attr4 >= 0)
                {
                    playSound(S_JUMPPAD, entity.attr4, player == followingplayer(self) ? vec(0, 0, 0) : entity.o);
                }
                sway.addevent(player, SwayEvent_Land, 250, -2);
                if (isLocal && player->clientnum >= 0)
                {
                    sendposition(player);
                    packetbuf p(16, ENET_PACKET_FLAG_RELIABLE);
                    putint(p, N_JUMPPAD);
                    putint(p, player->clientnum);
                    putint(p, sourceEntityId);
                    sendclientpacket(p.finalize(), 0);
                    flushclient();
                }
                break;

            default:
                break;
        }
    }

    // Teleporting a game entity (player, NPC, prop, etc.).
    void teleport(const int teleportId, gameent* player)
    {
        int destinationId = -1;
        const int tag = ents[teleportId]->attr1;
        int previous = -1;
        for(;;)
        {
            destinationId = findentity(TELEDEST, destinationId + 1);
            if (destinationId == previous || destinationId < 0)
            {
                conoutf(CON_WARN, "no teleport destination for tag %d", tag);
                return;
            }
            if (previous < 0)
            {
                previous = destinationId;
            }
            if (ents[destinationId]->attr2 == tag)
            {
                doEntityEffects(player, teleportId, true, destinationId);
                player->o = ents[destinationId]->o;
                player->yaw = ents[destinationId]->attr1;
                if (ents[destinationId]->attr3 > 0)
                {
                    vec direction;
                    vecfromyawpitch(player->yaw, 0, 1, 0, direction);
                    const float speed = player->vel.magnitude2();
                    player->vel.x = direction.x * speed;
                    player->vel.y = direction.y * speed;
                }
                else
                {
                    player->vel = vec(0, 0, 0);
                }
                entinmap(player);
                updatedynentcache(player);
                ai::inferwaypoints(player, ents[teleportId]->o, ents[destinationId]->o, 16.f);
                break;
            }
        }
    }

    // Triggers in proximity of the player, used for "Distance" events.
    vector<int> proximityTriggers;

    // Contains information to display when hovering over items and entities.
    struct HoverInfo
    {
        int hoveredWeapon = GUN_INVALID;

        void resetInteraction()
        {
            self->interacting[Interaction::Available] = false;
        }

        void reset()
        {
            hoveredWeapon = GUN_INVALID;
            resetInteraction();
        }
    };

    static HoverInfo hover;

    VARR(teleteam, 0, 1, 1);

    // The client tries to pick up the entity (item) and lets the server know.
    static void tryPickup(const int id, gameent* player)
    {
        switch(ents[id]->type)
        {
            case TELEPORT:
            {
                if
                (
                    (player->lastpickup == ents[id]->type && lastmillis - player->lastpickupmillis < 500) ||
                    (!teleteam && m_teammode)
                )
                {
                    break;
                }
                if (ents[id]->attr3 > 0)
                {
                    defformatstring(hookname, "can_teleport_%d", ents[id]->attr3);
                    if (!execidentbool(hookname, true))
                    {
                        break;
                    }
                }
                player->lastpickup = ents[id]->type;
                player->lastpickupmillis = lastmillis;
                teleport(id, player);
                break;
            }

            case JUMPPAD:
            {
                if (player->lastpickup == ents[id]->type && lastmillis - player->lastpickupmillis < 300)
                {
                    break;
                }
                player->lastpickup = ents[id]->type;
                player->lastpickupmillis = lastmillis;
                doEntityEffects(player, id, true);
                if (player->ai)
                {
                    player->ai->becareful = true;
                }
                player->falling = vec(0, 0, 0);
                player->physstate = PHYS_FALL;
                player->timeinair = 1;
                player->vel = vec(ents[id]->attr3 * 10.0f, ents[id]->attr2 * 10.0f, ents[id]->attr1 * 12.5f);
                break;
            }

            case TRIGGER:
            {
                if (player->lastpickup == ents[id]->type && lastmillis - player->lastpickupmillis < 100)
                {
                    break;
                }
                event::emit<event::Trigger>(id, event::Proximity);
                const int triggerType = ents[id]->attr5;
                if (triggerType == TriggerType::Usable)
                {
                    if (self->interacting[Interaction::Active])
                    {
                        ents[id]->setactivity(false);
                        self->interacting[Interaction::Active] = false;
                        event::emit<event::Trigger>(id, event::Use);
                    }
                }
                else if (triggerType == TriggerType::Item || triggerType == TriggerType::Marker)
                {
                    // Disable the item trigger once the player reaches it.
                    ents[id]->setactivity(false);
                }

                // Add the entity's ID to the list of triggers in proximity.
                if (proximityTriggers.find(id) < 0)
                {
                    proximityTriggers.add(id);
                }

                player->lastpickup = ents[id]->type;
                break;
            }

            default:
            {
                if (player->canpickup(ents[id]->type) && isPickupAllowed())
                {
                    addmsg(N_ITEMPICKUP, "rci", player, id);

                    // Clear the item regardless of who picked it up, as it will be no longer available.
                    ents[id]->clearspawned();
                }
                break;
            }
        }
    }

    /*
        Performs a "raycast" to check whether the cursor is hovering over an entity.
        The hovered entity's ID is returned.
    */
    static int findHovered()
    {
        int id, orient = 0;
        rayent(camera1->o, camdir, 1e16f, RAY_ENTS | RAY_SKIPFIRST, 0, orient, id);
        return id;
    }

    FVARP(itemhoverdistance, 0, 100.0f, 500.0f);

   /*
        Checks if the player is currently hovering over a valid item entity.
        Conditions to perform the check:
            - Item hover distance must be enabled (non-zero).
            - Items must be allowed in the current mutator settings.
            - The player must be alive.
            - The player must be the one currently being followed (e.g., in spectate mode).
        If hovering over a valid item within the hover distance:
        If the item is ammo (within ammo type range), update `hoveredweapon` accordingly.
        If no valid hover item is found or conditions fail, reset `hoveredweapon` to invalid if it was previously valid.
    */
    static void checkHovered(const gameent* player)
    {
        if (player->state != CS_ALIVE || player != followingplayer(self))
        {
            hover.reset();
            return;
        }
        const int id = findHovered();
        if (ents.inrange(id))
        {
            extentity* entity = ents[id];
            if (!entity)
            {
                hover.reset();
                return;
            }
            if (!m_noitems(mutators) && validitem(entity->type) && entity->spawned())
            {
                const bool isClose = itemhoverdistance && camera1->o.dist(entity->o) <= itemhoverdistance;
                if (isClose)
                {
                    if (entity->type >= I_AMMO_SG && entity->type <= I_AMMO_GRENADE)
                    {
                        hover.hoveredWeapon = itemstats[entity->type - I_AMMO_SG].info;
                        hover.resetInteraction();
                        return;
                    }
                }
            }
            else
            {
                loopv(proximityTriggers)
                {
                    const int triggerId = proximityTriggers[i];
                    extentity* trigger = ents[triggerId];
                    if
                    (
                        !trigger || !trigger->isactive() ||
                        !ents.inrange(i) || trigger->type != TRIGGER)
                    {
                        continue;
                    }
                    const bool isClose = entity->o.dist(trigger->o) <= trigger->attr3;
                    if (isClose && query::match(entity->label, trigger->label))
                    {
                        self->interacting[Interaction::Available] = true;
                        return;
                    }
                }
            }
        }
        // Remove valid hover information.
        hover.reset();
    }
    ICOMMAND(gethoverweapon, "", (), intret(hover.hoveredWeapon));

    /*
        Checks various entity-related interactions for the specified game entity.
        This includes:
            - Updating hovered items.
            - Checking proximity or distance to active entities (items, teleports, triggers).
            - Attempting to pick up items or trigger events if within relevant radii.
    */
    void checkItems(gameent* player)
    {
        if (player->state != CS_ALIVE && player->state != CS_SPECTATOR)
        {
            return;
        }
        checkHovered(player);
        const vec origin = player->feetpos();
        loopv(ents)
        {
            extentity& entity = *ents[i];
            const int id = i;
            if (entity.type == NOTUSED || !entity.isactive())
            {
                continue;
            }
            const float distance = entity.o.dist(origin);
            if (entity.type == TRIGGER && m_story)
            {
                const int radius = entity.attr3 ? entity.attr3 : ENTITY_COLLECT_RADIUS;
                if (distance < radius)
                {
                    tryPickup(id, player);
                }
                continue;
            }
            if
            (
                (player->state == CS_SPECTATOR && entity.type != TELEPORT) ||
                (!entity.spawned() && entity.type != TELEPORT && entity.type != JUMPPAD)
            )
            {
                continue;
            }
            const int radius = entity.type == TELEPORT ? ENTITY_TELEPORT_RADIUS : ENTITY_COLLECT_RADIUS;
            if (distance < radius)
            {
                tryPickup(id, player);
            }
        }
        loopv(projectiles::items)
        {
            ProjEnt& proj = *projectiles::items[i];
            const int id = i;
            const float distance = proj.o.dist(origin);
            const int radius = ENTITY_COLLECT_RADIUS;
            if (distance < radius)
            {
                projectiles::pick(&proj, proj.item, player);
            }
        }

        // Check if the player moved away from a trigger in proximity.
        if (m_story && player == self)
        {
            loopvrev(proximityTriggers)
            {
                const int id = proximityTriggers[i];
                if (!ents.inrange(id) || ents[id]->type != TRIGGER)
                {
                    continue;
                }
                const extentity& e = *ents[id];
                const float distance = e.o.dist(origin);
                const int radius = e.attr3 ? e.attr3 : ENTITY_COLLECT_RADIUS;
                const int exitRadius = max(0, max(radius, e.attr4 ? e.attr4 : ENTITY_COLLECT_RADIUS));
                if (distance > exitRadius)
                {
                    event::emit<event::Trigger>(id, event::Distance);
                    proximityTriggers.remove(i);
                    hover.resetInteraction();
                }
            }
        }
    }

    // Update power-up effects and client-side timer.
    void updatePowerups(const int time, gameent* player)
    {
        const int sound = player->role == ROLE_BERSERKER ? S_BERSERKER_LOOP : (S_LOOP_DAMAGE + player->poweruptype - 1);
        player->playchannelsound(Chan_PowerUp, sound, 200, true);
        if (m_berserker && player->role == ROLE_BERSERKER && !player->powerupmillis)
        {
            return;
        }

        // Client-side power-up timer.
        if ((player->powerupmillis -= time) <= 0)
        {
            player->powerupmillis = 0;
            playsound(S_TIMEOUT_DAMAGE + player->poweruptype - 1, player);
            player->poweruptype = PU_NONE;
            if (player->role != ROLE_BERSERKER)
            {
                player->stopchannelsound(Chan_PowerUp, 500);
            }
        }
    }

    // Spawns items locally and puts them in the network stream.
    void sendItems(packetbuf& p)
    {
        putint(p, N_ITEMLIST);
        loopv(ents) if (canSpawnItem(ents[i]->type))
        {
            putint(p, i);
            putint(p, ents[i]->type);
        }
        putint(p, -1);
    }

    // Resets the items' spawn state on map load/reset.
    void resetSpawn()
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

    // Spawns all items immediately (if forced by the server) or delay them.
    void spawnItems(const bool isForced)
    {
        loopv(ents) if (canSpawnItem(ents[i]->type))
        {
            ents[i]->setspawned(isForced || !server::delayspawn(ents[i]->type));
            ents[i]->lastspawn = lastmillis;
        }
    }

    // Items' spawn effect.
    static void doSpawnEffect(const extentity* entity)
    {
        const int spawnColor = 0x00E463;
        particle_splash(PART_SPARK, 20, 100, entity->o, spawnColor, 1.0f, 100, 60);
        particle_flare(entity->o, entity->o, 500, PART_EXPLODE1, 0x83E550, 1.0f, nullptr, 16.0f);
        adddynlight(entity->o, 116, vec::hexcolor(spawnColor), 500, 75, DL_EXPAND | L_NOSHADOW);
        playsound(S_ITEM_SPAWN, nullptr, &entity->o, nullptr, 0, 0, 0, -1, 0, 1500);
        if (entity->type >= I_DDAMAGE && entity->type <= I_INVULNERABILITY)
        {  
            conoutf(CON_GAMEINFO, "\f2%s power-up available!", gentities[entity->type].prettyname);
            playsound(S_POWERUP_SPAWN);
        }
    }

    // Spawn an item based on its ID.
    void setSpawn(const int id, const bool shouldSpawn, const bool isForced)
    {
        if (ents.inrange(id))
        {
            extentity* entity = ents[id];
            entity->setspawned(shouldSpawn);
            if (!isForced)
            {
                doSpawnEffect(entity);
            }
            entity->lastspawn = lastmillis;
        }
    }

    // Create a new game entity.
    extentity *make()
    { 
        return new gameentity();
    }
    
    // Delete a game entity.
    void remove(extentity* e)
    { 
        delete (gameentity*) e;
    }

    // Clear all game entities.
    void clear()
    {
        while(ents.length()) remove(ents.pop());
    }

    // Animate an entity based on its trigger state.
    void animateMapModel(const extentity& entity, int& animation, int& baseTime)
    {
        switch (entity.triggerstate)
        {
            case TriggerState::Reset:
            {
                animation = ANIM_TRIGGER | ANIM_START;
                break;
            }

            case TriggerState::Triggering:
            {
                animation = ANIM_TRIGGER;
                baseTime = entity.lasttrigger;
                break;
            }

            case TriggerState::Triggered:
            {
                animation = ANIM_TRIGGER | ANIM_END;
                break;
            }

            case TriggerState::Resetting:
            {
                animation = ANIM_TRIGGER | ANIM_REVERSE;
                baseTime = entity.lasttrigger;
                break;
            }

            default: break;
        }
    }

    // Fix an entity's parameters if necessary (older map versions).
    void fix(extentity& entity)
    {
        switch (entity.type)
        {
            case FLAG:
                entity.attr5 = entity.attr4;
                entity.attr4 = entity.attr3;
                // Fall through.
            case TELEDEST:
                entity.attr3 = entity.attr2;
                entity.attr2 = entity.attr1;
                entity.attr1 = static_cast<int>(self->yaw);
                break;

            case TARGET:
                entity.attr2 = entity.attr1;
                break;
        }
    }

    // Renders the radius, arrows and other visual indicators for game entities.
    void renderRadius(const extentity& entity)
    {
        const float arrowRadius = 4;
        switch (entity.type)
        {
            case TELEPORT:
            {
                loopv(ents) if (ents[i]->type == TELEDEST && entity.attr1 == ents[i]->attr2)
                {
                    renderentarrow(entity, vec(ents[i]->o).sub(entity.o).normalize(), entity.o.dist(ents[i]->o));
                    break;
                }
                break;
            }
            case JUMPPAD:
            {
                const vec direction = vec
                (
                    static_cast<int>(static_cast<char>(entity.attr3) * 10.0f),
                    static_cast<int>(static_cast<char>(entity.attr2) * 10.0f),
                    entity.attr1 * 12.5f
                ).normalize();
                renderentarrow(entity, direction, arrowRadius);
                break;
            }
            case FLAG:
            case TELEDEST:
            {
                vec direction;
                vecfromyawpitch(entity.attr1, 0, 1, 0, direction);
                renderentarrow(entity, direction, arrowRadius);
                break;
            }
            case TRIGGER:
            {
                vec direction;
                vecfromyawpitch(entity.attr1, 0, 1, 0, direction);
                renderentarrow(entity, direction, arrowRadius);
                gle::color(bvec4(0x7F, 0xFF, 0xD4, 0xFF));
                renderentsphere(entity, entity.attr3);
                if (entity.attr3 > 0 && entity.attr4 > entity.attr3)
                {
                    gle::color(bvec4(0xFF, 0xA0, 0x7A, 0xFF));
                    renderentsphere(entity, entity.attr4);
                }
                gle::color(bvec4(0xFF, 0xFF, 0xFF, 0xFF));
                break;
            }
            case TARGET:
            {
                vec direction;
                vecfromyawpitch(entity.attr2, 0, 1, 0, direction);
                renderentarrow(entity, direction, arrowRadius);
                break;
            }
        }
    }

    bool shouldPrint(const extentity& entity, const char* buffer, const int len)
    {
        return false;
    }

    // Returns the simple or pretty name of an entity.
    const char* getName(const int type, const bool isPretty)
    {
        if (type >= 0 && type < MAXENTTYPES)
        {
            return isPretty ? gentities[type].prettyname : gentities[type].name;
        }
        return "";
    }

    // Clear the trigger state of an entity.
    static inline void clearTriggerFlags(extentity* entity)
    {
        if (entity)
        {
            entity->flags &= ~(EF_ANIM | EF_NOVIS | EF_NOSHADOW | EF_NOCOLLIDE);
            entity->lasttrigger = 0;
            entity->triggerstate = TriggerState::Null;
        }
    }

    // Clear the trigger state of each entity.
    void resetTriggers()
    {
        loopv(ents)
        {
            extentity* e = ents[i];
            if (e->type != ET_MAPMODEL)
            {
                continue;
            }
            clearTriggerFlags(e);
        }
    }

    // Edits an entity locally and sends the changes to the server.
    void edit(const int id, const bool isLocal)
    {
        extentity& entity = *ents[id];
        clearTriggerFlags(&entity);
        entity.setactivity(true);
        if (isLocal)
        {
            addmsg
            (
                N_EDITENT, "rii3ii5", id, static_cast<int>(entity.o.x * DMF), static_cast<int>(entity.o.y * DMF), static_cast<int>(entity.o.z * DMF),
                entity.type, entity.attr1, entity.attr2, entity.attr3, entity.attr4, entity.attr5
            );
        }
        if (canSpawnItem(entity.type) && !entity.spawned())
        {
            entity.lastspawn = lastmillis;
        }
    }

    // Edits an entity's label locally and sends the updated label to the server.
    void editLabel(const int id, const bool isLocal)
    {
        const extentity& entity = *ents[id];
        if (isLocal)
        {
            addmsg(N_EDITENTLABEL, "ris", id, entity.label ? entity.label : "");
        }
    }

    // Returns the drop height of an entity based on its type (e.g. player drops a flag).
    float dropHeight(const entity& entity)
    {
        if (entity.type == FLAG)
        {
            return 0.0f;
        }
        return 4.0f;
    }

    // Search entities by type and label and returns a list of IDs.
    ICOMMAND(entquery, "ss", (char* type, char* query),
    {
        vector<extentity*>&ents = entities::getents();
        int typeIndex = -1;
        if (type[0])
        {
            if ((typeIndex = findenttype(type)) == ET_EMPTY)
            {
                result("");
                return;
            }
        }
        vector<char> buffer;
        string id;
        int ids = 0;
        loopv(ents)
        {
            if
            (
                (typeIndex < 0 || typeIndex == ents[i]->type) &&
                (query::match(query, ents[i]->label))
            )
            {
                if (ids++)
                {
                    buffer.add(' ');
                }
                formatstring(id, "%d", i);
                buffer.put(id, strlen(id));
            }
        }
        buffer.add('\0');
        result(buffer.getbuf());
    })

    // Clear the list of triggers in proximity (when starting a new map).
    static void clearProximityTriggers()
    {
        proximityTriggers.setsize(0);
    }

    // Emit "Distance" events for all triggers in proximity (also called when the player dies).
    static void emitDistanceEvents()
    {
        loopvrev(proximityTriggers)
        {
            event::emit<event::Trigger>(proximityTriggers[i], event::Distance);
            proximityTriggers.remove(i);
        }
    }

    // Events: player death.
    void onPlayerDeath(const gameent* player, const gameent* actor)
    {
        if (player != self)
        {
            return;
        }
        emitDistanceEvents();
    }

    // Events: player enters spectator mode.
    void onPlayerSpectate(const gameent* player)
    {
        if (player != self)
        {
            return;
        }
        emitDistanceEvents();
    }

    // Events: player exits spectator mode.
    void onPlayerUnspectate(const gameent* player)
    {
    }

    // Events: world loaded.
    void onMapStart()
    {
        clearProximityTriggers();
    }

    /*
        Sets the ID of the last respawn point.
        Mostly used by triggers.
    */
    static void setRespawnPoint(const int id)
    {
        self->respawnPoint = ents.inrange(id) ? id : -1;
    }
    ICOMMAND(setrespawnpoint, "i", (int* id), setRespawnPoint(*id));

    // Returns whether or not a trigger is enabled.
    static int getTriggerState(const int id)
    {
        if (!ents.inrange(id))
        {
            return 0;
        }
        return ents[id]->isactive() ? 1 : 0;
    }

    // Enables or disables a trigger.
    static void setTriggerState(const int id, const int state)
    {
        if (!ents.inrange(id))
        {
            return;
        }
        ents[id]->setactivity(state != 0 ? true : false);
    }
    ICOMMAND(triggerstate, "iiN", (int* id, int* state, int* numargs),
    {
        if (*numargs > 1)
        {
            setTriggerState(*id, *state);
        }
        else
        {
            intret(getTriggerState(*id));
        }
    });

    // Sets the trigger state of a map model.
    static void triggerMapModel(const int id, const int state, const int sound = S_INVALID)
    {
        extentity* entity = nullptr;
        if (ents.inrange(id))
        {
            entity = ents[id];
        }
        if (!entity || !m_story || entity->type != ET_MAPMODEL)
        {
            return;
        }
        clearTriggerFlags(entity);
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
            playSound(S_TRIGGER, sound, entity->o);
        }
    }
    ICOMMAND(triggermapmodel, "iib", (int* id, int* state, int* sound), triggerMapModel(*id, *state, *sound));
    ICOMMAND(mapmodeltriggerstate, "i", (int* id),
    {
        const extentity* entity = ents.inrange(*id) ? ents[*id] : nullptr;
        intret(entity && entity->type == ET_MAPMODEL ? entity->triggerstate : 0);
    });
#endif
}
