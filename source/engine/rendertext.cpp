#include "unicode.h"
#include "engine.h"

// just a wrapper for `SDL_Surface`, used for the glyph cache
struct glyph
{
    SDL_Surface *surf;
    glyph() : surf(NULL) {};
    ~glyph() { if(surf) SDL_FreeSurface(surf); }
};

// a cache for the bitmaps of characters used in the console
static hashtable<uint, glyph> glyph_cache;
void clearglyphcache()
{
    glyph_cache.clear();
}

static hashnameset<font> fonts;
static vector<font *> fontstack;
static int faceid = 0; // used by UI for change detection, unique for each font face
int lastfontreset = 0;

font *curfont = NULL, *lastfont = NULL;
const matrix4x3 *textmatrix = NULL; // used for text particles
Shader *textshader = NULL;          // used for text particles

font *findfont(const char *name)
{
    return fonts.access(name);
}

// the actual ttf file is loaded by `loadfontface()` right before rendering text
bool setfont(font *f, const char *script)
{
    if(!f) return false;
    // no script specified, use the default face
    if(!script || !script[0])
    {
        if(f->face->id != f->default_face.id)
        {
            if(f->face->face)
            {
                if(f->openface)
                {
                    TTF_CloseFont(f->openface->face);
                    f->openface->face = NULL;
                }
                f->openface = f->face;
            }
            f->face = &f->default_face;
        }
    }
    // script specified: look for a suitable face or fall back to the default one
    else if(strcmp(f->face->name, script))
    {
        f->face = f->faces.access(script);
        if(f->face && f->face != f->openface)
        {
            if(f->openface)
            {
                // close the previously opened face
                TTF_CloseFont(f->openface->face);
                f->openface->face = NULL;
            }
            f->openface = f->face;
        }
        else if(!f->face) f->face = &f->default_face;
    }
    curfont = f;
    return true;
}
bool setfont(const char *name, const char *script)
{
    font *f = fonts.access(name);
    return setfont(f, script);
}

void pushfont()
{
    fontstack.add(curfont);
}

bool popfont()
{
    if(fontstack.empty()) return false;
    curfont = fontstack.pop();
    return true;
}

bool init_ttf()
{
    if(TTF_Init() < 0) return false;
    const SDL_version *v_ttf = TTF_Linked_Version();
    int v_ft_M, v_ft_m, v_ft_p, v_hb_M, v_hb_m, v_hb_p;
    TTF_GetFreeTypeVersion(&v_ft_M, &v_ft_m, &v_ft_p);
    TTF_GetHarfBuzzVersion(&v_hb_M, &v_hb_m, &v_hb_p);
    conoutf(CON_INIT, "Text rendering: SDL_ttf %d.%d.%d, FreeType %d.%d.%d, HarfBuzz %d.%d.%d",
        v_ttf->major, v_ttf->minor, v_ttf->patch,
        v_ft_M, v_ft_m, v_ft_p,
        v_hb_M, v_hb_m, v_hb_p
    );
    return true;
}

void newfont(const char *name, const char *default_face_filename)
{
    font *f = &fonts[name];
    if(!f->name) f->name = newstring(name);
    f->pts = 0; // always call `setfontsize()` before drawing text

    // load the default font face
    fontface face;
    copystring(face.name, "Latn", 5);
    copystring(face.ttf_filename, default_face_filename);
    face.rtl = false;
    face.face = TTF_OpenFontDPI(findfile(default_face_filename, "rb"), f->pts, 54, 54);
    if(!face.face)
    {
        conoutf("could not load font: %s", default_face_filename);
        return;
    }
    TTF_SetFontScriptName(face.face, "Latn");
    TTF_SetFontDirection(face.face, TTF_DIRECTION_LTR);
    TTF_SetFontWrappedAlign(face.face, TTF_WRAPPED_ALIGN_LEFT);
    face.id = faceid++;

    f->default_face = face;
    f->face = &f->default_face;
    lastfont = f;
}
COMMANDN(font, newfont, "ss");

// adds a font face to the last font, to be used for a specific writing script
void addfontface(const char *script, int *rtl, const char *filename)
{
    font *f = lastfont;
    if(!f) return;
    fontface *face = &f->faces[script];
    copystring(face->name, script, 5);
    copystring(face->ttf_filename, filename);
    face->rtl = *rtl;
    face->id = faceid++;
}
COMMANDN(fontface, addfontface, "sis");

void closefont(font *f)
{
    if(!f) return;
    TTF_CloseFont(f->default_face.face);
    f->default_face.face = NULL;
    enumerate(f->faces, fontface, face, {
        TTF_CloseFont(face.face);
        face.face = NULL;
    });
}

