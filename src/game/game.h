#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"

// animations

enum
{
    ANIM_DEAD = ANIM_GAMESPECIFIC, ANIM_DYING,
    ANIM_IDLE, ANIM_RUN_N, ANIM_RUN_NE, ANIM_RUN_E, ANIM_RUN_SE, ANIM_RUN_S, ANIM_RUN_SW, ANIM_RUN_W, ANIM_RUN_NW,
    ANIM_JUMP, ANIM_JUMP_N, ANIM_JUMP_NE, ANIM_JUMP_E, ANIM_JUMP_SE, ANIM_JUMP_S, ANIM_JUMP_SW, ANIM_JUMP_W, ANIM_JUMP_NW,
    ANIM_SINK, ANIM_SWIM,
    ANIM_CROUCH, ANIM_CROUCH_N, ANIM_CROUCH_NE, ANIM_CROUCH_E, ANIM_CROUCH_SE, ANIM_CROUCH_S, ANIM_CROUCH_SW, ANIM_CROUCH_W, ANIM_CROUCH_NW,
    ANIM_CROUCH_JUMP, ANIM_CROUCH_JUMP_N, ANIM_CROUCH_JUMP_NE, ANIM_CROUCH_JUMP_E, ANIM_CROUCH_JUMP_SE, ANIM_CROUCH_JUMP_S, ANIM_CROUCH_JUMP_SW, ANIM_CROUCH_JUMP_W, ANIM_CROUCH_JUMP_NW,
    ANIM_CROUCH_SINK, ANIM_CROUCH_SWIM,
    ANIM_SHOOT, ANIM_MELEE,
    ANIM_PAIN,
    ANIM_EDIT, ANIM_LAG, ANIM_TAUNT, ANIM_WIN, ANIM_LOSE,
    ANIM_GUN_IDLE, ANIM_GUN_SHOOT, ANIM_GUN_MELEE,
    ANIM_VWEP_IDLE, ANIM_VWEP_SHOOT, ANIM_VWEP_MELEE,
    NUMANIMS
};

static const char * const animnames[] =
{
    "mapmodel",
    "dead", "dying",
    "idle", "run N", "run NE", "run E", "run SE", "run S", "run SW", "run W", "run NW",
    "jump", "jump N", "jump NE", "jump E", "jump SE", "jump S", "jump SW", "jump W", "jump NW",
    "sink", "swim",
    "crouch", "crouch N", "crouch NE", "crouch E", "crouch SE", "crouch S", "crouch SW", "crouch W", "crouch NW",
    "crouch jump", "crouch jump N", "crouch jump NE", "crouch jump E", "crouch jump SE", "crouch jump S", "crouch jump SW", "crouch jump W", "crouch jump NW",
    "crouch sink", "crouch swim",
    "shoot", "melee",
    "pain",
    "edit", "lag", "taunt", "win", "lose",
    "gun idle", "gun shoot", "gun melee",
    "vwep idle", "vwep shoot", "vwep melee"
};

// console message types

enum
{
    CON_CHAT       = 1<<8,
    CON_TEAMCHAT   = 1<<9,
    CON_GAMEINFO   = 1<<10,
    CON_FRAG_SELF  = 1<<11,
    CON_FRAG_OTHER = 1<<12,
    CON_TEAMKILL   = 1<<13
};

// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

enum                            // static entity types
{
    NOTUSED = ET_EMPTY,         // entity slot not in use in map
    LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
    MAPMODEL = ET_MAPMODEL,     // attr1 = idx, attr2 = yaw, attr3 = pitch, attr4 = roll, attr5 = scale
    PLAYERSTART,                // attr1 = angle, attr2 = team
    ENVMAP = ET_ENVMAP,         // attr1 = radius
    PARTICLES = ET_PARTICLES,
    MAPSOUND = ET_SOUND,
    SPOTLIGHT = ET_SPOTLIGHT,
    DECAL = ET_DECAL,
    TELEPORT,                   // attr1 = idx, attr2 = model, attr3 = tag
    TELEDEST,                   // attr1 = angle, attr2 = idx
    JUMPPAD,                    // attr1 = zpush, attr2 = ypush, attr3 = xpush
    FLAG,                       // attr1 = angle, attr2 = team

    I_AMMO_SG, I_AMMO_SMG, I_AMMO_PULSE, I_AMMO_RL, I_AMMO_RAIL,
    I_HEALTH, I_YELLOWSHIELD, I_REDSHIELD,
    I_SUPERHEALTH, I_MEGAHEALTH, I_DDAMAGE, I_HASTE, I_ARMOUR, I_UAMMO, I_ITEM1, I_ITEM2, I_INVULNERABILITY,
    MAXENTTYPES
};

struct gameentity : extentity
{
};

enum
{
    // main weapons
    GUN_SG = 0, GUN_SMG, GUN_PULSE, GUN_RL, GUN_RAIL, GUN_PISTOL,

    // special weapons
    GUN_INSTA, GUN_ZOMBIE,

    NUMGUNS
};
enum { ACT_IDLE = 0, ACT_MELEE, ACT_PRIMARY, ACT_SECONDARY, NUMACTS };
enum
{
    ATK_MELEE = 0,
    ATK_SG1, ATK_SG2, ATK_SMG1, ATK_SMG2, ATK_PULSE1, ATK_PULSE2, ATK_RL1, ATK_RL2, ATK_RAIL, ATK_PISTOL1, ATK_PISTOL2,
    ATK_INSTA, ATK_ZOMBIE,
    ATK_TELEPORT, ATK_STOMP,
    NUMATKS
};

#define validgun(n) ((n) >= 0 && (n) < NUMGUNS)
#define validact(n) ((n) >= 0 && (n) < NUMACTS)
#define validatk(n) ((n) >= 0 && (n) < ATK_TELEPORT)
#define validsatk(n) ((n) >= ATK_TELEPORT && (n) < NUMATKS)

