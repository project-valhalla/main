
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
    MTYPE_LURKER,
    MTYPE_SPIDER,
    NUMMONSTERS
};
inline bool validmonster(int monster) { return monster >= 0 && monster < NUMMONSTERS; }

static const int MONSTER_EXPLODE_DELAY = 8000;
static const int MONSTER_DETONATION_DELAY = 350;

static const int TOTMFREQ = 14;

struct monstertype // see docs for how these values modify behaviour
{
    int type, atk, speed, speedbonus, health, healthbonus, freq, lag, rate, pain, loyalty, bscale, weight, bloodcolor;
    int painsound, diesound, haltsound, attacksound;
    const char *name, *mdlname, *worldgunmodel;
    bool isexplosive, isneutral, hasragdoll;
};

static const monstertype monstertypes[NUMMONSTERS] =
{
    { MTYPE_PLAYER, ATK_PULSE1,   30,  0, 100, 25, 0, 200, 100, 100, 1, 12, 100, 0x60FFFF, S_PAIN_MALE,    S_DIE_MALE,      NULL,           NULL,            "clone",     "player/bones",   "weapon/pulserifle/world", false, false, true  },
    { MTYPE_BRUTE,  ATK_PISTOL2,  12,  0, 100, 25, 3,   0, 100, 800, 1, 12, 100, 0x60FFFF, S_OGRO_PAIN,    S_OGRO_DEATH,    S_OGRO_HALT,    NULL,            "brute",     "monster/ogro",   "monster/ogro/weapon",     false, false, true  },
    { MTYPE_GRAY,   ATK_SCATTER1, 14,  0, 120, 30, 1, 100, 300, 400, 4, 15,  60, 0xFFFF90, S_RATA_PAIN,    S_RATA_DEATH,    S_RATA_HALT,    NULL,            "gray",      "monster/gray",   "weapon/scattergun/world", false, false, false },
    { MTYPE_GUARD,  ATK_RAIL1,    15,  0, 200, 50, 1,  80, 400, 300, 4, 14, 140, 0x60FFFF, S_GUARD_PAIN,   S_GUARD_DEATH,   S_GUARD_HALT,   S_GUARD_ATTACK,  "guard",     "monster/guard",  "monster/guard/weapon",    false, false, true  },
    { MTYPE_MECH,   ATK_ROCKET1,  12, 14, 500, 80, 1,   0, 200, 200, 6, 22, 220,       -1, S_BAUUL_PAIN,   S_BAUUL_DEATH,   S_BAUUL_HALT,   NULL,            "mech",      "monster/mech",   NULL,                      true,  false, true  },
    { MTYPE_LURKER, ATK_MELEE,    20,  0,  50,  0, 3,   0, 100, 100, 1, 13,  75, 0xFFFF90, S_HELLPIG_PAIN, S_HELLPIG_DEATH, S_HELLPIG_HALT, NULL,            "lurker",    "monster/lurker", NULL,                      false, false, true  },
    { MTYPE_SPIDER, ATK_MELEE,    19,  0,  30, 30, 1,   0, 200, 400, 1,  7,  20, 0xFF90FF, S_SPIDER_PAIN,  S_SPIDER_DEATH,  S_SPIDER_HALT,  S_SPIDER_ATTACK, "cybercrab", "monster/spider", NULL,                      false, false, false }
};
