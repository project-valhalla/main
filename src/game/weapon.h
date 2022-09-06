// hit flags
enum
{
    HIT_TORSO = 1<<0,
    HIT_LEGS = 1<<1,
    HIT_HEAD = 1<<2,
    HIT_MATERIAL = 1<<3
};

// attack types depend on player actions
enum
{
    ACT_IDLE = 0,
    ACT_MELEE, // use melee attack, currently the same for all weapons
    ACT_PRIMARY, // either use the weapon primary fire
    ACT_SECONDARY, // or use the secondary fire
    NUMACTS
};

enum
{
    // main weapons
    GUN_SG = 0, GUN_SMG, GUN_PULSE, GUN_RL, GUN_RAIL, GUN_PISTOL,

    // special weapons
    GUN_INSTA, GUN_ZOMBIE,

    NUMGUNS
};

#define validgun(n) ((n) >= 0 && (n) < NUMGUNS)

// weapon attacks
enum
{
    ATK_MELEE = 0,

    ATK_SG1, ATK_SG2,
    ATK_SMG1, ATK_SMG2,
    ATK_PULSE1, ATK_PULSE2,
    ATK_RL1, ATK_RL2,
    ATK_RAIL,
    ATK_PISTOL1, ATK_PISTOL2,

    ATK_INSTA, ATK_ZOMBIE,

    ATK_TELEPORT, ATK_STOMP,

    NUMATKS
};

#define validact(n) ((n) >= 0 && (n) < NUMACTS)
#define validatk(n) ((n) >= 0 && (n) < ATK_TELEPORT)
#define validsatk(n) ((n) >= ATK_TELEPORT && (n) < NUMATKS)

#define MAXRAYS 20
#define EXP_SELFPUSH 2.5f
#define EXP_DISTSCALE 1.5f

#define ALLY_DAMDIV 2 // divide damage dealt to self or allies
#define ENV_DAM 5 // environmental damage like lava, damage material and fall damage