enum { MM_AUTH = -1, MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD, MM_START = MM_AUTH, MM_INVALID = MM_START - 1 };

static const char * const mastermodenames[] =  { "Auth",   "Open",   "Veto",       "Locked",     "Private",    "Password" };
static const char * const mastermodecolors[] = { "",       "\f0",    "\f2",        "\f1",        "\f3",        "\f3" };
static const char * const mastermodeicons[] =  { "server", "server", "serverlock", "serverlock", "serverpriv", "serverpriv" };

enum { VAR_TIMELIMIT = 0, VAR_SCORELIMIT, VAR_MAXROUNDS, VAR_SELFDAMAGE, VAR_TEAMDAMAGE, VAR_WEAPON, VAR_TEAM1, VAR_TEAM2 };

// hardcoded sounds, defined in sound.cfg
enum
{
    // player
    S_JUMP1 = 0, S_JUMP2, S_LAND, S_LAND_WATER, S_FOOTSTEP, S_FOOTSTEP_WATER,
    S_SPLASHIN, S_SPLASHOUT, S_UNDERWATER, S_BURN,

    S_SPAWN, S_REGENERATION, S_PLAYER_DAMAGE,

    S_PAIN, S_DIE, S_TAUNT,
    S_PAIN_ZOMBIE, S_DIE_ZOMBIE, S_TAUNT_ZOMBIE,

    S_CORPSE, S_GIB,

    // item / entity
    S_TELEPORT, S_TELEDEST, S_JUMPPAD,

    S_ITEM_SPAWN, S_ITEM_BOUNCE,

    S_AMMO_SG, S_AMMO_SMG, S_AMMO_PULSE, S_AMMO_ROCKET, S_AMMO_RAIL,
    S_HEALTH, S_SUPERHEALTH, S_MEGAHEALTH,
    S_SHIELD_LIGHT, S_SHIELD_HEAVY, S_SHIELD_HIT,

    S_DAMAGE, S_HASTE, S_ARMOUR, S_UAMMO, S_INVULNERABILITY,
    S_ACTION_DAMAGE, S_ACTION_HASTE, S_ACTION_ARMOUR, S_ACTION_UAMMO, S_ACTION_INVULNERABILITY,
    S_LOOP_DAMAGE, S_LOOP_HASTE, S_LOOP_ARMOUR, S_LOOP_UAMMO, S_LOOP_INVULNERABILITY,
    S_TIMEOUT_DAMAGE, S_TIMEOUT_HASTE, S_TIMEOUT_ARMOUR, S_TIMEOUT_UAMMO, S_TIMEOUT_INVULNERABILITY,
    S_ACTIVATION_INVULNERABILITY,

    // weapon
    S_MELEE,
    S_SG1_A, S_SG1_B, S_SG2,
    S_SMG,
    S_PULSE1, S_PULSE2_A, S_PULSE2_B, S_PULSE_LOOP, S_PULSE_EXPLODE,
    S_ROCKET1, S_ROCKET2, S_ROCKET_LOOP, S_ROCKET_EXPLODE,
    S_RAIL_A, S_RAIL_B, S_RAIL_INSTAGIB,
    S_PISTOL1, S_PISTOL2,
    S_ZOMBIE, S_ZOMBIE_IDLE,

    S_IMPACT_SG, S_IMPACT_SMG, S_IMPACT_PULSE, S_IMPACT_RAILGUN,
    S_IMPACT_WATER, S_IMPACT_WATER_PROJ, S_IMPACT_GLASS,

    S_HIT_MELEE, S_HIT_RAILGUN,
    S_HIT_WEAPON, S_HIT_WEAPON_HEAD,

    S_BOUNCE_ROCKET,
    S_BOUNCE_CARTRIDGE_SMG, S_BOUNCE_CARTRIDGE_SG, S_BOUNCE_CARTRIDGE_RAILGUN,

    S_WEAPON_LOAD, S_WEAPON_NOAMMO,

    // announcer
    S_ANNOUNCER_FIGHT, S_ANNOUNCER_WIN,

    S_ANNOUNCER_FIRST_BLOOD, S_ANNOUNCER_HEADSHOT,
    S_ANNOUNCER_DOUBLE_KILL, S_ANNOUNCER_MULTI_KILL,
    S_ANNOUNCER_KILLING_SPREE, S_ANNOUNCER_UNSTOPPABLE,
    S_ANNOUNCER_10_KILLS, S_ANNOUNCER_5_KILLS, S_ANNOUNCER_1_KILL,

    S_ANNOUNCER_DDAMAGE, S_ANNOUNCER_HASTE, S_ANNOUNCER_ARMOUR, S_ANNOUNCER_UAMMO, S_ANNOUNCER_INVULNERABILITY,

    S_ANNOUNCER_FLAGSCORE_BLUE, S_ANNOUNCER_FLAGSCORE_RED,
    S_ANNOUNCER_JUGGERNAUT,
    S_ANNOUNCER_INFECTION, S_ANNOUNCER_ZOMBIE, S_ANNOUNCER_SURVIVOR,
    S_ANNOUNCER_WIN_ROUND,

    // miscellaneous
    S_FLAGPICKUP, S_FLAGDROP, S_FLAGRETURN, S_FLAGSCORE, S_FLAGRESET, S_FLAGFAIL, S_FLAGLOOP,
    S_JUGGERNAUT, S_JUGGERNAUT_LOOP, S_JUGGERNAUT_ACTION,
    S_INFECTION, S_INFECTED, S_WIN_ZOMBIES, S_WIN_SURVIVORS,
    S_LMS_ROUND, S_LMS_ROUND_WIN,
    S_VOOSH,

    S_HIT1, S_HIT2, S_HIT_ALLY, S_KILL1, S_KILL2, S_KILL_ALLY, S_SUICIDE,
    S_CHAT,
    S_INTERMISSION, S_INTERMISSION_WIN
};

