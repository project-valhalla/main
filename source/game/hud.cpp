#include "game.h"
#include "engine.h"

namespace game
{
    float clipconsole(float w, float h)
    {
        if (cmode)
        {
            return cmode->clipconsole(w, h);
        }
        return 0;
    }

    bool detachedcamera = false;

    bool isthirdperson()
    {
        return player != camera1 || detachedcamera;
    }

    void fixcamerarange()
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

    VARP(fov, 10, 100, 150);
    VAR(avatarzoomfov, 1, 1, 1);
    VAR(avatarfov, 10, 39, 100);
    VAR(zoom, -1, 0, 1);
    VARP(zoominvel, 0, 90, 500);
    VARP(zoomoutvel, 0, 80, 500);
    VARP(zoomfov, 10, 42, 90);

    static float zoomprogress = 0;

    void disablezoom()
    {
        zoom = 0;
        zoomprogress = 0;
    }

    void computezoom()
    {
        if (!zoom)
        {
            zoomprogress = 0;
            curfov = fov;
            curavatarfov = avatarfov;
            return;
        }
        if (zoom > 0)
        {
            zoomprogress = zoominvel ? min(zoomprogress + float(elapsedtime) / zoominvel, 1.0f) : 1;
        }
        else
        {
            zoomprogress = zoomoutvel ? max(zoomprogress - float(elapsedtime) / zoomoutvel, 0.0f) : 0;
            if (zoomprogress <= 0) zoom = 0;
        }
        curfov = zoomfov * zoomprogress + fov * (1 - zoomprogress);
        curavatarfov = avatarzoomfov * zoomprogress + avatarfov * (1 - zoomprogress);
    }

    VARP(firstpersondeath, 0, 0, 1);

