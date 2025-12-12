// Hit information to send to the server for hit registration.
struct hitmsg
{
    int target, lifesequence, dist, rays, flags, id;
    ivec dir;
};

// Hit flags: information on how the player has been damaged.
enum
{
    Hit_Torso       = 1 << 0,
    Hit_Head        = 1 << 1,
    Hit_Legs        = 1 << 2,
    Hit_Projectile  = 1 << 3,
    Hit_Environment = 1 << 4
};

// Kill flags: information on how the player has been killed.
enum
{
    KILL_FIRST       = 1 << 0,
    KILL_SPREE       = 1 << 1,
    KILL_SAVAGE      = 1 << 2,
    KILL_UNSTOPPABLE = 1 << 3,
    KILL_LEGENDARY   = 1 << 4,
    KILL_HEADSHOT    = 1 << 5,
    KILL_BERSERKER   = 1 << 6,
    KILL_TRAITOR     = 1 << 7,
    KILL_EXPLOSION   = 1 << 8
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
    GUN_INVALID	= -1,
    GUN_SCATTER, GUN_SMG, GUN_PULSE, GUN_ROCKET, GUN_RAIL, GUN_GRENADE, GUN_PISTOL, // Main weapons.
    GUN_INSTA, GUN_ZOMBIE, GUN_MELEE, // Special weapons.
    NUMGUNS
};
inline bool validgun(int gun) { return gun > GUN_INVALID && gun < NUMGUNS; }

// Attack types depend on player actions.
enum
{
    ACT_IDLE = -1,
    ACT_MELEE,     // Use melee attack, currently the same for all weapons.
	ACT_THROW,     // Throw a grenade.
    ACT_PRIMARY,   // Either use the weapon primary fire,
    ACT_SECONDARY, // or use the secondary fire.
    NUMACTS
};
inline bool validact(int act) { return act > ACT_IDLE && act < NUMACTS; }

// Weapon attacks: multiple attacks may be contained in a single weapon.
enum
{
    ATK_INVALID = -1,
    ATK_MELEE, ATK_MELEE2,
    ATK_SCATTER1, ATK_SCATTER2,
    ATK_SMG1, ATK_SMG2,
    ATK_PULSE1, ATK_PULSE2,
    ATK_ROCKET1, ATK_ROCKET2,
    ATK_RAIL1, ATK_RAIL2,
    ATK_GRENADE1, ATK_GRENADE2,
    ATK_PISTOL1, ATK_PISTOL2, ATK_PISTOL3,
    ATK_INSTA, ATK_ZOMBIE,
    NUMATKS
};
inline bool validatk(int atk) { return atk > ATK_INVALID && atk < NUMATKS; }

const int THRESHOLD_GIB = -50; // If health equals or falls below this threshold, the player bursts into a bloody mist.

const int GUN_MAXRAYS = 20; // Maximum rays a player can shoot, we can't change that.
const int GUN_AIR_PUSH = 2; // Hit push multiplier for the mid-air friends.
const int GUN_ZOMBIE_PUSH = 3; // Hit push multiplier for zombies.
const int GUN_THROW_DELAY = 200; // Delay grenade throws.
const int GUN_EMPTY_DELAY = 600; // Delay when trying to shoot with an empty gun.

const int DAMAGE_ALLYDIV = 2; // Divide damage dealt to self or allies.
const int DAMAGE_ENVIRONMENT = 5; // Environmental damage like lava, damage material and fall damage.
const int DAMAGE_ENVIRONMENT_DELAY = 500; // Environmental damage is dealt again after a specific number of milliseconds.

const float EXP_SELFPUSH = 2.5f; // How much our player is going to be pushed from our own projectiles.
const float EXP_DISTSCALE = 1.5f; // Explosion damage is going to be scaled by distance.

