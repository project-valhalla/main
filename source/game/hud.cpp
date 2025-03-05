#include "engine.h"
#include "game.h"

namespace game
{
    float clipconsole(const float w, const float h)
    {
        if (cmode)
        {
            return cmode->clipconsole(w, h);
        }
        return 0;
    }

    VARNP(damagecompass, usedamagecompass, 0, 1, 1);
    VARP(damagecompassfade, 1, 1000, 10000);
    VARP(damagecompasssize, 1, 30, 100);
    VARP(damagecompassalpha, 1, 25, 100);
    VARP(damagecompassmin, 1, 25, 1000);
    VARP(damagecompassmax, 1, 200, 1000);

    float damagedirs[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    void damagecompass(const int amount, const vec& loc)
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
        damagedirs[dir] += max(amount, damagecompassmin) / float(damagecompassmax);
        if (damagedirs[dir] > 1) damagedirs[dir] = 1;

    }

    static void drawdamagecompass(const int w, const int h)
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
    VARP(damagescreenfactor, 1, 11, 100);
    VARP(damagescreenalpha, 1, 60, 100);
    VARP(damagescreenfade, 0, 500, 1000);
    VARP(damagescreenmin, 1, 15, 1000);
    VARP(damagescreenmax, 1, 100, 1000);

    void damageblend(const int amount, const int factor)
    {
        if (!damagescreen) return;
        if (lastmillis > damageblendmillis) damageblendmillis = lastmillis;
        damageblendmillis += clamp(amount, damagescreenmin, damagescreenmax) * (factor ? factor : damagescreenfactor);
    }

    static void drawdamagescreen(const int w, const int h)
    {
        if (lastmillis >= damageblendmillis)
        {
            return;
        }

        hudshader->set();
        static Texture* damagetex = NULL;
        if (!damagetex)
        {
            damagetex = textureload("data/interface/hud/damage_gradient.png", 3);
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
            if (validsound(hud->chan[Chan_LowHealth]))
            {
                hud->stopchannelsound(Chan_LowHealth, 400);
            }
            return;
        }

        hud->playchannelsound(Chan_LowHealth, S_LOW_HEALTH, 200, true);
        if (!lastheartbeat || lastmillis - lastheartbeat >= lowhealthscreenmillis)
        {
            damageblend(lowhealthscreenamount, lowhealthscreenfactor);
            lastheartbeat = lastmillis;
        }
    }

    struct Splatter
    {
        int amount, millis;
        float x, y;
        vec color;

        Splatter() : amount(0), millis(0), x(0), y(0), color(0, 0, 0)
        {
        }
        ~Splatter()
        {
        }

        void setcolor(const int hex)
        {
            color = vec::hexcolor(hex);
            color.x = 1.0f - color.x;
            color.y = 1.0f - color.y;
            color.z = 1.0f - color.z;
        }

        void setaxes()
        {
            x = rnd(hudw) - hudw / 2;
            y = rnd(hudh) - hudh / 2;
        }
    };
    vector<Splatter> splatters;

    VARP(splatterscreen, 0, 1, 1);
    VARP(splatterscreenmin, 1, 50, 1000);
    VARP(splatterscreenmax, 1, 250, 1000);
    VARP(splatterscreenfactor, 1, 100, 1000);
    VARP(splatterscreenfade, 0, 1000, 1000);
    VARP(splatterscreenalpha, 1, 80, 100);

    void addbloodsplatter(const int amount, const int color)
    {
        if (!splatterscreen)
        {
            return;
        }

        Splatter& splatter = splatters.add();
        splatter.amount = amount;
        splatter.millis = lastmillis + (clamp(amount, splatterscreenmin, splatterscreenmax) * splatterscreenfactor);
        splatter.setcolor(color);
        splatter.setaxes();
    }

    void drawsplatter(const int w, const int h, Splatter* splatter)
    {
        hudshader->set();
        static Texture* splattertex = NULL;
        if (!splattertex)
        {
            splattertex = textureload("<grey>data/interface/hud/splat.png", 3);
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        setusedtexture(splattertex);
        float fade = splatterscreenalpha / 100.0f;
        if (splatter->millis - lastmillis < splatterscreenfade)
        {
            fade *= float(splatter->millis - lastmillis) / splatterscreenfade;
        }
        gle::colorf(splatter->color.x, splatter->color.y, splatter->color.z, fade);
        hudquad(splatter->x, splatter->y, w, h);
    }

    void drawsplatters(const int w, const int h)
    {
        if (!splatters.length())
        {
            return;
        }
        loopv(splatters)
        {
            Splatter& splatter = splatters[i];
            if (lastmillis >= splatter.millis)
            {
                splatters.remove(i--);
            }
            else
            {
                drawsplatter(w, h, &splatter);
            }
        }
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

    void addscreenflash(const int amount)
    {
        if (!screenflash) return;
        if (lastmillis > screenflashmillis) screenflashmillis = lastmillis;
        screenflashmillis += clamp(amount, screenflashmin, screenflashmax) * screenflashfactor;
    }

    void drawscreenflash(const int w, const int h)
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

    void drawblend(const int x, const int y, const int w, const int h, const float r, const float g, const float b, const float a = 1.0f)
    {
        gle::colorf(r, g, b, a);
        gle::defvertex(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y);
        gle::attribf(x + w, y);
        gle::attribf(x, y + h);
        gle::attribf(x + w, y + h);
        gle::end();
    }

    static void drawzoom(const int w, const int h)
    {
        if (!camera::camera.zoomstate.isinprogress())
        {
            return;
        }

        const int zoomtype = weapon::checkweaponzoom();
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
        float alpha = camera::camera.zoomstate.progress;
        if (zoomtype == Zoom_Scope)
        {
            float x = 0, y = 0, dimension = 0;
            if (w > h)
            {
                dimension = h;
                x += (w - h) / 2;
                drawblend(0, 0, x, dimension, 0, 0, 0, alpha);
                drawblend(x + dimension, 0, x + 1, dimension, 0, 0, 0, alpha);
            }
            else if (h > w)
            {
                dimension = w;
                y += (h - w) / 2;
                drawblend(0, 0, dimension, y, 0, 0, 0, alpha);
                drawblend(0, y + dimension, dimension, y, 0, 0, 0, alpha);
            }
            else dimension = h;
            gle::colorf(1, 1, 1, alpha);
            drawquad(x, y, dimension, dimension, 0, 0, 1, 1, false, false);
        }
        else
        {
            gle::colorf(1, 1, 1, alpha);
            hudquad(0, 0, w, h);
        }
    }

    FVARP(damagerolldiv, 0, 4.0f, 10);

    void setdamagehud(const int damage, gameent* d, gameent* actor)
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
        camera::camera.zoomstate.disable();
        splatters.shrink(0);
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
            drawsplatters(w, h);
        }
    }