    bool isfirstpersondeath()
    {
        return firstpersondeath || m_story;
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

    static void updateorientation(float yaw, float pitch)
    {
        camera1->yaw += yaw;
        camera1->pitch += pitch;
        fixcamerarange();
        if (camera1 != player && !detachedcamera)
        {
            player->yaw = camera1->yaw;
            player->pitch = camera1->pitch;
        }
    }

    void mousemove(int dx, int dy)
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
                cursens = float(mousesensitivity * zoomfov) / fov;
                curaccel = float(mouseacceleration * zoomfov) / fov;
            }
            else
            {
                cursens = zoomsensitivity;
                curaccel = zoomacceleration;
            }
        }
        if (curaccel && curtime && (dx || dy)) cursens += curaccel * sqrtf(dx * dx + dy * dy) / curtime;
        cursens /= mousesensitivityscale;
        updateorientation(dx * cursens, dy * cursens * (mouseinvert ? 1 : -1));
    }

    void setupcamera()
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

    bool allowthirdperson()
    {
        return self->state == CS_SPECTATOR || m_edit || (m_berserker && self->role == ROLE_BERSERKER);
    }
    ICOMMAND(allowthirdperson, "", (), intret(allowthirdperson()));

    inline bool iscameradetached()
    {
        gameent* d = followingplayer();
        if (d)
        {
            return specmode > 1 || d->state == CS_DEAD;
        }
        return (intermission && self->state != CS_SPECTATOR) || (!isfirstpersondeath() && self->state == CS_DEAD);
    }

    inline bool iscameracolliding()
    {
        switch (self->state)
        {
            case CS_EDITING: return false;
            case CS_SPECTATOR: return followingplayer() != NULL;
        }
        return true;
    }

    VAR(thirdperson, 0, 0, 2);
    FVAR(thirdpersondistance, 0, 14, 50);
    FVAR(thirdpersonup, -25, 0.5f, 25);
    FVAR(thirdpersonside, -25, 5.0f, 25);
    FVAR(thirdpersondistancedead, 0, 30, 50);
    FVAR(thirdpersonupdead, -25, 0, 25);
    FVAR(thirdpersonsidedead, -25, 0, 25);

    void recomputecamera()
    {
        setupcamera();
        computezoom();

        bool shoulddetach = (allowthirdperson() && thirdperson > 1) || iscameradetached();
        if ((!allowthirdperson() || !thirdperson) && !shoulddetach)
        {
            camera1 = player;
            detachedcamera = false;
        }
        else
        {
            static physent tempcamera;
            camera1 = &tempcamera;
            if (detachedcamera && shoulddetach) camera1->o = player->o;
            else
            {
                *camera1 = *player;
                detachedcamera = shoulddetach;
            }
            camera1->reset();
            camera1->type = ENT_CAMERA;
            camera1->move = -1;
            camera1->eyeheight = camera1->aboveeye = camera1->radius = camera1->xradius = camera1->yradius = 2;
            matrix3 orient;
            orient.identity();
            orient.rotate_around_z(camera1->yaw * RAD);
            orient.rotate_around_x(camera1->pitch * RAD);
            orient.rotate_around_y(camera1->roll * -RAD);
            vec dir = vec(orient.b).neg(), side = vec(orient.a).neg(), up = orient.c;
            bool isalive = player->state == CS_ALIVE && !intermission;
            float verticaloffset = isalive ? thirdpersonup : thirdpersonupdead;
            float horizontaloffset = isalive ? thirdpersonside : thirdpersonsidedead;
            float distance = isalive ? thirdpersondistance : thirdpersondistancedead;
            if (iscameracolliding())
            {
                movecamera(camera1, dir, distance, 1);
                movecamera(camera1, dir, clamp(distance - camera1->o.dist(player->o), 0.0f, 1.0f), 0.1f);
                if (verticaloffset)
                {
                    vec pos = camera1->o;
                    float dist = fabs(verticaloffset);
                    if (verticaloffset < 0)
                    {
                        up.neg();
                    }
                    movecamera(camera1, up, dist, 1);
                    movecamera(camera1, up, clamp(dist - camera1->o.dist(pos), 0.0f, 1.0f), 0.1f);
                }
                if (horizontaloffset)
                {
                    vec pos = camera1->o;
                    float dist = fabs(horizontaloffset);
                    if (horizontaloffset < 0)
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
                if (verticaloffset)
                {
                    camera1->o.add(vec(up).mul(verticaloffset));
                }
                if (horizontaloffset)
                {
                    camera1->o.add(vec(side).mul(horizontaloffset));
                }
            }
        }

        setviewcell(camera1->o);
    }

    VARP(lowhealthscreen, 0, 1, 1);
    VARP(lowhealthscreenmillis, 500, 1000, 2000);
    VARP(lowhealthscreenamount, 50, 200, 1000);
    VARP(lowhealthscreenfactor, 1, 5, 100);

    int lastheartbeat = 0;

    void managelowhealthscreen()
    {
        gameent* hud = followingplayer(self);
        if (!lowhealthscreen || intermission || !hud->haslowhealth())
        {
            hud->stopchannelsound(Chan_LowHealth, 400);
            return;
        }

        hud->playchannelsound(Chan_LowHealth, S_LOW_HEALTH, 200, true);
        if (!lastheartbeat || lastmillis - lastheartbeat >= lowhealthscreenmillis)
        {
            damageblend(lowhealthscreenamount, lowhealthscreenfactor);
            lastheartbeat = lastmillis;
        }
    }

    VARNP(damagecompass, usedamagecompass, 0, 1, 1);
    VARP(damagecompassfade, 1, 1000, 10000);
    VARP(damagecompasssize, 1, 30, 100);
    VARP(damagecompassalpha, 1, 25, 100);
    VARP(damagecompassmin, 1, 25, 1000);
    VARP(damagecompassmax, 1, 200, 1000);

    float damagedirs[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    void damagecompass(int n, const vec& loc)
    {
        if (!usedamagecompass)
        {
            return;
        }
        vec delta(loc);
        delta.sub(camera1->o);
        float yaw = 0, pitch;
        if (delta.magnitude() > 4)
        {
            vectoyawpitch(delta, yaw, pitch);
            yaw -= camera1->yaw;
        }
        if (yaw >= 360) yaw = fmod(yaw, 360);
        else if (yaw < 0) yaw = 360 - fmod(-yaw, 360);
        int dir = (int(yaw + 22.5f) % 360) / 45;
        damagedirs[dir] += max(n, damagecompassmin) / float(damagecompassmax);
        if (damagedirs[dir] > 1) damagedirs[dir] = 1;

    }

    static void drawdamagecompass(int w, int h)
    {
        hudnotextureshader->set();

        int dirs = 0;
        float size = damagecompasssize / 100.0f * min(h, w) / 2.0f;
        loopi(8) if (damagedirs[i] > 0)
        {
            if (!dirs)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                gle::colorf(1, 0, 0, damagecompassalpha / 100.0f);
                gle::defvertex();
                gle::begin(GL_TRIANGLES);
            }
            dirs++;

            float logscale = 32;
            float scale = log(1 + (logscale - 1) * damagedirs[i]) / log(logscale);
            float offset = -size / 2.0f - min(h, w) / 4.0f;
            matrix4x3 m;
            m.identity();
            m.settranslation(w / 2, h / 2, 0);
            m.rotate_around_z(i * 45 * RAD);
            m.translate(0, offset, 0);
            m.scale(size * scale);

            gle::attrib(m.transform(vec2(1, 1)));
            gle::attrib(m.transform(vec2(-1, 1)));
            gle::attrib(m.transform(vec2(0, 0)));

            // fade in log space so short blips don't disappear too quickly
            scale -= float(curtime) / damagecompassfade;
            damagedirs[i] = scale > 0 ? (pow(logscale, scale) - 1) / (logscale - 1) : 0;
        }
        if (dirs) gle::end();
    }

    int damageblendmillis = 0;

    VARFP(damagescreen, 0, 1, 1, { if (!damagescreen) damageblendmillis = 0; });
    VARP(damagescreenfactor, 1, 10, 100);
    VARP(damagescreenalpha, 1, 70, 100);
    VARP(damagescreenfade, 0, 500, 1000);
    VARP(damagescreenmin, 1, 50, 1000);
    VARP(damagescreenmax, 1, 100, 1000);

    void damageblend(int n, const int factor)
    {
        if (!damagescreen) return;
        if (lastmillis > damageblendmillis) damageblendmillis = lastmillis;
        damageblendmillis += clamp(n, damagescreenmin, damagescreenmax) * (factor ? factor : damagescreenfactor);
    }

    static void drawdamagescreen(int w, int h)
    {
        if (lastmillis >= damageblendmillis)
        {
            return;
        }

        hudshader->set();
        static Texture* damagetex = NULL;
        if (!damagetex)
        {
            damagetex = textureload("data/interface/hud/damage.png", 3);
        }
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        setusedtexture(damagetex);
        float fade = damagescreenalpha / 100.0f;
        if (damageblendmillis - lastmillis < damagescreenfade)
        {
            fade *= float(damageblendmillis - lastmillis) / damagescreenfade;
        }  
        gle::colorf(fade, fade, fade, fade);
        hudquad(0, 0, w, h);
    }

    int screenflashmillis = 0;

    VARFP(screenflash, 0, 1, 1,
    {
        if (!screenflash)
        {
            screenflashmillis = 0;
        }
    });
    VARP(screenflashfactor, 1, 5, 100);
    VARP(screenflashalpha, 1, 20, 100);
    VARP(screenflashfade, 0, 600, 1000);
    VARP(screenflashmin, 1, 20, 1000);
    VARP(screenflashmax, 1, 200, 1000);

    void addscreenflash(int n)
    {
        if (!screenflash) return;
        if (lastmillis > screenflashmillis) screenflashmillis = lastmillis;
        screenflashmillis += clamp(n, screenflashmin, screenflashmax) * screenflashfactor;
    }

    void drawscreenflash(int w, int h)
    {
        if (lastmillis >= screenflashmillis) return;

        hudnotextureshader->set();

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float fade = screenflashalpha / 100.0f;
        if (screenflashmillis - lastmillis < screenflashfade)
        {
            fade *= float(screenflashmillis - lastmillis) / screenflashfade;
        }
        gle::colorf(1, 1, 1, fade);
        hudquad(0, 0, w, h);
    }

    void drawblend(int x, int y, int w, int h, float r, float g, float b)
    {
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        gle::colorf(r, g, b);
        gle::defvertex(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y);
        gle::attribf(x + w, y);
        gle::attribf(x, y + h);
        gle::attribf(x + w, y + h);
        gle::end();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    inline bool iszoomedin()
    {
        return zoomprogress >= 1 && zoom > 0;
    }

    inline int checkzoom()
    {
        gameent* hud = followingplayer(self);
        if (hud->state == CS_ALIVE || hud->state == CS_LAGGED)
        {
            return guns[hud->gunselect].zoom;
        }
        return 0;
    }

    static void drawzoom(int w, int h)
    {
        if (!iszoomedin())
        {
            return;
        }

        int zoomtype = checkzoom();
        if (!zoomtype)
        {
            return;
        }

        hudshader->set();
        Texture* scopetex = NULL;
        if (!scopetex)
        {
            if (zoomtype == Zoom_Scope) scopetex = textureload("data/interface/hud/scope.png", 3);
            else if (zoomtype == Zoom_Shadow) scopetex = textureload("data/interface/shadow.png", 3);
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        setusedtexture(scopetex);
        if (zoomtype == Zoom_Scope)
        {
            float x = 0, y = 0, dimension = 0, coverage = 1, blend = 1.f - coverage;
            if (w > h)
            {
                dimension = h;
                x += (w - h) / 2;
                drawblend(0, 0, x, dimension, blend, blend, blend);
                drawblend(x + dimension, 0, x + 1, dimension, blend, blend, blend);
            }
            else if (h > w)
            {
                dimension = w;
                y += (h - w) / 2;
                drawblend(0, 0, dimension, y, blend, blend, blend);
                drawblend(0, y + dimension, dimension, y, blend, blend, blend);
            }
            else dimension = h;
            gle::colorf(1, 1, 1, coverage);
            drawquad(x, y, dimension, dimension, 0, 0, 1, 1, false, false);
        }
        else
        {
            gle::colorf(1, 1, 1, 1);
            hudquad(0, 0, w, h);
        }
    }

    FVARP(damagerolldiv, 0, 4.0f, 10);

    void setdamagehud(int damage, gameent* d, gameent* actor)
    {
        if (!d)
        {
            return;
        }

        if (actor)
        {
            if (d != actor)
            {
                damagecompass(damage, actor->o);
            }
        }
        if (damagerolldiv)
        {
            float damageRoll = damage / damagerolldiv;
            physics::addroll(d, damageRoll);
        }
        damageblend(damage);
    }

    void clearscreeneffects()
    {
        damageblendmillis = screenflashmillis = 0;
        loopi(8)
        {
            damagedirs[i] = 0;
        }
        disablezoom();
    }

    void drawhud(int w, int h)
    {
        if (!minimized)
        {
            // Only render screen effects if there is in fact a screen to render.
            drawzoom(w, h);
            drawscreenflash(w, h);
            drawdamagescreen(w, h);
            drawdamagecompass(w, h);
        }
    }

    VARP(cursorsize, 0, 18, 40);
    VARP(crosshairsize, 0, 18, 40);
    VARP(crosshairfx, 0, 1, 1);
    VARP(crosshaircolors, 0, 1, 1);

    enum
    {
        Pointer_Null = -1,
        Pointer_Cursor,
        Pointer_Default,
        Pointer_Crosshair,
        Pointer_Scope,
        Pointer_Hit,
        Pointer_Ally
    };

    static const int MAX_CROSSHAIRS = 6;

    static Texture* crosshairs[MAX_CROSSHAIRS] = { NULL, NULL, NULL, NULL, NULL, NULL };

    static const char* getdefaultpointer(int index)
    {
        switch (index)
        {
            case Pointer_Cursor:    return "data/interface/cursor.png";
            case Pointer_Crosshair: return "data/interface/crosshair/regular.png";
            case Pointer_Scope:     return "data/interface/crosshair/dot.png";
            case Pointer_Hit:       return "data/interface/crosshair/hit.png";
            case Pointer_Ally:      return "data/interface/crosshair/ally.png";
            default:                return "data/interface/crosshair/default.png";
        }
    }

    static void loadcrosshair(const char* name, int index)
    {
        if (index < 0 || index >= MAX_CROSSHAIRS) return;
        crosshairs[index] = name ? textureload(name, 3) : notexture;
        if (crosshairs[index] == notexture)
        {
            name = getdefaultpointer(index);
            if (!name) name = "data/interface/crosshair/default.png";
            crosshairs[index] = textureload(name, 3);
        }
    }
    void loadCrosshair(const char* name, int* i)
    {
        /* We are doing this in two functions because CubeScript commands need pointers,
         * while we also need to load non-pointer indexes.
         */
        loadcrosshair(name, *i);
    }
    COMMANDN(crosshairload, loadCrosshair, "si");

    void writecrosshairs(stream* f)
    {
        loopi(MAX_CROSSHAIRS)
        {
            if (crosshairs[i] && crosshairs[i] != notexture)
            {
                f->printf("crosshairload %s %d\n", escapestring(crosshairs[i]->name), i);
            }
        }
        f->printf("\n");
    }

    static void drawpointerquad(float x, float y, float size, vec color, Texture* texture)
    {
        if (texture)
        {
            if (texture->type & Texture::ALPHA)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glBlendFunc(GL_ONE, GL_ONE);
            }
            hudshader->set();
            gle::color(color);
            setusedtexture(texture);
            hudquad(x, y, size, size);
        }
    }

    static void drawcursor(int w, int h, float x, float y, float size)
    {
        static Texture* pointer = NULL;
        if (!pointer) pointer = textureload(getdefaultpointer(Pointer_Cursor), 3);
        UI::getcursorpos(x, y);
        x *= w;
        y *= h;
        drawpointerquad(x, y, size, vec(1, 1, 1), pointer);
    }

    VARP(crosshairally, 0, 1, 1);
    VARP(crosshairscope, 0, 1, 1);
    VARP(crosshairhit, 0, 300, 1000);

    int selectcrosshair(vec& color, int type)
    {
        gameent* d = hudplayer();
        if (d->state == CS_SPECTATOR || d->state == CS_DEAD || intermission)
        {
            return Pointer_Null;
        }

        int crosshair = Pointer_Default;

        if (type == Pointer_Crosshair)
        {
            if (d->state != CS_ALIVE && d->state != CS_LAGGED)
            {
                return crosshair;
            }

            crosshair = Pointer_Crosshair;

            if (crosshairscope)
            {
                if (iszoomedin() && checkzoom() == Zoom_Scope)
                {
                    crosshair = Pointer_Scope;
                    color = vec(1, 0, 0);
                }
            }
            if (!betweenrounds && crosshairally)
            {
                dynent* o = intersectclosest(d->o, worldpos, d);
                if (o && o->type == ENT_PLAYER && isally(((gameent*)o), d))
                {
                    crosshair = Pointer_Ally;
                    color = vec::hexcolor(teamtextcolor[d->team]);
                }
            }
            if (d->gunwait) color.mul(0.5f);
        }
        else if (type == Pointer_Hit)
        {
            crosshair = type;
        }
        if (!crosshairfx || !crosshaircolors || type == Pointer_Hit)
        {
            color = vec(1, 1, 1);
        }
        if (type == Pointer_Crosshair && crosshair != Pointer_Crosshair && !crosshairfx)
        {
            crosshair = Pointer_Crosshair;
        }
        return crosshair;
    }

    bool isvalidpointer(Texture*& texture, vec& color, int type = Pointer_Crosshair)
    {
        int index = selectcrosshair(color, type);
        if (texture != crosshairs[index])
        {
            if (index < 0)
            {
                return false;
            }
            texture = crosshairs[index];
            if (!texture)
            {
                loadcrosshair(NULL, index);
                texture = crosshairs[index];
            }
        }
        return true;
    }

    const float center = 0.5f;

    inline float getpointercenter(float dimension, float size)
    {
        return center * dimension - size / 2.0f;
    }

    static void drawcrosshair(int w, int h, float x, float y, float size)
    {
        vec color(1, 1, 1);
        Texture* crosshair = NULL;
        if (isvalidpointer(crosshair, color))
        {
            x = getpointercenter(w, size);
            y = getpointercenter(h, size);
            drawpointerquad(x, y, size, color, crosshair);
        }
        if (!crosshairfx || !crosshairhit)
        {
            return;
        }

        static Texture* hit = NULL;
        if (!hit)
        {
            if (!isvalidpointer(hit, color, Pointer_Hit))
            {
                return;
            }
        }
        else
        {
            color = vec(1, 1, 1);
        }

        gameent* hud = followingplayer(self);
        if (hud->lasthit && lastmillis - hud->lasthit <= crosshairhit)
        {
            float progress = min((lastmillis - hud->lasthit) / static_cast<float>(crosshairhit), 1.0f);
            size *= sin(progress * PI) * 1.5f;
            x = getpointercenter(w, size);
            y = getpointercenter(h, size);
            drawpointerquad(x, y, size, color, hit);
        }
    }

    void drawpointers(int w, int h)
    {
        bool hascursor = UI::hascursor();
        if ((hascursor && !cursorsize) || (!hascursor && (!showhud || mainmenu || !crosshairsize)))
        {
            return;
        }
        float scale = 3 * UI::uiscale;
        if (hascursor)
        {
            float size = cursorsize * scale;
            drawcursor(w, h, center, center, size);
        }
        else
        {
            float x = 0, y = 0, size = crosshairsize * scale;
            drawcrosshair(w, h, x, y, size);
        }
    }

    bool hasminimap()
    {
        return m_ctf;
    }

    VARP(radarminscale, 0, 384, 10000);
    VARP(radarmaxscale, 1, 1024, 10000);
    VARP(radarteammates, 0, 1, 1);
    FVARP(minimapalpha, 0, 1, 1);

    float calcradarscale()
    {
        return clamp(max(minimapradius.x, minimapradius.y) / 3, float(radarminscale), float(radarmaxscale));
    }
    ICOMMAND(calcradarscale, "", (), floatret(calcradarscale()));

    void drawminimap(gameent* d, float x, float y, float s)
    {
        vec pos = vec(d->o).sub(minimapcenter).mul(minimapscale).add(0.5f), dir;
        vecfromyawpitch(camera1->yaw, 0, 1, 0, dir);
        float scale = calcradarscale();
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_FAN);
        loopi(16)
        {
            vec v = vec(0, -1, 0).rotate_around_z(i / 16.0f * 2 * M_PI);
            gle::attribf(x + 0.5f * s * (1.0f + v.x), y + 0.5f * s * (1.0f + v.y));
            vec tc = vec(dir).rotate_around_z(i / 16.0f * 2 * M_PI);
            gle::attribf(1.0f - (pos.x + tc.x * scale * minimapscale.x), pos.y + tc.y * scale * minimapscale.y);
        }
        gle::end();
    }

    void setradartex()
    {
        settexture("data/interface/radar/radar.png", 3);
    }

    void drawradar(float x, float y, float s)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y);   gle::attribf(0, 0);
        gle::attribf(x + s, y);   gle::attribf(1, 0);
        gle::attribf(x, y + s); gle::attribf(0, 1);
        gle::attribf(x + s, y + s); gle::attribf(1, 1);
        gle::end();
    }

    void drawteammate(gameent* d, float x, float y, float s, gameent* o, float scale, float blipsize = 1)
    {
        vec dir = d->o;
        dir.sub(o->o).div(scale);
        float dist = dir.magnitude2(), maxdist = 1 - 0.05f - 0.05f;
        if (dist >= maxdist) dir.mul(maxdist / dist);
        dir.rotate_around_z(-camera1->yaw * RAD);
        float bs = 0.06f * blipsize * s,
            bx = x + s * 0.5f * (1.0f + dir.x),
            by = y + s * 0.5f * (1.0f + dir.y);
        vec v(-0.5f, -0.5f, 0);
        v.rotate_around_z((90 + o->yaw - camera1->yaw) * RAD);
        gle::attribf(bx + bs * v.x, by + bs * v.y); gle::attribf(0, 0);
        gle::attribf(bx + bs * v.y, by - bs * v.x); gle::attribf(1, 0);
        gle::attribf(bx - bs * v.x, by - bs * v.y); gle::attribf(1, 1);
        gle::attribf(bx - bs * v.y, by + bs * v.x); gle::attribf(0, 1);
    }

    void setbliptex(int team, const char* type)
    {
        defformatstring(blipname, "data/interface/radar/blip%s%s.png", teamblipcolor[validteam(team) ? team : 0], type);
        settexture(blipname, 3);
    }

    void drawplayerblip(gameent* d, float x, float y, float s, float blipsize = 1)
    {
        if (d->state != CS_ALIVE && d->state != CS_DEAD) return;
        float scale = calcradarscale();
        setbliptex(d->team, d->state == CS_DEAD ? "_dead" : "_alive");
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_QUADS);
        drawteammate(d, x, y, s, d, scale, blipsize);
        gle::end();
    }

    void drawteammates(gameent* d, float x, float y, float s)
    {
        if (!radarteammates) return;
        float scale = calcradarscale();
        int alive = 0, dead = 0;
        loopv(players)
        {
            gameent* o = players[i];
            if (o != d && o->state == CS_ALIVE && o->team == d->team)
            {
                if (!alive++)
                {
                    setbliptex(d->team, "_alive");
                    gle::defvertex(2);
                    gle::deftexcoord0();
                    gle::begin(GL_QUADS);
                }
                drawteammate(d, x, y, s, o, scale);
            }
        }
        if (alive) gle::end();
        loopv(players)
        {
            gameent* o = players[i];
            if (o != d && o->state == CS_DEAD && o->team == d->team)
            {
                if (!dead++)
                {
                    setbliptex(d->team, "_dead");
                    gle::defvertex(2);
                    gle::deftexcoord0();
                    gle::begin(GL_QUADS);
                }
                drawteammate(d, x, y, s, o, scale);
            }
        }
        if (dead) gle::end();
    }
}