// network messages codes, c2s, c2c, s2c

enum { PRIV_NONE = 0, PRIV_MASTER, PRIV_ADMIN, PRIV_AUTH};

enum
{
    N_CONNECT = 0, N_SERVINFO, N_WELCOME, N_INITCLIENT, N_POS, N_TEXT, N_SOUND, N_CDIS,
    N_SHOOT, N_SPECIALATK, N_EXPLODE, N_HURTPLAYER, N_SUICIDE,
    N_DIED, N_DAMAGE, N_HITPUSH, N_SHOTEVENT, N_SHOTFX, N_EXPLODEFX, N_REGENERATE, N_REPAMMO, N_USEITEM,
    N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_FORCEDEATH,
    N_GUNSELECT, N_SETWEAPONS, N_ANIMATION,
    N_ANNOUNCE,
    N_MAPCHANGE, N_SERVERVARIABLES, N_MAPVOTE, N_SENDVARIABLES, N_TEAMINFO, N_ITEMSPAWN, N_ITEMPICKUP, N_ITEMACC, N_TELEPORT, N_JUMPPAD,
    N_PING, N_PONG, N_CLIENTPING,
    N_TIMEUP, N_FORCEINTERMISSION,
    N_SERVMSG, N_ITEMLIST, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_CALCLIGHT, N_REMIP, N_EDITVSLOT, N_UNDO, N_REDO, N_NEWMAP, N_GETMAP, N_SENDMAP, N_CLIPBOARD, N_EDITVAR,
    N_MASTERMODE, N_MUTE, N_KICK, N_CLEARBANS, N_CURRENTMASTER, N_SPECTATOR, N_SETMASTER, N_SETTEAM,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO,
    N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_TAKEFLAG, N_RETURNFLAG, N_RESETFLAG, N_TRYDROPFLAG, N_DROPFLAG, N_SCOREFLAG, N_INITFLAGS,
    N_ROUNDSCORE, N_JUGGERNAUT, N_INFECT, N_SCORE, N_TRAITOR, N_FORCEWEAPON,
    N_SAYTEAM, N_WHISPER,
    N_CLIENT,
    N_AUTHTRY, N_AUTHKICK, N_AUTHCHAL, N_AUTHANS, N_REQAUTH,
    N_PAUSEGAME, N_GAMESPEED,
    N_ADDBOT, N_DELBOT, N_INITAI, N_FROMAI, N_BOTLIMIT, N_BOTBALANCE,
    N_MAPCRC, N_CHECKMAPS,
    N_SWITCHNAME, N_SWITCHMODEL, N_SWITCHCOLOR, N_SWITCHPTYPE, N_SWITCHTEAM,
    N_SERVCMD,
    N_DEMOPACKET,
    NUMMSG
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    N_CONNECT, 0, N_SERVINFO, 0, N_WELCOME, 1, N_INITCLIENT, 0, N_POS, 0, N_TEXT, 0, N_SOUND, 2, N_CDIS, 2,
    N_SHOOT, 0, N_SPECIALATK, 0, N_EXPLODE, 0, N_HURTPLAYER, 0, N_SUICIDE, 1,
    N_DIED, 6, N_DAMAGE, 8, N_HITPUSH, 7, N_SHOTEVENT, 3, N_SHOTFX, 12, N_EXPLODEFX, 6, N_REGENERATE, 2, N_REPAMMO, 3, N_USEITEM, 1,
    N_TRYSPAWN, 1, N_SPAWNSTATE, 12, N_SPAWN, 3, N_FORCEDEATH, 2,
    N_GUNSELECT, 2, N_SETWEAPONS, 4, N_ANIMATION, 2,
    N_ANNOUNCE, 4,
    N_MAPCHANGE, 0, N_SERVERVARIABLES, 8, N_MAPVOTE, 0, N_SENDVARIABLES, 0, N_TEAMINFO, 0, N_ITEMSPAWN, 2, N_ITEMPICKUP, 2, N_ITEMACC, 3,
    N_PING, 2, N_PONG, 2, N_CLIENTPING, 2,
    N_TIMEUP, 2, N_FORCEINTERMISSION, 1,
    N_SERVMSG, 0, N_ITEMLIST, 0, N_RESUME, 0,
    N_EDITMODE, 2, N_EDITENT, 11, N_EDITF, 16, N_EDITT, 16, N_EDITM, 16, N_FLIP, 14, N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14, N_CALCLIGHT, 1, N_REMIP, 1, N_EDITVSLOT, 16, N_UNDO, 0, N_REDO, 0, N_NEWMAP, 2, N_GETMAP, 1, N_SENDMAP, 0, N_EDITVAR, 0,
    N_MASTERMODE, 2, N_MUTE, 0, N_KICK, 0, N_CLEARBANS, 1, N_CURRENTMASTER, 0, N_SPECTATOR, 4, N_SETMASTER, 0, N_SETTEAM, 0,
    N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 2, N_SENDDEMO, 0,
    N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
    N_TAKEFLAG, 3, N_RETURNFLAG, 4, N_RESETFLAG, 3, N_TRYDROPFLAG, 1, N_DROPFLAG, 7, N_SCOREFLAG, 9, N_INITFLAGS, 0,
    N_ROUNDSCORE, 0, N_JUGGERNAUT, 3, N_INFECT, 4, N_SCORE, 3, N_TRAITOR, 2,  N_FORCEWEAPON, 3,
    N_SAYTEAM, 0, N_WHISPER, 0,
    N_CLIENT, 0,
    N_AUTHTRY, 0, N_AUTHKICK, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_REQAUTH, 0,
    N_PAUSEGAME, 0, N_GAMESPEED, 0,
    N_ADDBOT, 2, N_DELBOT, 1, N_INITAI, 0, N_FROMAI, 2, N_BOTLIMIT, 2, N_BOTBALANCE, 2,
    N_MAPCRC, 0, N_CHECKMAPS, 1,
    N_SWITCHNAME, 0, N_SWITCHMODEL, 2, N_SWITCHCOLOR, 2, N_SWITCHPTYPE, 2, N_SWITCHTEAM, 2,
    N_SERVCMD, 0,
    N_DEMOPACKET, 0,
    -1
};

