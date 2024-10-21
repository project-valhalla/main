#include "unicode.h"
#include "engine.h"

#include <cairo.h>
#include <pango/pangocairo.h>
#include <fontconfig/fontconfig.h>

static int fontid = 0;              // used by UI for change detection
float fontsize = 0;                 // pixel height of the current font
const matrix4x3 *textmatrix = NULL; // used for text particles
Shader *textshader = NULL;          // used for text particles

static cairo_font_options_t *options = NULL;  // global font options
static cairo_surface_t *dummy_surface = NULL; // used to measure text

static cairo_antialias_t antialias_ = CAIRO_ANTIALIAS_GRAY;
static cairo_hint_style_t hintstyle_= CAIRO_HINT_STYLE_DEFAULT;
VARFP(fontantialias, 0, 1, 1,
{
    antialias_ = fontantialias ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE;
    if(options) cairo_font_options_set_antialias(options, antialias_);
    clearconsoletextures();
    reloadfonts();
});
VARFP(fonthinting, -1, -1, 3,
{
    switch(fonthinting)
    {
        case  0: hintstyle_ = CAIRO_HINT_STYLE_NONE   ; break;
        case  1: hintstyle_ = CAIRO_HINT_STYLE_SLIGHT ; break;
        case  2: hintstyle_ = CAIRO_HINT_STYLE_MEDIUM ; break;
        case  3: hintstyle_ = CAIRO_HINT_STYLE_FULL   ; break;
        default: hintstyle_ = CAIRO_HINT_STYLE_DEFAULT;
    }
    if(options) cairo_font_options_set_hint_style(options, hintstyle_);
    clearconsoletextures();
    reloadfonts();
})

VARFP(cursorblink, 0, 750, 2000, { cursorblink = cursorblink ? max(250, cursorblink) : 0; });
CVARP(cursorcolor, 0xFFFFFF);

// a cache for rendered text particles
struct partinfo
{
    textinfo ti;
    partinfo() { ti.tex = 0; }
};
static hashtable<uint, partinfo> particle_cache;
static vector<uint> particle_queue;
static void clear_text_particles()
{
    enumerate(particle_cache, partinfo, info, { if(info.ti.tex) glDeleteTextures(1, &info.ti.tex); });
    particle_cache.clear();
    particle_queue.setsize(0);
}

// register a TTF file
// NOTE: on Windows and MacOs, this assumes that pango was compiled with fontconfig support,
// and that the `PANGOCAIRO_BACKEND` env var is set to `fc` or `fontconfig`
void addfontfile(const char *filename)
{
    const char *found = findfile(filename, "rb");
    if(!found || !found[0]) return;
    FcConfigAppFontAddFile(FcConfigGetCurrent(), (const unsigned char *)found);
}
COMMANDN(registerfont, addfontfile, "s");

struct font
{
    char *name;
    int id;
    string features; // OpenType features
    float letter_spacing;
    PangoFontDescription *desc;

    font() : name(NULL), letter_spacing(0), desc(NULL) { features[0] = '\0'; };
    ~font()
    {
        DELETEA(name);
        if(desc) pango_font_description_free(desc);
    }
};
static font *curfont = NULL, *lastfont = NULL;
static hashnameset<font> fonts;
static vector<font *> fontstack;

void newfont(const char *name, const char *family)
{
    font *f = &fonts[name];
    if(!f->name) f->name = newstring(name);
    if(!f->desc) f->desc = pango_font_description_from_string(family);
    f->id = fontid++;

    lastfont = f;
}
COMMANDN(font, newfont, "ss");

void fontweight(int *weight)
{
    if(!lastfont || !lastfont->desc) return;
    PangoWeight w;
    switch(*weight)
    {
        case -2: w = PANGO_WEIGHT_ULTRALIGHT; break;
        case -1: w = PANGO_WEIGHT_LIGHT; break;
        case  1: w = PANGO_WEIGHT_MEDIUM; break;
        case  2: w = PANGO_WEIGHT_SEMIBOLD; break;
        case  3: w = PANGO_WEIGHT_BOLD; break;
        case  4: w = PANGO_WEIGHT_ULTRABOLD; break;
        case  5: w = PANGO_WEIGHT_HEAVY; break;
        default: w = *weight < 0 ? PANGO_WEIGHT_THIN : (*weight > 0 ? PANGO_WEIGHT_ULTRAHEAVY : PANGO_WEIGHT_NORMAL);
    }
    pango_font_description_set_weight(lastfont->desc, w);
}
COMMAND(fontweight, "i");

