
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
    MTYPE_PLAYER = 0,
    MTYPE_BRUTE,
    MTYPE_GRAY,
    MTYPE_GUARD,
    MTYPE_MECH,
    MTYPE_SPIDER,
    NUMMONSTERS
};
inline bool validmonster(int monster) { return monster >= 0 && monster < NUMMONSTERS; }

static const int MONSTER_EXPLODE_DELAY = 8000;
static const int MONSTER_DETONATION_DELAY = 350;

static const int TOTMFREQ = 14;

struct monstertype // see docs for how these values modify behaviour
{
    int type, atk, meleeatk, speed, speedbonus, health, healthbonus, burstshots, freq, lag, rate, pain, loyalty, bscale, weight, bloodcolor;
    int painsound, diesound, haltsound, unhaltsound, attacksound;
    const char *name, *mdlname, *worldgunmodel;
    bool isexplosive, isefficient, hasragdoll;
};

static const monstertype monstertypes[NUMMONSTERS] =
{
    { MTYPE_PLAYER, ATK_PULSE1,   ATK_MELEE,  30,  0, 100, 25, 0, 0, 200, 100, 100, 1, 12, 100, 0x60FFFF, S_PAIN_MALE,    S_DIE_MALE,      NULL,           NULL,           NULL,            "clone",       "player/bones",   "weapon/pulserifle/world", false, false, true  },
    { MTYPE_BRUTE,  ATK_PISTOL2,  ATK_MELEE2, 12,  0, 100, 25, 0, 3,   0, 100, 800, 1, 12, 100, 0x60FFFF, S_OGRO_PAIN,    S_OGRO_DEATH,    S_OGRO_HALT,    S_OGRO_HALT,    NULL,            "brute",       "monster/ogro",   "weapon/pistol/world",     false, false, true  },
    { MTYPE_GRAY,   ATK_SCATTER1, ATK_MELEE2, 14,  0, 120, 30, 0, 1, 100, 400, 400, 4, 15,  60, 0xFFFF90, S_RATA_PAIN,    S_RATA_DEATH,    S_RATA_HALT,    NULL,           NULL,            "gray",        "monster/gray",   "weapon/scattergun/world", false, false, false },
    { MTYPE_GUARD,  ATK_PULSE1,   ATK_MELEE2, 15,  0, 200, 50, 8, 1,  60, 100, 100, 4, 14, 140, 0x60FFFF, S_GUARD_PAIN,   S_GUARD_DEATH,   S_GUARD_HALT,   S_GUARD_UNHALT, S_GUARD_ATTACK,  "guard",       "monster/guard",  "weapon/pulserifle/world", false, true,  true  },
    { MTYPE_MECH,   ATK_ROCKET1,  ATK_MELEE,  12, 14, 500, 80, 0, 1,   0, 200, 200, 6, 22, 220,       -1, S_BAUUL_PAIN,   S_BAUUL_DEATH,   S_BAUUL_HALT,   NULL,           NULL,            "mech",        "monster/mech",   NULL,                      true,  false, true  },
    { MTYPE_SPIDER, ATK_MELEE2,   ATK_MELEE2, 19,  0,  30, 30, 0, 1,   0, 200, 400, 1,  7,  20, 0xFF90FF, S_SPIDER_PAIN,  S_SPIDER_DEATH,  S_SPIDER_HALT,  NULL,           S_SPIDER_ATTACK, "generantula", "monster/spider", NULL,                      false, false, false }
};
