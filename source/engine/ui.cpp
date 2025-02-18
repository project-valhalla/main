#include "engine.h"

namespace UI
{
    float cursorx = 0.499f, cursory = 0.499f;

    bool mousetracking = false, cursorlockedx = false, cursorlockedy = false;

    vec2 cursortrackvec;
    vec2 mousetrackvec;

    static void quads(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
    {
        gle::attribf(x,   y);   gle::attribf(tx,    ty);
        gle::attribf(x+w, y);   gle::attribf(tx+tw, ty);
        gle::attribf(x+w, y+h); gle::attribf(tx+tw, ty+th);
        gle::attribf(x,   y+h); gle::attribf(tx,    ty+th);
    }

    static void quads_rotation(float x, float y, float w, float h, float angle, float tx = 0, float ty = 0, float tw = 1, float th = 1)
    {
        vec v(-0.5, -0.5, 0);
        v.rotate_around_z((90+angle)*RAD);
        gle::attribf(x - w*v.x + w/2, y + h*v.y + h/2); gle::attribf(tx,    ty);
        gle::attribf(x - w*v.y + w/2, y - h*v.x + h/2); gle::attribf(tx+tw, ty);
        gle::attribf(x + w*v.x + w/2, y - h*v.y + h/2); gle::attribf(tx+tw, ty+th);
        gle::attribf(x + w*v.y + w/2, y + h*v.x + h/2); gle::attribf(tx,    ty+th);
    }

#if 0
    static void quad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);   gle::attribf(tx+tw, ty);
        gle::attribf(x,   y);   gle::attribf(tx,    ty);
        gle::attribf(x+w, y+h); gle::attribf(tx+tw, ty+th);
        gle::attribf(x,   y+h); gle::attribf(tx,    ty+th);
        gle::end();
    }
