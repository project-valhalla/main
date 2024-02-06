
enum // monster states
{
    MS_NONE = 0,
    MS_SEARCH,
    MS_HOME,
    MS_ATTACKING,
    MS_PAIN,
    MS_SLEEP,
    MS_AIMING
};

enum
{
    MTYPE_BRUTE = 0,
    MTYPE_GRAY,
    MTYPE_GUARD,
    MTYPE_MECH,
    MTYPE_LURKER,
    MTYPE_SPIDER,
    NUMMONSTERS
};
inline bool validmonster(int monster) { return monster >= 0 && monster < NUMMONSTERS; }

struct monstertype // see docs for how these values modify behaviour
{
    int type, atk, speed, healthbonus, health, freq, lag, rate, pain, loyalty, bscale, weight, bloodcolor;
    int painsound, diesound, haltsound;
    const char *name, *mdlname, *vwepname;
    bool neutral, ragdoll;
};

static const int TOTMFREQ = 14;

static const monstertype monstertypes[NUMMONSTERS] =
{
    { MTYPE_BRUTE,  ATK_PISTOL2,  12, 25, 100, 3,   0, 100, 800, 1, 10,  90, 0x60FFFF, S_OGRO_PAIN,    S_OGRO_DEATH,    S_OGRO_HALT,    "brute",       "monster/ogro",   "weapons/pistol/world",  false, true  },
    { MTYPE_GRAY,   ATK_SCATTER1, 14, 30, 120, 1, 100, 300, 400, 4, 14, 115, 0xFFFF90, S_RATA_PAIN,    S_RATA_DEATH,    S_RATA_HALT,    "gray",        "monster/gray",   "monster/rata/vwep",     false, false },
    { MTYPE_GUARD,  ATK_RAIL1,    15, 50, 200, 1,  80, 400, 300, 4, 18, 145, 0x60FFFF, S_SLITH_PAIN,   S_SLITH_DEATH,   S_SLITH_HALT,   "guard",       "monster/guard",  "weapons/railgun/world", false, true  },
    { MTYPE_MECH,   ATK_ROCKET1,  12, 30, 500, 1,   0, 200, 200, 6, 24, 210,       -1, S_BAUUL_PAIN,   S_BAUUL_DEATH,   S_BAUUL_HALT,   "mech",        "monster/mech",   NULL,                    false, true  },
    { MTYPE_LURKER, ATK_MELEE,    20,  0, 50,  3,   0, 100, 100, 1, 15,  75, 0xFFFF90, S_HELLPIG_PAIN, S_HELLPIG_DEATH, S_HELLPIG_HALT, "lurker",      "monster/lurker", NULL,                    true,  false },
    { MTYPE_SPIDER, ATK_MELEE,    19, 30, 30,  1,   0, 200, 400, 1, 10,  40, 0xFF90FF, S_SPIDER_PAIN,  S_SPIDER_DEATH,  S_SPIDER_HALT,  "cybercrab",   "monster/spider", NULL,                    false, false }
};
