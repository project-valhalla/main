#include "engine.h"
#include "game.h"

namespace game
{
    namespace camera
    {
        bool isthirdperson()
        {
            return self != camera1 || camera.isdetached;
        }

        VARP(firstpersondeath, 0, 0, 1);

        bool isfirstpersondeath()
        {
            return firstpersondeath || m_story;
        }

        bool isUnderwater()
        {
            const int material = lookupmaterial(camera1->o);
            return self->state != CS_EDITING && isliquidmaterial(material & MATF_VOLUME);
        }

        void fixrange()
        {
            static const float MAXPITCH = 90.0f;
            if (camera1->pitch > MAXPITCH)
            {
                camera1->pitch = MAXPITCH;
            }
            if (camera1->pitch < -MAXPITCH)
            {
                camera1->pitch = -MAXPITCH;
            }
            while (camera1->yaw < 0.0f)
            {
                camera1->yaw += 360.0f;
            }
            while (camera1->yaw >= 360.0f)
            {
                camera1->yaw -= 360.0f;
            }
        }

        VARP(killcamera, 0, 1, 1);
        FVARP(killcameramillis, 1.0f, 250.0f, 5000.0f);

        static void resetkillcamera()
        {
            gameent* hud = followingplayer(self);
            if (hud->lastattacker <= -1)
            {
                return;
            }

            hud->lastattacker = -1;
            if (hud->state == CS_DEAD)
            {
                restore();
            }
        }

        static void updatekillcamera()
        {
            if (!killcamera)
            {
                return;
            }

            gameent* hud = followingplayer(self);
            gameent* last = getclient(hud->lastattacker);
            if (!last || last == hud)
            {
                /* Safety check.
                 * If we don't have a fragger (or as an extra we fragged ourselves),
                 * we have none to follow, so we stop caring.
                 */
                resetkillcamera();
                return;
            }
            if (hud->state != CS_DEAD || last->state != CS_ALIVE)
            {
                /* If we are not dead, or our fragger is not alive,
                 * we will reset information and forget about this until it becomes relevant.
                 */
                resetkillcamera();
                return;
            }
            const vec camera = camera1->o;
            const vec direction = vec(last->o).sub(camera).normalize();
            vec position = vec(0, 0, 0);
            static int lastUpdate = 0;
            static int lastFollow = 0;
            const bool isVisible = raycubelos(camera1->o, last->o, position);
            const bool hasDelay = lastmillis - hud->lastpain < SPAWN_DURATION || lastmillis - lastFollow <= killcameramillis;
            if (isVisible || hasDelay)
            {
                vectoyawpitch(direction, camera1->yaw, camera1->pitch);
                fixrange();
                camera1->o = hud->o;
                if (lastUpdate)
                {
                    lastUpdate = 0;
                    lastFollow = lastmillis;
                }
            }
            else
            {
                if (!lastUpdate)
                {
                    lastUpdate = lastmillis;
                    lastFollow = 0;
                    clearscreeneffects();
                }
                const float progress = clamp((lastmillis - lastUpdate) / killcameramillis, 0.0f, 1.0f);
                vec startPoint = hud->o;
                const vec endPoint = last->o;
                camera1->o = startPoint.lerp(endPoint, progress);
            }
        }

        inline bool hasfreelook()
        {
            return (!ispaused() || !remote) && !(self->state == CS_DEAD && isfirstpersondeath());
        }

        FVARP(zoomsensitivity, 1e-4f, 4.5f, 1e4f);
        FVARP(zoomacceleration, 0, 0, 1000);
        VARP(zoomautosensitivity, 0, 1, 1);
        FVARP(mousesensitivity, 1e-4f, 10, 1e4f);
        FVARP(mousesensitivityscale, 1e-4f, 100, 1e4f);
        VARP(mouseinvert, 0, 0, 1);
        FVARP(mouseacceleration, 0, 0, 1000);

        static void updateorientation(const float yaw, const float pitch)
        {
            camera1->yaw += yaw;
            camera1->pitch += pitch;
            fixrange();
            if (camera1 != self && !camera.isdetached)
            {
                self->yaw = camera1->yaw;
                self->pitch = camera1->pitch;
            }
        }