#endif

    static void quad(float x, float y, float w, float h, const vec2 tc[4])
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x+w, y);   gle::attrib(tc[1]);
        gle::attribf(x,   y);   gle::attrib(tc[0]);
        gle::attribf(x+w, y+h); gle::attrib(tc[2]);
        gle::attribf(x,   y+h); gle::attrib(tc[3]);
        gle::end();
    }

    struct ClipArea
    {
        float x1, y1, x2, y2;

        ClipArea(float x, float y, float w, float h) : x1(x), y1(y), x2(x+w), y2(y+h) {}

        void intersect(const ClipArea &c)
        {
            x1 = max(x1, c.x1);
            y1 = max(y1, c.y1);
            x2 = max(x1, min(x2, c.x2));
            y2 = max(y1, min(y2, c.y2));

        }

        bool isfullyclipped(float x, float y, float w, float h)
        {
            return x1 == x2 || y1 == y2 || x >= x2 || y >= y2 || x+w <= x1 || y+h <= y1;
        }

        void scissor();
    };

    static vector<ClipArea> clipstack;

    static void pushclip(float x, float y, float w, float h)
    {
        if(clipstack.empty()) glEnable(GL_SCISSOR_TEST);
        ClipArea &c = clipstack.add(ClipArea(x, y, w, h));
        if(clipstack.length() >= 2) c.intersect(clipstack[clipstack.length()-2]);
        c.scissor();
    }

    static void popclip()
    {
        clipstack.pop();
        if(clipstack.empty()) glDisable(GL_SCISSOR_TEST);
        else clipstack.last().scissor();
    }

    static inline bool isfullyclipped(float x, float y, float w, float h)
    {
        if(clipstack.empty()) return false;
        return clipstack.last().isfullyclipped(x, y, w, h);
    }

    enum
    {
        ALIGN_MASK    = 0xF,

        ALIGN_HMASK   = 0x3,
        ALIGN_HSHIFT  = 0,
        ALIGN_HNONE   = 0,
        ALIGN_LEFT    = 1,
        ALIGN_HCENTER = 2,
        ALIGN_RIGHT   = 3,

        ALIGN_VMASK   = 0xC,
        ALIGN_VSHIFT  = 2,
        ALIGN_VNONE   = 0<<2,
        ALIGN_TOP     = 1<<2,
        ALIGN_VCENTER = 2<<2,
        ALIGN_BOTTOM  = 3<<2,

        CLAMP_MASK    = 0xF0,
        CLAMP_LEFT    = 0x10,
        CLAMP_RIGHT   = 0x20,
        CLAMP_TOP     = 0x40,
        CLAMP_BOTTOM  = 0x80,

        NO_ADJUST     = ALIGN_HNONE | ALIGN_VNONE,
    };

    enum
    {
        STATE_HOVER       = 1<<0,
        STATE_PRESS       = 1<<1,
        STATE_HOLD        = 1<<2,
        STATE_RELEASE     = 1<<3,
        STATE_ALT_PRESS   = 1<<4,
        STATE_ALT_HOLD    = 1<<5,
        STATE_ALT_RELEASE = 1<<6,
        STATE_ESC_PRESS   = 1<<7,
        STATE_ESC_HOLD    = 1<<8,
        STATE_ESC_RELEASE = 1<<9,
        STATE_SCROLL_UP   = 1<<10,
        STATE_SCROLL_DOWN = 1<<11,
        STATE_HIDDEN      = 1<<12,

        STATE_HOLD_MASK = STATE_HOLD | STATE_ALT_HOLD | STATE_ESC_HOLD
    };

    struct Object;

    static Object *buildparent = NULL;
    static int buildchild = -1;

    #define BUILD(type, o, setup, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            setup; \
            o->buildchildren(contents); \
        } \
    } while(0)

    enum
    {
        CHANGE_SHADER = 1<<0,
        CHANGE_COLOR  = 1<<1,
        CHANGE_BLEND  = 1<<2
    };
    static int changed = 0;

    static Object *drawing = NULL;

    enum { BLEND_ALPHA, BLEND_MOD };
    static int blendtype = BLEND_ALPHA;

    static inline void changeblend(int type, GLenum src, GLenum dst)
    {
        if(blendtype != type)
        {
            blendtype = type;
            glBlendFunc(src, dst);
        }
    }

    void resetblend() { changeblend(BLEND_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    void modblend() { changeblend(BLEND_MOD, GL_ZERO, GL_SRC_COLOR); }

    static int uimillis = 0;

    FVARP(uiscale, 0.5f, 1.0f, 1.5f);
    VARFP(uifps, 0, 60, 1000, { uimillis = uifps ? (1000/uifps) : 0; });

    struct Object
    {
        Object *parent;
        float x, y, w, h;
        uchar adjust;
        ushort state, childstate;
        vector<Object *> children;

        Object() : adjust(0), state(0), childstate(0) {}
        virtual ~Object()
        {
            clearchildren();
        }

        void resetlayout()
        {
            x = y = w = h = 0;
        }

        void reset()
        {
            resetlayout();
            parent = NULL;
            adjust = ALIGN_HCENTER | ALIGN_VCENTER;
        }

        virtual uchar childalign() const { return ALIGN_HCENTER | ALIGN_VCENTER; }

        void reset(Object *parent_)
        {
            resetlayout();
            parent = parent_;
            adjust = parent->childalign();
        }

        void setup()
        {
        }

        void clearchildren()
        {
            children.deletecontents();
        }

        #define loopchildren(o, body) do { \
            loopv(children) \
            { \
                Object *o = children[i]; \
                body; \
            } \
        } while(0)

        #define loopchildrenrev(o, body) do { \
            loopvrev(children) \
            { \
                Object *o = children[i]; \
                body; \
            } \
        } while(0)

        #define loopchildrange(start, end, o, body) do { \
            for(int i = start; i < end; i++) \
            { \
                Object *o = children[i]; \
                body; \
            } \
        } while(0)

        #define loopinchildren(o, cx, cy, inbody, outbody) \
            loopchildren(o, \
            { \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                if(o##x >= 0 && o##x < o->w && o##y >= 0 && o##y < o->h) \
                { \
                    inbody; \
                } \
                outbody; \
            })

        #define loopinchildrenrev(o, cx, cy, inbody, outbody) \
            loopchildrenrev(o, \
            { \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                if(o##x >= 0 && o##x < o->w && o##y >= 0 && o##y < o->h) \
                { \
                    inbody; \
                } \
                outbody; \
            })

        virtual void layout()
        {
            w = h = 0;
            loopchildren(o,
            {
                o->x = o->y = 0;
                o->layout();
                w = max(w, o->x + o->w);
                h = max(h, o->y + o->h);
            });
        }

        // called when the window is closed
        virtual void hide()
        {
            loopchildren(o,
            {
                o->hide();
            });
        }

        virtual void clearlabels()
        {
            loopchildren(o,
            {
                o->clearlabels();
            });
        }

        void adjustchildrento(float px, float py, float pw, float ph)
        {
            loopchildren(o, o->adjustlayout(px, py, pw, ph));
        }

        virtual void adjustchildren()
        {
            adjustchildrento(0, 0, w, h);
        }

        void adjustlayout(float px, float py, float pw, float ph)
        {
            switch(adjust&ALIGN_HMASK)
            {
                case ALIGN_LEFT:    x = px; break;
                case ALIGN_HCENTER: x = px + (pw - w) / 2; break;
                case ALIGN_RIGHT:   x = px + pw - w; break;
            }

            switch(adjust&ALIGN_VMASK)
            {
                case ALIGN_TOP:     y = py; break;
                case ALIGN_VCENTER: y = py + (ph - h) / 2; break;
                case ALIGN_BOTTOM:  y = py + ph - h; break;
            }

            if(adjust&CLAMP_MASK)
            {
                if(adjust&CLAMP_LEFT)   { w += x - px; x = px; }
                if(adjust&CLAMP_RIGHT)    w = px + pw - x;
                if(adjust&CLAMP_TOP)    { h += y - py; y = py; }
                if(adjust&CLAMP_BOTTOM)   h = py + ph - y;
            }

            adjustchildren();
        }

        void setalign(int xalign, int yalign)
        {
            adjust &= ~ALIGN_MASK;
            adjust |= (clamp(xalign, -2, 1)+2)<<ALIGN_HSHIFT;
            adjust |= (clamp(yalign, -2, 1)+2)<<ALIGN_VSHIFT;
        }

        void setclamp(int left, int right, int top, int bottom)
        {
            adjust &= ~CLAMP_MASK;
            if(left) adjust |= CLAMP_LEFT;
            if(right) adjust |= CLAMP_RIGHT;
            if(top) adjust |= CLAMP_TOP;
            if(bottom) adjust |= CLAMP_BOTTOM;
        }

        virtual bool target(float cx, float cy)
        {
            return false;
        }

        virtual bool rawkey(int code, bool isdown)
        {
            loopchildrenrev(o,
            {
                if(o->rawkey(code, isdown)) return true;
            });
            return false;
        }

        virtual bool key(int code, bool isdown)
        {
            loopchildrenrev(o,
            {
                if(o->key(code, isdown)) return true;
            });
            return false;
        }

        virtual bool textinput(const char *str, int len)
        {
            loopchildrenrev(o,
            {
                if(o->textinput(str, len)) return true;
            });
            return false;
        }

        virtual void startdraw() {}
        virtual void enddraw() {}

        void enddraw(int change)
        {
            enddraw();

            changed &= ~change;
            if(changed)
            {
                if(changed&CHANGE_SHADER) hudshader->set();
                if(changed&CHANGE_COLOR) gle::colorf(1, 1, 1);
                if(changed&CHANGE_BLEND) resetblend();
            }
        }

        void changedraw(int change = 0)
        {
            if(!drawing)
            {
                startdraw();
                changed = change;
            }
            else if(drawing->gettype() != gettype())
            {
                drawing->enddraw(change);
                startdraw();
                changed = change;
            }
            drawing = this;
        }

        virtual void draw(float sx, float sy)
        {
            loopchildren(o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
        }

        void resetstate()
        {
            state &= STATE_HOLD_MASK;
            childstate &= STATE_HOLD_MASK;
        }
        void resetchildstate()
        {
            resetstate();
            loopchildren(o, o->resetchildstate());
        }

        bool hasstate(int flags) const { return ((state & ~childstate) & flags) != 0; }
        bool haschildstate(int flags) const { return ((state | childstate) & flags) != 0; }

        #define DOSTATES \
            DOSTATE(STATE_HOVER, hover) \
            DOSTATE(STATE_PRESS, press) \
            DOSTATE(STATE_HOLD, hold) \
            DOSTATE(STATE_RELEASE, release) \
            DOSTATE(STATE_ALT_HOLD, althold) \
            DOSTATE(STATE_ALT_PRESS, altpress) \
            DOSTATE(STATE_ALT_RELEASE, altrelease) \
            DOSTATE(STATE_ESC_HOLD, eschold) \
            DOSTATE(STATE_ESC_PRESS, escpress) \
            DOSTATE(STATE_ESC_RELEASE, escrelease) \
            DOSTATE(STATE_SCROLL_UP, scrollup) \
            DOSTATE(STATE_SCROLL_DOWN, scrolldown)

        bool setstate(int state, float cx, float cy, int mask = 0, bool inside = true, int setflags = 0)
        {
            switch(state)
            {
            #define DOSTATE(flags, func) case flags: func##children(cx, cy, mask, inside, setflags | flags); return haschildstate(flags);
            DOSTATES
            #undef DOSTATE
            }
            return false;
        }

        void clearstate(int flags)
        {
            state &= ~flags;
            if(childstate & flags)
            {
                loopchildren(o, { if((o->state | o->childstate) & flags) o->clearstate(flags); });
                childstate &= ~flags;
            }
        }

        #define propagatestate(o, cx, cy, mask, inside, body) \
            loopchildrenrev(o, \
            { \
                if(((o->state | o->childstate) & mask) != mask) continue; \
                float o##x = cx - o->x; \
                float o##y = cy - o->y; \
                if(!inside) \
                { \
                    o##x = clamp(o##x, 0.0f, o->w); \
                    o##y = clamp(o##y, 0.0f, o->h); \
                    body; \
                } \
                else if(o##x >= 0 && o##x < o->w && o##y >= 0 && o##y < o->h) \
                { \
                    body; \
                } \
            })

        #define DOSTATE(flags, func) \
            virtual void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                propagatestate(o, cx, cy, mask, inside, \
                { \
                    o->func##children(ox, oy, mask, inside, setflags); \
                    childstate |= (o->state | o->childstate) & (setflags); \
                }); \
                if(target(cx, cy)) state |= (setflags); \
                func(cx, cy); \
            } \
            virtual void func(float cx, float cy) {}
        DOSTATES
        #undef DOSTATE

        static const char *typestr() { return "#Object"; }
        virtual const char *gettype() const { return typestr(); }
        virtual const char *getname() const { return gettype(); }
        virtual const char *gettypename() const { return gettype(); }

        template<class T> bool istype() const { return T::typestr() == gettype(); }
        bool isnamed(const char *name) const { return name[0] == '#' ? name == gettypename() : !strcmp(name, getname()); }

        Object *find(const char *name, bool recurse = true, const Object *exclude = NULL) const
        {
            loopchildren(o,
            {
                if(o != exclude && o->isnamed(name)) return o;
            });
            if(recurse) loopchildren(o,
            {
                if(o != exclude)
                {
                    Object *found = o->find(name);
                    if(found) return found;
                }
            });
            return NULL;
        }

        Object *findsibling(const char *name) const
        {
            for(const Object *prev = this, *cur = parent; cur; prev = cur, cur = cur->parent)
            {
                Object *o = cur->find(name, true, prev);
                if(o) return o;
            }
            return NULL;
        }

        template<class T> T *buildtype()
        {
            T *t;
            if(children.inrange(buildchild))
            {
                Object *o = children[buildchild];
                if(o->istype<T>()) t = (T *)o;
                else
                {
                    delete o;
                    t = new T;
                    children[buildchild] = t;
                }
            }
            else
            {
                t = new T;
                children.add(t);
            }
            t->reset(this);
            buildchild++;
            return t;
        }

        void buildchildren(uint *contents)
        {
            if((*contents&CODE_OP_MASK) == CODE_EXIT) children.deletecontents();
            else
            {
                Object *oldparent = buildparent;
                int oldchild = buildchild;
                buildparent = this;
                buildchild = 0;
                executeret(contents);
                while(children.length() > buildchild)
                    delete children.pop();
                buildparent = oldparent;
                buildchild = oldchild;
            }
            resetstate();
        }

        virtual int childcolumns() const { return children.length(); }
    };

    static inline void stopdrawing()
    {
        if(drawing)
        {
            drawing->enddraw(0);
            drawing = NULL;
        }
    }

    struct Window;

    static Window *window = NULL;

    struct Window : Object
    {
        char *name;
        uint *contents, *onshow, *onhide;
        bool allowinput, eschide, abovehud;
        float px, py, pw, ph;
        vec2 sscale, soffset;

        Window(const char *name, const char *contents, const char *onshow, const char *onhide) :
            name(newstring(name)),
            contents(compilecode(contents)),
            onshow(onshow && onshow[0] ? compilecode(onshow) : NULL),
            onhide(onhide && onhide[0] ? compilecode(onhide) : NULL),
            allowinput(true), eschide(true), abovehud(false),
            px(0), py(0), pw(0), ph(0),
            sscale(1, 1), soffset(0, 0)
        {
        }
        ~Window()
        {
            delete[] name;
            freecode(contents);
            freecode(onshow);
            freecode(onhide);
        }

        static const char *typestr() { return "#Window"; }
        const char *gettype() const { return typestr(); }
        const char *getname() const { return name; }

        void build();

        void hide()
        {
            Object::hide();
            if(onhide) execute(onhide);
        }

        void show()
        {
            state |= STATE_HIDDEN;
            clearstate(STATE_HOLD_MASK);
            if(onshow) execute(onshow);
        }

        void setup()
        {
            Object::setup();
            allowinput = eschide = true;
            abovehud = false;
            px = py = pw = ph = 0;
        }

        void layout()
        {
            if(state&STATE_HIDDEN) { w = h = 0; return; }
            window = this;
            Object::layout();
            window = NULL;
        }

        void draw(float sx, float sy)
        {
            if(state&STATE_HIDDEN) return;
            window = this;

            projection();
            hudshader->set();

            glEnable(GL_BLEND);
            blendtype = BLEND_ALPHA;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gle::colorf(1, 1, 1);

            changed = 0;
            drawing = NULL;

            Object::draw(sx, sy);

            stopdrawing();

            glDisable(GL_BLEND);

            window = NULL;
        }

        void draw()
        {
            draw(x, y);
        }

        void adjustchildren()
        {
            if(state&STATE_HIDDEN) return;
            window = this;
            Object::adjustchildren();
            window = NULL;
        }

        void adjustlayout()
        {
            float aspect = float(hudw)/hudh;
            ph = max(max(h, w/aspect), 1.0f);
            pw = aspect*ph;
            Object::adjustlayout(0, 0, pw, ph);
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                if(!allowinput || state&STATE_HIDDEN || pw <= 0 || ph <= 0) return; \
                cx = cx*pw + px-x; \
                cy = cy*ph + py-y; \
                if(!inside || (cx >= 0 && cy >= 0 && cx < w && cy < h)) \
                    Object::func##children(cx, cy, mask, inside, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void escrelease(float cx, float cy);

        void projection()
        {
            hudmatrix.ortho(px, px + pw, py + ph, py, -1, 1);
            resethudmatrix();
            sscale = vec2(hudmatrix.a.x, hudmatrix.b.y).mul(0.5f);
            soffset = vec2(hudmatrix.d.x, hudmatrix.d.y).mul(0.5f).add(0.5f);
        }

        void calcscissor(float x1, float y1, float x2, float y2, int &sx1, int &sy1, int &sx2, int &sy2, bool clip = true)
        {
            vec2 s1 = vec2(x1, y2).mul(sscale).add(soffset),
                 s2 = vec2(x2, y1).mul(sscale).add(soffset);
            sx1 = int(floor(s1.x*hudw + 0.5f));
            sy1 = int(floor(s1.y*hudh + 0.5f));
            sx2 = int(floor(s2.x*hudw + 0.5f));
            sy2 = int(floor(s2.y*hudh + 0.5f));
            if(clip)
            {
                sx1 = clamp(sx1, 0, hudw);
                sy1 = clamp(sy1, 0, hudh);
                sx2 = clamp(sx2, 0, hudw);
                sy2 = clamp(sy2, 0, hudh);
            }
        }

        float calcabovehud()
        {
            return 1 - (y*sscale.y + soffset.y);
        }
    };

    static hashnameset<Window *> windows;

    void ClipArea::scissor()
    {
        int sx1, sy1, sx2, sy2;
        window->calcscissor(x1, y1, x2, y2, sx1, sy1, sx2, sy2);
        glScissor(sx1, sy1, sx2-sx1, sy2-sy1);
    }

    struct World : Object
    {
        static const char *typestr() { return "#World"; }
        const char *gettype() const { return typestr(); }

        #define loopwindows(o, body) do { \
            loopv(children) \
            { \
                Window *o = (Window *)children[i]; \
                body; \
            } \
        } while(0)

        #define loopwindowsrev(o, body) do { \
            loopvrev(children) \
            { \
                Window *o = (Window *)children[i]; \
                body; \
            } \
        } while(0)

        void adjustchildren()
        {
            loopwindows(w, w->adjustlayout());
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                loopwindowsrev(w, \
                { \
                    if(((w->state | w->childstate) & mask) != mask) continue; \
                    w->func##children(cx, cy, mask, inside, setflags); \
                    int wflags = (w->state | w->childstate) & (setflags); \
                    if(wflags) { childstate |= wflags; break; } \
                }); \
            }
        DOSTATES
        #undef DOSTATE

        void build()
        {
            reset();
            setup();
            loopwindows(w,
            {
                w->build();
                if(!children.inrange(i)) break;
                if(children[i] != w) i--;
            });
            resetstate();
        }

        void hide() { Object::hide(); };

        bool show(Window *w)
        {
            if(children.find(w) >= 0) return false;
            w->resetchildstate();
            children.add(w);
            w->show();
            return true;
        }

        void hide(Window *w, int index)
        {
            ::textinput(false, TI_GUI);
            ::keyrepeat(false, KR_GUI);
            children.remove(index);
            childstate = 0;
            loopchildren(o, childstate |= o->state | o->childstate);
            w->hide();
        }

        bool hide(Window *w)
        {
            int index = children.find(w);
            if(index < 0) return false;
            hide(w, index);
            return true;
        }

        bool hidetop()
        {
            loopwindowsrev(w, { if(w->allowinput && !(w->state&STATE_HIDDEN)) { hide(w, i); return true; } });
            return false;
        }

        int hideall()
        {
            int hidden = 0;
            loopwindowsrev(w,
            {
                hide(w, i);
                hidden++;
            });
            return hidden;
        }

        bool allowinput() const { loopwindows(w, { if(w->allowinput && !(w->state&STATE_HIDDEN)) return true; }); return false; }

        void draw(float sx, float sy) {}

        void draw()
        {
            if(children.empty()) return;

            loopwindows(w, w->draw());
        }

        float abovehud()
        {
            float y = 1;
            loopwindows(w, { if(w->abovehud && !(w->state&STATE_HIDDEN)) y = min(y, w->calcabovehud()); });
            return y;
        }
    };

    static World *world = NULL;

    void Window::escrelease(float cx, float cy)
    {
        if(eschide) world->hide(this);
    }

    void Window::build()
    {
        reset(world);
        setup();
        window = this;
        buildchildren(contents);
        window = NULL;
    }

    struct HorizontalList : Object
    {
        float space, subw;

        static const char *typestr() { return "#HorizontalList"; }
        const char *gettype() const { return typestr(); }

        void setup(float space_ = 0)
        {
            Object::setup();
            space = space_ * uiscale;
        }

        uchar childalign() const { return ALIGN_VCENTER; }

        void layout()
        {
            subw = h = 0;
            loopchildren(o,
            {
                o->x = subw;
                o->y = 0;
                o->layout();
                subw += o->w;
                h = max(h, o->y + o->h);
            });
            w = subw + space*max(children.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offset = 0, sx = 0, cspace = (w - subw) / max(children.length() - 1, 1), cstep = (w - subw) / children.length();
            for(int i = 0; i < children.length(); i++)
            {
                Object *o = children[i];
                o->x = offset;
                offset += o->w + cspace;
                float sw = o->w + cstep;
                o->adjustlayout(sx, 0, sw, h);
                sx += sw;
            }
        }
    };

    struct VerticalList : Object
    {
        float space, subh;

        static const char *typestr() { return "#VerticalList"; }
        const char *gettype() const { return typestr(); }

        void setup(float space_ = 0)
        {
            Object::setup();
            space = space_ * uiscale;
        }

        uchar childalign() const { return ALIGN_HCENTER; }

        void layout()
        {
            w = subh = 0;
            loopchildren(o,
            {
                o->x = 0;
                o->y = subh;
                o->layout();
                subh += o->h;
                w = max(w, o->x + o->w);
            });
            h = subh + space*max(children.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offset = 0, sy = 0, rspace = (h - subh) / max(children.length() - 1, 1), rstep = (h - subh) / children.length();
            loopchildren(o,
            {
                o->y = offset;
                offset += o->h + rspace;
                float sh = o->h + rstep;
                o->adjustlayout(0, sy, w, sh);
                sy += sh;
            });
        }
    };

    struct Grid : Object
    {
        int columns;
        float spacew, spaceh, subw, subh;
        vector<float> widths, heights;

        static const char *typestr() { return "#Grid"; }
        const char *gettype() const { return typestr(); }

        void setup(int columns_, float spacew_ = 0, float spaceh_ = 0)
        {
            Object::setup();
            columns = columns_;
            spacew = spacew_ * uiscale;
            spaceh = spaceh_ * uiscale;
        }

        uchar childalign() const { return 0; }

        void layout()
        {
            widths.setsize(0);
            heights.setsize(0);

            int column = 0, row = 0;
            loopchildren(o,
            {
                o->layout();
                if(column >= widths.length()) widths.add(o->w);
                else if(o->w > widths[column]) widths[column] = o->w;
                if(row >= heights.length()) heights.add(o->h);
                else if(o->h > heights[row]) heights[row] = o->h;
                column = (column + 1) % columns;
                if(!column) row++;
            });

            subw = subh = 0;
            loopv(widths) subw += widths[i];
            loopv(heights) subh += heights[i];
            w = subw + spacew*max(widths.length() - 1, 0);
            h = subh + spaceh*max(heights.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            int row = 0, column = 0;
            float offsety = 0, sy = 0, offsetx = 0, sx = 0,
                  cspace = (w - subw) / max(widths.length() - 1, 1),
                  cstep = (w - subw) / widths.length(),
                  rspace = (h - subh) / max(heights.length() - 1, 1),
                  rstep = (h - subh) / heights.length();
            loopchildren(o,
            {
                o->x = offsetx;
                o->y = offsety;
                o->adjustlayout(sx, sy, widths[column] + cstep, heights[row] + rstep);
                offsetx += widths[column] + cspace;
                sx += widths[column] + cstep;
                column = (column + 1) % columns;
                if(!column)
                {
                    offsetx = sx = 0;
                    offsety += heights[row] + rspace;
                    sy += heights[row] + rstep;
                    row++;
                }
            });
        }
    };

    struct TableHeader : Object
    {
        int columns;

        TableHeader() : columns(-1) {}

        static const char *typestr() { return "#TableHeader"; }
        const char *gettype() const { return typestr(); }

        uchar childalign() const { return columns < 0 ? ALIGN_VCENTER : ALIGN_HCENTER | ALIGN_VCENTER; }

        int childcolumns() const { return columns; }

        void buildchildren(uint *columndata, uint *contents)
        {
            Object *oldparent = buildparent;
            int oldchild = buildchild;
            buildparent = this;
            buildchild = 0;
            executeret(columndata);
            if(columns != buildchild) while(children.length() > buildchild) delete children.pop();
            columns = buildchild;
            if((*contents&CODE_OP_MASK) != CODE_EXIT) executeret(contents);
            while(children.length() > buildchild) delete children.pop();
            buildparent = oldparent;
            buildchild = oldchild;
            resetstate();
        }

        void adjustchildren()
        {
            loopchildrange(columns, children.length(), o, o->adjustlayout(0, 0, w, h));
        }

        void draw(float sx, float sy)
        {
            loopchildrange(columns, children.length(), o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
            loopchildrange(0, columns, o,
            {
                if(!isfullyclipped(sx + o->x, sy + o->y, o->w, o->h))
                    o->draw(sx + o->x, sy + o->y);
            });
        }
    };

    struct TableRow : TableHeader
    {
        static const char *typestr() { return "#TableRow"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }
    };

    #define BUILDCOLUMNS(type, o, setup, columndata, contents) do { \
        if(buildparent) \
        { \
            type *o = buildparent->buildtype<type>(); \
            setup; \
            o->buildchildren(columndata, contents); \
        } \
    } while(0)

    struct Table : Object
    {
        float spacew, spaceh, subw, subh;
        vector<float> widths;

        static const char *typestr() { return "#Table"; }
        const char *gettype() const { return typestr(); }

        void setup(float spacew_ = 0, float spaceh_ = 0)
        {
            Object::setup();
            spacew = spacew_ * uiscale;
            spaceh = spaceh_ * uiscale;
        }

        uchar childalign() const { return 0; }

        void layout()
        {
            widths.setsize(0);

            w = subh = 0;
            loopchildren(o,
            {
                o->layout();
                int cols = o->childcolumns();
                while(widths.length() < cols) widths.add(0);
                loopj(cols)
                {
                    Object *c = o->children[j];
                    if(c->w > widths[j]) widths[j] = c->w;
                }
                w = max(w, o->w);
                subh += o->h;
            });

            subw = 0;
            loopv(widths) subw += widths[i];
            w = max(w, subw + spacew*max(widths.length() - 1, 0));
            h = subh + spaceh*max(children.length() - 1, 0);
        }

        void adjustchildren()
        {
            if(children.empty()) return;

            float offsety = 0, sy = 0,
                  cspace = (w - subw) / max(widths.length() - 1, 1),
                  cstep = (w - subw) / widths.length(),
                  rspace = (h - subh) / max(children.length() - 1, 1),
                  rstep = (h - subh) / children.length();
            loopchildren(o,
            {
                o->x = 0;
                o->y = offsety;
                o->w = w;
                offsety += o->h + rspace;
                float sh = o->h + rstep;
                o->adjustlayout(0, sy, w, sh);
                sy += sh;

                float offsetx = 0;
                float sx = 0;
                int cols = o->childcolumns();
                loopj(cols)
                {
                    Object *c = o->children[j];
                    c->x = offsetx;
                    offsetx += widths[j] + cspace;
                    float sw = widths[j] + cstep;
                    c->adjustlayout(sx, 0, sw, o->h);
                    sx += sw;
                }
            });
        }
    };

    struct Spacer : Object
    {
        float spacew, spaceh;

        void setup(float spacew_, float spaceh_)
        {
            Object::setup();
            spacew = spacew_ * uiscale;
            spaceh = spaceh_ * uiscale;
        }

        static const char *typestr() { return "#Spacer"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            w = spacew;
            h = spaceh;
            loopchildren(o,
            {
                o->x = spacew;
                o->y = spaceh;
                o->layout();
                w = max(w, o->x + o->w);
                h = max(h, o->y + o->h);
            });
            w += spacew;
            h += spaceh;
        }

        void adjustchildren()
        {
            adjustchildrento(spacew, spaceh, w - 2*spacew, h - 2*spaceh);
        }
    };

    struct Offsetter : Object
    {
        float offsetx, offsety;

        void setup(float offsetx_, float offsety_)
        {
            Object::setup();
            offsetx = offsetx_ * uiscale;
            offsety = offsety_ * uiscale;
        }

        static const char *typestr() { return "#Offsetter"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            Object::layout();

            loopchildren(o,
            {
                o->x += offsetx;
                o->y += offsety;
            });

            w += offsetx;
            h += offsety;
        }

        void adjustchildren()
        {
            adjustchildrento(offsetx, offsety, w - offsetx, h - offsety);
        }
    };

    struct Filler : Object
    {
        float minw, minh;

        void setup(float minw_, float minh_)
        {
            Object::setup();
            minw = minw_ * uiscale;
            minh = minh_ * uiscale;
        }

        static const char *typestr() { return "#Filler"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            Object::layout();

            w = max(w, minw);
            h = max(h, minh);
        }
    };

    struct Target : Filler
    {
        static const char *typestr() { return "#Target"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }
    };

    struct Color
    {
        uchar r, g, b, a;

        Color() {}
        Color(uint c) : r((c>>16)&0xFF), g((c>>8)&0xFF), b(c&0xFF), a(c>>24 ? c>>24 : 0xFF) {}
        Color(uint c, uchar a) : r((c>>16)&0xFF), g((c>>8)&0xFF), b(c&0xFF), a(a) {}
        Color(uchar r, uchar g, uchar b, uchar a = 255) : r(r), g(g), b(b), a(a) {}

        void init() { gle::colorub(r, g, b, a); }
        void attrib() { gle::attribub(r, g, b, a); }

        static void def() { gle::defcolor(4, GL_UNSIGNED_BYTE); }

        bool operator==(const Color &other) const
        {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }
        bool operator!=(const Color &other) const
        {
            return r != other.r || g != other.g || b != other.b || a != other.a;
        }
    };

    struct FillColor : Target
    {
        enum { SOLID = 0, MODULATE };

        int type;
        Color color;

        void setup(int type_, const Color &color_, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_);
            type = type_;
            color = color_;
        }

        static const char *typestr() { return "#FillColor"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            color.init();
            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(sx+w, sy);
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy+h);
            gle::attribf(sx,   sy+h);
            gle::end();

            Object::draw(sx, sy);
        }
    };

    struct Gradient : FillColor
    {
        enum { VERTICAL, HORIZONTAL };

        int dir;
        Color color2;

        void setup(int type_, int dir_, const Color &color_, const Color &color2_, float minw_ = 0, float minh_ = 0)
        {
            FillColor::setup(type_, color_, minw_, minh_);
            dir = dir_;
            color2 = color2_;
        }

        static const char *typestr() { return "#Gradient"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
            Color::def();
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            gle::begin(GL_TRIANGLE_STRIP);
            gle::attribf(sx+w, sy);   (dir == HORIZONTAL ? color2 : color).attrib();
            gle::attribf(sx,   sy);   color.attrib();
            gle::attribf(sx+w, sy+h); color2.attrib();
            gle::attribf(sx,   sy+h); (dir == HORIZONTAL ? color : color2).attrib();
            gle::end();

            Object::draw(sx, sy);
        }
    };

    struct Line : Filler
    {
        Color color;
        bool flip;

        void setup(const Color &color_, float minw_ = 0, float minh_ = 0, bool flip_ = false)
        {
            Filler::setup(minw_, minh_);
            color = color_;
            this->flip = flip_;
        }

        static const char *typestr() { return "#Line"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            color.init();
            gle::begin(GL_LINES);
            if (flip)
            {
                gle::attribf(sx,   sy+h);
                gle::attribf(sx+w, sy);
            }
            else
            {
                gle::attribf(sx,   sy);
                gle::attribf(sx+w, sy+h);
            }
            gle::end();

            Object::draw(sx, sy);
        }
    };

    struct Outline : Filler
    {
        Color color;

        void setup(const Color &color_, float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);
            color = color_;
        }

        static const char *typestr() { return "#Outline"; }
        const char *gettype() const { return typestr(); }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }

        void draw(float sx, float sy)
        {
            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            color.init();
            gle::begin(GL_LINE_LOOP);
            gle::attribf(sx,   sy);
            gle::attribf(sx+w, sy);
            gle::attribf(sx+w, sy+h);
            gle::attribf(sx,   sy+h);
            gle::end();

            Object::draw(sx, sy);
        }
    };

    static inline bool checkalphamask(Texture *tex, float x, float y)
    {
        if(!tex->alphamask)
        {
            loadalphamask(tex);
            if(!tex->alphamask) return true;
        }
        int tx = clamp(int(x*tex->xs), 0, tex->xs-1),
            ty = clamp(int(y*tex->ys), 0, tex->ys-1);
        if(tex->alphamask[ty*((tex->xs+7)/8) + tx/8] & (1<<(tx%8))) return true;
        return false;
    }

    struct Minimap : Filler
    {
        vec origin;
        float yaw, size;
        int sides;

        void setup(vec origin_, float yaw_, float size_, int sides_)
        {
            origin = vec(origin_);
            yaw = yaw_;
            size = size_;
            sides = clamp(sides_, 3, 64);
            Filler::setup(size_, size_);
        }

        static const char *typestr() { return "#Minimap"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        void startdraw()
        {
            gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_TRIANGLE_FAN);
        }

        void enddraw()
        {
            gle::end();
        }

        void bindtex()
        {
            changedraw(CHANGE_SHADER);

            SETSHADER(hudminimap);
            LOCALPARAMF(minimapalpha, game::minimapalpha);
            bindminimap();
        }

        void draw(float sx, float sy)
        {
            vec pos = vec(origin).sub(minimapcenter).mul(minimapscale).add(0.5f), dir;
            vecfromyawpitch(yaw, 0, 1, 0, dir);
            float scale = game::calcradarscale();
            bindtex();
            loopi(sides)
            {
                vec v = vec(0, -1, 0).rotate_around_z(i/float(sides)*2*M_PI);
                gle::attribf(sx + 0.5f*w*(1.0f + v.x), sy + 0.5f*h*(1.0f + v.y));
                vec tc = vec(dir).rotate_around_z(i/float(sides)*2*M_PI);
                gle::attribf(1.0f - (pos.x + tc.x*scale*minimapscale.x), pos.y + tc.y*scale*minimapscale.y);
            }
        }
    };

    struct Image : Filler
    {
        static Texture *lasttex;

        Texture *tex;
        float angle;

        void setup(Texture *tex_, float minw_ = 0, float minh_ = 0, float angle_ = 0)
        {
            Filler::setup(minw_, minh_);
            tex = tex_;
            angle = angle_;
        }

        static const char *typestr() { return "#Image"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return !(tex->type&Texture::ALPHA) || checkalphamask(tex, cx/w, cy/h);
        }

        void startdraw()
        {
            lasttex = NULL;

            gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_QUADS);
        }

        void enddraw()
        {
            gle::end();
        }

        void bindtex()
        {
            changedraw();
            if(lasttex != tex)
            {
                if(lasttex) gle::end();
                lasttex = tex;
                setusedtexture(tex);
            }
        }

        void draw(float sx, float sy)
        {
            if(tex != notexture)
            {
                bindtex();
                if(angle != 0) quads_rotation(sx, sy, w, h, angle);
                else quads(sx, sy, w, h);
            }

            Object::draw(sx, sy);
        }
    };

    Texture *Image::lasttex = NULL;

    struct CroppedImage : Image
    {
        float cropx, cropy, cropw, croph;

        void setup(Texture *tex_, float minw_ = 0, float minh_ = 0, float cropx_ = 0, float cropy_ = 0, float cropw_ = 1, float croph_ = 1)
        {
            Image::setup(tex_, minw_, minh_);
            cropx = cropx_;
            cropy = cropy_;
            cropw = cropw_;
            croph = croph_;
        }

        static const char *typestr() { return "#CroppedImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return !(tex->type&Texture::ALPHA) || checkalphamask(tex, cropx + cx/w*cropw, cropy + cy/h*croph);
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            bindtex();
            quads(sx, sy, w, h, cropx, cropy, cropw, croph);

            Object::draw(sx, sy);
        }
    };

    struct StretchedImage : Image
    {
        static const char *typestr() { return "#StretchedImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(!(tex->type&Texture::ALPHA)) return true;

            float mx, my;
            if(w <= minw) mx = cx/w;
            else if(cx < minw/2) mx = cx/minw;
            else if(cx >= w - minw/2) mx = 1 - (w - cx) / minw;
            else mx = 0.5f;
            if(h <= minh) my = cy/h;
            else if(cy < minh/2) my = cy/minh;
            else if(cy >= h - minh/2) my = 1 - (h - cy) / minh;
            else my = 0.5f;

            return checkalphamask(tex, mx, my);
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            bindtex();

            float splitw = (minw ? min(minw, w) : w) / 2,
                  splith = (minh ? min(minh, h) : h) / 2,
                  vy = sy, ty = 0;
            loopi(3)
            {
                float vh = 0, th = 0;
                switch(i)
                {
                    case 0: if(splith < h - splith) { vh = splith; th = 0.5f; } else { vh = h; th = 1; } break;
                    case 1: vh = h - 2*splith; th = 0; break;
                    case 2: vh = splith; th = 0.5f; break;
                }
                float vx = sx, tx = 0;
                loopj(3)
                {
                    float vw = 0, tw = 0;
                    switch(j)
                    {
                        case 0: if(splitw < w - splitw) { vw = splitw; tw = 0.5f; } else { vw = w; tw = 1; } break;
                        case 1: vw = w - 2*splitw; tw = 0; break;
                        case 2: vw = splitw; tw = 0.5f; break;
                    }
                    quads(vx, vy, vw, vh, tx, ty, tw, th);
                    vx += vw;
                    tx += tw;
                    if(tx >= 1) break;
                }
                vy += vh;
                ty += th;
                if(ty >= 1) break;
            }

            Object::draw(sx, sy);
        }
    };

    struct BorderedImage : Image
    {
        float texborder, screenborder;

        void setup(Texture *tex_, float texborder_, float screenborder_)
        {
            Image::setup(tex_);
            texborder = texborder_;
            screenborder = screenborder_;
        }

        static const char *typestr() { return "#BorderedImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(!(tex->type&Texture::ALPHA)) return true;

            float mx, my;
            if(cx < screenborder) mx = cx/screenborder*texborder;
            else if(cx >= w - screenborder) mx = 1-texborder + (cx - (w - screenborder))/screenborder*texborder;
            else mx = texborder + (cx - screenborder)/(w - 2*screenborder)*(1 - 2*texborder);
            if(cy < screenborder) my = cy/screenborder*texborder;
            else if(cy >= h - screenborder) my = 1-texborder + (cy - (h - screenborder))/screenborder*texborder;
            else my = texborder + (cy - screenborder)/(h - 2*screenborder)*(1 - 2*texborder);

            return checkalphamask(tex, mx, my);
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            bindtex();

            float vy = sy, ty = 0;
            loopi(3)
            {
                float vh = 0, th = 0;
                switch(i)
                {
                    case 0: vh = screenborder; th = texborder; break;
                    case 1: vh = h - 2*screenborder; th = 1 - 2*texborder; break;
                    case 2: vh = screenborder; th = texborder; break;
                }
                float vx = sx, tx = 0;
                loopj(3)
                {
                    float vw = 0, tw = 0;
                    switch(j)
                    {
                        case 0: vw = screenborder; tw = texborder; break;
                        case 1: vw = w - 2*screenborder; tw = 1 - 2*texborder; break;
                        case 2: vw = screenborder; tw = texborder; break;
                    }
                    quads(vx, vy, vw, vh, tx, ty, tw, th);
                    vx += vw;
                    tx += tw;
                }
                vy += vh;
                ty += th;
            }

            Object::draw(sx, sy);
        }
    };

    struct TiledImage : Image
    {
        float tilew, tileh;

        void setup(Texture *tex_, float minw_ = 0, float minh_ = 0, float tilew_ = 0, float tileh_ = 0)
        {
            Image::setup(tex_, minw_, minh_);
            tilew = tilew_ * uiscale;
            tileh = tileh_ * uiscale;
        }

        static const char *typestr() { return "#TiledImage"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(!(tex->type&Texture::ALPHA)) return true;

            return checkalphamask(tex, fmod(cx/tilew, 1), fmod(cy/tileh, 1));
        }

        void draw(float sx, float sy)
        {
            if(tex == notexture) { Object::draw(sx, sy); return; }

            bindtex();

            if(tex->clamp)
            {
                for(float dy = 0; dy < h; dy += tileh)
                {
                    float dh = min(tileh, h - dy);
                    for(float dx = 0; dx < w; dx += tilew)
                    {
                        float dw = min(tilew, w - dx);
                        quads(sx + dx, sy + dy, dw, dh, 0, 0, dw / tilew, dh / tileh);
                    }
                }
            }
            else quads(sx, sy, w, h, 0, 0, w/tilew, h/tileh);

            Object::draw(sx, sy);
        }
    };

    struct Shape : Filler
    {
        enum { SOLID = 0, OUTLINE, MODULATE };

        int type;
        Color color;

        void setup(const Color &color_, int type_ = SOLID, float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);

            color = color_;
            type = type_;
        }

        void startdraw()
        {
            hudnotextureshader->set();
            gle::defvertex(2);
        }
    };

    struct Triangle : Shape
    {
        vec2 a, b, c;

        void setup(const Color &color_, float w = 0, float h = 0, int angle = 0, int type_ = SOLID)
        {
            a = vec2(0, -h*2.0f/3);
            b = vec2(-w/2, h/3);
            c = vec2(w/2, h/3);
            if(angle)
            {
                vec2 rot = sincosmod360(-angle);
                a.rotate_around_z(rot);
                b.rotate_around_z(rot);
                c.rotate_around_z(rot);
            }
            vec2 bbmin = vec2(a).min(b).min(c);
            a.sub(bbmin);
            b.sub(bbmin);
            c.sub(bbmin);
            vec2 bbmax = vec2(a).max(b).max(c);

            Shape::setup(color_, type_, bbmax.x, bbmax.y);
        }

        static const char *typestr() { return "#Triangle"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(type == OUTLINE) return false;
            bool side = vec2(cx, cy).sub(b).cross(vec2(a).sub(b)) < 0;
            return (vec2(cx, cy).sub(c).cross(vec2(b).sub(c)) < 0) == side &&
                   (vec2(cx, cy).sub(a).cross(vec2(c).sub(a)) < 0) == side;
        }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            color.init();
            gle::begin(type == OUTLINE ? GL_LINE_LOOP : GL_TRIANGLES);
            gle::attrib(vec2(sx, sy).add(a));
            gle::attrib(vec2(sx, sy).add(b));
            gle::attrib(vec2(sx, sy).add(c));
            gle::end();
        }
    };

    struct Circle : Shape
    {
        float radius;

        void setup(const Color &color_, float size, int type_ = SOLID)
        {
            Shape::setup(color_, type_, size, size);

            radius = size/2;
        }

        static const char *typestr() { return "#Circle"; }
        const char *gettype() const { return typestr(); }

        bool target(float cx, float cy)
        {
            if(type == OUTLINE) return false;
            float r = radius <= 0 ? min(w, h)/2 : radius;
            return vec2(cx, cy).sub(r).squaredlen() <= r*r;
        }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER | CHANGE_COLOR | CHANGE_BLEND);
            if(type==MODULATE) modblend(); else resetblend();

            float r = radius <= 0 ? min(w, h)/2 : radius;
            color.init();
            vec2 center(sx + r, sy + r);
            if(type == OUTLINE)
            {
                gle::begin(GL_LINE_LOOP);
                for(int angle = 0; angle < 360; angle += 360/15)
                    gle::attrib(vec2(sincos360[angle]).mul(r).add(center));
                gle::end();
            }
            else
            {
                gle::begin(GL_TRIANGLE_FAN);
                gle::attrib(center);
                gle::attribf(center.x + r, center.y);
                for(int angle = 360/15; angle < 360; angle += 360/15)
                {
                    vec2 p = vec2(sincos360[angle]).mul(r).add(center);
                    gle::attrib(p);
                    gle::attrib(p);
                }
                gle::attribf(center.x + r, center.y);
                gle::end();
            }
        }
    };

    FVAR(uitextscale, 1, 0, 0);

    #define SETSTR(dst, src) do { \
        if(dst) { if(dst != src && strcmp(dst, src)) { delete[] dst; dst = newstring(src); } } \
        else dst = newstring(src); \
    } while(0)

    // text attributes
    static int curwrapalign = -1, curshadow = 0, curfontoutlinealpha = 0;
    static float curfontoutline = 0.f;
    static int curjustify = false, curnofallback = false;
    static const char *curlanguage = newstring("");

    #define WITHTEXTATTR(name, tmp, val, body) do { \
        tmp = cur##name; \
        cur##name = val; \
        body; \
        cur##name = tmp; \
    } while(0)

    struct WrapAlign : Object
    {
        static const char *typestr() { return "#WrapAlign"; }
        const char *gettype() const { return typestr(); }

        int val, tmp;
        void setup(int val_) { val = val_; }
        void layout()                      { WITHTEXTATTR(wrapalign, tmp, val, Object::layout()); }
        void draw(float sx, float sy)      { WITHTEXTATTR(wrapalign, tmp, val, Object::draw(sx, sy)); }
        void buildchildren(uint *contents) { WITHTEXTATTR(wrapalign, tmp, val, Object::buildchildren(contents)); }
    };
    struct Justify : Object
    {
        static const char *typestr() { return "#Justify"; }
        const char *gettype() const { return typestr(); }

        int val, tmp;
        void setup(int val_) { val = val_; }
        void layout()                      { WITHTEXTATTR(justify, tmp, val, Object::layout()); }
        void draw(float sx, float sy)      { WITHTEXTATTR(justify, tmp, val, Object::draw(sx, sy)); }
        void buildchildren(uint *contents) { WITHTEXTATTR(justify, tmp, val, Object::buildchildren(contents)); }
    };
    struct Shadow : Object
    {
        static const char *typestr() { return "#Shadow"; }
        const char *gettype() const { return typestr(); }

        int val, tmp;
        void setup(int val_) { val = val_; }
        void layout()                      { WITHTEXTATTR(shadow, tmp, val, Object::layout()); }
        void draw(float sx, float sy)      { WITHTEXTATTR(shadow, tmp, val, Object::draw(sx, sy)); }
        void buildchildren(uint *contents) { WITHTEXTATTR(shadow, tmp, val, Object::buildchildren(contents)); }
    };
    struct FontOutline : Object
    {
        static const char *typestr() { return "#FontOutline"; }
        const char *gettype() const { return typestr(); }

        float val, tmp;
        int alpha_val, alpha_tmp;
        void setup(float val_, int alpha_val_) { val = val_; alpha_val = alpha_val_; }
        void layout()                      { WITHTEXTATTR(fontoutline, tmp, val, WITHTEXTATTR(fontoutlinealpha, alpha_tmp, alpha_val, Object::layout())); }
        void draw(float sx, float sy)      { WITHTEXTATTR(fontoutline, tmp, val, WITHTEXTATTR(fontoutlinealpha, alpha_tmp, alpha_val, Object::draw(sx, sy))); }
        void buildchildren(uint *contents) { WITHTEXTATTR(fontoutline, tmp, val, WITHTEXTATTR(fontoutlinealpha, alpha_tmp, alpha_val, Object::buildchildren(contents))); }
    };
    struct NoFallback : Object
    {
        static const char *typestr() { return "#NoFallback"; }
        const char *gettype() const { return typestr(); }

        int val, tmp;
        void setup(int val_) { val = val_; }
        void layout()                      { WITHTEXTATTR(nofallback, tmp, val, Object::layout()); }
        void draw(float sx, float sy)      { WITHTEXTATTR(nofallback, tmp, val, Object::draw(sx, sy)); }
        void buildchildren(uint *contents) { WITHTEXTATTR(nofallback, tmp, val, Object::buildchildren(contents)); }
    };
    struct Language : Object
    {
        static const char *typestr() { return "#Language"; }
        const char *gettype() const { return typestr(); }

        const char *val, *tmp;
        Language() : val(nullptr), tmp(nullptr) {}
        ~Language() { DELETEA(val); DELETEA(tmp); }
        void setup(const char *val_) { SETSTR(val, val_); }
        void layout()                      { SETSTR(tmp, curlanguage); SETSTR(curlanguage, val); Object::layout(); SETSTR(curlanguage, tmp); }
        void draw(float sx, float sy)      { SETSTR(tmp, curlanguage); SETSTR(curlanguage, val); Object::draw(sx, sy); SETSTR(curlanguage, tmp); }
        void buildchildren(uint *contents) { SETSTR(tmp, curlanguage); SETSTR(curlanguage, val); Object::buildchildren(contents); SETSTR(curlanguage, tmp); }
    };

    #undef WITHTEXTATTR

    static int uicursorindex = -1;
    ICOMMAND(uicursorindex, "", (), intret(uicursorindex));

    // makes the text input cursor stop blinking for a short while, call this when setting the cursor position from cubescript
    ICOMMAND(resetcursorblink, "", (), { inputmillis = totalmillis; });

    string uikeycode, uitextinput;
    ICOMMAND(uikeycode, "", (), result(uikeycode));
    ICOMMAND(uitextinput, "", (), result(uitextinput));
    
    ICOMMAND(uisettextinput, "i", (int *val),
    {
        ::textinput(*val ? true : false, TI_GUI);
        ::keyrepeat(*val ? true : false, KR_GUI);
    });
  
    // NOTE: `scale` is the text height in screenfuls at `uiscale 1`
    struct Text : Object
    {
        float scale, wrap, alpha;
        Color color;
        text::Label label;
        int fontid, lastchange;
        int align, shadow, outlinealpha;
        float outline;
        int justify, nofallback;
        const char *language;
        int cursor;
        bool has_cursor;
        bool changed;
        uint crc; // string hash used for change detection

        Text() : scale(0), wrap(0), color(0), lastchange(0), align(curwrapalign), shadow(curshadow), outlinealpha(curfontoutlinealpha), outline(curfontoutline), justify(curjustify), nofallback(curnofallback), language(nullptr), cursor(-1), has_cursor(false), crc(0) {}

        void setup(float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1, int cursor_ = -1, bool has_cursor_ = false, float alpha_ = 1)
        {
            Object::setup();
            changed = false;
            const float newscale = scale_ * uiscale;
            const int curfontid = getcurrentfontid();
            if(!uimillis || (totalmillis - lastchange >= uimillis) || !lastchange)
            {
                if(
                    newscale            != scale                 ||
                    color_              != color                 ||
                    wrap_               != wrap                  ||
                    fontid              != curfontid             ||
                    curwrapalign        != align                 ||
                    curjustify          != justify               ||
                    curshadow           != shadow                ||
                    curfontoutline      != outline               ||
                    curfontoutlinealpha != outlinealpha          ||
                    curnofallback       != nofallback            ||
                    (!language || strcmp(curlanguage, language)) ||
                    cursor_             != cursor                ||
                    cursor_             >= 0 // ensures the cursor blinks
                )
                {
                    changed = true;
                    lastchange = totalmillis;
                }

                scale        = newscale;
                color        = color_;
                wrap         = wrap_;
                fontid       = curfontid;
                align        = curwrapalign;
                justify      = curjustify;
                shadow       = curshadow;
                outline      = curfontoutline;
                outlinealpha = curfontoutlinealpha;
                nofallback   = curnofallback;
                SETSTR(language, curlanguage);
                cursor       = cursor_;
            }
            alpha = alpha_;
            has_cursor = has_cursor_;
        }

        void press(float cx, float cy)
        {
            if(!has_cursor) return;
            const float k = drawscale();
            uicursorindex = label.xy_to_index(cx/k, cy/k);
        }
        bool rawkey(int code, bool isdown)
        {
            if(Object::rawkey(code, isdown)) return true;
            if(!isdown || cursor < 0) return false;
            const char *keyname = getkeyname(code);
            if(!keyname) return false;
            copystring(uikeycode, keyname);
            return code != -1; // propagate left clicks
        }
        bool textinput(const char *str, int len)
        {
            if(Object::textinput(str, len)) return true;
            if(cursor < 0) return false;
            copystring(uitextinput, str, len+1);
            return true;
        }

        static const char *typestr() { return "#Text"; }
        const char *gettype() const { return typestr(); }

        float drawscale() const { return 1.f / hudh; }

        virtual const char *getstr() const { return ""; }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            const double textscale = drawscale();
            const double x = round(sx/textscale), y = round(sy/textscale);
            const int coloralpha = alpha < 1 ? static_cast<int>(alpha * 255.0f) : color.a;
            pushhudscale(textscale);
            if(shadow)
            {
                label.draw(x-0.001/textscale, y+0.001/textscale, (coloralpha < shadow ? coloralpha : shadow), true);
            }
            label.draw(x, y, coloralpha);
            pophudmatrix();
        }

        void hide()        { Object::hide()       ; label.clear(); }
        void clearlabels() { Object::clearlabels(); label.clear(); }

        ~Text() { delete[] language; }

        void layout()
        {
            Object::layout();

            uicursorindex = -1;
            copystring(uikeycode, "");
            copystring(uitextinput, "");

            setfontsize(scale * hudh);

            const float k = drawscale();

            // text changes are detected here
            const char *text = getstr();
            if(!uimillis || (totalmillis - lastchange >= uimillis) || !crc)
            {
                const uint crc_new = crc32(0, (const Bytef *)text, strlen(text));
                if(crc_new != crc)
                {
                    changed = true;
                    lastchange = totalmillis;
                    crc = crc_new;
                }
            }
            if(changed && label.valid()) label.clear();

            if(!label.valid())
            {
                label = text::prepare(text, int(wrap/k), bvec(color.r, color.g, color.b), cursor, outline * FONTH / 16.f, bvec4(0, 0, 0, outlinealpha), align, justify, language, nofallback, /*keep_layout=*/has_cursor, /*reserve_cursor=*/has_cursor);
            }
            w = max(w, label.width()*k);
            h = max(h, label.height()*k);
        }
    };

    struct TextString : Text
    {
        char *str;

        TextString() : str(NULL) {}
        ~TextString() { delete[] str; }

        void setup(const char *str_, float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1, int cursor_ = -1, bool has_cursor_ = false, float alpha_ = 1)
        {
            Text::setup(scale_, color_, wrap_, cursor_, has_cursor_, alpha_);

            SETSTR(str, str_);
        }

        static const char *typestr() { return "#TextString"; }
        const char *gettype() const { return typestr(); }

        const char *getstr() const { return str; }
    };

    struct TextInt : Text
    {
        int val;
        char str[20];

        TextInt() : val(0) { str[0] = '0'; str[1] = '\0'; }

        void setup(int val_, float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1, int cursor_ = -1, bool has_cursor_ = false, float alpha_ = 1)
        {
            Text::setup(scale_, color_, wrap_, cursor_, has_cursor_, alpha_);

            if(val != val_) { val = val_; intformat(str, val, sizeof(str)); }
        }

        static const char *typestr() { return "#TextInt"; }
        const char *gettype() const { return typestr(); }

        const char *getstr() const { return str; }
    };

    struct TextFloat : Text
    {
        float val;
        char str[20];

        TextFloat() : val(0) { memcpy(str, "0.0", 4); }

        void setup(float val_, float scale_ = 1, const Color &color_ = Color(255, 255, 255), float wrap_ = -1, int cursor_ = -1, bool has_cursor_ = false, float alpha_ = 1)
        {
            Text::setup(scale_, color_, wrap_, cursor_, has_cursor_, alpha_);

            if(val != val_) { val = val_; floatformat(str, val, sizeof(str)); }
        }

        static const char *typestr() { return "#TextFloat"; }
        const char *gettype() const { return typestr(); }

        const char *getstr() const { return str; }
    };

    struct Font : Object
    {
        char *font;

        Font() : font(nullptr) {}
        ~Font() { delete[] font; }

        static const char* typestr() { return "#Font"; }
        const char* gettype() const { return typestr(); }

        void setup(const char *name)
        {
            Object::setup();
            SETSTR(font, name);
        }

        void layout()
        {
            pushfont();
            setfont(font);
            Object::layout();
            popfont();
        }

        void draw(float sx, float sy)
        {
            pushfont();
            setfont(font);
            Object::draw(sx, sy);
            popfont();
        }

        void buildchildren(uint *contents)
        {
            pushfont();
            setfont(font);
            Object::buildchildren(contents);
            popfont();
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                pushfont(); \
                setfont(font); \
                Object::func##children(cx, cy, mask, inside, setflags); \
                popfont(); \
            }
        DOSTATES
        #undef DOSTATE

        bool rawkey(int code, bool isdown)
        {
            pushfont();
            setfont(font);
            bool result = Object::rawkey(code, isdown);
            popfont();
            return result;
        }

        bool key(int code, bool isdown)
        {
            pushfont();
            setfont(font);
            bool result = Object::key(code, isdown);
            popfont();
            return result;
        }

        bool textinput(const char *str, int len)
        {
            pushfont();
            setfont(font);
            bool result = Object::textinput(str, len);
            popfont();
            return result;
        }
    };

    float uicontextscale = 0;
    ICOMMAND(uicontextscale, "", (), floatret(FONTH*uicontextscale));

    struct Console : Filler
    {
        void setup(float minw_ = 0, float minh_ = 0)
        {
            Filler::setup(minw_, minh_);
        }

        static const char *typestr() { return "#Console"; }
        const char *gettype() const { return typestr(); }

        float drawscale() const { return uicontextscale; }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            const float k = drawscale() / uiscale;
            pushhudtranslate(sx, sy, k);
            renderfullconsole(w/k, h/k);
            pophudmatrix();
        }
    };

    struct Clipper : Object
    {
        float clipw, cliph, virtw, virth;

        void setup(float clipw_ = 0, float cliph_ = 0)
        {
            Object::setup();
            clipw = clipw_ * uiscale;
            cliph = cliph_ * uiscale;
            virtw = virth = 0;
        }

        static const char *typestr() { return "#Clipper"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            Object::layout();

            virtw = w;
            virth = h;
            if(clipw) w = min(w, clipw);
            if(cliph) h = min(h, cliph);
        }

        void adjustchildren()
        {
            adjustchildrento(0, 0, virtw, virth);
        }

        void draw(float sx, float sy)
        {
            if((clipw && virtw > clipw) || (cliph && virth > cliph))
            {
                stopdrawing();
                pushclip(sx, sy, w, h);
                Object::draw(sx, sy);
                stopdrawing();
                popclip();
            }
            else Object::draw(sx, sy);
        }
    };

    struct Scroller : Clipper
    {
        float offsetx, offsety;

        Scroller() : offsetx(0), offsety(0) {}

        void setup(float clipw_ = 0, float cliph_ = 0)
        {
            Clipper::setup(clipw_, cliph_);
        }

        static const char *typestr() { return "#Scroller"; }
        const char *gettype() const { return typestr(); }

        void layout()
        {
            Clipper::layout();
            offsetx = min(offsetx, hlimit());
            offsety = min(offsety, vlimit());
        }

        #define DOSTATE(flags, func) \
            void func##children(float cx, float cy, int mask, bool inside, int setflags) \
            { \
                cx += offsetx; \
                cy += offsety; \
                if(cx < virtw && cy < virth) Clipper::func##children(cx, cy, mask, inside, setflags); \
            }
        DOSTATES
        #undef DOSTATE

        void draw(float sx, float sy)
        {
            if((clipw && virtw > clipw) || (cliph && virth > cliph))
            {
                stopdrawing();
                pushclip(sx, sy, w, h);
                Object::draw(sx - offsetx, sy - offsety);
                stopdrawing();
                popclip();
            }
            else Object::draw(sx, sy);
        }

        float hlimit() const { return max(virtw - w, 0.0f); }
        float vlimit() const { return max(virth - h, 0.0f); }
        float hoffset() const { return offsetx / max(virtw, w); }
        float voffset() const { return offsety / max(virth, h); }
        float hscale() const { return w / max(virtw, w); }
        float vscale() const { return h / max(virth, h); }

        void addhscroll(float hscroll) { sethscroll(offsetx + hscroll); }
        void addvscroll(float vscroll) { setvscroll(offsety + vscroll); }
        void sethscroll(float hscroll) { offsetx = clamp(hscroll, 0.0f, hlimit()); }
        void setvscroll(float vscroll) { offsety = clamp(vscroll, 0.0f, vlimit()); }

        void scrollup(float cx, float cy);
        void scrolldown(float cx, float cy);
    };

    struct ScrollButton : Object
    {
        static const char *typestr() { return "#ScrollButton"; }
        const char *gettype() const { return typestr(); }
    };

    struct ScrollBar : Object
    {
        float offsetx, offsety;

        ScrollBar() : offsetx(0), offsety(0) {}

        static const char *typestr() { return "#ScrollBar"; }
        const char *gettype() const { return typestr(); }
        const char *gettypename() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        virtual void scrollto(float cx, float cy, bool closest = false) {}

        void hold(float cx, float cy)
        {
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(button && button->haschildstate(STATE_HOLD)) movebutton(button, offsetx, offsety, cx - button->x, cy - button->y);
        }

        void press(float cx, float cy)
        {
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(button && button->haschildstate(STATE_PRESS)) { offsetx = cx - button->x; offsety = cy - button->y; }
            else scrollto(cx, cy, true);
        }

        virtual void addscroll(Scroller *scroller, float dir) = 0;

        void addscroll(float dir)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(scroller) addscroll(scroller, dir);
        }

        void arrowscroll(float dir) { addscroll(dir*curtime/1000.0f); }
        void wheelscroll(float step);
        virtual int wheelscrolldirection() const { return 1; }

        void scrollup(float cx, float cy) { wheelscroll(-wheelscrolldirection()); }
        void scrolldown(float cx, float cy) { wheelscroll(wheelscrolldirection()); }

        virtual void movebutton(Object *o, float fromx, float fromy, float tox, float toy) = 0;
    };

    void Scroller::scrollup(float cx, float cy)
    {
        ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
        if(scrollbar) scrollbar->wheelscroll(-scrollbar->wheelscrolldirection());
    }

    void Scroller::scrolldown(float cx, float cy)
    {
        ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
        if(scrollbar) scrollbar->wheelscroll(scrollbar->wheelscrolldirection());
    }

    struct ScrollArrow : Object
    {
        float arrowspeed;

        void setup(float arrowspeed_ = 0)
        {
            Object::setup();
            arrowspeed = arrowspeed_;
        }

        static const char *typestr() { return "#ScrollArrow"; }
        const char *gettype() const { return typestr(); }

        void hold(float cx, float cy)
        {
            ScrollBar *scrollbar = (ScrollBar *)findsibling(ScrollBar::typestr());
            if(scrollbar) scrollbar->arrowscroll(arrowspeed);
        }
    };

    VARP(uiscrollsteptime, 0, 50, 1000);

    void ScrollBar::wheelscroll(float step)
    {
        ScrollArrow *arrow = (ScrollArrow *)findsibling(ScrollArrow::typestr());
        if(arrow) addscroll(arrow->arrowspeed*step*uiscrollsteptime/1000.0f);
    }

    struct HorizontalScrollBar : ScrollBar
    {
        static const char *typestr() { return "#HorizontalScrollBar"; }
        const char *gettype() const { return typestr(); }

        void addscroll(Scroller *scroller, float dir)
        {
            scroller->addhscroll(dir);
        }

        void scrollto(float cx, float cy, bool closest = false)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bscale = (w - button->w) / (1 - scroller->hscale()),
                  offset = bscale > 1e-3f ? (closest && cx >= button->x + button->w ? cx - button->w : cx)/bscale : 0;
            scroller->sethscroll(offset*scroller->virtw);
        }

        void adjustchildren()
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bw = w*scroller->hscale();
            button->w = max(button->w, bw);
            float bscale = scroller->hscale() < 1 ? (w - button->w) / (1 - scroller->hscale()) : 1;
            button->x = scroller->hoffset()*bscale;
            button->adjust &= ~ALIGN_HMASK;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float fromy, float tox, float toy)
        {
            scrollto(o->x + tox - fromx, o->y + toy);
        }
    };

    struct VerticalScrollBar : ScrollBar
    {
        static const char *typestr() { return "#VerticalScrollBar"; }
        const char *gettype() const { return typestr(); }

        void addscroll(Scroller *scroller, float dir)
        {
            scroller->addvscroll(dir);
        }

        void scrollto(float cx, float cy, bool closest = false)
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bscale = (h - button->h) / (1 - scroller->vscale()),
                  offset = bscale > 1e-3f ? (closest && cy >= button->y + button->h ? cy - button->h : cy)/bscale : 0;
            scroller->setvscroll(offset*scroller->virth);
        }

        void adjustchildren()
        {
            Scroller *scroller = (Scroller *)findsibling(Scroller::typestr());
            if(!scroller) return;
            ScrollButton *button = (ScrollButton *)find(ScrollButton::typestr(), false);
            if(!button) return;
            float bh = h*scroller->vscale();
            button->h = max(button->h, bh);
            float bscale = scroller->vscale() < 1 ? (h - button->h) / (1 - scroller->vscale()) : 1;
            button->y = scroller->voffset()*bscale;
            button->adjust &= ~ALIGN_VMASK;

            ScrollBar::adjustchildren();
        }

        void movebutton(Object *o, float fromx, float fromy, float tox, float toy)
        {
            scrollto(o->x + tox, o->y + toy - fromy);
        }

        int wheelscrolldirection() const { return -1; }
    };

    struct SliderButton : Object
    {
        static const char *typestr() { return "#SliderButton"; }
        const char *gettype() const { return typestr(); }
    };

    static double getfval(ident *id, double val = 0)
    {
        switch(id->type)
        {
            case ID_VAR: val = *id->storage.i; break;
            case ID_FVAR: val = *id->storage.f; break;
            case ID_SVAR: val = parsenumber(*id->storage.s); break;
            case ID_ALIAS: val = id->getnumber(); break;
            case ID_COMMAND:
            {
                tagval t;
                executeret(id, NULL, 0, true, t);
                val = t.getnumber();
                t.cleanup();
                break;
            }
        }
        return val;
    }

    static void setfval(ident *id, double val, uint *onchange = NULL)
    {
        switch(id->type)
        {
            case ID_VAR: setvarchecked(id, int(clamp(val, double(INT_MIN), double(INT_MAX)))); break;
            case ID_FVAR: setfvarchecked(id, val); break;
            case ID_SVAR: setsvarchecked(id, numberstr(val)); break;
            case ID_ALIAS: alias(id->name, numberstr(val)); break;
            case ID_COMMAND:
            {
                tagval t;
                t.setnumber(val);
                execute(id, &t, 1);
                break;
            }
        }
        if(onchange && (*onchange&CODE_OP_MASK) != CODE_EXIT) execute(onchange);
    }

    struct Slider : Object
    {
        ident *id;
        double val, vmin, vmax, vstep;
        bool changed;

        Slider() : id(NULL), val(0), vmin(0), vmax(0), vstep(0), changed(false) {}

        void setup(ident *id_, double vmin_ = 0, double vmax_ = 0, double vstep_ = 1, uint *onchange = NULL)
        {
            Object::setup();
            if(!vmin_ && !vmax_) switch(id_->type)
            {
                case ID_VAR: vmin_ = id_->minval; vmax_ = id_->maxval; break;
                case ID_FVAR: vmin_ = id_->minvalf; vmax_ = id_->maxvalf; break;
            }
            if(id != id_) changed = false;
            id = id_;
            vmin = vmin_;
            vmax = vmax_;
            vstep = vstep_ > 0 ? vstep_ : 1;
            if(changed)
            {
                setfval(id, val, onchange);
                changed = false;
            }
            else val = getfval(id, vmin);
        }

        static const char *typestr() { return "#Slider"; }
        const char *gettype() const { return typestr(); }
        const char *gettypename() const { return typestr(); }

        bool target(float cx, float cy)
        {
            return true;
        }

        void arrowscroll(double dir)
        {
            double newval = val + dir*vstep;
            newval += vstep * (newval < 0 ? -0.5 : 0.5);
            newval -= fmod(newval, vstep);
            newval = clamp(newval, min(vmin, vmax), max(vmin, vmax));
            if(val != newval) changeval(newval);
        }

        void wheelscroll(float step);
        virtual int wheelscrolldirection() const { return 1; }

        void scrollup(float cx, float cy) { wheelscroll(-wheelscrolldirection()); }
        void scrolldown(float cx, float cy) { wheelscroll(wheelscrolldirection()); }

        virtual void scrollto(float cx, float cy) {}

        void hold(float cx, float cy)
        {
            scrollto(cx, cy);
        }

        void changeval(double newval)
        {
            val = newval;
            changed = true;
        }
    };

    VARP(uislidersteptime, 0, 50, 1000);

    struct SliderArrow : Object
    {
        double stepdir;
        int laststep;

        SliderArrow() : laststep(0) {}

        void setup(double dir_ = 0)
        {
            Object::setup();
            stepdir = dir_;
        }

        static const char *typestr() { return "#SliderArrow"; }
        const char *gettype() const { return typestr(); }

        void press(float cx, float cy)
        {
            laststep = totalmillis + 2*uislidersteptime;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }

        void hold(float cx, float cy)
        {
            if(totalmillis < laststep + uislidersteptime)
                return;
            laststep = totalmillis;

            Slider *slider = (Slider *)findsibling(Slider::typestr());
            if(slider) slider->arrowscroll(stepdir);
        }
    };

    void Slider::wheelscroll(float step)
    {
        SliderArrow *arrow = (SliderArrow *)findsibling(SliderArrow::typestr());
        if(arrow) step *= arrow->stepdir;
        arrowscroll(step);
    }

    struct HorizontalSlider : Slider
    {
        static const char *typestr() { return "#HorizontalSlider"; }
        const char *gettype() const { return typestr(); }

        void scrollto(float cx, float cy)
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            float offset = w > button->w ? clamp((cx - button->w/2)/(w - button->w), 0.0f, 1.0f) : 0.0f;
            int step = round((val - vmin) / vstep),
                bstep = round(offset * (vmax - vmin) / vstep);
            if(step != bstep) changeval(bstep * vstep + vmin);
        }

        void adjustchildren()
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            int step = round((val - vmin) / vstep),
                bstep = round(button->x / (w - button->w) * (vmax - vmin) / vstep);
            if(step != bstep) button->x = (w - button->w) * step * vstep / (vmax - vmin);
            button->adjust &= ~ALIGN_HMASK;

            Slider::adjustchildren();
        }
    };

    struct VerticalSlider : Slider
    {
        static const char *typestr() { return "#VerticalSlider"; }
        const char *gettype() const { return typestr(); }

        void scrollto(float cx, float cy)
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            float offset = h > button->h ? clamp((cy - button->h/2)/(h - button->h), 0.0f, 1.0f) : 0.0f;
            int step = round((val - vmin) / vstep),
                bstep = round(offset * (vmax - vmin) / vstep);
            if(step != bstep) changeval(bstep * vstep + vmin);
        }

        void adjustchildren()
        {
            SliderButton *button = (SliderButton *)find(SliderButton::typestr(), false);
            if(!button) return;
            int step = round((val - vmin) / vstep),
                bstep = round(button->y / (h - button->h) * (vmax - vmin) / vstep);
            if(step != bstep) button->y = (h - button->h) * step * vstep / (vmax - vmin);
            button->adjust &= ~ALIGN_VMASK;

            Slider::adjustchildren();
        }

        int wheelscrolldirection() const { return -1; }
    };

    struct Preview : Target
    {
        void startdraw()
        {
            glDisable(GL_BLEND);

            if(clipstack.length()) glDisable(GL_SCISSOR_TEST);
        }

        void enddraw()
        {
            glEnable(GL_BLEND);

            if(clipstack.length()) glEnable(GL_SCISSOR_TEST);
        }
    };

    struct ModelPreview : Preview
    {
        char *name;
        int anim;

        ModelPreview() : name(NULL) {}
        ~ModelPreview() { delete[] name; }

        void setup(const char *name_, const char *animspec, float minw_, float minh_)
        {
            Preview::setup(minw_, minh_);
            SETSTR(name, name_);

            anim = ANIM_ALL;
            if(animspec[0])
            {
                if(isdigit(animspec[0]))
                {
                    anim = parseint(animspec);
                    if(anim >= 0) anim %= ANIM_INDEX;
                    else anim = ANIM_ALL;
                }
                else
                {
                    vector<int> anims;
                    game::findanims(animspec, anims);
                    if(anims.length()) anim = anims[0];
                }
            }
            anim |= ANIM_LOOP;
        }

        static const char *typestr() { return "#ModelPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.length() > 0);
            model *m = loadmodel(name);
            if(m)
            {
                vec center, radius;
                m->boundbox(center, radius);
                float yaw;
                vec o = calcmodelpreviewpos(radius, yaw).sub(center);
                rendermodel(name, anim, o, yaw, 0, 0, 0, NULL, NULL, 0);
            }
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end();
        }
    };

    struct PlayerPreview : Preview
    {
        int model, color, team, weapon;

        void setup(int model_, int color_, int team_, int weapon_, float minw_, float minh_)
        {
            Preview::setup(minw_, minh_);
            model = model_;
            color = color_;
            team = team_;
            weapon = weapon_;
        }

        static const char *typestr() { return "#PlayerPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.length() > 0);
            game::renderplayerpreview(model, color, team, weapon);
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end();
        }
    };

    struct PrefabPreview : Preview
    {
        char *name;
        vec color;

        PrefabPreview() : name(NULL) {}
        ~PrefabPreview() { delete[] name; }

        void setup(const char *name_, int color_, float minw_, float minh_)
        {
            Preview::setup(minw_, minh_);
            SETSTR(name, name_);
            color = vec::hexcolor(color_);
        }

        static const char *typestr() { return "#PrefabPreview"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            Object::draw(sx, sy);

            changedraw(CHANGE_SHADER);

            int sx1, sy1, sx2, sy2;
            window->calcscissor(sx, sy, sx+w, sy+h, sx1, sy1, sx2, sy2, false);
            modelpreview::start(sx1, sy1, sx2-sx1, sy2-sy1, false, clipstack.length() > 0);
            previewprefab(name, color);
            if(clipstack.length()) clipstack.last().scissor();
            modelpreview::end();
        }
    };

    VARP(uislotviewtime, 0, 25, 1000);
    static int lastthumbnail = 0;

    struct SlotViewer : Target
    {
        int index;

        void setup(int index_, float minw_ = 0, float minh_ = 0)
        {
            Target::setup(minw_, minh_);
            index = index_;
        }

        static const char *typestr() { return "#SlotViewer"; }
        const char *gettype() const { return typestr(); }

        void previewslot(Slot &slot, VSlot &vslot, float x, float y)
        {
            if(slot.sts.empty()) return;
            VSlot *layer = NULL, *detail = NULL;
            Texture *t = NULL, *glowtex = NULL, *layertex = NULL, *detailtex = NULL;
            if(slot.loaded)
            {
                t = slot.sts[0].t;
                if(t == notexture) return;
                Slot &slot = *vslot.slot;
                if(slot.texmask&(1<<TEX_GLOW)) { loopvj(slot.sts) if(slot.sts[j].type==TEX_GLOW) { glowtex = slot.sts[j].t; break; } }
                if(vslot.layer)
                {
                    layer = &lookupvslot(vslot.layer);
                    if(!layer->slot->sts.empty()) layertex = layer->slot->sts[0].t;
                }
                if(vslot.detail)
                {
                    detail = &lookupvslot(vslot.detail);
                    if(!detail->slot->sts.empty()) detailtex = detail->slot->sts[0].t;
                }
            }
            else
            {
                if(!slot.thumbnail)
                {
                    if(totalmillis - lastthumbnail < uislotviewtime) return;
                    slot.loadthumbnail();
                    lastthumbnail = totalmillis;
                }
                if(slot.thumbnail != notexture) t = slot.thumbnail;
                else return;
            }

            changedraw(CHANGE_SHADER | CHANGE_COLOR);

            SETSHADER(hudrgb);
            LOCALPARAMF(previewhsv, vslot.hsv.x, vslot.hsv.y, vslot.hsv.z);
            LOCALPARAMF(previewtransform, vslot.transform.x, vslot.transform.y, vslot.transform.z, vslot.transform.w);
            vec2 tc[4] = { vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1) };
            int xoff = vslot.offset.x, yoff = vslot.offset.y;
            if(vslot.rotation)
            {
                const texrotation &r = texrotations[vslot.rotation];
                if(r.swapxy) { swap(xoff, yoff); loopk(4) swap(tc[k].x, tc[k].y); }
                if(r.flipx) { xoff *= -1; loopk(4) tc[k].x *= -1; }
                if(r.flipy) { yoff *= -1; loopk(4) tc[k].y *= -1; }
            }
            float xt = min(1.0f, t->xs/float(t->ys)), yt = min(1.0f, t->ys/float(t->xs));
            loopk(4) { tc[k].x = tc[k].x/xt - float(xoff)/t->xs; tc[k].y = tc[k].y/yt - float(yoff)/t->ys; }
            setusedtexture(t);
            if(slot.loaded) gle::color(vslot.colorscale);
            else gle::colorf(1, 1, 1);
            quad(x, y, w, h, tc);
            if(detailtex)
            {
                setusedtexture(detailtex);
                quad(x + w/2, y + h/2, w/2, h/2, tc);
            }
            if(glowtex)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                setusedtexture(glowtex);
                gle::color(vslot.glowcolor);
                quad(x, y, w, h, tc);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            if(layertex)
            {
                setusedtexture(layertex);
                LOCALPARAMF(previewhsv, layer->hsv.x, layer->hsv.y, layer->hsv.z);
                LOCALPARAMF(previewtransform, layer->transform.x, layer->transform.y, layer->transform.z, layer->transform.w);
                gle::color(layer->colorscale);
                quad(x, y, w/2, h/2, tc);
            }
        }

        void draw(float sx, float sy)
        {
            Slot &slot = lookupslot(index, false);
            previewslot(slot, *slot.variants, sx, sy);

            Object::draw(sx, sy);
        }
    };

    struct VSlotViewer : SlotViewer
    {
        static const char *typestr() { return "#VSlotViewer"; }
        const char *gettype() const { return typestr(); }

        void draw(float sx, float sy)
        {
            VSlot &vslot = lookupvslot(index, false);
            previewslot(*vslot.slot, vslot, sx, sy);

            Object::draw(sx, sy);
        }
    };

    ICOMMAND(newui, "ssss", (char *name, char *contents, char *onshow, char *onhide),
    {
        Window *window = windows.find(name, NULL);
        if(window) { if (window == UI::window) return; world->hide(window); windows.remove(name); delete window; }
        windows[name] = new Window(name, contents, onshow, onhide);
    });
    ICOMMAND(settopui, "s", (char *name),
    {
        Window *window = windows.find(name, NULL);
        if(!window || world->children.last() == window) return;
        world->children.remove(world->children.find(window));
        world->children.add(window);
    });

    ICOMMAND(uiallowinput, "b", (int *val), { if(window) { if(*val >= 0) window->allowinput = *val!=0; intret(window->allowinput ? 1 : 0); } });
    ICOMMAND(uieschide, "b", (int *val), { if(window) { if(*val >= 0) window->eschide = *val!=0; intret(window->eschide ? 1 : 0); } });

    ICOMMAND(uibind, "ss", (char* key, char* action),
    {
        const char *window = world->children.last()->getname();
        clearuibinds(window);
        binduikey(key, action, window);
    });

    ICOMMAND(uiaspect,      "", (), floatret(float(hudw)/hudh));
    ICOMMAND(uilockcursor,  "", (), { cursorlockedx = true; cursorlockedy = true; });
    ICOMMAND(uilockcursorx, "", (), cursorlockedx = true);
    ICOMMAND(uilockcursory, "", (), cursorlockedy = true);

    ICOMMAND(uicursor,  "", (), intret(world->allowinput()));
    ICOMMAND(uicursorx, "", (), floatret(cursortrackvec.x));
    ICOMMAND(uicursory, "", (), floatret(cursortrackvec.y));

    ICOMMAND(uimousex, "", (), { mousetracking = true; floatret(mousetrackvec.x); });
    ICOMMAND(uimousey, "", (), { mousetracking = true; floatret(mousetrackvec.y); });

    bool showui(const char *name)
    {
        Window *window = windows.find(name, NULL);
        return window && world->show(window);
    }

    bool hideui(const char *name)
    {
        if(!name) return world->hideall() > 0;
        Window *window = windows.find(name, NULL);
        return window && world->hide(window);
    }

    bool toggleui(const char *name)
    {
        if(showui(name)) return true;
        hideui(name);
        return false;
    }

    void holdui(const char *name, bool on)
    {
        if(on) showui(name);
        else hideui(name);
    }

    bool uivisible(const char *name)
    {
        if(!name) return world->children.length() > 0;
        Window *window = windows.find(name, NULL);
        return window && world->children.find(window) >= 0;
    }

    ICOMMAND(showui, "s", (char *name), intret(showui(name) ? 1 : 0));
    ICOMMAND(hideui, "s", (char *name), intret(hideui(name) ? 1 : 0));
    ICOMMAND(hidetopui, "", (), intret(world->hidetop() ? 1 : 0));
    ICOMMAND(hideallui, "", (), intret(world->hideall()));
    ICOMMAND(toggleui, "s", (char *name), intret(toggleui(name) ? 1 : 0));
    ICOMMAND(holdui, "sD", (char *name, int *down), holdui(name, *down!=0));
    ICOMMAND(uivisible, "s", (char *name), intret(uivisible(name) ? 1 : 0));
    ICOMMAND(uiname, "", (), { if(window) result(window->name); });

    #define IFSTATEVAL(state,t,f) { if(state) { if(t->type == VAL_NULL) intret(1); else result(*t); } else if(f->type == VAL_NULL) intret(0); else result(*f); }
    #define DOSTATE(flags, func) \
        ICOMMANDNS("ui!" #func, uinot##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->hasstate(flags) ? t : f)); \
        ICOMMANDNS("ui" #func, ui##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->haschildstate(flags) ? t : f)); \
        ICOMMANDNS("ui!" #func "?", uinot##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->hasstate(flags), t, f)); \
        ICOMMANDNS("ui" #func "?", ui##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->haschildstate(flags), t, f)); \
        ICOMMANDNS("ui!" #func "+", uinextnot##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->hasstate(flags) ? t : f)); \
        ICOMMANDNS("ui" #func "+", uinext##func##_, "ee", (uint *t, uint *f), \
            executeret(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->haschildstate(flags) ? t : f)); \
        ICOMMANDNS("ui!" #func "+?", uinextnot##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->hasstate(flags), t, f)); \
        ICOMMANDNS("ui" #func "+?", uinext##func##__, "tt", (tagval *t, tagval *f), \
            IFSTATEVAL(buildparent && buildparent->children.inrange(buildchild) && buildparent->children[buildchild]->haschildstate(flags), t, f));
    DOSTATES
    #undef DOSTATE

    ICOMMAND(uialign, "ii", (int *xalign, int *yalign),
    {
        if(buildparent) buildparent->setalign(*xalign, *yalign);
    });
    ICOMMANDNS("uialign-", uialign_, "ii", (int *xalign, int *yalign),
    {
        if(buildparent && buildchild > 0) buildparent->children[buildchild-1]->setalign(*xalign, *yalign);
    });
    ICOMMANDNS("uialign*", uialign__, "ii", (int *xalign, int *yalign),
    {
        if(buildparent) loopi(buildchild) buildparent->children[i]->setalign(*xalign, *yalign);
    });

    ICOMMAND(uiclamp, "iiii", (int *left, int *right, int *top, int *bottom),
    {
        if(buildparent) buildparent->setclamp(*left, *right, *top, *bottom);
    });
    ICOMMANDNS("uiclamp-", uiclamp_, "iiii", (int *left, int *right, int *top, int *bottom),
    {
        if(buildparent && buildchild > 0) buildparent->children[buildchild-1]->setclamp(*left, *right, *top, *bottom);
    });
    ICOMMANDNS("uiclamp*", uiclamp__, "iiii", (int *left, int *right, int *top, int *bottom),
    {
        if(buildparent) loopi(buildchild) buildparent->children[i]->setclamp(*left, *right, *top, *bottom);
    });

    ICOMMAND(uigroup, "e", (uint *children),
        BUILD(Object, o, o->setup(), children));

    ICOMMAND(uihlist, "fe", (float *space, uint *children),
        BUILD(HorizontalList, o, o->setup(*space), children));

    ICOMMAND(uivlist, "fe", (float *space, uint *children),
        BUILD(VerticalList, o, o->setup(*space), children));

    ICOMMAND(uilist, "fe", (float *space, uint *children),
    {
        for(Object *parent = buildparent; parent && !parent->istype<VerticalList>(); parent = parent->parent)
        {
            if(parent->istype<HorizontalList>())
            {
                BUILD(VerticalList, o, o->setup(*space), children);
                return;
            }
        }
        BUILD(HorizontalList, o, o->setup(*space), children);
    });

    ICOMMAND(uigrid, "iffe", (int *columns, float *spacew, float *spaceh, uint *children),
        BUILD(Grid, o, o->setup(*columns, *spacew, *spaceh), children));

    ICOMMAND(uitableheader, "ee", (uint *columndata, uint *children),
        BUILDCOLUMNS(TableHeader, o, o->setup(), columndata, children));
    ICOMMAND(uitablerow, "ee", (uint *columndata, uint *children),
        BUILDCOLUMNS(TableRow, o, o->setup(), columndata, children));
    ICOMMAND(uitable, "ffe", (float *spacew, float *spaceh, uint *children),
        BUILD(Table, o, o->setup(*spacew, *spaceh), children));

    ICOMMAND(uispace, "ffe", (float *spacew, float *spaceh, uint *children),
        BUILD(Spacer, o, o->setup(*spacew, *spaceh), children));

    ICOMMAND(uioffset, "ffe", (float *offsetx, float *offsety, uint *children),
        BUILD(Offsetter, o, o->setup(*offsetx, *offsety), children));

    ICOMMAND(uifill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw, *minh), children));

    ICOMMAND(uitarget, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Target, o, o->setup(*minw, *minh), children));

    ICOMMAND(uiclip, "ffe", (float *clipw, float *cliph, uint *children),
        BUILD(Clipper, o, o->setup(*clipw, *cliph), children));

    ICOMMAND(uiscroll, "ffe", (float *clipw, float *cliph, uint *children),
        BUILD(Scroller, o, o->setup(*clipw, *cliph), children));

    ICOMMAND(uihscrolloffset, "", (),
    {
        if(buildparent && buildparent->istype<Scroller>())
        {
            Scroller *scroller = (Scroller *)buildparent;
            floatret(scroller->offsetx);
        }
    });

    ICOMMAND(uivscrolloffset, "", (),
    {
        if(buildparent && buildparent->istype<Scroller>())
        {
            Scroller *scroller = (Scroller *)buildparent;
            floatret(scroller->offsety);
        }
    });

    ICOMMAND(uihscrollbar, "e", (uint *children),
        BUILD(HorizontalScrollBar, o, o->setup(), children));

    ICOMMAND(uivscrollbar, "e", (uint *children),
        BUILD(VerticalScrollBar, o, o->setup(), children));

    ICOMMAND(uiscrollarrow, "fe", (float *dir, uint *children),
        BUILD(ScrollArrow, o, o->setup(*dir), children));

    ICOMMAND(uiscrollbutton, "e", (uint *children),
        BUILD(ScrollButton, o, o->setup(), children));

    ICOMMAND(uihslider, "rfffee", (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children),
        BUILD(HorizontalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children));

    ICOMMAND(uivslider, "rfffee", (ident *var, float *vmin, float *vmax, float *vstep, uint *onchange, uint *children),
        BUILD(VerticalSlider, o, o->setup(var, *vmin, *vmax, *vstep, onchange), children));

    ICOMMAND(uisliderarrow, "fe", (float *dir, uint *children),
        BUILD(SliderArrow, o, o->setup(*dir), children));

    ICOMMAND(uisliderbutton, "e", (uint *children),
        BUILD(SliderButton, o, o->setup(), children));

    ICOMMAND(uicolor, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(FillColor::SOLID, Color(*c), *minw, *minh), children));

    ICOMMAND(uimodcolor, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(FillColor, o, o->setup(FillColor::MODULATE, Color(*c), *minw, *minh), children));

    ICOMMAND(uivgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Gradient::SOLID, Gradient::VERTICAL, Color(*c), Color(*c2), *minw, *minh), children));

    ICOMMAND(uimodvgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Gradient::MODULATE, Gradient::VERTICAL, Color(*c), Color(*c2), *minw, *minh), children));

    ICOMMAND(uihgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Gradient::SOLID, Gradient::HORIZONTAL, Color(*c), Color(*c2), *minw, *minh), children));

    ICOMMAND(uimodhgradient, "iiffe", (int *c, int *c2, float *minw, float *minh, uint *children),
        BUILD(Gradient, o, o->setup(Gradient::MODULATE, Gradient::HORIZONTAL, Color(*c), Color(*c2), *minw, *minh), children));

    ICOMMAND(uioutline, "iffe", (int *c, float *minw, float *minh, uint *children),
        BUILD(Outline, o, o->setup(Color(*c), *minw, *minh), children));

    ICOMMAND(uiline, "iffie", (int *c, float *minw, float *minh, bool *flip, uint *children),
        BUILD(Line, o, o->setup(Color(*c), *minw, *minh, *flip), children));

    ICOMMAND(uitriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw, *minh, *angle, Triangle::SOLID), children));

    ICOMMAND(uitriangleoutline, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw, *minh, *angle, Triangle::OUTLINE), children));

    ICOMMAND(uimodtriangle, "iffie", (int *c, float *minw, float *minh, int *angle, uint *children),
        BUILD(Triangle, o, o->setup(Color(*c), *minw, *minh, *angle, Triangle::MODULATE), children));

    ICOMMAND(uicircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size, Circle::SOLID), children));

    ICOMMAND(uicircleoutline, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size, Circle::OUTLINE), children));

    ICOMMAND(uimodcircle, "ife", (int *c, float *size, uint *children),
        BUILD(Circle, o, o->setup(Color(*c), *size, Circle::MODULATE), children));

    ICOMMAND(uiwrapalign, "ie", (int *val, uint *children),
        BUILD(WrapAlign, o, o->setup(*val), children));
    
    ICOMMAND(uijustify, "ie", (int *val, uint *children),
        BUILD(Justify, o, o->setup(*val), children));
    
    ICOMMAND(uishadow, "ie", (int *val, uint *children),
        BUILD(Shadow, o, o->setup(*val), children));
    
    ICOMMAND(uifontoutline, "fie", (float *val, int *a, uint *children),
        BUILD(FontOutline, o, o->setup(*val, *a), children));
    
    ICOMMAND(uinofallback, "ie", (int *val, uint *children),
        BUILD(NoFallback, o, o->setup(*val), children));
    
    ICOMMAND(uilanguage, "se", (char *val, uint *children),
        BUILD(Language, o, o->setup(val), children));

    static inline void buildtext(tagval &t, float scale, float scalemod, const Color &color, float wrap, int cursor, bool has_cursor, uint *children, float alpha = 1)
    {
        if(scale <= 0) scale = 1;
        scale *= scalemod;
        switch(t.type)
        {
            case VAL_INT:
                BUILD(TextInt, o, o->setup(t.i, scale, color, wrap, cursor, has_cursor, alpha), children);
                break;
            case VAL_FLOAT:
                BUILD(TextFloat, o, o->setup(t.f, scale, color, wrap, cursor, has_cursor, alpha), children);
                break;
            case VAL_CSTR:
            case VAL_MACRO:
            case VAL_STR:
                if(t.s[0])
                {
                    BUILD(TextString, o, o->setup(t.s, scale, color, wrap, cursor, has_cursor, alpha), children);
                    break;
                }
                // fall-through
            default:
                BUILD(Object, o, o->setup(), children);
                break;
        }
    }

    ICOMMAND(uicolortext, "tifieN", (tagval *text, int *c, float *scale, int *cursor, uint *children, int *numargs),
        buildtext(*text, *scale, uitextscale, Color(*c), -1, *numargs>=4 ? *cursor : -1, *numargs>=4, children));

    ICOMMAND(uicolorblendtext, "tiffieN", (tagval *text, int *c, float *alpha, float *scale, int *cursor, uint *children, int *numargs),
        buildtext(*text, *scale, uitextscale, Color(*c), -1, *numargs>=5 ? *cursor : -1, *numargs>=5, children, *alpha));

    ICOMMAND(uitext, "tfieN", (tagval *text, float *scale, int *cursor, uint *children, int *numargs),
        buildtext(*text, *scale, uitextscale, Color(255, 255, 255), -1, *numargs>=3 ? *cursor : -1, *numargs>=3, children));

    ICOMMAND(uitextfill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw * uitextscale*0.5f, *minh * uitextscale), children));

    ICOMMAND(uiwrapcolortext, "tfifieN", (tagval *text, float *wrap, int *c, float *scale, int *cursor, uint *children, int *numargs),
        buildtext(*text, *scale, uitextscale, Color(*c), *wrap, *numargs>=5 ? *cursor : -1, *numargs>=5, children));

    ICOMMAND(uiwraptext, "tffieN", (tagval *text, float *wrap, float *scale, int *cursor, uint *children, int *numargs),
        buildtext(*text, *scale, uitextscale, Color(255, 255, 255), *wrap, *numargs>=4 ? *cursor : -1, *numargs>=4, children));

    ICOMMAND(uicolorcontext, "tife", (tagval *text, int *c, float *scale, uint *children),
        buildtext(*text, *scale, FONTH*uicontextscale, Color(*c), -1, -1, false, children));

    ICOMMAND(uicontext, "tfe", (tagval *text, float *scale, uint *children),
        buildtext(*text, *scale, FONTH*uicontextscale, Color(255, 255, 255), -1, -1, false, children));

    ICOMMAND(uicontextfill, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Filler, o, o->setup(*minw * FONTH*uicontextscale*0.5f, *minh * FONTH*uicontextscale), children));

    ICOMMAND(uiwrapcolorcontext, "tfife", (tagval *text, float *wrap, int *c, float *scale, uint *children),
        buildtext(*text, *scale, FONTH*uicontextscale, Color(*c), *wrap, -1, false, children));

    ICOMMAND(uiwrapcontext, "tffe", (tagval *text, float *wrap, float *scale, uint *children),
        buildtext(*text, *scale, FONTH*uicontextscale, Color(255, 255, 255), *wrap, -1, false, children));

    ICOMMAND(uifont, "se", (char *name, uint *children),
        BUILD(Font, o, o->setup(name), children));

    ICOMMAND(uiabovehud, "", (), { if(window) window->abovehud = true; });

    ICOMMAND(uiconsole, "ffe", (float *minw, float *minh, uint *children),
        BUILD(Console, o, o->setup(*minw, *minh), children));

    ICOMMAND(uiminimap, "ffffie", (float *ox, float *oy, float *yaw, float *size, int *sides, uint *children),
        BUILD(Minimap, o, o->setup(vec(*ox, *oy, 0), *yaw, *size, *sides), children));

    ICOMMAND(uiimage, "sffe", (char *texname, float *minw, float *minh, uint *children),
        BUILD(Image, o, o->setup(textureload(texname, 3, true, false), *minw, *minh), children));

    ICOMMAND(uirotatedimage, "sfffe", (char *texname, float *angle, float *minw, float *minh, uint *children),
        BUILD(Image, o, o->setup(textureload(texname, 3, true, false), *minw, *minh, *angle), children));

    ICOMMAND(uistretchedimage, "sffe", (char *texname, float *minw, float *minh, uint *children),
        BUILD(StretchedImage, o, o->setup(textureload(texname, 3, true, false), *minw, *minh), children));

    static inline float parsepixeloffset(const tagval *t, int size)
    {
        switch(t->type)
        {
            case VAL_INT: return t->i;
            case VAL_FLOAT: return t->f;
            case VAL_NULL: return 0;
            default:
            {
                const char *s = t->getstr();
                char *end;
                float val = strtod(s, &end);
                return *end == 'p' ? val/size : val;
            }
        }
    }

    ICOMMAND(uicroppedimage, "sfftttte", (char *texname, float *minw, float *minh, tagval *cropx, tagval *cropy, tagval *cropw, tagval *croph, uint *children),
        BUILD(CroppedImage, o, {
            Texture *tex = textureload(texname, 3, true, false);
            o->setup(tex, *minw, *minh,
                parsepixeloffset(cropx, tex->xs), parsepixeloffset(cropy, tex->ys),
                parsepixeloffset(cropw, tex->xs), parsepixeloffset(croph, tex->ys));
        }, children));

    ICOMMAND(uiborderedimage, "stfe", (char *texname, tagval *texborder, float *screenborder, uint *children),
        BUILD(BorderedImage, o, {
            Texture *tex = textureload(texname, 3, true, false);
            o->setup(tex,
                parsepixeloffset(texborder, tex->xs),
                *screenborder);
        }, children));

    ICOMMAND(uitiledimage, "sffffe", (char *texname, float *tilew, float *tileh, float *minw, float *minh, uint *children),
        BUILD(TiledImage, o, {
            Texture *tex = textureload(texname, 0, true, false);
            o->setup(tex, *minw, *minh, *tilew <= 0 ? 1 : *tilew, *tileh <= 0 ? 1 : *tileh);
        }, children));

    ICOMMAND(uimodelpreview, "ssffe", (char *model, char *animspec, float *minw, float *minh, uint *children),
        BUILD(ModelPreview, o, o->setup(model, animspec, *minw, *minh), children));

    ICOMMAND(uiplayerpreview, "iiiiffe", (int *model, int *color, int *team, int *weapon, float *minw, float *minh, uint *children),
        BUILD(PlayerPreview, o, o->setup(*model, *color, *team, *weapon, *minw, *minh), children));

    ICOMMAND(uiprefabpreview, "siffe", (char *prefab, int *color, float *minw, float *minh, uint *children),
        BUILD(PrefabPreview, o, o->setup(prefab, *color, *minw, *minh), children));

    ICOMMAND(uislotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(SlotViewer, o, o->setup(*index, *minw, *minh), children));

    ICOMMAND(uivslotview, "iffe", (int *index, float *minw, float *minh, uint *children),
        BUILD(VSlotViewer, o, o->setup(*index, *minw, *minh), children));

    FVARP(uisensitivity, 1e-4f, 1, 1e4f);

    bool hascursor()
    {
        return world->allowinput();
    }

    void getcursorpos(float &x, float &y)
    {
        if(hascursor())
        {
            x = cursorx;
            y = cursory;
        }
        else x = y = 0.5f;
    }

    void resetcursor()
    {
        cursorx = cursory = 0.5f;
    }

    bool movecursor(int dx, int dy, int w, int h)
    {
        if(!hascursor()) return false;

        #define mousesens(a,b,c) ((float(a)/float(b))*c)
        float mousemovex = mousesens(dx, w, uisensitivity);
        float mousemovey = mousesens(dy, h, uisensitivity);

        mousetrackvec.add(vec2(mousemovex, mousemovey));

        if (!cursorlockedx) cursorx = clamp(cursorx + mousemovex, 0.0f, 1.0f);
        if (!cursorlockedy) cursory = clamp(cursory + mousemovey, 0.0f, 1.0f);

        cursortrackvec = vec2(cursorx, cursory);

        return true;
    }

    bool keypress(int code, bool isdown)
    {
        if(world->rawkey(code, isdown)) return true;
        int action = 0, hold = 0;
        switch(code)
        {
        case -1: action = isdown ? STATE_PRESS : STATE_RELEASE; hold = STATE_HOLD; break;
        case -2: action = isdown ? STATE_ALT_PRESS : STATE_ALT_RELEASE; hold = STATE_ALT_HOLD; break;
        case -3: action = isdown ? STATE_ESC_PRESS : STATE_ESC_RELEASE; hold = STATE_ESC_HOLD; break;
        case -4: action = STATE_SCROLL_UP; break;
        case -5: action = STATE_SCROLL_DOWN; break;
        }
        if(action)
        {
            if(isdown)
            {
                if(hold) world->clearstate(hold);
                if(world->setstate(action, cursorx, cursory, 0, true, action|hold)) return true;
            }
            else if(hold)
            {
                if(world->setstate(action, cursorx, cursory, hold, true, action))
                {
                    world->clearstate(hold);
                    return true;
                }
                world->clearstate(hold);
            }
            return world->allowinput();
        }
        return world->key(code, isdown);
    }

    bool textinput(const char *str, int len)
    {
        return world->textinput(str, len);
    }

    void setup()
    {
        world = new World;
    }

    void cleanup()
    {
        world->children.setsize(0);
        enumerate(windows, Window *, w, delete w);
        windows.clear();
        DELETEP(world);
    }

    void clearlabels()
    {
        world->clearlabels();
    }

    void calctextscale()
    {
        uitextscale = 1.0f/UITEXTROWS;

        int tw = hudw, th = hudh;
        if(forceaspect) tw = int(ceil(th*forceaspect));
        text::getres(tw, th);
        uicontextscale = 1.f/th * uiscale;
    }

    void update()
    {
        mousetracking = false;
        cursorlockedx = false;
        cursorlockedy = false;

        world->setstate(STATE_HOVER, cursorx, cursory, world->childstate&STATE_HOLD_MASK);
        if(world->childstate&STATE_HOLD) world->setstate(STATE_HOLD, cursorx, cursory, STATE_HOLD, false);
        if(world->childstate&STATE_ALT_HOLD) world->setstate(STATE_ALT_HOLD, cursorx, cursory, STATE_ALT_HOLD, false);
        if(world->childstate&STATE_ESC_HOLD) world->setstate(STATE_ESC_HOLD, cursorx, cursory, STATE_ESC_HOLD, false);

        calctextscale();

        world->build();

        if(!mousetracking) mousetrackvec = vec2(0, 0);
    }

    void render()
    {
        world->layout();
        world->adjustchildren();
        world->draw();
    }

    float abovehud()
    {
        return world->abovehud();
    }
}

