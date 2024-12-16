// Hit information to send to the server for hit registration.
struct hitmsg
{
    int target, lifesequence, dist, rays, flags;
    ivec dir;
};

// Hit flags: information on how the player has been damaged.
enum
{
    Hit_Torso       = 1 << 0,
    Hit_Head        = 1 << 1,
    Hit_Legs        = 1 << 2,
    Hit_Environment = 1 << 3
};

// Kill flags: information on how the player has been killed.
enum
{
    KILL_NONE        = 1 << 0,
    KILL_FIRST       = 1 << 1,
    KILL_SPREE       = 1 << 2,
    KILL_SAVAGE      = 1 << 3,
    KILL_UNSTOPPABLE = 1 << 4,
    KILL_LEGENDARY   = 1 << 5,
    KILL_HEADSHOT    = 1 << 6,
    KILL_BERSERKER   = 1 << 7,
    KILL_TRAITOR     = 1 << 8
};

// Death states: information on the logic to apply for each death.
enum
{
    Death_Default = 0,
    Death_Fist,
    Death_Explosion,
    Death_Headshot,
    Death_Gib,
    Death_Shock,
    Death_Disrupt,
    Death_Fall,
    Death_Num
};
inline bool validdeathstate(int state) { return state >= 0 && state < Death_Num; }

// Weapons.
enum
{
    // Main weapons.
    GUN_SCATTER = 0, GUN_SMG, GUN_PULSE, GUN_ROCKET, GUN_RAIL, GUN_GRENADE, GUN_PISTOL,

    // Special weapons.
    GUN_INSTA, GUN_ZOMBIE, GUN_MELEE,

    NUMGUNS
};
inline bool validgun(int gun) { return gun >= 0 && gun < NUMGUNS; }

// Attack types depend on player actions.
enum
{
    ACT_IDLE = 0,
    ACT_MELEE,     // Use melee attack, currently the same for all weapons.
    ACT_PRIMARY,   // Either use the weapon primary fire,
    ACT_SECONDARY, // or use the secondary fire.
    NUMACTS
};
inline bool validact(int act) { return act >= 0 && act < NUMACTS; }

// Weapon attacks: multiple attacks may be contained in a single weapon.
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

const int THRESHOLD_GIB = -50; // If health equals or falls below this threshold, the player bursts into a bloody mist.

const int GUN_MAXRAYS = 20; // Maximum rays a player can shoot, we can't change that.
const int GUN_AIR_PUSH = 2; // Hit push multiplier for the mid-air friends.
const int GUN_ZOMBIE_PUSH = 3; // Hit push multiplier for zombies.
const float GUN_PULSE_SHOCK_IMPULSE = 95.0f; // Funny upward velocity applied to targets dying of electrocution.

const int DELAY_ENVIRONMENT_DAMAGE = 500; // Environmental damage is dealt again after a specific number of milliseconds.
const int DELAY_RESPAWN = 1500; // Spawn is possible after a specific number of milliseconds has elapsed.

const int DAMAGE_ALLYDIV = 2; // Divide damage dealt to self or allies.
const int DAMAGE_ENVIRONMENT = 5; // Environmental damage like lava, damage material and fall damage.

const float EXP_SELFPUSH = 2.5f; // How much our player is going to be pushed from our own projectiles.
const float EXP_DISTSCALE = 1.5f; // Explosion damage is going to be scaled by distance.

const int DURATION_SPAWN = 1000;