// applies a black shadow to console text to improve visibility, the value controls the intensity of the shadow
VARP(conshadow, 0, 255, 255);

void setfontsize(int pts)
{
    if(curfont->pts == pts) return;
    curfont->pts = pts;
    if(curfont->face->face) TTF_SetFontSizeDPI(curfont->face->face, pts, 54, 54);
}

// loads the chosen font face from disk
static inline bool loadfontface()
{
    if(!curfont->face->face)
    {
        curfont->face->face = TTF_OpenFontDPI(findfile(curfont->face->ttf_filename, "rb"), curfont->pts, 54, 54);
        if(!curfont->face->face)
        {
            logoutf("could not load font face: %s", curfont->face->ttf_filename);
            return false;
        }
        TTF_SetFontScriptName(curfont->face->face, curfont->face->name[0] ? curfont->face->name : "Latn");
        TTF_SetFontDirection(curfont->face->face, curfont->face->rtl ? TTF_DIRECTION_RTL : TTF_DIRECTION_LTR);
        TTF_SetFontWrappedAlign(curfont->face->face, curfont->face->rtl ? TTF_WRAPPED_ALIGN_RIGHT : TTF_WRAPPED_ALIGN_LEFT);
    }
    return true;
}

static inline bvec get_text_color(char c, bvec def)
{
    switch(c)
    {
        case '0': return bvec( 64, 255, 128); // green: player talk
        case '1': return bvec( 96, 160, 255); // blue: "echo" command
        case '2': return bvec(255, 192,  64); // yellow: gameplay messages
        case '3': return bvec(255,  64,  64); // red: important errors
        case '4': return bvec(128, 128, 128); // gray
        case '5': return bvec(192,  64, 192); // magenta
        case '6': return bvec(255, 128,   0); // orange
        case '7': return bvec(255, 255, 255); // white
        case '8': return bvec(  0, 255, 255); // cyan
        case '9': return bvec(255, 192, 203); // pink
    }
    return def; // provided color: everything else
}
//stack[sp] is current color index
static inline bvec text_color(char c, char *stack, int size, int &sp, bvec color)
{
    if(c=='s') // save color
    {
        c = stack[sp];
        if(sp<size-1) stack[++sp] = c;
    }
    else
    {
        if(c=='r') { if(sp > 0) --sp; c = stack[sp]; } // restore color
        else stack[sp] = c;
        return get_text_color(c, color);
    }
    return color;
}

/////
/// UI
/////

// renders a string of text to a texture to be cached and drawn later, and measures its dimensions
void text_prepare(const char *str, textinfo &info, int wrap, bool outline)
{
    if(!loadfontface()) { info = {0, 0, 0}; return; }
    if(outline) TTF_SetFontOutline(curfont->face->face, 1);
    SDL_Surface* surf;
    if(wrap > 0) surf = TTF_RenderUTF8_Blended_Wrapped(curfont->face->face, str, {255, 255, 255}, wrap);
    else         surf = TTF_RenderUTF8_Blended(curfont->face->face, str, {255, 255, 255});
    if(outline) TTF_SetFontOutline(curfont->face->face, 0);
    if(!surf) { logoutf("failed to render text: %s", str); info = {0, 0, 0}; return; }
    GLuint tex;
    glGenTextures(1, &tex);
    if(!tex) { logoutf("failed to render text: %s", str); info = {0, 0, 0}; SDL_FreeSurface(surf); return; }
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_COMPRESSED_RGBA, surf->pitch/4, surf->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, surf->pixels);
    info = {tex, surf->w, surf->h};
    SDL_FreeSurface(surf);
    return;
}