#define TESSERACT_SERVER_PORT 21217
#define TESSERACT_LANINFO_PORT 21216
#define TESSERACT_MASTER_PORT 21215
#define PROTOCOL_VERSION 2              // bump when protocol changes
#define DEMO_VERSION 1                  // bump when demo format changes
#define DEMO_MAGIC "TESSERACT_DEMO\0\0"

struct demoheader
{
    char magic[16];
    int version, protocol;
};

#define MAXNAMELEN 15

enum
{
    HICON_RED_FLAG = 0,
    HICON_BLUE_FLAG,

    HICON_SHIELD, HICON_HEALTH,
    HICON_SG, HICON_SMG, HICON_PULSE, HICON_RL, HICON_RAIL, HICON_GL, HICON_PUNCH, HICON_INSTAGIB, HICON_PISTOL, HICON_ZOMBIE,
    HICON_ALLY,
    HICON_HASTE, HICON_DDAMAGE, HICON_ARMOUR, HICON_UAMMO,
    HICON_MEDKIT, HICON_INVULNERABILITY,


    HICON_X       = 20,
    HICON_Y       = 1650,
    HICON_TEXTY   = 1644,
    HICON_STEP    = 490,
    HICON_SIZE    = 120,
    HICON_SPACE   = 40
};

static const char * const iconnames[] =
{
    "data/interface/icon/flag_red.png",
    "data/interface/icon/flag_blue.png",
    "data/interface/icon/shield.png",
    "data/interface/icon/health.png",
    "data/interface/icon/shotgun.png",
    "data/interface/icon/smg.png",
    "data/interface/icon/pulse.png",
    "data/interface/icon/rocket.png",
    "data/interface/icon/railgun.png",
    "data/interface/icon/pistol.png",
    "data/interface/icon/unknown.png",
    "data/interface/icon/punch.png",
    "data/interface/icon/skull01.png",
    "data/interface/icon/zombie.png",
    "data/interface/icon/ally.png",
    "data/interface/icon/haste.png",
    "data/interface/icon/skull02.png",
    "data/interface/icon/armour.png",
    "data/interface/icon/infinity.png",
    "data/interface/icon/asclepius.png",
    "data/interface/icon/ankh.png"
};

static struct itemstat { int add, max, sound, info; } itemstats[] =
{
    { 12,    60,    S_AMMO_SG,          GUN_SG,       }, // shotgun ammo
    { 40,    200,   S_AMMO_SMG,         GUN_SMG,      }, // smg ammo
    { 80,    400,   S_AMMO_PULSE,       GUN_PULSE,    }, // pulse battery
    { 6,     30,    S_AMMO_ROCKET,      GUN_RL,       }, // rockets
    { 8,     40,    S_AMMO_RAIL,        GUN_RAIL,     }, // railgun ammo
    { 25,    100,   S_HEALTH,           NULL,         }, // regular health
    { 50,    200,   S_SHIELD_LIGHT,     NULL,         }, // light shield
    { 100,   200,   S_SHIELD_HEAVY,     NULL,         }, // heavy shield
    { 50,    200,   S_SUPERHEALTH,      NULL,         }, // super health
    { 100,   250,   S_MEGAHEALTH,       NULL,         }, // megahealth
    { 30000, 60000, S_DAMAGE,           NULL,         }, // double damage
    { 30000, 60000, S_HASTE,            NULL,         }, // haste
    { 30000, 60000, S_ARMOUR,           NULL,         }, // armour
    { 30000, 60000, S_UAMMO,            NULL,         }, // unlimited ammo
    {     0,     0,    NULL,            NULL,         }, // ?
    {     0,     0,    NULL,            NULL,         }, // ?
    {     1,     1, S_INVULNERABILITY,     1,         }  // invulnerability
};

#define validitem(n) (((n) >= I_AMMO_SG && (n) <= I_INVULNERABILITY))

#define MAXRAYS 20
#define EXP_SELFDAMDIV 2
#define EXP_SELFPUSH 5.0f
#define EXP_DISTSCALE 1.5f
#define ENV_DAM 5

