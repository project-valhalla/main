#include "engine.h"

#include <pango/pangocairo.h>

#if (!PANGO_VERSION_CHECK(1, 56, 0) || defined(OLDPANGO))
    #if (defined(WIN32) || defined(__APPLE__))
        #error this project requires pango >= 1.56
    #else
        #define OLDPANGO 1
        #pragma message ("compiling with support for old pango (>= 1.38)")
    #endif
#endif

#ifdef OLDPANGO
    #include <fontconfig/fontconfig.h>
#endif

static int fontid = 0;                 // used by UI for change detection
double fontsize = 0;                   // pixel height of the current font
const matrix4x3 *textmatrix = nullptr; // used for text particles
Shader *textshader = nullptr;          // used for text particles

#pragma region global_font_settings
// apply a black shadow or outline to console text to improve visibility
VARFP(conshadow,  0, 255, 255, clearconsoletextures());
VARFP(conoutline, 0,   0, 255, clearconsoletextures());

// how often the cursor blinks, in milliseconds; set to zero to disable blinking
VARFP(cursorblink, 0, 750, 2000,
{
    cursorblink = cursorblink ? max(250, cursorblink) : 0;
});

CVARP(cursorcolor, 0xFFFFFF);

// configurable text colors
static bvec palette[10];
ICOMMAND(textcolor, "ii", (int *i, int *c),
{
    if(*i >= 0 && *i <= 9)
    {
        palette[*i] = bvec::hexcolor(*c);
        clearconsoletextures();
        reloadfonts();
    }
});

static cairo_font_options_t *global_font_options = nullptr; // global font options
static cairo_surface_t      *dummy_surface       = nullptr; // used to measure text

// NOTE: subpixel antialiasing is not available because we need to render on transparent backgrounds
static cairo_antialias_t  antialias_ = CAIRO_ANTIALIAS_GRAY;
static cairo_hint_style_t hintstyle_ = CAIRO_HINT_STYLE_DEFAULT;
VARFP(fontantialias, 0, 1, 1,
{
    antialias_ = fontantialias ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE;
    if(!global_font_options) return;
    cairo_font_options_set_antialias(global_font_options, antialias_);
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
    if(!global_font_options) return;
    cairo_font_options_set_hint_style(global_font_options, hintstyle_);
    clearconsoletextures();
    reloadfonts();
})
#pragma endregion global_font_settings

#pragma region particles
// a cache for rendered text particles
static hashtable<uint, text::Label> particle_cache;
static vector<uint> particle_queue;
static void clear_text_particles()
{
    enumerate(particle_cache, text::Label, label, { label.clear(); });
    particle_cache.clear();
    particle_queue.setsize(0);
}
#pragma endregion particles

#pragma region font_management
// register a TTF file
void addfontfile(const char *filename)
{
    const char *found = findfile(filename, "rb");
    if(!found || !found[0]) return;
    //conoutf(CON_DEBUG, "Loading font file: %s", found);
    #ifdef OLDPANGO
        FcConfigAppFontAddFile(FcConfigGetCurrent(), (const unsigned char *)found);
    #else
        static PangoFontMap *fontMap = pango_cairo_font_map_get_default();
        pango_font_map_add_font_file(fontMap, found, nullptr);
    #endif
}
COMMANDN(registerfont, addfontfile, "s");

struct Font
{
    char *name;
    int id;
    char *opentype_features;
    PangoFontDescription *desc;

    Font() : name(nullptr), opentype_features(nullptr), desc(nullptr) {}
    ~Font()
    {
        delete[] name;
        delete[] opentype_features;
        if(desc) pango_font_description_free(desc);
    }
};
static Font *curfont = nullptr, *lastfont = nullptr;
static hashnameset<Font> fonts;
static vector<Font *> fontstack;