void fontstretch(int *stretch)
{
    if(!lastfont || !lastfont->desc) return;
    PangoStretch s;
    switch(*stretch)
    {
        case -3: s = PANGO_STRETCH_EXTRA_CONDENSED; break;
        case -2: s = PANGO_STRETCH_CONDENSED; break;
        case -1: s = PANGO_STRETCH_SEMI_CONDENSED; break;
        case  0: s = PANGO_STRETCH_NORMAL; break;
        case  1: s = PANGO_STRETCH_SEMI_EXPANDED; break;
        case  2: s = PANGO_STRETCH_EXPANDED; break;
        case  3: s = PANGO_STRETCH_EXTRA_EXPANDED; break;
        default: s = *stretch < 0 ? PANGO_STRETCH_ULTRA_CONDENSED : PANGO_STRETCH_ULTRA_EXPANDED;
    }
    pango_font_description_set_stretch(lastfont->desc, s);
}
COMMAND(fontstretch, "i");

// 0 = normal, 1 = oblique, 2 = italic
void fontstyle(int *style)
{
    if(!lastfont || !lastfont->desc) return;
    PangoStyle s;
    switch(*style)
    {
        case 1: s = PANGO_STYLE_OBLIQUE; break;
        case 2: s = PANGO_STYLE_ITALIC; break;
        default: s = PANGO_STYLE_NORMAL;
    }
    pango_font_description_set_style(lastfont->desc, s);
}
COMMAND(fontstyle, "i");

void fontsmallcaps(int *val)
{
    if(!lastfont || !lastfont->desc) return;
    pango_font_description_set_variant(lastfont->desc, *val ? PANGO_VARIANT_SMALL_CAPS : PANGO_VARIANT_NORMAL);
}
COMMAND(fontsmallcaps, "i");

void fontletterspacing(float *val) { if(lastfont) lastfont->letter_spacing = *val; }
COMMAND(fontletterspacing, "f");

void fontfeatures(char *features) { if(lastfont) copystring(lastfont->features, features, MAXSTRLEN); }
COMMAND(fontfeatures, "s");

void fontvariations(char *variations)
{
    if(!lastfont || !lastfont->desc) return;
    pango_font_description_set_variations(lastfont->desc, variations);
}
COMMAND(fontvariations, "s");

static inline bool setfont(font *f)
{
    if(!f) return false;
    curfont = f;
    return true;
}
bool setfont(const char *name)
{
    font *f = fonts.access(name);
    return setfont(f);
}
void pushfont() { fontstack.add(curfont); }
bool popfont()
{
    if(fontstack.empty()) return false;
    curfont = fontstack.pop();
    return true;
}
int getcurfontid() { return curfont->id; }

bool init_pangocairo()
{
    options = cairo_font_options_create();
    if(!options) return false;
    cairo_font_options_set_antialias(options, antialias_);
    cairo_font_options_set_hint_style(options, hintstyle_);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_color_mode(options, CAIRO_COLOR_MODE_COLOR); // cairo 1.18

    dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
    if(!dummy_surface) return false;

    defformatstring(msg, "Text rendering: Pango %s, Cairo %s", pango_version_string(), cairo_version_string());
    conoutf(CON_INIT, msg);
    return true;
}