// a version of the function above that supports colored spans but doesn't support newlines or line wrapping
void text_prepare_colored(const char *str, textinfo &info, bvec initial_color, uchar a, bool outline)
{
    if(!loadfontface()) { info = {0, 0, 0}; return; }

    int totalw = 0, totalh = 0;
    bvec tcolor = initial_color;
    char colorstack[10];
    colorstack[0] = 'c';
    int cpos = 0;

    vector<SDL_Surface *> surfs;
    char *span_begin = (char*)str;
    int span_len = 0;

    // split the string into colored spans
    for(char *p = (char *)str; *p; ++p)
    {
        if(*p == '\f')
        {
            if(*(p+1))
            {
                if(span_len)
                {
                    *p = '\0';
                    if(outline) TTF_SetFontOutline(curfont->face->face, 1);
                    SDL_Surface *surf = TTF_RenderUTF8_Blended(curfont->face->face, span_begin, {tcolor.r, tcolor.g, tcolor.b, a});
                    if(outline) TTF_SetFontOutline(curfont->face->face, 0);
                    if(!surf) { logoutf("failed to render text: %s", str); info = {0, 0, 0}; loopv(surfs) SDL_FreeSurface(surfs[i]); return; }
                    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
                    surfs.add(surf);
                    totalw += surf->w;
                    totalh = max(totalh, surf->h);
                    *p = '\f';
                }
                tcolor = text_color(*(p+1), colorstack, sizeof(colorstack), cpos, initial_color);
                span_begin = ++p+1; span_len = 0;
            }
            continue;
        }
        span_len++;
    }
    if(span_len)
    {
        if(outline) TTF_SetFontOutline(curfont->face->face, 1);
        SDL_Surface *surf = TTF_RenderUTF8_Blended(curfont->face->face, span_begin, {tcolor.r, tcolor.g, tcolor.b, a});
        if(outline) TTF_SetFontOutline(curfont->face->face, 0);
        if(surf)
        {
            SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
            surfs.add(surf);
            totalw += surf->w;
            totalh = max(totalh, surf->h);
        }
    }

    if(!surfs.length()) { info = {0, 0, 0}; return; }

    // copy all the colored spans onto a single surface
    SDL_Surface *dest = surfs[0];
    if(surfs.length() > 1)
    {
        dest = SDL_CreateRGBSurfaceWithFormat(0, totalw, totalh, 32, SDL_PIXELFORMAT_ARGB8888);
        if(!dest) { logoutf("failed to render text: %s", str); info = {0, 0, 0}; loopv(surfs) SDL_FreeSurface(surfs[i]); return; }
        int x = 0;
        loopv(surfs)
        {
            SDL_Surface *surf = surfs[i];
            SDL_Rect r = {curfont->face->rtl ? (totalw - x - surf->w) : x, (totalh-surf->h)/2, 0, 0};
            SDL_BlitSurface(surf, NULL, dest, &r);
            x += surf->w;
            SDL_FreeSurface(surf);
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if(!tex) { logoutf("failed to render text: %s", str); info = {0, 0, 0}; SDL_FreeSurface(dest); return; }
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_COMPRESSED_RGBA, dest->pitch/4, dest->h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, dest->pixels);
    info = { tex, dest->w, dest->h };
    SDL_FreeSurface(dest);
}

// draws a string of text that has already been rendered to a texture
void draw_text(textinfo info, float left, float top, int r, int g, int b, int a)
{
    const int w = info.w, h = info.h;

    if(textshader) textshader->set(); // text particles
    else SETSHADER(hudrect);          // UI text

    gle::color(bvec(r, g, b), a);
    glBindTexture(GL_TEXTURE_RECTANGLE, info.tex);
    gle::defvertex(textmatrix ? 3 : 2);
    gle::deftexcoord0();

    // NOTE: `GL_TRIANGLE_STRIP` does not work with `textmatrix` so we have to use `GL_QUADS` instead
    gle::begin(textmatrix ? GL_QUADS : GL_TRIANGLE_STRIP);
    if(textmatrix) // text particle inside the world
    {
        gle::attrib(textmatrix->transform(vec2(left  , top  ))); gle::attribf(0, 0);
        gle::attrib(textmatrix->transform(vec2(left+w, top  ))); gle::attribf(w, 0);
        gle::attrib(textmatrix->transform(vec2(left+w, top+h))); gle::attribf(w, h);
        gle::attrib(textmatrix->transform(vec2(left  , top+h))); gle::attribf(0, h);
    }
    else // UI text
    {
        gle::attribf(left+w, top  ); gle::attribf(w, 0);
        gle::attribf(left  , top  ); gle::attribf(0, 0);
        gle::attribf(left+w, top+h); gle::attribf(w, h);
        gle::attribf(left  , top+h); gle::attribf(0, h);
    }
    gle::end();
    // NOTE: you must take care of deleting the texture `info.tex` manually!
}
// a version of the function above that can be called directly without preparing the text beforehand
void draw_text(const char *str, float left, float top, int r, int g, int b, int a)
{
    textinfo info;
    text_prepare(str, info, 0);
    if(!info.tex) return;
    draw_text(info, left, top, r, g, b, a);
    glDeleteTextures(1, &info.tex);
}
void draw_textf(const char *fstr, float left, float top, ...)
{
    defvformatstring(str, top, fstr);
    draw_text(str, left, top);
}

/////
/// CONSOLE
/////

// ensures that only supported characters end up in the console
static inline uint uni_confilter(uint c)
{
    if(iscubeprint(c) || iscubespace(c)) return c;
    return 0xFFFD;
}

// retrieves a glyph bitmap from the glyph cache
static inline SDL_Surface *fetch_glyph(uint codepoint)
{
    codepoint = uni_confilter(codepoint);
    // return the cached surface if available
    glyph *g = &glyph_cache[codepoint];
    if(g->surf) return g->surf;

    // draw the character in white, or the replacement character (0xFFFD) if the requested glyph is not available
    SDL_Surface *surf = TTF_RenderGlyph32_Blended(curfont->face->face, codepoint, {255, 255, 255});
    if(!surf)    surf = TTF_RenderGlyph32_Blended(curfont->face->face, 0xFFFD, {255, 255, 255});
    if(!surf) return NULL;
    
    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
    g->surf = surf;
    return g->surf;
}
static inline void measure_glyph(uint codepoint, int &width, int &height) {
    SDL_Surface *surf = fetch_glyph(codepoint);
    if(!surf)
    {
        width = 0;
        height = FONTH;
        return;
    }
    width = surf->w;
    height = surf->h;
}

// computes the dimensions of a line of console text and splits it into colored spans
void text_prepare_console(const char *str, int &width, int &height, vector<conspan> &spans, int maxwidth, int cursor, int *curx, int *cury)
{
    int x = 0, y = FONTH, maxw = 0;
    bvec color(255, 255, 255), tcolor = color;
    if(curx) *curx = -1;

    char colorstack[10];
    colorstack[0] = 'c';
    int cpos = 0;

    conspan span(str, tcolor);

    uint c;
    int s = uni_getchar(str, c);
    for(char *p = (char *)str; c; p += s, s = uni_getchar(p, c))
    {
        if(cursor == p - str) // check if the cursor is somewhere in the middle of the string
        {
            if(curx) *curx = x;
            if(cury) *cury = y - FONTH;
        }
        if(*p == '\f') // color code
        {
            if(*(p+1))
            {
                if(span.len) spans.add(span);
                tcolor = text_color(*(p+1), colorstack, sizeof(colorstack), cpos, color);
                span = conspan(++p+1, tcolor);
                span.x = x; span.y = y - FONTH;
            }
            continue;
        }
        if(*p == '\r' || *p == '\n') // new line
        {
            if(span.len) spans.add(span);
            span = conspan(p+1, tcolor);
            span.y = y;
            if(x > maxw) maxw = x;
            x = 0; y += FONTH;
            continue;
        }
        // check if we are exceeding the maximum width
        int w, h;
        measure_glyph(c, w, h);
        if(maxwidth > 0 && x + w > maxwidth)
        {
            if(span.len) spans.add(span);
            span = conspan(p, tcolor);
            span.y = y; span.w = w; span.len = s;
            if(x > maxw) maxw = x;
            x = w; y += FONTH;
            continue;
        }
        // regular character: append to the span
        x += w;
        if(x > maxw) maxw = x;
        span.w += w; span.len += s;
    }
    if(span.len) spans.add(span);
    width = maxw; height = y;

    // check if the cursor is at the end of the string
    if(curx && cursor >= 0 && *curx < 0)
    {
        *curx = x;
        if(cury) *cury = y - FONTH;
    }
}

// like the function above, but doesn't deal with spans and cursor position
void text_bounds_console(const char *str, int &width, int &height, int maxwidth)
{
    int x = 0, y = FONTH, maxw = 0;

    uint c;
    int s = uni_getchar(str, c);
    for(char *p = (char *)str; c; p += s, s = uni_getchar(p, c))
    {
        if(*p == '\f') // color code
        {
            if(*(p+1)) { p++; }; continue;
        }
        if(*p == '\r' || *p == '\n') // new line
        {
            if(x > maxw) maxw = x;
            x = 0; y += FONTH;
            continue;
        }
        // check if we are exceeding the maximum width
        int w, h;
        measure_glyph(c, w, h);
        if(maxwidth > 0 && x + w > maxwidth)
        {
            if(x > maxw) maxw = x;
            x = w; y += FONTH;
            continue;
        }
        // regular character
        x += w;
        if(x > maxw) maxw = x;
    }
    width = maxw; height = y;
}

// draws a surface that contains text to the screen
static inline void draw_surface(SDL_Surface *surf, int x, int y, bvec color)
{
    const int w = surf->w, h = surf->h;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if(!tex)
    {
        logoutf("failed to render console text");
        return;
    }
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, surf->pitch/4, surf->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, surf->pixels);
    gle::defvertex(2);
    gle::deftexcoord0();
    if(conshadow)
    {
        const float d = 3.f/4.f * conscale;
        gle::color(bvec(0, 0, 0), conshadow);
        gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(x+w-d, y+d  ); gle::attribf(w, 0);
            gle::attribf(x-d  , y+d  ); gle::attribf(0, 0);
            gle::attribf(x+w-d, y+h+d); gle::attribf(w, h);
            gle::attribf(x-d  , y+h+d); gle::attribf(0, h);
        gle::end();
    }
    gle::color(color);
    gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y  ); gle::attribf(w, 0);
        gle::attribf(x  , y  ); gle::attribf(0, 0);
        gle::attribf(x+w, y+h); gle::attribf(w, h);
        gle::attribf(x  , y+h); gle::attribf(0, h);
    gle::end();
    glDeleteTextures(1, &tex);
}