static const struct attackinfo
{
    int gun, action, projectile, attackdelay, damage, headshotdamage, spread, margin, projspeed, kickamount, range, rays, hitpush, exprad, lifetime, use;
    float gravity, elasticity;
    bool isfullauto;
    int anim, vwepanim, hudanim, sound, impactsound, hitsound, deathstate;
    const char* obituary, * gibobituary;
} attacks[NUMATKS] =
{
    // melee: default melee for all weapons
    { -1,          ACT_MELEE,                      -1,  650,  60,  0,   0, 2,    0,  0,   14,  1,   50,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         S_IMPACT_MELEE,    S_HIT_MELEE,   Death_Fist,      "pummeled", "shattered"   },
    { GUN_MELEE,   ACT_MELEE,                      -1,  420,  25,  0,   0, 1,    0,  0,   16,  1,   50,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         S_IMPACT_MELEE,    S_HIT_MELEE,   Death_Fist,      "pummeled", "shattered"   },
    // shotgun
    { GUN_SCATTER, ACT_PRIMARY,     Projectile_Bullet,  880,   5,  5, 260, 0, 2000, 20, 1024, 20,   60,  0,  500, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SG_A,          S_IMPACT_SG,       S_HIT_WEAPON,  Death_Default,   "shot",     "splattered"  },
    { GUN_SCATTER, ACT_SECONDARY,                  -1, 1000,   6,  5, 120, 0,    0, 28, 1024, 10,   60,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT2, S_SG_A,          S_IMPACT_SG,       S_HIT_WEAPON,  Death_Default,   "shot",     "splattered"  },
    // smg
    { GUN_SMG,     ACT_PRIMARY,     Projectile_Bullet,  110,  16, 14,  85, 0, 2000,  7, 1024,  1,   60,  0,  500, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SMG,           S_IMPACT_SMG,      S_HIT_WEAPON,  Death_Default,   "sprayed",  NULL          },
    { GUN_SMG,     ACT_SECONDARY,                  -1,  160,  17, 15,  30, 0,    0, 14, 1024,  1,   80,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SMG,           S_IMPACT_SMG,      S_HIT_WEAPON,  Death_Default,   "peppered", NULL          },
    // pulse
    { GUN_PULSE,   ACT_PRIMARY,      Projectile_Pulse,  180,  22,  0,   0, 1, 1000,  8, 2048,  1,   80, 18, 3000, 2,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PULSE1,        S_PULSE_EXPLODE,   S_HIT_WEAPON,  Death_Explosion, "fried",    "vaporised"   },
    { GUN_PULSE,   ACT_SECONDARY,                  -1,   80,  14,  0,   0, 0,    0,  2,  200,  1,  150,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT2, S_PULSE2_A,      S_IMPACT_PULSE,    S_HIT_WEAPON,  Death_Shock,     "zapped",   NULL          },
    // rocket
    { GUN_ROCKET,  ACT_PRIMARY,     Projectile_Rocket,  920, 110,  0,   0, 0,  300,  0, 2048,  1,  120, 33, 5000, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_ROCKET1,       S_ROCKET_EXPLODE,  S_HIT_WEAPON,  Death_Explosion, "blasted",  "chunked"     },
    { GUN_ROCKET,  ACT_SECONDARY,  Projectile_Rocket2,  920, 110,  0,   0, 0,  300,  0, 2048,  1,  120, 33, 2000, 1, 0.6f, 0.7f, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_ROCKET2,       S_ROCKET_EXPLODE,  S_HIT_WEAPON,  Death_Explosion, "cooked",   "chunked"     },
    // railgun
    { GUN_RAIL,    ACT_PRIMARY,     Projectile_Bullet, 1200,  70, 30,   0, 0, 2000, 30, 4096,  1,  100,  0,  500, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_A,        S_IMPACT_RAILGUN,  S_HIT_RAILGUN, Death_Default,   "skewered", "pulverised"  },
    { GUN_RAIL,    ACT_SECONDARY,   Projectile_Bullet, 1500, 100, 10,   0, 0, 2000, 50, 4096,  1,  100,  0,  500, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_A,        S_IMPACT_RAILGUN,  S_HIT_RAILGUN, Death_Default,   "sniped",   "pulverised"  },
    // grenade launcher
    { GUN_GRENADE, ACT_PRIMARY,    Projectile_Grenade,  650,  90,  0,   0, 0,  200, 10, 2048,  1,  120, 45, 1500, 1, 0.7f, 0.8f, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_GRENADE,       S_GRENADE_EXPLODE, S_HIT_WEAPON,  Death_Explosion, "blasted",  "obliterated" },
    { GUN_GRENADE, ACT_SECONDARY, Projectile_Grenade2,  750,  90,  0,   0, 0,  190, 10, 2048,  1,  120, 35, 2000, 1, 1.0f,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_GRENADE,       S_GRENADE_EXPLODE, S_HIT_WEAPON,  Death_Explosion, "cooked",   "obliterated" },
    // pistol
    { GUN_PISTOL,  ACT_PRIMARY,     Projectile_Bullet,  300,  18, 17,  60, 0, 1800, 12, 1024,  1,  180,  0,  500, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PISTOL1,       S_IMPACT_PULSE,    S_HIT_WEAPON,  Death_Default,   "beamed",   NULL          },
    { GUN_PISTOL,  ACT_SECONDARY,   Projectile_Plasma,  600,  15,  0,   0, 5,  400, 15, 2048,  1,  500,  8, 2000, 2,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PISTOL2,       S_IMPACT_PULSE,    S_HIT_WEAPON,  Death_Explosion, "toasted",  NULL          },
    { GUN_PISTOL,  ACT_SECONDARY,                  -1, 1000,  80,  0,   0, 0,  400,  0, 2048,  1, -200, 50,    0, 0,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  -1,              S_IMPACT_PISTOL,   S_HIT_RAILGUN, Death_Disrupt,   "warped",   "atomised"    },
    // instagib
    { GUN_INSTA,   ACT_PRIMARY,                    -1, 1200,  -1, 50,   0, 0,    0, 36, 4096,  1,  200,  0,    0, 0,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_INSTAGUN,      S_IMPACT_RAILGUN,  S_HIT_WEAPON,  Death_Default,   "fragged",  "gibbed"      },
    // zombie
    { GUN_ZOMBIE,  ACT_MELEE,                      -1,  600, 100,  0,   0, 4,    0,  0,   15,  1,   20,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_ZOMBIE,        S_IMPACT_MELEE,    S_HIT_MELEE,   Death_Fist,      "infected", NULL          }
};

