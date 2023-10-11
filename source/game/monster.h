
enum { MS_NONE = 0, MS_SEARCH, MS_HOME, MS_ATTACKING, MS_PAIN, MS_SLEEP, MS_AIMING };  // monster states

struct monstertype      // see docs for how these values modify behaviour
{
    short atk, speed, health, freq, lag, rate, pain, loyalty, bscale, weight;
    short painsound, diesound;
    const char *name, *mdlname, *vwepname;
};

static const int TOTMFREQ = 14;
static const int NUMMONSTERTYPES = 7;

static const monstertype monstertypes[NUMMONSTERTYPES] =
{
    { ATK_PULSE1,   15, 100, 3, 0,   100, 800, 1, 10,  90, S_PAIN_MALE, S_DIE_MALE, "brute",       "monster/ogro",    "ogro/vwep"           },
    { ATK_SMG2,     18,  70, 2, 70,   10, 400, 2, 10,  50, S_PAIN_MALE, S_DIE_MALE, "rhinotron",   "monster/rhino",   NULL                  },
    { ATK_SCATTER1, 13, 120, 1, 100, 300, 400, 4, 14, 115, S_PAIN_MALE, S_DIE_MALE, "gorn",        "monster/rata",    "monster/rata/vwep"   },
    { ATK_RAIL1,    14, 200, 1, 80,  400, 300, 4, 18, 145, S_PAIN_MALE, S_DIE_MALE, "slith",       "monster/slith",   "monster/slith/vwep"  },
    { ATK_ROCKET1,  12, 500, 1, 0,   200, 200, 6, 24, 210, S_PAIN_MALE, S_DIE_MALE, "mogul",       "monster/bauul",   "monster/bauul/vwep"  },
    { ATK_MELEE,    24,  50, 3, 0,   100, 400, 1, 15,  75, S_PAIN_MALE, S_DIE_MALE, "lurker",      "monster/hellpig", NULL                  },
    { ATK_MELEE,    22,  50, 1, 0,   200, 400, 1, 10,  40, S_PAIN_MALE, S_DIE_MALE, "cybercrab",   "monster/spider",  NULL                  }
};