// copies (blits) a single glyph onto a surface
static inline void copy_glyph_to_surface(uint codepoint, int x, int y, SDL_Surface *surf)
{
    SDL_Surface *g = fetch_glyph(codepoint);
    if(!g) return;

    SDL_Rect r = {x, y, 0, 0};
    if(SDL_BlitSurface(g, NULL, surf, &r) != 0)
    {
        logoutf("Blit failed: %c", codepoint);
    }
}

// draws a single glyph, used for the cursor
static inline void draw_glyph_console(uint codepoint, int x, int y, bvec color)
{
    SDL_Surface *g = fetch_glyph(codepoint);
    if(g) draw_surface(g, x, y, color);
}

// draws a line of console text that has already been split into spans
void draw_text_console(vector<conspan> spans, float left, float top, int curx, int cury)
{
    SETSHADER(hudrect);
    loopv(spans)
    {
        float x = left + spans[i].x, y = top + spans[i].y;
        SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, spans[i].w, FONTH, 32, SDL_PIXELFORMAT_ABGR8888);
        if(!surf) return;
        int sx = 0;
        uint c;
        int s = uni_getchar(spans[i].begin, c);
        for(char *p = (char *)spans[i].begin; (p - spans[i].begin) < spans[i].len; p += s, s = uni_getchar(p, c))
        {
            int w, h;
            measure_glyph(c, w, h);
            copy_glyph_to_surface(c, sx, 0, surf);
            sx += w;
        }
        draw_surface(surf, x, y, spans[i].color);
        SDL_FreeSurface(surf);
    }
    // cursor
    if(curx >= 0 && (totalmillis/350)&1) draw_glyph_console('_', left + curx, top + cury, bvec(255, 255, 255));
}