static const struct attackinfo { int gun, action, anim, vwepanim, hudanim, sound, impactsound, hitsound,
                                     attackdelay, damage, headshotdam, spread, margin, projspeed, kickamount, range, rays, hitpush, exprad, lifetime, use; } attacks[NUMATKS] =
{
    //melee: default melee for all weapons
    { NULL, ACT_MELEE, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE, S_MELEE, S_HIT_MELEE, S_HIT_MELEE,
      650,    54,    0,    0,   1,    0,    0,   15,    1,   30,    0,    0,    0
    },
    //shotgun
    { GUN_SG, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SG1_A, S_IMPACT_SG, S_HIT_WEAPON,
      1080,    6,    0,  320,   0,    0,    0, 1000,   20,   50,    0,    0,    1
    },
    { GUN_SG, ACT_SECONDARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SG2, S_ROCKET_EXPLODE, S_HIT_WEAPON,
      600,    65,    0,    0,   2,  180,    0, 2048,    1,   60,   30, 2000,    2
    },
    //smg
    { GUN_SMG, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SMG, S_IMPACT_SMG, S_HIT_WEAPON,
      110,    15,   12,   84,   0,    0,    0, 1000,    1,   20,    0,    0,    1
    },
    { GUN_SMG, ACT_SECONDARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_SMG, S_IMPACT_SMG, S_HIT_WEAPON,
      240,    18,   10,   30,   0,    0,    0, 1000,    1,   40,    0,    0,    1
    },
    //pulse
    { GUN_PULSE, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_PULSE1, S_PULSE_EXPLODE, S_HIT_WEAPON,
      180,    22,    0,    0,   1, 1000,    0, 2048,    1,   75,   18, 3000,    2
    },
    { GUN_PULSE, ACT_SECONDARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_IDLE, S_PULSE2_A, S_IMPACT_PULSE, S_HIT_WEAPON,
      80,     12,    4,    0,   0,    0,    0,  200,    1,  100,    0,    0,    1
    },
    //rocket
    { GUN_RL, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_ROCKET1, S_ROCKET_EXPLODE, S_HIT_WEAPON,
      920,   110,    0,    0,   0,  300,    0, 2048,    1,  120,   33, 5000,    1
    },
    { GUN_RL, ACT_SECONDARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_ROCKET2, S_ROCKET_EXPLODE, S_HIT_WEAPON,
      920,   110,    0,    0,   0,  200,    0, 2048,    1,  120,   33, 1500,    1
    },
    //railgun
    { GUN_RAIL, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_RAIL_A, S_IMPACT_RAILGUN, S_HIT_RAILGUN,
      1200,   70,   70,    0,   0,    0,   40, 4000,    1,  110,    0,    0,    1
    },
    //pistol
    { GUN_PISTOL, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_PISTOL1, S_IMPACT_PULSE, S_HIT_WEAPON,
      300,     8,    5,    0,   0,    0,    0, 1000,    1,  200,    0,    0,    1
    },
    { GUN_PISTOL, ACT_SECONDARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_PISTOL2, S_IMPACT_PULSE, S_HIT_WEAPON,
      600,    10,    0,    0,   5, 1000,    0, 2048,    1,  400,    8,  800,    2
    },

    //instagib
    { GUN_INSTA, ACT_PRIMARY, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT, S_RAIL_INSTAGIB, S_IMPACT_RAILGUN, S_HIT_WEAPON,
      1200,  150,    0,    0,   0,    0,   60, 4000,    1,   30,    0,    0,    0
    },
    //zombie
    { GUN_ZOMBIE, ACT_MELEE, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE, S_ZOMBIE, S_HIT_MELEE, S_HIT_MELEE,
      600,   100,    0,    0,   4,    0,    0,   15,    1,   20,    0,    0,    0
    },

    //telefrag
    { -1,             ACT_MELEE,  ANIM_IDLE,  ANIM_VWEP_IDLE,  ANIM_GUN_IDLE,             -1, NULL,  NULL,    0, 1050,   0,   0,   8,    0,   0,    6,  1,   0,  0,    0,    0 },
    //stomp
    { -1,             ACT_MELEE,  ANIM_IDLE,  ANIM_VWEP_IDLE,  ANIM_GUN_IDLE,             -1, NULL, NULL,    0,  100,   0,   0,   3,    0,   0,    2,  1,   0,  0,    0,    0 }

};

static const struct guninfo { const char *name, *file, *vwep; int attacks[NUMACTS]; } guns[NUMGUNS] =
{
    { "shotgun", "sg", "worldgun/sg", { -1, ATK_MELEE, ATK_SG1, ATK_SG2 } },
    { "smg", "smg", "worldgun/smg", { -1, ATK_MELEE, ATK_SMG1, ATK_SMG2 }, },
    { "pulse rifle (upgraded)", "pulserifle", "worldgun/pulserifle", { -1, ATK_MELEE, ATK_PULSE1, ATK_PULSE2 }, },
    { "rocket launcher", "rl", "worldgun/rl", { -1, ATK_MELEE, ATK_RL1, ATK_RL2 } },
    { "railgun", "railgun", "worldgun/railgun", { -1, ATK_MELEE, ATK_RAIL, ATK_RAIL }, },
    { "pulse rifle", "pistol", "worldgun/pulserifle", { -1, ATK_MELEE, ATK_PISTOL1, ATK_PISTOL2 }, },
    { "railgun", "railgun", "worldgun/railgun", { -1, ATK_MELEE, ATK_INSTA, ATK_INSTA }, },
    { "zombie", "zombie", "", { -1, ATK_ZOMBIE, ATK_ZOMBIE, ATK_ZOMBIE }, }
};

enum { HIT_TORSO = 1<<0, HIT_LEGS = 1<<1, HIT_HEAD = 1<<2, HIT_MATERIAL = 1<<3 };

enum
{
    K_NONE = 1<<0, K_FIRST = 1<<1, K_DOUBLE = 1<<2, K_MULTI = 1<<3, K_SPREE = 1<<4, K_UNSTOPPABLE = 1<<5,  K_HEADSHOT = 1<<6,
    K_TELEFRAG = 1<<7, K_STOMP = 1<<8,
    K_JUGGERNAUT = 1<<9
};

#include "ai.h"
#include "gamemode.h"

// inherited by gameent and server clients
struct gamestate
{
    int health, maxhealth, shield, item;
    int gunselect, gunwait, primary, secondary;
    int ammo[NUMGUNS];
    int aitype, skill;
    int damagemillis, hastemillis, armourmillis, ammomillis, invulnmillis;
    int lastdamage;
    int juggernaut, zombie;

    gamestate() : maxhealth(100), primary(-1), secondary(-1), aitype(AI_NONE), skill(0), lastdamage(0) { }

