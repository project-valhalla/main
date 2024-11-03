// hit flags
enum
{
    HIT_TORSO    = 1<<0,
    HIT_LEGS     = 1<<1,
    HIT_HEAD     = 1<<2,
    HIT_MATERIAL = 1<<3
};

// attack types depend on player actions
enum
{
    ACT_IDLE = 0,
    ACT_MELEE,     // use melee attack, currently the same for all weapons
    ACT_PRIMARY,   // either use the weapon primary fire
    ACT_SECONDARY, // or use the secondary fire
    NUMACTS
};
inline bool validact(int act) { return act >= 0 && act < NUMACTS; }

enum
{
    // main weapons
    GUN_SCATTER = 0, GUN_SMG, GUN_PULSE, GUN_ROCKET, GUN_RAIL, GUN_GRENADE, GUN_PISTOL,

    // special weapons
    GUN_INSTA, GUN_ZOMBIE,

    NUMGUNS
};
inline bool validgun(int gun) { return gun >= 0 && gun < NUMGUNS; }

// weapon attacks
enum
{
    ATK_MELEE = 0, ATK_MELEE2,

    ATK_SCATTER1, ATK_SCATTER2,
    ATK_SMG1, ATK_SMG2,
    ATK_PULSE1, ATK_PULSE2,
    ATK_ROCKET1, ATK_ROCKET2,
    ATK_RAIL1, ATK_RAIL2,
    ATK_GRENADE1, ATK_GRENADE2,
    ATK_PISTOL1, ATK_PISTOL2, ATK_PISTOL_COMBO,

    ATK_INSTA, ATK_ZOMBIE,

    NUMATKS
};
inline bool validatk(int atk) { return atk >= 0 && atk < NUMATKS; }

const int HEALTH_GIB = -50; // if health equals or falls below this threshold, the player bursts into a bloody mist

const int GUN_MAXRAYS = 20; // maximum rays a player can shoot, we can't change that

const int DELAY_ENVDAM = 500; // environmental damage is dealt again after a specific number of milliseconds
const int DELAY_RESPAWN = 1500; // spawn is possible after a specific number of milliseconds has elapsed

const int DAM_ALLYDIV = 2;  // divide damage dealt to self or allies
const int DAM_ENV = 5;  // environmental damage like lava, damage material and fall damage

const float EXP_SELFPUSH = 2.5f; // how much our player is going to be pushed from our own projectiles
const float EXP_DISTSCALE = 1.5f; // explosion damage is going to be scaled by distance

static const struct attackinfo
{
    int gun, action, attackdelay, damage, headshotdam, spread, margin, projspeed, kickamount, range, rays, hitpush, exprad, lifetime, use;
    float gravity, elasticity;
    bool isfullauto;
    int anim, vwepanim, hudanim, sound, impactsound, hitsound;
} attacks[NUMATKS] =
{
    // melee: default melee for all weapons
    { -1,          ACT_MELEE,      650,  60,  0,   0, 2,    0,  0,   14,  1,  50,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         S_IMPACT_MELEE,    S_HIT_MELEE   },
    { -1,          ACT_MELEE,      420,  25,  0,   0, 1,    0,  0,   16,  1,  50,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         S_IMPACT_MELEE,    S_HIT_MELEE   },
    // shotgun
    { GUN_SCATTER, ACT_PRIMARY,    880,   5,  5, 260, 0,    0, 20, 1000, 20,  60,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SG_A,          S_IMPACT_SG,       S_HIT_WEAPON  },
    { GUN_SCATTER, ACT_SECONDARY, 1000,   6,  5, 120, 0,    0, 25, 1000, 10,  60,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SG_A,          S_IMPACT_SG,       S_HIT_WEAPON  },
    // smg
    { GUN_SMG,     ACT_PRIMARY,    110,  16, 14,  85, 0,    0,  7, 1000,  1,  60,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SMG,           S_IMPACT_SMG,      S_HIT_WEAPON  },
    { GUN_SMG,     ACT_SECONDARY,  160,  17, 15,  30, 0,    0, 14, 1000,  1,  80,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SMG,           S_IMPACT_SMG,      S_HIT_WEAPON  },
    // pulse
    { GUN_PULSE,   ACT_PRIMARY,    180,  22,  0,   0, 1, 1000,  8, 2048,  1,  80, 18, 3000, 2,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PULSE1,        S_PULSE_EXPLODE,   S_HIT_WEAPON  },
    { GUN_PULSE,   ACT_SECONDARY,   80,  14,  0,   0, 0,    0,  2,  200,  1, 150,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT2, S_PULSE2_A,      S_IMPACT_PULSE,    S_HIT_WEAPON  },
    // rocket
    { GUN_ROCKET,  ACT_PRIMARY,    920, 110,  0,   0, 0,  300,  0, 2048,  1, 110, 33, 5000, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_ROCKET1,       S_ROCKET_EXPLODE,  S_HIT_WEAPON  },
    { GUN_ROCKET,  ACT_SECONDARY,  920, 110,  0,   0, 0,  300,  0, 2048,  1, 110, 33, 2000, 1, 0.6f, 0.7f, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_ROCKET2,       S_ROCKET_EXPLODE,  S_HIT_WEAPON  },
    // railgun
    { GUN_RAIL,    ACT_PRIMARY,   1200,  70, 30,   0, 0,    0, 30, 5000,  1, 100,  0,    0, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_A,        S_IMPACT_RAILGUN,  S_HIT_RAILGUN },
    { GUN_RAIL,    ACT_SECONDARY, 1500, 100, 10,   0, 0,    0, 50, 5000,  1, 100,  0,    0, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_A,        S_IMPACT_RAILGUN,  S_HIT_RAILGUN },
    // grenade launcher
    { GUN_GRENADE, ACT_PRIMARY,    650,  90,  0,   0, 0,  200, 10, 2024,  1, 250, 45, 1500, 1, 0.7f, 0.8f, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_GRENADE,       S_GRENADE_EXPLODE, S_HIT_WEAPON  },
    { GUN_GRENADE, ACT_SECONDARY,  750,  90,  0,   0, 0,  190, 10, 2024,  1, 200, 35, 2000, 1, 1.0f,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_GRENADE,       S_GRENADE_EXPLODE, S_HIT_WEAPON  },
    // pistol
    { GUN_PISTOL,  ACT_PRIMARY,    300,  18, 17,  60, 0,    0, 12, 1000,  1, 180,  0,    0, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PISTOL1,       S_IMPACT_PULSE,    S_HIT_WEAPON  },
    { GUN_PISTOL,  ACT_SECONDARY,  600,  15,  0,   0, 5,  400, 15, 2048,  1, 500,  8, 2000, 2,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PISTOL2,       S_IMPACT_PULSE,    S_HIT_WEAPON  },
    { GUN_PISTOL,  ACT_SECONDARY, 1000,  80,  0,   0, 0,  400,  0, 2048,  1, 350, 50,    0, 0,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  -1,              S_IMPACT_PISTOL,   S_HIT_RAILGUN },
    // instagib
    { GUN_INSTA,   ACT_PRIMARY,   1200,   1,  0,   0, 0,    0, 36, 4000,  1,   1,  0,    0, 0,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_INSTAGIB, S_IMPACT_RAILGUN,  S_HIT_WEAPON  },
    // zombie
    { GUN_ZOMBIE,  ACT_MELEE,      600, 100,  0,   0, 4,    0,  0,   15,  1,  20,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_ZOMBIE,        S_IMPACT_MELEE,    S_HIT_MELEE   }
};