void done_pangocairo()
{
    fonts.clear();
    if(options) cairo_font_options_destroy(options);
    if(dummy_surface) cairo_surface_destroy(dummy_surface);
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

#define MARKUP_CASE(x, X, pango_attr_func, PANGO_ATTR, name) \
    case x: \
        if(begin_##name < 0) begin_##name = j; \
        ++n_##name; \
        break; \
    case X: \
        if(begin_##name >= 0) \
        { \
            if(n_##name >= 0) --n_##name; \
            if(n_##name) break; \
            m = pango_attr_func(PANGO_ATTR); \
            m->start_index = begin_##name; \
            m->end_index = j; \
            pango_attr_list_insert(list, m); \
            begin_##name = -1; \
        } \
        break;

// adds a string to the layout parsing basic markup (\f codes)
// NOTE: `markup` is the original string with \f codes; `text` is the stripped version without \f codes
static inline void add_text_to_layout(const char *markup, int len, PangoLayout *layout, bvec initial_color, int *map_markup_to_text, int *map_text_to_markup, const char *language)
{
    char *text = newstring(len);

    bvec tcolor = initial_color;
    char colorstack[10];
    colorstack[0] = 'c';
    int cpos = 0;

    PangoAttrList *list = pango_attr_list_new();
    PangoAttribute *attr; // colors and global features
    PangoAttribute *m;    // markup

    // OpenType features
    if(curfont->features[0])
    {
        attr = pango_attr_font_features_new(curfont->features); // pango 1.38
        pango_attr_list_insert(list, attr);
    }

    // letter spacing
    if(curfont->letter_spacing != 0)
    {
        attr = pango_attr_letter_spacing_new(curfont->letter_spacing * PANGO_SCALE); // pango 1.6
        pango_attr_list_insert(list, attr);
    }

    // language
    if(language)
    {
        attr = pango_attr_language_new(pango_language_from_string(language));
        pango_attr_list_insert(list, attr);
    }

    // arguments are 16-bit values so colors must be multiplied by 257
    attr = pango_attr_foreground_new(tcolor.r * 257, tcolor.g * 257, tcolor.b * 257);
    if(attr) attr->start_index = 0;

    int begin_bold = -1, begin_italic = -1, begin_underline = -1, begin_strikethrough = -1, begin_overline = -1;
    int n_bold = 0, n_italic = 0, n_underline = 0, n_strikethrough = 0, n_overline = 0;

    // parse markup
    int i = 0, j = 0;
    for(; i < len; ++i)
    {
        if(markup[i] == '\f' && i < (len - 1))
        {
            switch(markup[i+1])
            {
                MARKUP_CASE('b', 'B', pango_attr_weight_new       , PANGO_WEIGHT_BOLD     , bold);
                MARKUP_CASE('i', 'I', pango_attr_style_new        , PANGO_STYLE_ITALIC    , italic);
                MARKUP_CASE('u', 'U', pango_attr_underline_new    , PANGO_UNDERLINE_SINGLE, underline);
                MARKUP_CASE('t', 'T', pango_attr_strikethrough_new, TRUE                  , strikethrough);
                MARKUP_CASE('o', 'O', pango_attr_overline_new     , PANGO_OVERLINE_SINGLE , overline);      // pango 1.46
                default:
                {
                    tcolor = text_color(markup[i+1], colorstack, sizeof(colorstack), cpos, initial_color);
                    if(attr)
                    {
                        attr->end_index = j + 1;
                        pango_attr_list_insert(list, attr);
                        attr = NULL;
                    }
                }
            }
            if(map_markup_to_text) map_markup_to_text[i] = j;
            ++i;
            continue;
        }
        if(!attr)
        {
            attr = pango_attr_foreground_new(tcolor.r * 257, tcolor.g * 257, tcolor.b * 257);
            if(attr) attr->start_index = j;
        }
        if(map_markup_to_text) map_markup_to_text[i] = j;
        if(map_text_to_markup) map_text_to_markup[j] = i;
        text[j++] = markup[i];
    }
    text[j] = '\0';
    if(map_markup_to_text) map_markup_to_text[i] = j;

    if(attr)
    {
        attr->end_index = j;
        pango_attr_list_insert(list, attr);
    }
    if(begin_bold >= 0)
    {
        m = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        m->start_index = begin_bold; m->end_index = j;
        pango_attr_list_insert(list, m);
    }
    if(begin_italic >= 0)
    {
        m = pango_attr_style_new(PANGO_STYLE_ITALIC);
        m->start_index = begin_italic; m->end_index = j;
        pango_attr_list_insert(list, m);
    }
    if(begin_underline >= 0)
    {
        m = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
        m->start_index = begin_underline; m->end_index = j;
        pango_attr_list_insert(list, m);
    }
    if(begin_strikethrough >= 0)
    {
        m = pango_attr_strikethrough_new(TRUE);
        m->start_index = begin_strikethrough; m->end_index = j;
        pango_attr_list_insert(list, m);
    }
    if(begin_overline >= 0)
    {
        m = pango_attr_overline_new(PANGO_OVERLINE_SINGLE);
        m->start_index = begin_overline; m->end_index = j;
        pango_attr_list_insert(list, m);
    }

    pango_layout_set_text(layout, text, -1);
    delete[] text;
    pango_layout_set_attributes(layout, list);
    pango_attr_list_unref(list);
}
#undef MARKUP_CASE

static inline PangoLayout *measure_text_internal(const char *str, int len, int maxwidth, int align, int justify, bvec initial_color, int &width, int &height, int &offset, int *map_markup_to_text, int *map_text_to_markup, const char *language)
{
    // create cairo context
    cairo_t *cr = cairo_create(dummy_surface);
    cairo_set_font_options(cr, options);

    // create layout
    PangoLayout *layout = pango_cairo_create_layout(cr);
    if(!layout)
    {
        width = height = offset = 0;
        cairo_destroy(cr);
        return NULL;
    }

    // set font family and size
    pango_font_description_set_absolute_size(curfont->desc, fontsize * PANGO_SCALE); // pango 1.8
    pango_layout_set_font_description(layout, curfont->desc);

    // line wrapping: set maximum width, alignment and justification
    if(maxwidth > 0)
    {
        pango_layout_set_width(layout, maxwidth * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(layout, align > 0 ? PANGO_ALIGN_RIGHT : align < 0 ? PANGO_ALIGN_LEFT : PANGO_ALIGN_CENTER);
        pango_layout_set_justify(layout, justify ? TRUE : FALSE);
    }

    add_text_to_layout(str, len, layout, initial_color, map_markup_to_text, map_text_to_markup, language);

    // get pixel size
    PangoRectangle r;
    pango_layout_get_extents(layout, NULL, &r);
    width  = r.width  / PANGO_SCALE;
    height = r.height / PANGO_SCALE;
    offset = -r.x     / PANGO_SCALE;

    cairo_destroy(cr);
    return layout;
}
void measure_text(const char *str, int maxwidth, int &width, int &height, int align, int justify, const char *language)
{
    int _offset;
    PangoLayout *layout = measure_text_internal(str, strlen(str), maxwidth, align, justify, bvec(0, 0, 0), width, height, _offset, NULL, NULL, language);
    if(layout) g_object_unref(layout);
}

void prepare_text(const char *str, textinfo &info, int maxwidth, bvec initial_color, int cursor, float outline, bvec outline_color, int align, int justify, const char *language)
{
    // get dimensions and pango layout
    int width, height, offset;
    const int len = strlen(str);
    int map_markup_to_text[len+1];
    PangoLayout *layout = measure_text_internal(str, len, maxwidth, align, justify, initial_color, width, height, offset, cursor >= 0 ? map_markup_to_text : NULL, NULL, language);
    if(!layout) { info = {0, 0, 0}; return; }
    if(!width || !height) { g_object_unref(layout); info = {0, 0, 0}; return; }

    // create surface and cairo context
    if(cursor >= 0) width += max(4.f, fontsize); // make space for the cursor
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    cairo_set_font_options(cr, options);
    cairo_move_to(cr, offset, 0);

    // draw text onto the surface
    if(outline)
    {
        cairo_set_source_rgba(cr, outline_color.r / 255.f, outline_color.g / 255.f, outline_color.b / 255.f, 1.0);
        cairo_set_line_width(cr, 2 * outline);
        pango_cairo_layout_path(cr, layout);
        cairo_stroke(cr);
    }
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    pango_cairo_show_layout(cr, layout);

    // add the cursor
    if(cursor >= 0 && ((totalmillis - inputmillis <= cursorblink) || !cursorblink || ((totalmillis - inputmillis) % (2*cursorblink)) <= cursorblink))
    {
        cursor = min((int)strlen(pango_layout_get_text(layout)), map_markup_to_text[cursor]);
        PangoRectangle cursor_rect;
        pango_layout_get_cursor_pos(layout, cursor, &cursor_rect, NULL);

        const float curw = max(1.f, fontsize / 16);

        cairo_rectangle(cr, cursor_rect.x / PANGO_SCALE, cursor_rect.y / PANGO_SCALE, curw, cursor_rect.height / PANGO_SCALE);
        cairo_set_source_rgba(cr, cursorcolor.r / 255.f, cursorcolor.g / 255.f, cursorcolor.b / 255.f, 1.0);
        cairo_fill(cr);
    }

    // create and upload texture
    glGenTextures(1, &info.tex);
    if(!info.tex)
    {
        info = {0, 0, 0};
        g_object_unref(layout);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return;
    }
    glBindTexture(GL_TEXTURE_RECTANGLE, info.tex);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_COMPRESSED_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, cairo_image_surface_get_data(surface));
    info.w = width;
    info.h = height;

    // clean up
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
void prepare_text_particle(const char *str, textinfo &info, bvec initial_color, float outline, bvec outline_color, const char *language)
{
    const int c = initial_color.tohexcolor(), d = outline_color.tohexcolor();
    const char *l = language ? language : "";
    uint key = crc32(0, (const Bytef *)str, strlen(str)) + curfont->id;
         key = crc32(key, (const Bytef *)(&outline), sizeof(float));
         key = crc32(key, (const Bytef *)(&c), sizeof(int));
         key = crc32(key, (const Bytef *)(&d), sizeof(int));
         key = crc32(key, (const Bytef *)l, strlen(l));
    partinfo &p = particle_cache[key];
    if(p.ti.tex)
    {
        info = p.ti;
        return;
    }
    prepare_text(str, p.ti, 0, initial_color, -1, outline, outline_color, -1, 0, language);
    if(!p.ti.tex) { info = {0, 0, 0}; return; }
    if(particle_queue.length() >= 256)
    {
        const uint oldkey = particle_queue[0];
        partinfo &oldp = particle_cache[oldkey];
        if(oldp.ti.tex) glDeleteTextures(1, &oldp.ti.tex);
        particle_cache.remove(oldkey);
        particle_queue.remove(0);
    }
    particle_queue.add(key);
    info = p.ti;
}

// draw text to the screen
void draw_text(textinfo info, float left, float top, int a, bool black)
{
    const int w = info.w, h = info.h;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(textshader) textshader->set(); // text particles
    else hudtextshader->set();        // UI text

    gle::color(black ? bvec(0, 0, 0) : bvec(255*a/255.f, 255*a/255.f, 255*a/255.f), a);
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
    // NOTE: `info.tex` is not deleted here!
}
void draw_text(const char *str, float left, float top, int r, int g, int b, int a, int maxwidth, int align, int justify, const char *language)
{
    textinfo info;
    prepare_text(str, info, maxwidth, bvec(r, g, b), -1, 0, bvec(0, 0, 0), align, justify, language);
    if(!info.tex) return;
    draw_text(info, left, top, a);
    glDeleteTextures(1, &info.tex);
}
void draw_textf(const char *fstr, float left, float top, ...)
{
    defvformatstring(str, top, fstr);
    draw_text(str, left, top);
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
int text_visible(const char *str, float hitx, float hity, int maxwidth, int align, int justify, const char *language)
{
    int width, height, _offset;
    const int len = strlen(str);
    if(!len) return 0;
    int map_text_to_markup[len+1];
    PangoLayout *layout = measure_text_internal(str, len, maxwidth, align, justify, bvec(0, 0, 0), width, height, _offset, NULL, map_text_to_markup, language);
    if(!layout) return len;
    if(!width || !height) { g_object_unref(layout); return len; }

    int index;
    const int res = pango_layout_xy_to_index(layout, hitx * PANGO_SCALE, hity * PANGO_SCALE, &index, NULL);
    g_object_unref(layout);
    if(!res) return len;
    return map_text_to_markup[index];
}

// used by the text editor
void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth, int align, int justify, const char *language)
{
    int width, height, _offset;
    const int len = strlen(str);
    if(!len) { cx = cy = 0; return; }
    int map_markup_to_text[len+1];
    PangoLayout *layout = measure_text_internal(str, len, maxwidth, align, justify, bvec(0, 0, 0), width, height, _offset, map_markup_to_text, NULL, language);
    if(!layout) { cx = cy = 0; return; }
    if(!width || !height) { g_object_unref(layout); cx = cy = 0; return; }

    cursor = max(0, min((int)strlen(pango_layout_get_text(layout)), map_markup_to_text[cursor]));
    PangoRectangle pos;
    pango_layout_index_to_pos(layout, cursor, &pos);
    cx = pos.x / PANGO_SCALE;
    cy = pos.y / PANGO_SCALE;

    g_object_unref(layout);
}

void reloadfonts() { clear_text_particles(); UI::cleartext(); }