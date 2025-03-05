// this file defines static map entities ("entity") and dynamic entities (players/monsters, "dynent")
// the gamecode extends these types to add game specific functionality

// ET_*: the only static entity types dictated by the engine... rest are gamecode dependent

enum
{
    ET_EMPTY = 0,
    ET_LIGHT,
    ET_MAPMODEL,
    ET_PLAYERSTART,
    ET_ENVMAP,
    ET_PARTICLES,
    ET_SOUND,
    ET_SPOTLIGHT,
    ET_DECAL,
    ET_GAMESPECIFIC
};

struct entity                                   // persistent map entity
{
    vec o;                                      // position
    short attr1, attr2, attr3, attr4, attr5;
    uchar type;                                 // type is one of the above
    uchar reserved;
};

enum
{
    EF_NOVIS      = 1<<0,
    EF_NOSHADOW   = 1<<1,
    EF_NOCOLLIDE  = 1<<2,
    EF_ANIM       = 1<<3,
    EF_SHADOWMESH = 1<<4,
    EF_OCTA       = 1<<5,
    EF_RENDER     = 1<<6,
    EF_SOUND      = 1<<7,
    EF_SPAWNED    = 1<<8,
    EF_ACTIVE     = 1<<9

};

struct extentity : entity                       // part of the entity that doesn't get saved to disk
{
    int flags, triggerstate, lasttrigger, lastspawn;
    extentity *attached;

    extentity() : flags(0), triggerstate(-1), lasttrigger(0), lastspawn(0), attached(NULL) {}

    bool spawned() const { return (flags&EF_SPAWNED) != 0; }
    void setspawned(bool val) { if(val) flags |= EF_SPAWNED; else flags &= ~EF_SPAWNED; }
    void setspawned() { flags |= EF_SPAWNED; }
    void clearspawned() { flags &= ~EF_SPAWNED; }
    void setactivity(bool val) { if (val) flags |= EF_ACTIVE; else flags &= ~EF_ACTIVE; }
    bool isactive() const { return (flags & EF_ACTIVE) != 0; }
};

#define MAXENTS 10000

//extern vector<extentity *> ents;                // map entities

enum { CS_ALIVE = 0, CS_DEAD, CS_SPAWNING, CS_LAGGED, CS_EDITING, CS_SPECTATOR };

enum { PHYS_FLOAT = 0, PHYS_FALL, PHYS_SLIDE, PHYS_SLOPE, PHYS_FLOOR, PHYS_STEP_UP, PHYS_STEP_DOWN, PHYS_BOUNCE };

enum { ENT_PLAYER = 0, ENT_AI, ENT_CAMERA, ENT_PROJECTILE };

enum { COLLIDE_NONE = 0, COLLIDE_ELLIPSE, COLLIDE_OBB, COLLIDE_TRI };

enum
{ 
    // used to mark dynamic entities for certain operations
    DYN_PLAYER     = 1<<0,
    DYN_AI         = 1<<1,
    DYN_PROJECTILE = 1<<2
};

const float CROUCH_HEIGHT = 0.75f;

struct physent // base entity type, can be affected by physics
{
    vec o, vel, falling; // origin, velocity
    vec deltapos, newpos; // movement interpolation
    float yaw, pitch, roll;
    float speed; // cubes per second
    int timeinair;
    float radius, eyeheight, maxheight, aboveeye; // bounding box size
    float xradius, yradius, zmargin;
    float headradius, legsradius;
    vec head;
    vec rfoot, lfoot, lastfootright, lastfootleft;
    vec floor; // the normal of floor the dynent is on

    int inwater;
    bool jumping, doublejumping, climbing;
    char move, strafe, crouching;

    uchar physstate;                            // one of PHYS_* above
    uchar state, editstate;                     // one of CS_* above
    uchar type;                                 // one of ENT_* above
    uchar collidetype;                          // one of COLLIDE_* above

    bool blocked;                               // used by physics to signal ai

    physent() : o(0, 0, 0), deltapos(0, 0, 0), newpos(0, 0, 0), yaw(0), pitch(0), roll(0), speed(100),
               radius(4.1f), eyeheight(15.5f), maxheight(16), aboveeye(2), xradius(4.1f), yradius(4.1f), zmargin(0),
               headradius(2.0f), legsradius(5.0f), head(0, 0, 0),
               rfoot(0, 0, 0), lfoot(0, 0, 0), lastfootright(0, 0, 0), lastfootleft(0, 0, 0),
               state(CS_ALIVE), editstate(CS_ALIVE), type(ENT_PLAYER),
               collidetype(COLLIDE_ELLIPSE),
               blocked(false)
               { reset(); }