void newfont(const char *name, const char *family)
{
    Font *f = &fonts[name];
    if(f->name) f->~Font();
    f->name = newstring(name);
    f->id = fontid++;
    f->opentype_features = newstring("");
    f->desc = pango_font_description_from_string(family);

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

void fontfeatures(char *features)
{
    if(!lastfont) return;
    if(lastfont->opentype_features) delete[] lastfont->opentype_features;
    lastfont->opentype_features = newstring(features);
}
COMMAND(fontfeatures, "s");

void fontvariations(char *variations)
{
    if(!lastfont || !lastfont->desc) return;
    pango_font_description_set_variations(lastfont->desc, variations);
}
COMMAND(fontvariations, "s");

// set the current font
static inline bool setfont(Font *f)
{
    if(!f) return false;
    curfont = f;
    return true;
}
bool setfont(const char *name)
{
    Font *f = fonts.access(name);
    return setfont(f);
}

// font stack management
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

int getcurrentfontid()
{
    return curfont->id;
}

void reloadfonts()
{
    clear_text_particles();
    UI::clearlabels();
}
#pragma endregion font_management

#pragma region pango_initialization
bool init_pangocairo()
{
    global_font_options = cairo_font_options_create();
    if(CAIRO_STATUS_SUCCESS != cairo_font_options_status(global_font_options))
    {
        return false;
    }
    cairo_font_options_set_antialias   (global_font_options, antialias_);
    cairo_font_options_set_hint_style  (global_font_options, hintstyle_);
    cairo_font_options_set_hint_metrics(global_font_options, CAIRO_HINT_METRICS_ON);

    dummy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
    if(CAIRO_STATUS_SUCCESS != cairo_surface_status(dummy_surface))
    {
        return false;
    }

    loopi(10) palette[i] = bvec(255, 255, 255);

    conoutf(CON_INIT, "Text rendering: Pango %s, Cairo %s", pango_version_string(), cairo_version_string());
    return true;
}

void done_pangocairo()
{
    fonts.clear();
    if(global_font_options) cairo_font_options_destroy(global_font_options);
    if(dummy_surface) cairo_surface_destroy(dummy_surface);
}
#pragma endregion pango_initialization

#pragma region markup_and_layout
//stack[sp] is current color index
static inline bvec text_color(char c, char *stack, int size, int& sp, bvec color)
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
        if(c >= '0' && c <= '9') return palette[c - '0'];
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
static void add_text_to_layout(
    const char *markup,      // the input string with \f codes
    int markup_length,       // length of the above
    PangoLayout *layout,     // the layout to fill
    const bvec& color,       // initial color of the text
    int *map_markup_to_text, // output: maps character indices from markup to text
    int *map_text_to_markup, // output: reverse of the above
    const char *lang,        // optional language code of the text (can affect text shaping)
    bool no_fallback         // disable fallback fonts for unavailable glyphs?
)
{
    char *text = newstring(markup_length);

    bvec tcolor = color;
    char colorstack[10];
    colorstack[0] = '\0';
    int cpos = 0;

    PangoAttrList *list = pango_attr_list_new();
    PangoAttribute *attr; // colors and global features
    PangoAttribute *m;    // used for markup attributes

    // OpenType features
    if(curfont->opentype_features[0])
    {
        attr = pango_attr_font_features_new(curfont->opentype_features); // pango 1.38
        pango_attr_list_insert(list, attr);
    }
    // no fallback to system fonts
    if(no_fallback)
    {
        attr = pango_attr_fallback_new(FALSE);
        pango_attr_list_insert(list, attr);
    }
    // language
    if(lang)
    {
        attr = pango_attr_language_new(pango_language_from_string(lang));
        pango_attr_list_insert(list, attr);
    }

    // initial color: arguments must be converted to 16-bit values
    attr = pango_attr_foreground_new(tcolor.r * 257, tcolor.g * 257, tcolor.b * 257);
    attr->start_index = 0;

    int begin_bold = -1, begin_italic = -1, begin_underline = -1, begin_strikethrough = -1;
    int n_bold = 0, n_italic = 0, n_underline = 0, n_strikethrough = 0; // counters used for nesting

    // parse markup
    int i = 0, j = 0;
    for(; i < markup_length; ++i)
    {
        if(markup[i] == '\f' && i < (markup_length - 1))
        {
            switch(markup[i+1])
            {
                MARKUP_CASE('b', 'B', pango_attr_weight_new       , PANGO_WEIGHT_BOLD     , bold);
                MARKUP_CASE('i', 'I', pango_attr_style_new        , PANGO_STYLE_ITALIC    , italic);
                MARKUP_CASE('u', 'U', pango_attr_underline_new    , PANGO_UNDERLINE_SINGLE, underline);
                MARKUP_CASE('t', 'T', pango_attr_strikethrough_new, TRUE                  , strikethrough);
                default:
                {
                    tcolor = text_color(markup[i+1], colorstack, sizeof(colorstack), cpos, color);
                    if(!attr) break;

                    attr->end_index = j + 1;
                    pango_attr_list_insert(list, attr);
                    attr = nullptr;
                }
            }
            if(map_markup_to_text) map_markup_to_text[i] = j;
            ++i;
            continue;
        }
        if(!attr)
        {
            attr = pango_attr_foreground_new(tcolor.r * 257, tcolor.g * 257, tcolor.b * 257);
            attr->start_index = j;
        }
        if(map_markup_to_text) map_markup_to_text[i] = j;
        if(map_text_to_markup) map_text_to_markup[j] = i;
        text[j++] = markup[i];
    }
    text[j] = '\0';
    if(map_markup_to_text) map_markup_to_text[i] = j;
    if(map_text_to_markup) map_text_to_markup[j] = i;

    if(attr) // color
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

    pango_layout_set_text(layout, text, -1);
    delete[] text;
    pango_layout_set_attributes(layout, list);
    pango_attr_list_unref(list);
}
#undef MARKUP_CASE

// creates a layout and fills it with text, and measures its width, height and horizontal offset
static PangoLayout *measure_text_internal(
    const char *str,         // the string to measure
    int len,                 // length of the string
    int max_width,           // maximum width in pixels (no limit if <=0)
    int align,               // text alignment: -1 = left, 0 = center, 1 = right
    int justify,             // text justification (0 = disable, enable otherwise)
    const bvec& color,       // initial color of the text
    int& width,              // output: the measured width
    int& height,             // output: the measured height
    int& offset,             // output: the measured offset
    int *map_markup_to_text, // output: maps character indices from markup to text
    int *map_text_to_markup, // output: reverse of the above
    const char *lang,        // optional language code of the text (can affect text shaping)
    bool no_fallback         // disable fallback fonts for unavailable glyphs?
)
{
    // create cairo context
    cairo_t *cairo_context = cairo_create(dummy_surface);
    cairo_set_font_options(cairo_context, global_font_options);

    // create layout
    PangoLayout *layout = pango_cairo_create_layout(cairo_context);

    // set font family and size
    pango_font_description_set_absolute_size(curfont->desc, fontsize * PANGO_SCALE); // pango 1.8
    pango_layout_set_font_description(layout, curfont->desc);

    // line wrapping: set maximum width, alignment and justification
    if(max_width > 0)
    {
        pango_layout_set_width(layout, max_width * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(layout, align > 0 ? PANGO_ALIGN_RIGHT : align < 0 ? PANGO_ALIGN_LEFT : PANGO_ALIGN_CENTER);
        pango_layout_set_justify(layout, justify ? TRUE : FALSE);
    }

    // fill layout
    add_text_to_layout(str, len, layout, color, map_markup_to_text, map_text_to_markup, lang, no_fallback);

    // get pixel size
    PangoRectangle r;
    pango_layout_get_extents(layout, nullptr, &r);
    width  = r.width  / PANGO_SCALE;
    height = r.height / PANGO_SCALE;
    offset = -r.x     / PANGO_SCALE;

    cairo_destroy(cairo_context);
    return layout;
}
#pragma endregion markup_and_layout

#pragma region label
namespace text
{
    Label::Label() :
        tex(0),
        layout(nullptr),
        map_markup_to_text(nullptr),
        map_text_to_markup(nullptr)
        {};
    Label::~Label()
    {
        if(tex != 0) glDeleteTextures(1, &tex);
        if(layout != nullptr) g_object_unref(layout);
        delete[] map_markup_to_text;
        delete[] map_text_to_markup;
    }
    Label::Label(Label&& other) noexcept :
        w(other.w), h(other.h), ox(other.ox), oy(other.oy),
        tex(other.tex),
        layout(other.layout),
        map_markup_to_text(other.map_markup_to_text),
        map_text_to_markup(other.map_text_to_markup)
    {
        other.tex = 0;
        other.layout = nullptr;
        other.map_markup_to_text = nullptr;
        other.map_text_to_markup = nullptr;
    }
    Label& Label::operator=(Label&& other) noexcept
    {
        if(&other == this) return *this;
        this->~Label();
        w = other.w;
        h = other.h;
        ox = other.ox;
        oy = other.oy;
        tex = exchange(other.tex, 0);
        layout = exchange(other.layout, nullptr);
        map_markup_to_text = exchange(other.map_markup_to_text, nullptr);
        map_text_to_markup = exchange(other.map_text_to_markup, nullptr);
        return *this;
    }
    // empties the text content of the label
    void Label::clear()
    {
        if(tex != 0)
        {
            glDeleteTextures(1, &tex);
            tex = 0;
        }
        if(layout != nullptr)
        {
            g_object_unref(layout);
            layout = nullptr;
        }
        delete[] map_markup_to_text;
        delete[] map_text_to_markup;
        map_markup_to_text = map_text_to_markup = nullptr;
    }

    // draws the label to the screen
    void Label::draw(
        double left, // screen X coordinate
        double top,  // screen Y coordinate
        int alpha,   // text opacity (0-255)
        bool black   // make it black? (used for shadows)
    ) const
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if(textshader) textshader->set(); // text particles
        else hudtextshader->set();        // UI text

        gle::color(black ? bvec(0, 0, 0) : bvec(255*alpha/255.f, 255*alpha/255.f, 255*alpha/255.f), alpha);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
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
    }
    // like `draw()` but with a shadow
    void Label::draw_as_console(double left, double top) const
    {
        if(conshadow)
        {
            const double d = 0.75 * conscale;
            draw(left - d, top + d, conshadow, true);
        }
        draw(left, top);
    }

    // converts (x,y) pixel coordinates to a character (byte) index
    // NOTE: do not call if the label was not prepared with `keep_layout=true`
    int Label::xy_to_index(float x, float y) const
    {
        const int px = (x - ox) * PANGO_SCALE, py = (y - oy) * PANGO_SCALE;
        int ix, _trailing;
        if(!pango_layout_xy_to_index(layout, px, py, &ix, &_trailing))
        {
            // user clicked outside of the label: set the cursor to the end of the string
            ix = strlen(pango_layout_get_text(layout));
        }
        return map_text_to_markup ? map_text_to_markup[ix] : ix;
    }

    // creates a label from a string
    Label prepare(
        const char *str,            // the label text
        int max_width,              // maximum width in pixels
        const bvec& color,          // initial color of the text
        int cursor,                 // byte index of the cursor (disabled if <0)
        double outline,             // outline thickness
        const bvec4& outline_color, // outline color (RGBA)
        int align,                  // text alignment: -1 = left, 0 = center, 1 = right
        int justify,                // text justification (0 = disable, enable otherwise)
        const char *lang,           // optional language code of the text (can affect text shaping)
        bool no_fallback,           // disable fallback fonts for unavailable glyphs?
        bool keep_layout,           // keep layout in memory? (necessary if you need to call `xy_to_index()` on the label)
        bool reserve_cursor         // reserve space to the right for the cursor?
    )
    {
        Label label;
        
        // measure text dimensions and create pango layout
        int width, height, offset;
        const int len = strlen(str);
        if(cursor >= 0 || keep_layout) label.map_markup_to_text = new int[len+1];
        if(keep_layout) label.map_text_to_markup = new int[len+1];
        label.layout = measure_text_internal(str, len, max_width, align, justify, color, width, height, offset, label.map_markup_to_text, label.map_text_to_markup, lang, no_fallback);
        if(!width || !height)
        {
            return label;
        }

        // create surface and cairo context
        if(cursor >= 0 || reserve_cursor) // reserve space for the cursor
        {
            width += max(1., ceil(fontsize / 16.));
        }
        const int outline_offset = ceil(outline);
        width  += 2 * outline_offset;
        height += 2 * outline_offset;
        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cairo_t *cairo_context = cairo_create(surface);
        cairo_set_font_options(cairo_context, global_font_options);
        cairo_move_to(cairo_context, offset + outline_offset, outline_offset);

        // draw text outline
        if(outline)
        {
            cairo_set_source_rgba(cairo_context,
                outline_color.r/255.,
                outline_color.g/255.,
                outline_color.b/255.,
                outline_color.a/255.
            );
            cairo_set_line_join(cairo_context, CAIRO_LINE_JOIN_ROUND);
            cairo_set_line_width(cairo_context, 2 * outline);
            pango_cairo_layout_path(cairo_context, label.layout);
            cairo_stroke(cairo_context);
        }

        // draw text
        cairo_set_source_rgba(cairo_context, 1.0, 1.0, 1.0, 1.0);
        cairo_move_to(cairo_context, offset + outline_offset, outline_offset);
        pango_cairo_show_layout(cairo_context, label.layout);

        // draw the cursor
        if(cursor >= 0 && (
            !cursorblink                                                   ||
            (totalmillis - inputmillis <= cursorblink)                     ||
            ((totalmillis - inputmillis) % (2 * cursorblink)) <= cursorblink
        ))
        {
            if(cursor > len) cursor = len;
            cursor = min((int)strlen(pango_layout_get_text(label.layout)), label.map_markup_to_text[cursor]);
            PangoRectangle cursor_rect;
            pango_layout_get_cursor_pos(label.layout, cursor, &cursor_rect, nullptr);

            const double cursor_width = max(1., fontsize / 16.);
            cairo_rectangle(cairo_context,
                cursor_rect.x / PANGO_SCALE + outline_offset,
                cursor_rect.y / PANGO_SCALE + outline_offset,
                cursor_width,
                cursor_rect.height / PANGO_SCALE
            );
            cairo_set_source_rgba(cairo_context,
                cursorcolor.r / 255.,
                cursorcolor.g / 255.,
                cursorcolor.b / 255.,
                1.
            );
            cairo_fill(cairo_context);
        }

        if(!keep_layout)
        {
            g_object_unref(label.layout);
            label.layout = nullptr;
        }

        // create GPU texture
        glGenTextures(1, &label.tex);
        if(!label.tex)
        {
            cairo_destroy(cairo_context);
            cairo_surface_destroy(surface);
            return label;
        }
        glBindTexture(GL_TEXTURE_RECTANGLE, label.tex);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_COMPRESSED_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, cairo_image_surface_get_data(surface));
        label.w = width;
        label.h = height;
        label.ox = offset + outline_offset;
        label.oy = outline_offset;

        // clean up
        cairo_destroy(cairo_context);
        cairo_surface_destroy(surface);
        return label;
    }

    // same as `prepare()` but with appropriate settings for console text
    Label prepare_for_console(const char *str, int max_width, int cursor)
    {
        return prepare(str, max_width, bvec(255, 255, 255), cursor, conoutline ? ceil(FONTH / 32.) : 0, bvec4(0, 0, 0, conoutline));
    }

    // same as `prepare()` but use a cache so that the same label doesn't have to be recreated every time
    const Label& prepare_for_particle(
        const char *str,
        const bvec& color,
        double outline,
        const bvec4& outline_color,
        const char *lang,
        bool no_fallback
    )
    {
        const int c = color.tohexcolor();
        const char *l = lang ? lang : "";
        uint
            key = crc32(0  , (const Bytef *)str, strlen(str)) + curfont->id;
            key = crc32(key, (const Bytef *)(&outline), sizeof(double));
            key = crc32(key, (const Bytef *)(&c), sizeof(int));
            key = crc32(key, (const Bytef *)(&outline_color.mask), sizeof(uint));
            key = crc32(key, (const Bytef *)l, strlen(l));

        Label& label = particle_cache[key];
        if(label.valid()) return label;

        label = prepare(str, 0, color, -1, outline, outline_color, -1, 0, lang, no_fallback);
        if(particle_queue.length() >= 256)
        {
            const uint oldkey = particle_queue[0];
            particle_cache[oldkey].clear();
            particle_cache.remove(oldkey);
            particle_queue.remove(0);
        }
        particle_queue.add(key);
        return label;
    }

    // measure the dimensions of a string without creating the label (skips texture creation)
    void measure(
        const char *str,  // the string to measure
        int max_width,    // maximum width in pixels
        int& width,       // output: the measured width
        int& height,      // output: the measured height
        int align,        // text alignment: -1 = left, 0 = center, 1 = right
        int justify,      // text justification (0 = disable, enable otherwise)
        const char *lang, // optional language code of the text (can affect text shaping)
        bool no_fallback  // disable fallback fonts for unavailable glyphs?
    )
    {
        int _offset;
        PangoLayout *layout = measure_text_internal(str, strlen(str), max_width, align, justify, bvec(0, 0, 0), width, height, _offset, nullptr, nullptr, lang, no_fallback);
        g_object_unref(layout);
    }

    // draw a string directly to the screen
    void draw(
        const char *str,   // the string to draw
        double left,       // screen X coordinate
        double top,        // screen Y coordinate
        const bvec& color, // initial color of the text
        int alpha,         // text opacity (0-255)
        int max_width,     // maximum width in pixels
        int align,         // text alignment: -1 = left, 0 = center, 1 = right
        int justify,       // text justification (0 = disable, enable otherwise)
        const char *lang,  // optional language code of the text (can affect text shaping)
        bool no_fallback   // disable fallback fonts for unavailable glyphs?
    )
    {
        const Label label = prepare(str, max_width, color, -1, 0, bvec4(color, alpha), align, justify, lang, no_fallback);
        if(label.valid()) label.draw(left, top, alpha);
    }

    // same as `draw()` but with appropriate settings for console text
    void draw_as_console(const char *str, double left, double top, int max_width, int cursor)
    {
        const Label label = prepare_for_console(str, max_width, cursor);
        if(label.valid())
        {
            if(conshadow)
            {
                const double d = 0.75 * conscale;
                label.draw(left - d, top + d, conshadow, true);
            }
            label.draw(left, top);
        }
    }

    void getres(int& w, int& h)
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
} // namespace text
#pragma endregion label