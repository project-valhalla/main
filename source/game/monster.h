
enum { MS_NONE = 0, MS_SEARCH, MS_HOME, MS_ATTACKING, MS_PAIN, MS_SLEEP, MS_AIMING };  // monster states

struct monstertype      // see docs for how these values modify behaviour
{
    short atk, speed, health, freq, lag, rate, pain, loyalty, bscale, weight;
    short painsound, diesound;
    const char *name, *mdlname, *vwepname;
};

static const int TOTMFREQ = 14;
static const int NUMMONSTERTYPES = 9;

static const monstertype monstertypes[NUMMONSTERTYPES] =
{
    { ATK_ROCKET2,  15, 100, 3, 0,   100, 800, 1, 10,  90, S_PAIN_MALE, S_DIE_MALE, "an ogro",     "ogro",            "ogro/vwep"           },
    { ATK_SCATTER2, 18,  70, 2, 70,   10, 400, 2, 10,  50, S_PAIN_MALE, S_DIE_MALE, "a rhino",     "monster/rhino",   NULL                  },
    { ATK_SCATTER1, 13, 120, 1, 100, 300, 400, 4, 14, 115, S_PAIN_MALE, S_DIE_MALE, "ratamahatta", "monster/rat",     "monster/rat/vwep"    },
    { ATK_RAIL1,    14, 200, 1, 80,  400, 300, 4, 18, 145, S_PAIN_MALE, S_DIE_MALE, "a slith",     "monster/slith",   "monster/slith/vwep"  },
    { ATK_ROCKET1,  12, 500, 1, 0,   200, 200, 6, 24, 210, S_PAIN_MALE, S_DIE_MALE, "bauul",       "monster/bauul",   "monster/bauul/vwep"  },
    { ATK_MELEE,    24,  50, 3, 0,   100, 400, 1, 15,  75, S_PAIN_MALE, S_DIE_MALE, "a hellpig",   "monster/hellpig", NULL                  },
    { ATK_PULSE1,   11, 250, 1, 0,    10, 400, 6, 18, 160, S_PAIN_MALE, S_DIE_MALE, "a knight",    "monster/knight",  "monster/knight/vwep" },
    { ATK_PULSE2,   15, 100, 1, 0,   200, 400, 2, 10,  60, S_PAIN_MALE, S_DIE_MALE, "a goblin",    "monster/goblin",  "monster/goblin/vwep" },
    { ATK_GRENADE,  22,  50, 1, 0,   200, 400, 1, 10,  40, S_PAIN_MALE, S_DIE_MALE, "a spider",    "monster/spider",  NULL                  },
};