    SVAR(lasthudpickupinfo, "");

    void checkentity(int type)
    {
        switch (type)
        {
            case I_HEALTH:
            case I_MEGAHEALTH:
            case I_ULTRAHEALTH:
            {
                const int fade = lastmillis + damagescreenfade;
                if (damageblendmillis > fade)
                {
                    damageblendmillis = fade; // Force damage screen to fade out.
                }
                break;
            }
        }
        if (type == TRIGGER)
        {
            return;
        }

        string pickupinfo;
        if (validitem(type))
        {
            itemstat& is = itemstats[type - I_AMMO_SG];
            formatstring(pickupinfo, "+%d %s", is.add, is.name);
        }
        else
        {
            formatstring(pickupinfo, "%s", gentities[type].prettyname);
        }
        setsvar("lasthudpickupinfo", pickupinfo);
    }

    VARP(cursorsize, 0, 25, 40);
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

    static void drawpointerquad(const float x, const float y, const float size, const vec &color, Texture* texture)
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

    static void drawcursor(const int w, const int h, float x, float y, const float size)
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

    int selectcrosshair(vec& color, const int type)
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
                const int zoomtype = weapon::checkweaponzoom();
                if (camera::camera.zoomstate.isenabled() && zoomtype == Zoom_Scope)
                {
                    crosshair = Pointer_Scope;
                    color = vec(1, 0, 0);
                }
            }
            if (!betweenrounds && crosshairally)
            {
                const dynent* o = weapon::intersectclosest(d->o, worldpos, d);
                if (o && o->type == ENT_PLAYER && isally(((gameent*)o), d) && !m_hideallies)
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

    bool isvalidpointer(Texture*& texture, vec& color, const int type = Pointer_Crosshair)
    {
        int index = selectcrosshair(color, type);
        if (index < 0)
        {
            return false;
        }
        if (texture != crosshairs[index])
        {
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

    static inline void calculatecrosshairsize(float& size)
    {
        const float crouchprogress = self->eyeheight / self->maxheight;
        float zoomprogress = 1.0f;
        if (camera::camera.zoomstate.progress < 1)
        {
            zoomprogress = 1.0f - camera::camera.zoomstate.progress;
        }
        const float spawnprogress = clamp((lastmillis - self->lastspawn) / float(SPAWN_DURATION), 0.0f, 1.0f);
        size *= crouchprogress * zoomprogress * spawnprogress;
    }

    static void drawcrosshair(const int w, const int h, float x, float y, float size)
    {
        vec color(1, 1, 1);
        Texture* crosshair = NULL;
        if (isvalidpointer(crosshair, color))
        {
            calculatecrosshairsize(size);
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
            size *= sin(progress * M_PI) * 1.5f;
            x = getpointercenter(w, size);
            y = getpointercenter(h, size);
            drawpointerquad(x, y, size, color, hit);
        }
    }

    void drawpointers(const int w, const int h)
    {
        bool hascursor = UI::hascursor();
        if ((hascursor && !cursorsize) || (!hascursor && (!showhud || mainmenu || !crosshairsize)))
        {
            return;
        }
        if (hascursor)
        {
            const float scale = UI::uiscale * w / 900.0f;
            float size = cursorsize * scale;
            drawcursor(w, h, center, center, size);
        }
        else
        {
            float scale = 3 * UI::uiscale;
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

    void drawminimap(gameent* d, const float x, const float y, const float s)
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

    void drawradar(const float x, const float y, const float s)
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

    void drawteammate(gameent* d, const float x, const float y, const float s, gameent* o, const float scale, const float blipsize = 1)
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

    void setbliptex(const int team, const char* type)
    {
        defformatstring(blipname, "data/interface/radar/blip%s%s.png", teamblipcolor[validteam(team) ? team : 0], type);
        settexture(blipname, 3);
    }

    void drawplayerblip(gameent* d, const float x, const float y, float s, const float blipsize = 1)
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

    void drawteammates(gameent* d, const float x, const float y, const float s)
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