        void moveMouse(const int x, const int y)
        {
            if (!hasfreelook())
            {
                return;
            }
            float cursens = mousesensitivity, curaccel = mouseacceleration;
            if (zoom)
            {
                if (zoomautosensitivity)
                {
                    cursens = float(mousesensitivity * zoomfov) / camerafov;
                    curaccel = float(mouseacceleration * zoomfov) / camerafov;
                }
                else
                {
                    cursens = zoomsensitivity;
                    curaccel = zoomacceleration;
                }
            }
            if (curaccel && curtime && (x || y)) cursens += curaccel * sqrtf(x * x + y * y) / curtime;
            cursens /= mousesensitivityscale;
            updateorientation(x * cursens, y * cursens * (mouseinvert ? 1 : -1));
        }

        bool allowthirdperson()
        {
            return self->state == CS_SPECTATOR || m_edit || (m_berserker && self->role == ROLE_BERSERKER);
        }
        ICOMMAND(allowthirdperson, "", (), intret(allowthirdperson()));

        inline bool isdetached()
        {
            gameent* d = followingplayer();
            if (d)
            {
                return specmode > 1 || d->state == CS_DEAD;
            }
            return (intermission && self->state != CS_SPECTATOR) || (!isfirstpersondeath() && self->state == CS_DEAD);
        }

        inline bool iscolliding()
        {
            switch (self->state)
            {
                case CS_EDITING: return false;
                case CS_SPECTATOR: return followingplayer() != NULL;
            }
            return true;
        }

        void update()
        {
            camera.update();
        }

        void reset()
        {
            camera.events.shrink(0);
            camera.shakes.shrink(0);
        }

        FVAR(cameraparallaxstrength, 0, 0.01f, 1);
        FVAR(cameraparallaxsmooth, 0, 0.1f, 1);
        FVAR(cameraparallaxdecay, 0, 6.0f, 1);

        void updateParallax(const int x, const int y, const bool isClick)
        {
            if (!isClick)
            {
                camera.parallaxVelocity.x += x * cameraparallaxstrength;
                camera.parallaxVelocity.y -= y * cameraparallaxstrength;
                camera.parallaxVelocity.x = clamp(camera.parallaxVelocity.x, -1.f, 1.f);
                camera.parallaxVelocity.y = clamp(camera.parallaxVelocity.y, -1.f, 1.f);
            }
            else if (mainmenu && self)
            {
                float& yaw = self->yaw;
                yaw -= x * cameraparallaxstrength * 10.0f;
                while (yaw < 0.0f)
                {
                    yaw += 360.0f;
                }
                while (yaw >= 360.0f)
                {
                    yaw -= 360.0f;
                }
            }
        }

        VAR(cameraentityid, 0, 0, MAXENTS);
        FVAR(cameraentityspeed, 0, 5.0f, 100.0f);
        FVAR(cameraentitybob, 0, 1.0f, 2.0f);

        void updateParallax()
        {
            camera.parallax.x += (camera.parallaxVelocity.x - camera.parallax.x) * cameraparallaxsmooth;
            camera.parallax.y += (camera.parallaxVelocity.y - camera.parallax.y) * cameraparallaxsmooth;
            camera1->yaw -= camera.parallax.x;
            camera1->pitch -= camera.parallax.y;
            const float decay = cameraparallaxdecay * curtime / 1000.f;
            camera.parallaxVelocity.x = lerp(camera.parallaxVelocity.x, 0.f, decay);
            camera.parallaxVelocity.y = lerp(camera.parallaxVelocity.y, 0.f, decay);

            // Add bob to make the "camera point" more dynamic.
            if (cameraentitybob)
            {
                const float progress = 0.001f * totalmillis;
                const float yaw = sinf(progress * 0.6f);
                const float pitch = sinf(progress * 1.2f) * 0.5f;
                camera1->yaw -= yaw * cameraentitybob;
                camera1->pitch -= pitch * cameraentitybob;
            }
        }

        void set()
        {
            if (mainmenu)
            {
                static physent temporaryCamera;
                camera1 = &temporaryCamera;
                const int cameraEntity = findentity(CAMERA, 0, cameraentityid);
                if (cameraEntity >= 0)
                {
                    const vector<extentity*>& ents = entities::getents();
                    if (mainmenu)
                    {
                        /*
                            Do not interpolate during a game, only in the menu.
                            Repeating the condition to ensure this behaviour remains isolated to the menu,
                            even when other uses of the camera entity are implemented.
                        */
                        const vec target = ents[cameraEntity]->o;
                        const float progress = min(cameraentityspeed * curtime / 1000.0f, 1.0f);
                        camera1->o.lerp(camera1->o, target, progress);
                    }
                    else
                    {
                        camera1->o = ents[cameraEntity]->o;
                    }
                    camera1->yaw = ents[cameraEntity]->attr2;
                    camera1->pitch = ents[cameraEntity]->attr3;
                    camera1->resetinterp();
                }
                updateParallax();
            }
            else
            {
                gameent* target = followingplayer();
                if (target)
                {
                    self->yaw = target->yaw;
                    self->pitch = target->state == CS_DEAD ? 0 : target->pitch;
                    self->o = target->o;
                    self->resetinterp();
                }
            }
        }