    bool canpickup(int type)
    {
        if(!validitem(type) || juggernaut || zombie) return false;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:  case I_SUPERHEALTH: case I_MEGAHEALTH: return health<is.max;
            case I_YELLOWSHIELD: case I_REDSHIELD: return shield<is.max;
            case I_DDAMAGE: return damagemillis<is.max;
            case I_HASTE:   return armourmillis<is.max;
            case I_ARMOUR:  return hastemillis<is.max;
            case I_UAMMO:   return ammomillis<is.max;
            case I_INVULNERABILITY: return item<is.max;
            default: return ammo[is.info]<is.max;
        }
    }

    void pickup(int type)
    {
        if(!validitem(type) || juggernaut || zombie) return;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_SUPERHEALTH:
            case I_MEGAHEALTH:
                health = min(health+is.add, is.max);
                break;
            case I_YELLOWSHIELD:
            case I_REDSHIELD:
                shield = min(shield+is.add, is.max);
                break;
            case I_DDAMAGE:
                damagemillis = min(damagemillis+is.add, is.max);
                break;
            case I_ARMOUR:
                armourmillis = min(armourmillis+is.add, is.max);
                break;
            case I_HASTE:
                hastemillis = min(hastemillis+is.add, is.max);
                break;
            case I_UAMMO:
                ammomillis = min(ammomillis+is.add, is.max);
                break;
            case I_INVULNERABILITY:
                item = is.info;
                break;
            default:
                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
                break;
        }
    }

    void useitem(int type)
    {
        if(item > 2 || !item || juggernaut || zombie) return;
        switch(type)
        {
            case 1:
                invulnmillis = min(15000, 15000);
                break;
            case 2:
                health = maxhealth+20;
                break;
            default: break;
        }
        item = type = 0;
    }

    void baseammo(int gun, int k = 3, int scale = 1)
    {
        ammo[gun] = (itemstats[gun-GUN_SG].add*k)/scale;
    }

    void addammo(int gun, int k = 1, int scale = 1)
    {
        itemstat &is = itemstats[gun-GUN_SG];
        ammo[gun] = min(ammo[gun] + (is.add*k)/scale, is.max);
    }

    bool hasmaxammo(int type)
    {
       const itemstat &is = itemstats[type-I_AMMO_SG];
       return ammo[type-I_AMMO_SG+GUN_SG]>=is.max;
    }

    void resetitems()
    {
        shield = item = 0;
        damagemillis = hastemillis = armourmillis = ammomillis = invulnmillis = 0;
        gunwait = 0;
        loopi(NUMGUNS) ammo[i] = 0;
    }

    void respawn()
    {
        maxhealth = 100;
        health = maxhealth;
        gunselect = GUN_PISTOL;
        gunwait = 0;
        resetitems();
        juggernaut = zombie = 0;
    }

    void setweapons(int weap1, int weap2)
    {
        primary = weap1;
        secondary = weap2;
    }

    void spawnstate(int gamemode, int mutators, int forceweapon)
    {
        if(m_classic(mutators) && !(m_randomweapon(mutators) || m_oneweapon(mutators)))
        {
            gunselect = GUN_PISTOL;
            ammo[GUN_PISTOL] = 100;
            shield = 0;
        }
        else if(m_insta(mutators))
        {
            shield = 0;
            gunselect = GUN_INSTA;
            ammo[GUN_INSTA] = 1;
        }
        else if(m_effic(mutators))
        {
            shield = 100;
            loopi(NUMGUNS-5) baseammo(i);
            gunselect = GUN_SMG;
        }
        else if((m_randomweapon(mutators) || m_oneweapon(mutators)) && forceweapon >= 0)
        {
            loopi(NUMGUNS) ammo[i] = 0;
            ammo[forceweapon] = 100;
            gunselect = forceweapon;
        }
        else
        {
            if(primary <= -1)
            {
                do primary = rnd(5); while(primary==secondary);
            }
            if(secondary <= -1)
            {
                do secondary = rnd(5); while(secondary==primary);
            }
            ammo[primary] = itemstats[primary-GUN_SG].add*4;
            ammo[secondary] = itemstats[secondary-GUN_SG].add*4;
            ammo[GUN_PISTOL] = 100;
            gunselect = primary;
            shield = 0;
        }
    }

    // just subtract damage here, can set death, etc. later in code calling this
    int dodamage(int damage, bool environment = false)
    {
        if(!environment)
        {
            int ad = damage/3*2; // suggestions are welcome
            if(ad>shield) ad = shield;
            shield -= ad;
            damage -= ad;
        }
        health -= damage;
        return damage;
    }

    int hasammo(int gun, int exclude = -1)
    {
        return validgun(gun) && gun != exclude && ammo[gun] > 0;
    }

    bool haspowerups()
    {
        return damagemillis || hastemillis || armourmillis || ammomillis || invulnmillis;
    }
};

#define MAXTEAMS 2
static const char * const teamnames[1+MAXTEAMS] = { "", "Blue", "Red" };
static const char * const teamtextcode[1+MAXTEAMS] = { "\ff", "\f1", "\f3" };
static const int teamtextcolor[1+MAXTEAMS] = { 0x1EC850, 0x6496FF, 0xFF4B19 };
static const int teamscoreboardcolor[1+MAXTEAMS] = { 0, 0x3030C0, 0xC03030 };
static const char * const teamblipcolor[1+MAXTEAMS] = { "_neutral", "_blue", "_red" };
static inline int teamnumber(const char *name) { loopi(MAXTEAMS) if(!strcmp(teamnames[1+i], name)) return 1+i; return 0; }
#define validteam(n) ((n) >= 1 && (n) <= MAXTEAMS)
#define teamname(n) (teamnames[validteam(n) ? (n) : 0])