static const struct attackinfo
{
    int gun, action, projectile, attackdelay, damage, headshotdamage, spread, margin, projspeed, recoilamount, range, rays, hitpush, exprad, lifetime, use;
    float gravity, elasticity;
    bool isfullauto;
    int anim, vwepanim, hudanim, sound, impactsound, hitsound, deathstate;
} attacks[NUMATKS] =
{
    // melee: default melee for all weapons
    { GUN_INVALID, ACT_MELEE,      Projectile_Invalid,  650,  60,  0,   0, 2,    0, 80,   14,  1,   50,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         S_IMPACT_MELEE,    S_HIT_MELEE,   Death_Fist      },
    { GUN_MELEE,   ACT_MELEE,      Projectile_Invalid,  420,  25,  0,   0, 1,    0,  0,   16,  1,   50,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         S_IMPACT_MELEE,    S_HIT_MELEE,   Death_Fist      },
    // shotgun
    { GUN_SCATTER, ACT_PRIMARY,     Projectile_Bullet,  880,   5,  5, 260, 0, 2000, 20, 1024, 20,   60,  0,  500, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_SG_A,          S_IMPACT_SG,       S_HIT_WEAPON,  Death_Default   },
    { GUN_SCATTER, ACT_SECONDARY,  Projectile_Invalid, 1000,   6,  5, 120, 0,    0, 28, 1024, 10,   60,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SECONDARY, S_SG_A,          S_IMPACT_SG,       S_HIT_WEAPON,  Death_Default   },
    // smg
    { GUN_SMG,     ACT_PRIMARY,     Projectile_Bullet,  110,  16, 14,  85, 0, 2000,  7, 1024,  1,   60,  0,  500, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_SMG,           S_IMPACT_SMG,      S_HIT_WEAPON,  Death_Default   },
    { GUN_SMG,     ACT_SECONDARY,  Projectile_Invalid,  125,  20, 15,  30, 0,    0, 14, 1024,  1,   80,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_SMG,           S_IMPACT_SMG,      S_HIT_WEAPON,  Death_Default   },
    // pulse
    { GUN_PULSE,   ACT_PRIMARY,      Projectile_Pulse,  180,  22,  0,   0, 1, 1000,  8, 2048,  1,   80, 18, 3000, 2,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_PULSE1,        S_PULSE_EXPLODE,   S_HIT_WEAPON,  Death_Explosion },
    { GUN_PULSE,   ACT_SECONDARY,  Projectile_Invalid,   80,  14,  0,   0, 0,    0,  0,  200,  1,  150,  0,    0, 1,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SECONDARY, S_PULSE2_A,      S_IMPACT_PULSE,    S_HIT_WEAPON,  Death_Shock     },
    // rocket
    { GUN_ROCKET,  ACT_PRIMARY,     Projectile_Rocket,  920, 110,  0,   0, 0,  300,  0, 2048,  1,  120, 33, 5000, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_ROCKET1,       S_ROCKET_EXPLODE,  S_HIT_WEAPON,  Death_Explosion },
    { GUN_ROCKET,  ACT_IDLE,       Projectile_Invalid,    0, 120,  0,   0, 0,    0,  0, 2048,  1,  140, 38,    0, 0,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_INVALID,       S_ROCKET_EXPLODE,  S_HIT_WEAPON,  Death_Explosion },
    // railgun
    { GUN_RAIL,    ACT_PRIMARY,     Projectile_Bullet, 1200,  70, 30,   0, 0, 2000, 30, 4096,  1,  100,  0,  500, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_RAIL_A,        S_IMPACT_RAILGUN,  S_HIT_RAILGUN, Death_Default   },
    { GUN_RAIL,    ACT_SECONDARY,   Projectile_Bullet, 1500, 100, 10,   0, 0, 2000, 50, 4096,  1,  100,  0,  500, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_RAIL_A,        S_IMPACT_RAILGUN,  S_HIT_RAILGUN, Death_Default   },
    // grenade launcher
    { GUN_INVALID, ACT_THROW,      Projectile_Grenade, 1000,  90,  0,   0, 0,  200,  0, 2048,  1,  140, 45, 1500, 0, 0.6f, 0.8f, false, ANIM_THROW, ANIM_VWEP_THROW, ANIM_GUN_THROW,     S_THROW,         S_GRENADE_EXPLODE, S_HIT_WEAPON,  Death_Explosion },
    { GUN_INVALID, ACT_IDLE,       Projectile_Invalid,    0, 100,  0,   0, 0,    0,  0, 2048,  1,  160, 50,    0, 0,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_INVALID,       S_GRENADE_EXPLODE, S_HIT_WEAPON,  Death_Explosion },
    // pistol
    { GUN_PISTOL,  ACT_PRIMARY,     Projectile_Bullet,  300,  18, 17,  60, 0, 1800, 12, 1024,  1,  180,  0,  500, 1,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_PISTOL1,       S_IMPACT_PULSE,    S_HIT_WEAPON,  Death_Default   },
    { GUN_PISTOL,  ACT_SECONDARY,   Projectile_Plasma,  600,  15,  0,   0, 5,  400,  0, 2048,  1,  500,  8, 2000, 2,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_PISTOL2,       S_IMPACT_PULSE,    S_HIT_WEAPON,  Death_Explosion },
    { GUN_PISTOL,  ACT_IDLE,       Projectile_Invalid,    0,  80,  0,   0, 0,    0,  0, 2048,  1, -200, 50,    0, 0,    0,    0, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_INVALID,       S_IMPACT_PISTOL,   S_HIT_RAILGUN, Death_Disrupt   },
    // instagib
    { GUN_INSTA,   ACT_PRIMARY,    Projectile_Invalid, 1200,  -1, 50,   0, 0,    0, 36, 4096,  1,   90,  0,    0, 0,    0,    0, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_PRIMARY,   S_INSTAGUN,      S_IMPACT_RAILGUN,  S_HIT_WEAPON,  Death_Default   },
    // zombie
    { GUN_ZOMBIE,  ACT_MELEE,      Projectile_Invalid,  600, 100,  0,   0, 4,    0,  0,   15,  1,   20,  0,    0, 0,    0,    0, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,     S_ZOMBIE,        S_IMPACT_MELEE,    S_HIT_MELEE,   Death_Fist      }
};