static const struct attackinfo
{
    int gun, action, attackdelay, damage, headshotdam, spread, margin, projspeed, kickamount, range, rays, hitpush, exprad, lifetime, use, anim, vwepanim, hudanim, sound, impactsound, hitsound;
} attacks[NUMATKS] =
{
    // melee: default melee for all weapons
    { NULL,       ACT_MELEE,     650,  54,  0,   0, 2,    0,  0,   14,  1,  30,  0,    0, 0, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE, S_MELEE,         S_HIT_MELEE,      S_HIT_MELEE,  },
    // shotgun
    { GUN_SG,     ACT_PRIMARY,  1080,   6,  0, 320, 0,    0,  0, 1000, 20,  50,  0,    0, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SG1_A,         S_IMPACT_SG,      S_HIT_WEAPON  },
    { GUN_SG,     ACT_SECONDARY, 600,  65,  0,   0, 2,  180,  0, 2048,  1,  60, 30, 2000, 2, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SG2,           S_ROCKET_EXPLODE, S_HIT_WEAPON  },
    // smg
    { GUN_SMG,    ACT_PRIMARY,   110,  15, 12,  84, 0,    0,  0, 1000,  1,  20,  0,    0, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SMG,           S_IMPACT_SMG,     S_HIT_WEAPON  },
    { GUN_SMG,    ACT_SECONDARY, 240,  18, 10,  30, 0,    0,  0, 1000,  1,  40,  0,    0, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SMG,           S_IMPACT_SMG,     S_HIT_WEAPON  },
    // pulse
    { GUN_PULSE,  ACT_PRIMARY,   180,  22,  0,   0, 1, 1000,  0, 2048,  1,  75, 18, 3000, 2, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_PULSE1,        S_PULSE_EXPLODE,  S_HIT_WEAPON  },
    { GUN_PULSE,  ACT_SECONDARY,  80,  12,  4,   0, 0,    0,  0,  200,  1, 100,  0,    0, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_IDLE,  S_PULSE2_A,      S_IMPACT_PULSE,   S_HIT_WEAPON  },
    // rocket
    { GUN_RL,     ACT_PRIMARY,   920, 110,  0,   0, 0,  300,  0, 2048,  1, 120, 33, 5000, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_ROCKET1,       S_ROCKET_EXPLODE, S_HIT_WEAPON  },
    { GUN_RL,     ACT_SECONDARY, 920, 110,  0,   0, 0,  200,  0, 2048,  1, 120, 33, 1500, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_ROCKET2,       S_ROCKET_EXPLODE, S_HIT_WEAPON  },
    // railgun
    { GUN_RAIL,   ACT_PRIMARY,  1200,  70, 70,   0, 0,    0, 40, 4000,  1, 110,  0,    0, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_RAIL_A,        S_IMPACT_RAILGUN, S_HIT_RAILGUN },
    // pistol
    { GUN_PISTOL, ACT_PRIMARY,   300,   8,  5,   0, 0,    0,  0, 1000,  1, 200,  0,    0, 1, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_PISTOL1,       S_IMPACT_PULSE,   S_HIT_WEAPON  },
    { GUN_PISTOL, ACT_SECONDARY, 600,  10,  0,   0, 5, 1000,  0, 2048,  1, 500,  8,  800, 2, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_PISTOL2,       S_IMPACT_PULSE,   S_HIT_WEAPON  },
    // instagib
    { GUN_INSTA,  ACT_PRIMARY,  1200, 150,  0,   0, 0,    0, 60, 4000,  1,  30,  0,    0, 0, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_RAIL_INSTAGIB, S_IMPACT_RAILGUN, S_HIT_WEAPON  },
    // zombie
    { GUN_ZOMBIE, ACT_PRIMARY,   600, 100,  0,   0, 4,    0,  0,   15,  1,  20,  0,    0, 0, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE, S_ZOMBIE,        S_HIT_MELEE,      S_HIT_MELEE   },

    // telefrag
    { -1, ACT_MELEE, ANIM_IDLE, ANIM_VWEP_IDLE, ANIM_GUN_IDLE, -1, NULL, NULL, 0, 1050, 0, 0, 8, 0, 0, 6, 1, 0, 0, 0, 0 },
    // stomp
    { -1, ACT_MELEE, ANIM_IDLE, ANIM_VWEP_IDLE, ANIM_GUN_IDLE, -1, NULL, NULL, 0, 100, 0, 0, 3, 0, 0, 2, 1, 0, 0, 0, 0 }
};

static const struct guninfo { const char *name, *file, *vwep; int attacks[NUMACTS]; } guns[NUMGUNS] =
{
    { "shotgun", "shotgun", "weapon/worldgun/shotgun", { -1, ATK_MELEE, ATK_SG1, ATK_SG2 } },
    { "smg", "smg", "weapon/worldgun/smg", { -1, ATK_MELEE, ATK_SMG1, ATK_SMG2 }, },
    { "pulse", "pulserifle", "weapon/worldgun/pulserifle", { -1, ATK_MELEE, ATK_PULSE1, ATK_PULSE2 }, },
    { "rocket", "rocket", "weapon/worldgun/rocket", { -1, ATK_MELEE, ATK_RL1, ATK_RL2 } },
    { "railgun", "railgun", "weapon/worldgun/railgun", { -1, ATK_MELEE, ATK_RAIL, ATK_RAIL }, },
    { "pistol", "pistol", "weapon/worldgun/pulserifle", { -1, ATK_MELEE, ATK_PISTOL1, ATK_PISTOL2 }, },
    { "instagun", "railgun", "weapon/worldgun/railgun", { -1, ATK_MELEE, ATK_INSTA, ATK_INSTA }, },
    { "zombie", "zombie", "", { -1, ATK_ZOMBIE, ATK_ZOMBIE, ATK_ZOMBIE }, }
};