struct gameent : dynent, gamestate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain;
    int lastaction, lastattack, lasthit;
    int attacking, lastact;
    int lasttaunt, lastfootstep, lastyelp;
    int lastpickup, lastpickupmillis, flagpickup;
    int frags, flags, deaths, points, totaldamage, totalshots;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll;
    int smoothmillis, landmillis;
    float trans;

    int attackchan, attacksound, idlechan, idlesound;
    int ddamagechan, hastechan, armourchan, ammochan, invulnchan, juggernautchan;

    string name, info;
    int team, playermodel, playercolor, playertype;
    ai::aiinfo *ai;
    int ownernum, lastnode;
    bool headless, queue;

    vec muzzle;

    gameent() : weight(100), clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0), lifesequence(0), respawned(-1), suicided(-1), lastpain(0), lastfootstep(0), lastyelp(0), frags(0), flags(0), deaths(0), points(0), totaldamage(0), totalshots(0), edit(NULL), smoothmillis(-1), landmillis(0), trans(1), attackchan(-1), attacksound(-1), ddamagechan(-1), hastechan(-1), armourchan(-1), ammochan(-1), invulnchan(-1), juggernautchan(-1), team(0), playermodel(-1), playercolor(0), ai(NULL), ownernum(-1), muzzle(-1, -1, -1)
    {
        name[0] = info[0] = 0;
        headless = queue = false;
        respawn();
    }
    ~gameent()
    {
        freeeditinfo(edit);
        if(ai) delete ai;
        if(attackchan >= 0) stopsound(attacksound, attackchan);
        if(idlechan >= 0) stopsound(idlesound, idlechan);
        if(ddamagechan >= 0) stopsound(S_LOOP_DAMAGE, ddamagechan);
        if(hastechan >= 0) stopsound(S_LOOP_HASTE, hastechan);
        if(armourchan >= 0) stopsound(S_LOOP_ARMOUR, armourchan);
        if(ammochan >= 0) stopsound(S_LOOP_UAMMO, ammochan);
        if(invulnchan >= 0) stopsound(S_LOOP_INVULNERABILITY, invulnchan);
        if(juggernautchan >= 0) stopsound(S_JUGGERNAUT_LOOP, juggernautchan);
    }

    void hitpush(int damage, const vec &dir, gameent *actor, int atk)
    {
        vec push(dir);
        if(timeinair && (attacks[atk].gun == GUN_RL || atk == ATK_PISTOL2))
            falling.z = 1; // rocket launcher and pistol reduce gravity while falling so trick jumps are more rewarding and players are pushed further
        if(zombie) damage *= 3; // zombies are pushed "a bit" more
        push.mul((actor==this && attacks[atk].exprad ? EXP_SELFPUSH : 1.0f)*attacks[atk].hitpush*damage/weight);
        vel.add(push);
    }

    void startgame()
    {
        frags = flags = deaths = points = 0;
        totaldamage = totalshots = 0;
        maxhealth = 100;
        shield = 0;
        lifesequence = -1;
        respawned = suicided = -2;
        lasthit = 0;
    }

    void stopweaponsound()
    {
        if(attackchan >= 0) stopsound(attacksound, attackchan, 300);
        attacksound = attackchan = -1;
    }

    void stopidlesound()
    {
        if(idlechan >= 0) stopsound(idlesound, idlechan, 400);
        idlesound = idlechan = -1;
    }

    void respawn()
    {
        dynent::reset();
        gamestate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattack = -1;
        attacking = lastact = ACT_IDLE;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        flagpickup = 0;
        lastnode = -1;
        lasthit = 0;
        stopweaponsound();
        stoppowerupsound();
    }

    void stoppowerupsound()
    {
        if(ddamagechan >= 0)
        {
            stopsound(S_LOOP_DAMAGE, ddamagechan, 500);
            ddamagechan = -1;
        }
        if(hastechan >= 0)
        {
            stopsound(S_LOOP_HASTE, hastechan, 500);
            hastechan = -1;
        }
        if(armourchan >= 0)
        {
            stopsound(S_LOOP_ARMOUR, armourchan, 500);
            armourchan = -1;
        }
        if(ammochan >= 0)
        {
            stopsound(S_LOOP_UAMMO, ammochan, 500);
            ammochan = -1;
        }
        if(invulnchan >= 0)
        {
            stopsound(S_LOOP_INVULNERABILITY, invulnchan, 500);
            invulnchan = -1;
        }
    }

    bool gibbed() { return state == CS_DEAD && health<=-50; }

    int painsound(){ return !zombie? S_PAIN: S_PAIN_ZOMBIE; }

    int diesound() { return !zombie? S_DIE: S_DIE_ZOMBIE; }

    int tauntsound() { return !zombie? S_TAUNT: S_TAUNT_ZOMBIE; }

    int bloodcolour() { return !zombie? 0x60FFFFF: 0xFF90FF; }
};

struct teamscore
{
    int team, score;
    teamscore() {}
    teamscore(int team, int n) : team(team), score(n) {}

    static bool compare(const teamscore &x, const teamscore &y)
    {
        if(x.score > y.score) return true;
        if(x.score < y.score) return false;
        return x.team < y.team;
    }
};

static inline uint hthash(const teamscore &t) { return hthash(t.team); }
static inline bool htcmp(int team, const teamscore &t) { return team == t.team; }

struct teaminfo
{
    int frags;

    teaminfo() { reset(); }

    void reset() { frags = 0; }
};

namespace entities
{
    extern vector<extentity *> ents;

    extern const char *entmdlname(int type);
    extern const char *itemname(int i);
    extern int itemicon(int i);

    extern void preloadentities();
    extern void renderentities();
    extern void checkitems(gameent *d);
    extern void updatepowerups(int time, gameent *d);
    extern void resetspawns();
    extern void spawnitems(bool force = false);
    extern void putitems(packetbuf &p);
    extern void setspawn(int i, bool on);
    extern void teleport(int n, gameent *d);
    extern void pickupeffects(int n, gameent *d);
    extern void teleporteffects(gameent *d, int tp, int td, bool local = true);
    extern void jumppadeffects(gameent *d, int jp, bool local = true);
}