static const struct guninfo
{
    const char *name, *model, *worldmodel;
    int attacks[NUMACTS], switchsound, zoom, ejectprojectile;
} guns[NUMGUNS] =
{
    { "scattergun", "scattergun", "weapon/scattergun/world", { -1, ATK_MELEE,  ATK_SCATTER1, ATK_SCATTER2 }, S_SG_SWITCH,      Zoom_None,   Projectile_Casing3 },
    { "smg",        "smg",        "weapon/smg/world",        { -1, ATK_MELEE,  ATK_SMG1,     ATK_SMG2     }, S_SG_SWITCH,      Zoom_Shadow, Projectile_Casing  },
    { "pulse",      "pulserifle", "weapon/pulserifle/world", { -1, ATK_MELEE,  ATK_PULSE1,   ATK_PULSE2   }, S_PULSE_SWITCH,   Zoom_None,   -1                 },
    { "rocket",     "rocket",     "weapon/rocket/world",     { -1, ATK_MELEE,  ATK_ROCKET1,  ATK_ROCKET2  }, S_ROCKET_SWITCH,  Zoom_None,   -1                 },
    { "railgun",    "railgun",    "weapon/railgun/world",    { -1, ATK_MELEE,  ATK_RAIL1,    ATK_RAIL2    }, S_RAIL_SWITCH,    Zoom_Scope,  Projectile_Casing2 },
    { "grenade",    "grenade",    "weapon/grenade/world",    { -1, ATK_MELEE,  ATK_GRENADE1, ATK_GRENADE2 }, S_GRENADE_SWITCH, Zoom_None,   -1                 },
    { "pistol",     "pistol",     "weapon/pistol/world",     { -1, ATK_MELEE,  ATK_PISTOL1,  ATK_PISTOL2  }, S_PISTOL_SWITCH,  Zoom_None,   -1                 },
    { "instagun",   "instagun",   "weapon/railgun/world",    { -1, ATK_MELEE,  ATK_INSTA,    ATK_INSTA    }, S_RAIL_SWITCH,    Zoom_Scope,  -1                 },
    { "zombie",     "zombie",     NULL,                      { -1, ATK_ZOMBIE, ATK_ZOMBIE,   ATK_ZOMBIE   }, -1,               Zoom_Shadow, -1                 },
    { "melee",      "melee",      NULL,                      { -1, ATK_MELEE2, ATK_MELEE2,   ATK_MELEE2   }, -1,               Zoom_None,   -1                 }
};