static const struct guninfo
{
    const char *name, *model, *worldmodel;
    int attacks[NUMACTS], switchsound, abilitySound, abilityFailSound, zoom, ejectprojectile;
} guns[NUMGUNS] =
{
    {
        "scattergun",
        "scattergun",
        "weapon/scattergun/world",
        { 
            ATK_MELEE, ATK_GRENADE1, ATK_SCATTER1, ATK_SCATTER2,
        },
        S_SG_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_None,
        Projectile_Casing3
    },
    {
        "smg",
        "smg",
        "weapon/smg/world",
        {
            ATK_MELEE, ATK_GRENADE1, ATK_SMG1, ATK_SMG2
        },
        S_SG_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_Shadow,
        Projectile_Casing
    },
    {
        "pulse",
        "pulserifle",
        "weapon/pulserifle/world",
        {
            ATK_MELEE, ATK_GRENADE1, ATK_PULSE1, ATK_PULSE2,
        },
        S_PULSE_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_None,
        Projectile_Invalid
    },
    {
        "rocket",
        "rocket",
        "weapon/rocket/world",
        {
            ATK_MELEE, ATK_GRENADE1, ATK_ROCKET1, ATK_INVALID,
        },
        S_ROCKET_SWITCH,
        S_ROCKET_ABILITY,
        S_ROCKET_ABILITY_FAIL,
        Zoom_None,
        Projectile_Invalid
    },
    {
        "railgun",
        "railgun",
        "weapon/railgun/world",
        {
            ATK_MELEE, ATK_GRENADE1, ATK_RAIL1, ATK_RAIL2
        },
        S_RAIL_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_Scope,
        Projectile_Casing2
    },
    {
        "grenade",
        "grenade",
        "weapon/grenade/world",
        {
            ATK_MELEE, ATK_GRENADE1, ATK_GRENADE1, ATK_GRENADE1
        },
        S_GRENADE_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_None,
        Projectile_Invalid
    },
    {
        "pistol",
        "pistol",
        "weapon/pistol/world",
        {
            ATK_MELEE, ATK_GRENADE1, ATK_PISTOL1, ATK_PISTOL2
        },
        S_PISTOL_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_None,
        Projectile_Invalid
    },
    {
        "instagun",
        "instagun",
        "weapon/railgun/world",
        {
            ATK_MELEE, ATK_MELEE, ATK_INSTA, ATK_INSTA
        },
        S_RAIL_SWITCH,
        S_INVALID,
        S_INVALID,
        Zoom_Scope,
        Projectile_Invalid
    },
    {
        "zombie",
        "zombie",
        nullptr,
        {
            ATK_ZOMBIE, ATK_ZOMBIE, ATK_ZOMBIE, ATK_ZOMBIE
        },
        S_INVALID,
        S_INVALID,
        S_INVALID,
        Zoom_Shadow,
        Projectile_Invalid
    },
    {
        "melee",
        nullptr,
        nullptr,
        {
            ATK_MELEE2, ATK_MELEE2, ATK_MELEE2, ATK_MELEE2
        },
        S_INVALID,
        S_INVALID,
        S_INVALID,
        Zoom_None,
        Projectile_Invalid
    }
};