namespace game
{
    extern int gamemode, mutators;

    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual float clipconsole(float w, float h) { return 0; }
        virtual void drawhud(gameent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned(gameent *d) {}
        virtual void setup() {}
        virtual void checkitems(gameent *d) {}
        virtual int respawnwait(gameent *d) { return 0; }
        virtual void pickspawn(gameent *d) { findplayerspawn(d, -1, m_teammode ? d->team : 0); }
        virtual void senditems(packetbuf &p) {}
        virtual void removeplayer(gameent *d) {}
        virtual void gameover() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual bool canfollow(gameent *s, gameent *f) { return true; }
        virtual void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        virtual bool aicheck(gameent *d, ai::aistate &b) { return false; }
        virtual bool aidefend(gameent *d, ai::aistate &b) { return false; }
        virtual bool aipursue(gameent *d, ai::aistate &b) { return false; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // game
    extern int nextmode, Timelimit, nexttimelimit, Scorelimit, nextscorelimit;
    extern int selfdam, teamdam, forceweapon;
    extern string clientmap;
    extern bool intermission;
    extern int maptime, maprealtime, maplimit;
    extern gameent *player1;
    extern vector<gameent *> players, clients;
    extern int lastspawnattempt;
    extern int following;
    extern int smoothmove, smoothdist;
    extern int gore;

    extern bool clientoption(const char *arg);
    extern gameent *getclient(int cn);
    extern gameent *newclient(int cn);
    extern const char *colorname(gameent *d, const char *name = NULL, const char *alt = NULL, const char *color = "");
    extern const char *teamcolorname(gameent *d, const char *alt = "you");
    extern const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt);
    extern gameent *pointatplayer();
    extern gameent *hudplayer();
    extern gameent *followingplayer();
    extern void taunt(gameent *d);
    extern void stopfollowing();
    extern void checkfollow();
    extern void nextfollow(int dir = 1);
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void startgame();
    extern void spawnplayer(gameent *);
    extern bool isally(gameent *a, gameent *b);
    extern void deathstate(gameent *d, bool restore = false);
    extern void damaged(int damage, vec &p, gameent *d, gameent *actor, int atk, int flags = 0, bool local = true);
    extern void killed(gameent *d, gameent *actor, int flags = K_NONE);
    extern void timeupdate(int timeremain);
    extern void msgsound(int n, physent *d = NULL);
    extern void drawicon(int icon, float x, float y, float sz = 120);
    extern void doaction(int act);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);

    // client
    extern bool connected, remote, demoplayback;
    extern string servdesc;
    extern vector<uchar> messages;
    extern const vec teamlightcolor[1+MAXTEAMS];
    extern int parseplayer(const char *arg);
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern bool isignored(int cn);
    extern bool addmsg(int type, const char *fmt = NULL, ...);
    extern void switchname(const char *name);
    extern void switchteam(const char *name);
    extern void switchplayermodel(int playermodel);
    extern void switchplayercolor(int playercolor);
    extern void sendmapinfo();
    extern void stopdemo();
    extern void changemap(const char *name, int mode, int muts, int tl, int sl);
    extern void c2sinfo(bool force = false);
    extern void sendposition(gameent *d, bool reliable = false);

    // weapon
    extern int getweapon(const char *name);
    extern void shoot(gameent *d, const vec &targ);
    extern void specialattack(gameent *d, int atk, vec from, const vec &targ);
    extern void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit = false);
    extern void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int dam, int atk);
    extern void explodeeffects(int atk, gameent *d, bool local, int id = 0);
    extern void damageeffect(int damage, dynent *d, vec &p, int atk, bool thirdperson = true);
    extern void gibeffect(int damage, const vec &vel, gameent *d);
    extern float intersectdist;
    extern bool intersect(dynent *d, const vec &from, const vec &to, float margin = 0, float &dist = intersectdist);
    extern dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin = 0, float &dist = intersectdist);
    extern bool intersecthead(dynent *d, const vec &from, const vec &to, float &dist = intersectdist);
    extern void clearbouncers();
    extern void updatebouncers(int curtime);
    extern void removebouncers(gameent *owner);
    extern void renderbouncers();
    extern void clearprojectiles();
    extern void updateprojectiles(int curtime);
    extern void removeprojectiles(gameent *owner);
    extern void renderprojectiles();
    extern void preloadbouncers();
    extern void removeweapons(gameent *owner);
    extern void updateweapons(int curtime);
    extern void gunselect(int gun, gameent *d);
    extern void weaponswitch(gameent *d);
    extern void avoidweapons(ai::avoidset &obstacles, float radius);

    // scoreboard
    extern void showscores(bool on);
    extern void getbestplayers(vector<gameent *> &best);
    extern void getbestteams(vector<int> &best);
    extern void clearteaminfo();
    extern void setteaminfo(int team, int frags);
    extern void removegroupedplayer(gameent *d);

    // render
    struct playermodelinfo
    {
        const char *model[1+MAXTEAMS], *hudguns[1+MAXTEAMS],
                   *zombiemodel, *icon[1+MAXTEAMS];
        bool ragdoll;
    };

    extern void saveragdoll(gameent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern const playermodelinfo &getplayermodelinfo(gameent *d);
    extern int getplayercolor(gameent *d, int team);
    extern int chooserandomplayermodel(int seed);
    extern void syncplayer();
    extern void swayhudgun(int curtime);
    extern vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d);
}

namespace server
{
    extern const char *modename(int n, const char *unknown = "unknown");
    extern const char *modeprettyname(int n, const char *unknown = "unknown");
    extern const char *mastermodename(int n, const char *unknown = "unknown");
    extern void startintermission();
    extern void stopdemo();
    extern void forcemap(const char *map, int mode, int muts, int tl, int sl);
    extern void forcevariables(int roundlimit, int selfdamage, int teamdamage, int forceweapon);
    extern void forcepaused(bool paused);
    extern void forcegamespeed(int speed);
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern void resetgamelimit();
    extern void gameover();
    extern int msgsizelookup(int msg);
    extern bool serveroption(const char *arg);
    extern bool delayspawn(int type);
    extern bool canspawnitem(int type);
    extern bool betweenrounds;
    extern int teamdamage, selfdamage;
}

#endif