static const struct guninfo
{
    const char *name, *model, *worldmodel;
    int attacks[NUMACTS], switchsound, zoom;
} guns[NUMGUNS] =
{
    { "scattergun", "scattergun", "weapon/scattergun/world", { -1, ATK_MELEE,  ATK_SCATTER1, ATK_SCATTER2 }, S_SG_SWITCH,      ZOOM_NONE   },
    { "smg",        "smg",        "weapon/smg/world",        { -1, ATK_MELEE,  ATK_SMG1,     ATK_SMG2     }, S_SG_SWITCH,      ZOOM_SHADOW },
    { "pulse",      "pulserifle", "weapon/pulserifle/world", { -1, ATK_MELEE,  ATK_PULSE1,   ATK_PULSE2   }, S_PULSE_SWITCH,   ZOOM_NONE   },
    { "rocket",     "rocket",     "weapon/rocket/world",     { -1, ATK_MELEE,  ATK_ROCKET1,  ATK_ROCKET2  }, S_ROCKET_SWITCH,  ZOOM_NONE   },
    { "railgun",    "railgun",    "weapon/railgun/world",    { -1, ATK_MELEE,  ATK_RAIL1,    ATK_RAIL2    }, S_RAIL_SWITCH,    ZOOM_SCOPE  },
    { "grenade",    "grenade",    "weapon/grenade/world",    { -1, ATK_MELEE,  ATK_GRENADE1, ATK_GRENADE2 }, S_GRENADE_SWITCH, ZOOM_NONE   },
    { "pistol",     "pistol",     "weapon/pistol/world",     { -1, ATK_MELEE,  ATK_PISTOL1,  ATK_PISTOL2  }, S_PISTOL_SWITCH,  ZOOM_NONE   },
    { "instagun",   "instagun",   "weapon/railgun/world",    { -1, ATK_MELEE,  ATK_INSTA,    ATK_INSTA    }, S_RAIL_SWITCH,    ZOOM_SCOPE  },
    { "zombie",     "zombie",     "",                        { -1, ATK_ZOMBIE, ATK_ZOMBIE,   ATK_ZOMBIE   }, -1,               ZOOM_SHADOW }
};

enum
{
    KILL_NONE        = 1<<0,
    KILL_FIRST       = 1<<1,
    KILL_SPREE       = 1<<2,
    KILL_SAVAGE      = 1<<3,
    KILL_UNSTOPPABLE = 1<<4,
    KILL_LEGENDARY   = 1<<5,
    KILL_HEADSHOT    = 1<<6,
    KILL_BERSERKER   = 1<<7,
    KILL_TRAITOR     = 1<<8
};

enum
{
    DEATH_FIST = -1,
    DEATH_DEFAULT = 0,
    DEATH_GIB,
    DEATH_FALL,
    DEATH_DISRUPT
};