        VAR(thirdperson, 0, 0, 2);
        FVAR(thirdpersondistance, 0, 14, 50);
        FVAR(thirdpersonup, -25, 0.5f, 25);
        FVAR(thirdpersonside, -25, 5.0f, 25);
        FVAR(thirdpersondistancedead, 0, 30, 50);
        FVAR(thirdpersonupdead, -25, 0, 25);
        FVAR(thirdpersonsidedead, -25, 0, 25);

        void compute()
        {
            set();
            camera.updateFov();
            if (mainmenu)
            {
                return;
            }
            const bool shouldDetach = (allowthirdperson() && thirdperson > 1) || isdetached();
            if ((!allowthirdperson() || !thirdperson) && !shouldDetach)
            {
                camera1 = self;
                camera1->o = camera.getposition();
                camera.isdetached = false;
            }
            else
            {
                static physent tempcamera;
                camera1 = &tempcamera;
                if (camera.isdetached && shouldDetach)
                {
                    camera1->o = self->o;
                }
                else
                {
                    *camera1 = *self;
                    camera.isdetached = shouldDetach;
                }
                camera1->reset();
                camera1->type = ENT_CAMERA;
                camera1->move = -1;
                camera1->eyeheight = camera1->aboveeye = camera1->radius = camera1->xradius = camera1->yradius = 2;
                updatekillcamera();
                matrix3 orient;
                orient.identity();
                orient.rotate_around_z(camera1->yaw * RAD);
                orient.rotate_around_x(camera1->pitch * RAD);
                orient.rotate_around_y(camera1->roll * -RAD);
                vec dir = vec(orient.b).neg(), side = vec(orient.a).neg(), up = orient.c;
                const bool isAlive = self->state == CS_ALIVE && !intermission;
                const float upOffset = isAlive ? thirdpersonup : thirdpersonupdead;
                const float sideOffset = isAlive ? thirdpersonside : thirdpersonsidedead;
                const float distance = isAlive ? thirdpersondistance : thirdpersondistancedead;
                if (iscolliding())
                {
                    movecamera(camera1, dir, distance, 1);
                    movecamera(camera1, dir, clamp(distance - camera1->o.dist(self->o), 0.0f, 1.0f), 0.1f);
                    if (upOffset)
                    {
                        vec pos = camera1->o;
                        float dist = fabs(upOffset);
                        if (upOffset < 0)
                        {
                            up.neg();
                        }
                        movecamera(camera1, up, dist, 1);
                        movecamera(camera1, up, clamp(dist - camera1->o.dist(pos), 0.0f, 1.0f), 0.1f);
                    }
                    if (sideOffset)
                    {
                        vec pos = camera1->o;
                        float dist = fabs(sideOffset);
                        if (sideOffset < 0)
                        {
                            side.neg();
                        }
                        movecamera(camera1, side, dist, 1);
                        movecamera(camera1, side, clamp(dist - camera1->o.dist(pos), 0.0f, 1.0f), 0.1f);
                    }
                }
                else
                {
                    camera1->o.add(vec(dir).mul(distance));
                    if (upOffset)
                    {
                        camera1->o.add(vec(up).mul(upOffset));
                    }
                    if (sideOffset)
                    {
                        camera1->o.add(vec(side).mul(sideOffset));
                    }
                }
            }
            setviewcell(camera1->o);
        }

        VARP(deathpitch, 0, 2, 2);

        void restore(bool shouldIgnorePitch)
        {
            camera.zoomstate.disable();
            if (self->state == CS_DEAD)
            {
                if (!isfirstpersondeath())
                {
                    if (deathpitch && !shouldIgnorePitch)
                    {
                        if (deathpitch == 2)
                        {
                            self->pitch = camera1->pitch = -90; // Lower your pitch to see your death from above.
                        }
                        else
                        {
                            self->pitch = camera1->pitch = 0;
                        }
                    }
                    self->roll = camera1->roll = 0;
                }
            }
            else
            {
                self->pitch = self->roll = camera1->pitch = camera1->roll = 0;
            }
            if (thirdperson)
            {
                thirdperson = 0;
            }
        }