void gettextres(int &w, int &h)
{
    if(w < MINRESW || h < MINRESH)
    {
        if(MINRESW > w*MINRESH/h)
        {
            h = h*MINRESW/w;
            w = MINRESW;
        }
        else
        {
            w = w*MINRESH/h;
            h = MINRESH;
        }
    }
}

// used by the text editor
int text_visible(const char *str, float hitx, float hity, int maxwidth)
{
    int x = 0, y = 0;
    char *p = (char *)str;
    uint c;
    int s = uni_getchar(str, c);
    for(; c; p += s, s = uni_getchar(p, c))
    {
        if(*p == '\f')
        {
            if(*(p+1)) ++p;
            continue;
        }
        if(*p == '\r' || *p == '\n')
        {
            if(y + FONTH > hity) return (p - str);
            x = 0; y += FONTH;
            continue;
        }
        int w, h;
        measure_glyph(c, w, h);
        if(maxwidth > 0 && x + w > maxwidth)
        {
            x = w; y += FONTH;
        }
        else x += w;
        if(y + FONTH > hity && x >= hitx) return (p - str);
    }
    return (p - str);
}

// used by the text editor
void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth)
{
    cx = cy = 0;
    if(cursor <= 0) return;
    int x = 0, y = 0;
    uint c;
    int s = uni_getchar(str, c);
    for(char *p = (char *)str; (p - str) < cursor; p += s, s = uni_getchar(p, c))
    {
        if(!*p) break;
        if(*p == '\f')
        {
            if(*(p+1)) p++;
            continue;
        }
        if(*p == '\r' || *p == '\n')
        {
            x = 0; y += FONTH;
        }
        else
        {
            int w, h;
            measure_glyph(c, w, h);
            if(maxwidth > 0 && x + w > maxwidth)
            {
                x = w; y += FONTH;
            }
            else x += w;
        }
        if(cursor == (p - str))
        {
            cx = x; cy = y;
            return;
        }
    }
    cx = x; cy = y;
}

void reloadfonts() { UI::cleartext(); }