    void resetinterp()
    {
        newpos = o;
        deltapos = vec(0, 0, 0);
    }

    void reset()
    {
        inwater = 0;
        timeinair = 0;
        eyeheight = maxheight;
        jumping = doublejumping = climbing = false;
        strafe = move = crouching = 0;
        physstate = PHYS_FALL;
        vel = falling = vec(0, 0, 0);
        floor = vec(0, 0, 1);
    }

    vec feetpos(float offset = 0) const { return vec(o).addz(offset - eyeheight); }
    vec headpos(float offset = 0) const { return vec(o).addz(offset); }

    bool crouched() const { return fabs(eyeheight - maxheight* CROUCH_HEIGHT) < 1e-4f; }
    bool maymove() const { return timeinair || physstate < PHYS_FLOOR || vel.squaredlen() > 1e-4f || deltapos.squaredlen() > 1e-4f; }
};

enum
{
    ANIM_MAPMODEL = 0,
    ANIM_GAMESPECIFIC
};

#define ANIM_ALL         0x1FF
#define ANIM_INDEX       0x1FF
#define ANIM_LOOP        (1<<9)
#define ANIM_CLAMP       (1<<10)
#define ANIM_REVERSE     (1<<11)
#define ANIM_START       (ANIM_LOOP|ANIM_CLAMP)
#define ANIM_END         (ANIM_LOOP|ANIM_CLAMP|ANIM_REVERSE)
#define ANIM_DIR         0xE00
#define ANIM_SECONDARY   12
#define ANIM_REUSE       0xFFFFFF
#define ANIM_NOSKIN      (1<<24)
#define ANIM_SETTIME     (1<<25)
#define ANIM_FULLBRIGHT  (1<<26)
#define ANIM_NORENDER    (1<<27)
#define ANIM_RAGDOLL     (1<<28)
#define ANIM_SETSPEED    (1<<29)
#define ANIM_NOPITCH     (1<<30)
#define ANIM_FLAGS       0xFF000000

struct animinfo // description of a character's animation
{
    int anim, frame, range, basetime;
    float speed;
    uint varseed;

    animinfo() : anim(0), frame(0), range(0), basetime(0), speed(100.0f), varseed(0) { }

    bool operator==(const animinfo &o) const { return frame==o.frame && range==o.range && (anim&(ANIM_SETTIME|ANIM_DIR))==(o.anim&(ANIM_SETTIME|ANIM_DIR)) && (anim&ANIM_SETTIME || basetime==o.basetime) && speed==o.speed; }
    bool operator!=(const animinfo &o) const { return frame!=o.frame || range!=o.range || (anim&(ANIM_SETTIME|ANIM_DIR))!=(o.anim&(ANIM_SETTIME|ANIM_DIR)) || (!(anim&ANIM_SETTIME) && basetime!=o.basetime) || speed!=o.speed; }
};

struct animinterpinfo // used for animation blending of animated characters
{
    animinfo prev, cur;
    int lastswitch;
    void *lastmodel;

    animinterpinfo() : lastswitch(-1), lastmodel(NULL) {}

    void reset() { lastswitch = -1; }
};

#define MAXANIMPARTS 3

struct occludequery;
struct ragdolldata;

struct dynent : physent                         // animated characters, or characters that can receive input
{
    bool k_left, k_right, k_up, k_down;         // see input code

    animinterpinfo animinterp[MAXANIMPARTS];
    ragdolldata *ragdoll;
    occludequery *query;
    int lastrendered;

    dynent() : ragdoll(NULL), query(NULL), lastrendered(0)
    {
        reset();
    }

    ~dynent()
    {
#ifndef STANDALONE
        extern void cleanragdoll(dynent *d);
        if(ragdoll) cleanragdoll(this);
#endif
    }

    void stopmoving()
    {
        k_left = k_right = k_up = k_down = jumping = false;
        move = strafe = crouching = 0;
    }

    void reset()
    {
        physent::reset();
        stopmoving();
        pitch = 0;
        loopi(MAXANIMPARTS) animinterp[i].reset();
    }

    vec abovehead() { return vec(o).addz(aboveeye+4); }
};