        camerainfo camera;

        VARP(camerafov, 10, 100, 150);
        VAR(avatarzoomfov, 1, 25, 60);
        VAR(avatarfov, 10, 39, 100);
        VARF(zoom, -1, 0, 1, shaders::toggleZoomEffects(zoom));
        VARP(zoominvel, 110, 110, 500);
        VARP(zoomoutvel, 0, 150, 500);
        VARP(zoomfov, 10, 42, 90);

        void camerainfo::zoominfo::update()
        {
            if (!zoom)
            {
                progress = 0;
                curfov = (camerafov + camera.fovAdd) * camera.fovMultiply;
                curavatarfov = (avatarfov + camera.fovAvatarAdd) * camera.fovAvatarMultiply;
                return;
            }
            if (zoom > 0)
            {
                progress = zoominvel ? min(progress + float(elapsedtime) / zoominvel, 1.0f) : 1;
                shaders::updateZoomEffects(progress);
                if (progress >= 1)
                {
                    self->zooming = 1;
                }
            }
            else
            {
                progress = zoomoutvel ? max(progress - float(elapsedtime) / zoomoutvel, 0.0f) : 0;
                shaders::updateZoomEffects(progress);
                if (progress <= 0)
                {
                    zoom = self->zooming = 0;
                    shaders::toggleZoomEffects(zoom);
                }
            }
            curfov = ((zoomfov * progress + camerafov * (1 - progress))) * camera.fovMultiply;
            curavatarfov = ((avatarzoomfov * progress + avatarfov * (1 - progress))) * camera.fovAvatarMultiply;
        }

        void camerainfo::zoominfo::disable()
        {
            zoom = 0;
            progress = 0;
            self->zooming = 0;
            shaders::toggleZoomEffects(zoom);
        }

        bool camerainfo::zoominfo::isenabled()
        {
            return progress >= 1 && zoom >= 1;
        }

        VARP(camerafovslidein, 10, 150, 500);
        VARP(camerafovslideout, 10, 180, 500);
        VARP(camerafovslide, -10, 8, 10);

        void camerainfo::updateMovementFov()
        {
            gameent* hudPlayer = followingplayer(self);
            if (camerafovslide > 0)
            {
                static float slideProgress = 0;
                if (hudPlayer->slide.isSliding())
                {
                    slideProgress = camerafovslidein > 0 ? min(slideProgress + float(elapsedtime) / camerafovslidein, 1.0f) : 1.0f;
                }
                else
                {
                    slideProgress = camerafovslideout > 0 ? max(slideProgress - float(elapsedtime) / camerafovslideout, 0.0f) : 0.0f;
                }
                fovAdd = camerafovslide * slideProgress;
                fovAvatarAdd = -camerafovslide * slideProgress * 0.33f;
            }
        }	

        void camerainfo::updateFov()
        {
            camera.zoomstate.update();
            updateMovementFov();
        }

        void toggleWeaponZoom()
        {
            if (identexists("dozoom"))
            {
                execident("dozoom");
            }
            else
            {
                camera::zoom = camera::zoom ? -1 : 1;
                shaders::toggleZoomEffects(zoom);
            }
        }

        VARP(camerabob, 0, 1, 1);
        FVAR(camerabobstep, 1, 45.0f, 100);
        FVAR(camerabobside, 0, 0.45f, 1);
        FVAR(camerabobup, -1, 0.9f, 1);
        FVAR(camerabobfade, 0, 6.5f, 30.0f);

        vec camerainfo::getposition()
        {
            const gameent* hud = followingplayer(self);
            if (!intermission && self->state == CS_ALIVE)
            {
                vec position = vec(camera.direction).add(hud->o);
                if (camerabob)
                {
                    vec bob = vec(0, 0, 0);
                    vecfromyawpitch(hud->yaw, hud->pitch, 0, 1, bob);
                    const float steps = bobdist / camerabobstep * M_PI;
                    bob.mul(camerabobside * cosf(steps) * bobfade);
                    bob.z = camerabobup * (fabs(sinf(steps)) - 1) * bobfade;
                    position.add(bob);
                }
                return position;
            }
            return self->o;
        }

        void camerainfo::update()
        {
            const gameent* hud = followingplayer(self);
            if (camerabob)
            {
                if (hud->onfloor() && !hud->slide.isSliding() && hud->vel.magnitude() > 5.0f)
                {
                    bobspeed = min(sqrtf(hud->vel.x * hud->vel.x + hud->vel.y * hud->vel.y), hud->speed);
                    bobdist += bobspeed * curtime / 1000.0f;
                    bobdist = fmod(bobdist, 2 * camerabobstep);
                    if (bobfade < 1)
                    {
                        bobfade += (1.0f - pow(bobfade, 2)) * (curtime / 1000.0f) * camerabobfade;
                    }
                }
                else if (bobfade > 0)
                {
                    bobdist += bobspeed * bobfade * curtime / 1000.0f;
                    bobdist = fmod(bobdist, 2 * camerabobstep);
                    bobfade -= pow(bobfade, 2) * (curtime / 1000.0f) * camerabobfade;
                }
            }
            else
            {
                bobfade = bobspeed = bobdist = 0.0f;
            }
            const float k = pow(0.7f, curtime / 10.0f);
            direction.mul(k);
            direction.add(vec(hud->vel).mul((1 - k) / (2 * max(hud->vel.magnitude(), hud->speed))));
            processevents();
            updateshake();
        }

        VARP(camerashake, 0, 1, 1);

        void camerainfo::addshake(int factor)
        {
            if (!camerashake)
            {
                return;
            }
            ShakeEvent& shake = shakes.add();
            shake.millis = lastmillis;
            shake.duration = factor;
            shake.intensity = factor / 100.0f;
        }

        void camerainfo::updateshake()
        {
            if (shakes.empty() || !camerashake)
            {
                if (shakes.length())
                {
                    shakes.shrink(0);
                }
                return;
            }
            loopv(shakes)
            {
                ShakeEvent& shake = shakes[i];
                const int elapsed = lastmillis - shake.millis;
                if (elapsed > shake.duration)
                {
                    shakes.remove(i--);
                }
                else
                {
                    vec offset(0, 0, 0);
                    const float progress = static_cast<float>(elapsed) / shake.duration;
                    const float amount = shake.intensity * (1.0f - progress);
                    offset.x += amount * cosf(progress * M_PI * 2.0f + rndscale(100.0f));
                    offset.y += amount * sinf(progress * M_PI * 2.0f + rndscale(100.0f));
                    offset.z += amount * cosf(progress * M_PI * 2.0f + rndscale(100.0f));
                    direction.add(offset);
                }
            }
        }

        VARP(cameramovement, 0, 1, 1);

        void camerainfo::addevent(const gameent* owner, int type, int duration, float factor)
        {
            if (!cameramovement || !owner || owner != followingplayer(self))
            {
                // Camera effects are rendered only for ourselves or the player being spectated.
                return;
            }
            CameraEvent& event = events.add();
            event.type = type;
            event.millis = lastmillis;
            event.duration = duration;
            event.factor = factor;
        }

        void camerainfo::processevents()
        {
            if (events.empty())
            {
                return;
            }

            if (!cameramovement)
            {
                events.shrink(0);
                return;
            }
            
            loopv(events)
            {
                CameraEvent& event = events[i];
                const int elapsed = lastmillis - event.millis;
                if (elapsed > event.duration)
                {
                    events.remove(i--);
                }
                else
                {
                    switch (event.type)
                    {
                        case CameraEvent_Land:
                        {
                            const float progress = clamp((lastmillis - event.millis) / static_cast<float>(event.duration), 0.0f, 1.0f);
                            const float curve = event.factor * sinf(progress * M_PI);
                            direction.z += curve;
                            break;
                        }

                        case CameraEvent_Shake:
                        {
                            addshake(event.duration);
                            break;
                        }

                        case CameraEvent_Spawn:
                        case CameraEvent_Teleport:
                        {
                            const float progress = min((lastmillis - event.millis) / static_cast<float>(event.duration), 1.0f);
                            const float ease = ease::outback(progress);
                            fovMultiply = ease;
                            fovAvatarMultiply = ease;
                            break;
                        }
                    }
                }
            }
        }

        ICOMMAND(getcamvelocityx, "", (), floatret(camera.velocity.x));
        ICOMMAND(getcamvelocityy, "", (), floatret(camera.velocity.y));
    }
}