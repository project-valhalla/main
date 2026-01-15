#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"

// animations

enum
{
    ANIM_DEAD = ANIM_GAMESPECIFIC, ANIM_DYING,
    ANIM_IDLE, ANIM_RUN_N, ANIM_RUN_NE, ANIM_RUN_E, ANIM_RUN_SE, ANIM_RUN_S, ANIM_RUN_SW, ANIM_RUN_W, ANIM_RUN_NW,
    ANIM_JUMP, ANIM_JUMP_N, ANIM_JUMP_NE, ANIM_JUMP_E, ANIM_JUMP_SE, ANIM_JUMP_S, ANIM_JUMP_SW, ANIM_JUMP_W, ANIM_JUMP_NW,
    ANIM_SWIM,
    ANIM_CROUCH, ANIM_CROUCH_N, ANIM_CROUCH_NE, ANIM_CROUCH_E, ANIM_CROUCH_SE, ANIM_CROUCH_S, ANIM_CROUCH_SW, ANIM_CROUCH_W, ANIM_CROUCH_NW,
    ANIM_CROUCH_JUMP, ANIM_CROUCH_JUMP_N, ANIM_CROUCH_JUMP_NE, ANIM_CROUCH_JUMP_E,
    ANIM_CROUCH_JUMP_SE, ANIM_CROUCH_JUMP_S, ANIM_CROUCH_JUMP_SW, ANIM_CROUCH_JUMP_W, ANIM_CROUCH_JUMP_NW,
    ANIM_CROUCH_SWIM,
    ANIM_SLIDE,
    ANIM_SHOOT, ANIM_MELEE, ANIM_THROW, ANIM_SWITCH,
    ANIM_PAIN,
    ANIM_EDIT, ANIM_LAG, ANIM_TAUNT, ANIM_WIN, ANIM_LOSE,
    ANIM_GUN_IDLE, ANIM_GUN_PRIMARY, ANIM_GUN_SECONDARY, ANIM_GUN_MELEE, ANIM_GUN_THROW, ANIM_GUN_SWITCH, ANIM_GUN_TAUNT,
    ANIM_VWEP_IDLE, ANIM_VWEP_SHOOT, ANIM_VWEP_MELEE, ANIM_VWEP_THROW,
    ANIM_TRIGGER,
    NUMANIMS
};

static const char * const animnames[] =
{
    "mapmodel",
    "dead", "dying",
    "idle", "run N", "run NE", "run E", "run SE", "run S", "run SW", "run W", "run NW",
    "jump", "jump N", "jump NE", "jump E", "jump SE", "jump S", "jump SW", "jump W", "jump NW",
    "swim",
    "crouch", "crouch N", "crouch NE", "crouch E", "crouch SE", "crouch S", "crouch SW", "crouch W", "crouch NW",
    "crouch jump", "crouch jump N", "crouch jump NE", "crouch jump E", "crouch jump SE", "crouch jump S", "crouch jump SW", "crouch jump W", "crouch jump NW",
    "crouch swim",
    "slide",
    "shoot", "melee", "throw", "switch",
    "pain",
    "edit", "lag", "taunt", "win", "lose",
    "gun idle", "gun primary", "gun secondary", "gun melee","gun throw", "gun switch", "gun taunt",
    "vwep idle", "vwep shoot", "vwep melee", "vwep throw"
    "trigger",
};

// network quantization scale
const float DMF   = 16.0f;  // for world locations
const float DNF   = 100.0f; // for normalized vectors
const float DVELF = 1.0f;   // for velocity vectors based on player's speed

struct gameentity : extentity
{
};

// physics stuff
enum
{
    LiquidTransition_None = 0,
    LiquidTransition_In,
    LiquidTransition_Out
};

enum { MM_AUTH = -1, MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD, MM_START = MM_AUTH, MM_INVALID = MM_START - 1 };

static const char * const mastermodenames[] =  { "Default",        "Open",        "Veto",        "Locked",        "Private",        "Password" };
static const char * const mastermodecolors[] = {       "",          "\f0",         "\f2",           "\f4",            "\f3",             "\f6" };
static const char * const mastermodeicons[] =  { "server",  "server_open", "server_veto", "server_locked", "server_private", "server_password" };

// Server privileges.
enum
{ 
    PRIV_NONE = 0,
    PRIV_HOST,
    PRIV_MODERATOR,
    PRIV_ADMINISTRATOR,
    PRIV_OWNER
};
static const int privilegecolors[5] =
{
    0xFFFFFF,
    0x00FFFF,
    0x00FF80,
    0xFFC575,
    0xF0A4F0
};
static const char * const privilegetextcodes[5] =
{
    "\ff",
    "\f8",
    "\f0",
    "\f6",
    "\f5"
};
static const char * const privilegenames[5] =
{
    "None",
    "Host",
    "Moderator",
    "Administrator",
    "Owner"
};
inline bool validprivilege(int privilege)
{
    return privilege > PRIV_NONE && privilege <= PRIV_ADMINISTRATOR;
}

// round states
enum
{
    ROUND_START  = 1<<0,
    ROUND_END    = 1<<1,
    ROUND_RESET  = 1<<2,
    ROUND_WAIT   = 1<<3,
    ROUND_UNWAIT = 1<<4
};

// Time updates.
enum
{
    TimeUpdate_Match = 0,
    TimeUpdate_Overtime,
    TimeUpdate_Intermission
};

// network messages codes, c2s, c2c, s2c
enum
{
    N_CONNECT = 0, N_SERVINFO, N_WELCOME, N_INITCLIENT, N_POS, N_TEXT, N_SOUND, N_CDIS,
    N_SHOOT, N_DESTROYPROJECTILE, N_DAMAGEPROJECTILE, N_SUICIDE,
    N_DIED, N_DAMAGE, N_HITPUSH, N_SHOTEVENT, N_EXPLODEFX, N_REGENERATE,
    N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_FORCEDEATH,
    N_GUNSELECT, N_TAUNT, N_PHYSICSEVENT,
    N_NOTICE, N_ANNOUNCE,
    N_MAPCHANGE, N_MAPVOTE, N_TEAMINFO, N_ITEMSPAWN, N_ITEMPICKUP, N_ITEMACC, N_TELEPORT, N_JUMPPAD,
    N_PING, N_PONG, N_CLIENTPING,
    N_TIMEUP, N_FORCEINTERMISSION,
    N_SERVMSG, N_ITEMLIST, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITENTLABEL, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_CALCLIGHT, N_REMIP, N_EDITVSLOT, N_UNDO, N_REDO, N_NEWMAP, N_GETMAP, N_SENDMAP, N_CLIPBOARD, N_EDITVAR,
    N_MASTERMODE, N_MUTE, N_KICK, N_CLEARBANS, N_CURRENTMASTER, N_SPECTATOR, N_SETMASTER, N_SETTEAM,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO,
    N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_TAKEFLAG, N_RETURNFLAG, N_RESETFLAG, N_TRYDROPFLAG, N_DROPFLAG, N_SCOREFLAG, N_INITFLAGS,
    N_ROUND, N_ROUNDSCORE, N_ASSIGNROLE, N_SCORE, N_VOOSH,
    N_SAYTEAM, N_WHISPER,
    N_CLIENT,
    N_AUTHTRY, N_AUTHKICK, N_AUTHCHAL, N_AUTHANS, N_REQAUTH,
    N_PAUSEGAME, N_GAMESPEED,
    N_ADDBOT, N_DELBOT, N_INITAI, N_FROMAI, N_BOTLIMIT, N_BOTBALANCE,
    N_MAPCRC, N_CHECKMAPS,
    N_SWITCHNAME, N_SWITCHMODEL, N_SWITCHCOLOR, N_SWITCHTEAM,
    N_SERVCMD,
    N_DEMOPACKET,
    N_COUNTRY,
    NUMMSG
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    N_CONNECT, 0, N_SERVINFO, 0, N_WELCOME, 1, N_INITCLIENT, 0, N_POS, 0, N_TEXT, 0, N_SOUND, 3, N_CDIS, 2,
    N_SHOOT, 0, N_DESTROYPROJECTILE, 0, N_DAMAGEPROJECTILE, 5, N_SUICIDE, 1,
    N_DIED, 7, N_DAMAGE, 11, N_HITPUSH, 7, N_SHOTEVENT, 11, N_EXPLODEFX, 4, N_REGENERATE, 2,
    N_TRYSPAWN, 1, N_SPAWNSTATE, 9, N_SPAWN, 3, N_FORCEDEATH, 2,
    N_GUNSELECT, 2, N_TAUNT, 1, N_PHYSICSEVENT, 3,
    N_NOTICE, 2, N_ANNOUNCE, 1,
    N_MAPCHANGE, 0, N_MAPVOTE, 0, N_TEAMINFO, 0, N_ITEMSPAWN, 2, N_ITEMPICKUP, 2, N_ITEMACC, 3,
    N_PING, 2, N_PONG, 2, N_CLIENTPING, 2,
    N_TIMEUP, 3, N_FORCEINTERMISSION, 1,
    N_SERVMSG, 0, N_ITEMLIST, 0, N_RESUME, 0,
    N_EDITMODE, 2, N_EDITENT, 11, N_EDITENTLABEL, 0, N_EDITF, 16, N_EDITT, 16, N_EDITM, 16, N_FLIP, 14, N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14, N_CALCLIGHT, 1, N_REMIP, 1, N_EDITVSLOT, 16, N_UNDO, 0, N_REDO, 0, N_NEWMAP, 2, N_GETMAP, 1, N_SENDMAP, 0, N_EDITVAR, 0,
    N_MASTERMODE, 2, N_MUTE, 0, N_KICK, 0, N_CLEARBANS, 1, N_CURRENTMASTER, 0, N_SPECTATOR, 4, N_SETMASTER, 0, N_SETTEAM, 0,
    N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 3, N_SENDDEMO, 0,
    N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
    N_TAKEFLAG, 3, N_RETURNFLAG, 4, N_RESETFLAG, 3, N_TRYDROPFLAG, 1, N_DROPFLAG, 7, N_SCOREFLAG, 9, N_INITFLAGS, 0,
    N_ROUND, 0, N_ROUNDSCORE, 0, N_ASSIGNROLE, 4, N_SCORE, 3, N_VOOSH, 3,
    N_SAYTEAM, 0, N_WHISPER, 0,
    N_CLIENT, 0,
    N_AUTHTRY, 0, N_AUTHKICK, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_REQAUTH, 0,
    N_PAUSEGAME, 0, N_GAMESPEED, 0,
    N_ADDBOT, 2, N_DELBOT, 1, N_INITAI, 0, N_FROMAI, 2, N_BOTLIMIT, 2, N_BOTBALANCE, 2,
    N_MAPCRC, 0, N_CHECKMAPS, 1,
    N_SWITCHNAME, 0, N_SWITCHMODEL, 2, N_SWITCHCOLOR, 2,  N_SWITCHTEAM, 2,
    N_SERVCMD, 0,
    N_DEMOPACKET, 0,
    N_COUNTRY, 0,
    -1
};

#define VALHALLA_SERVER_PORT 21217
#define VALHALLA_LANINFO_PORT 21216
#define VALHALLA_MASTER_PORT 21215
#define PROTOCOL_VERSION 3 // bump when protocol changes
#define DEMO_VERSION 1  // bump when demo format changes
#define DEMO_MAGIC "VALHALLA_DEMO\0\0"

struct demoheader
{
    char magic[16];
    int version, protocol;
};

#include "projectile.h"
#include "ai.h"
#include "gamemode.h"
#include "entity.h"
#include "announcer.h"

// inherited by gameent and server clients
struct gamestate
{
    int health, maxhealth, shield;
    int gunselect;
    int aitype, skill;
    int poweruptype, powerupmillis;
    int role;
    int ammo[NUMGUNS], lastAction[NUMGUNS], delay[NUMGUNS];

    gamestate() : health(100), maxhealth(100), shield(0), gunselect(GUN_PISTOL), aitype(AI_NONE), skill(0), poweruptype(PU_NONE), powerupmillis(0), role(ROLE_NONE)
    {
        resetWeapons();
    }

    void useAmmo(const int attack)
    {
        // If we have a power-up granting us infinite ammo.
        if (haspowerup(PU_AMMO))
        {
            return;
        }

        const int weapon = attacks[attack].gun;
        if (!validgun(weapon))
        {
            return;
        }
        ammo[weapon] = max(ammo[weapon] - attacks[attack].use, 0);
    }

    void resetLastAction(const int time = 0)
    {
        for (int i = 0; i < NUMGUNS; i++)
        {
            if (time > 0 && time - lastAction[i] < delay[i])
            {
                continue;
            }
            delay[i] = 0;
        }
    }

    void setLastAction(const int attack, const int time)
    {
        const int weapon = attacks[attack].gun;
        if (validgun(weapon))
        {
            lastAction[weapon] = time;
        }
        else for (int i = 0; i < NUMGUNS; i++)
        {
            lastAction[i] = time;
        }
    }

    void applyAttackDelay(const int attack)
    {
        const int gun = attacks[attack].gun;
        int attackDelay = attacks[attack].attackdelay;
        if (haspowerup(PU_HASTE) || role == ROLE_BERSERKER)
        {
            attackDelay /= 2;
        }
        if (validgun(gun))
        {
            delay[gun] = attackDelay;
        }
        else for (int i = 0; i < NUMGUNS; i++)
        {
            delay[i] = attackDelay;
        }
    }

    bool canShoot(const int attack, int weapon, const int time)
    {
        const int attackWeapon = attacks[attack].gun;
        if (validgun(attackWeapon))
        {
            weapon = attackWeapon;
        }
        const int wait = time - lastAction[weapon];
        return wait >= delay[weapon];
    }

    bool canpickup(int type)
    {
        if(!validitem(type) || role == ROLE_BERSERKER || role == ROLE_ZOMBIE) return false;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_MEGAHEALTH:
            case I_ULTRAHEALTH:
            {
                return health < is.max;
            }

            case I_YELLOWSHIELD:
            case I_REDSHIELD:
            {
                return shield < is.max;
            }

            case I_DDAMAGE:
            case I_HASTE:
            case I_ARMOR:
            case I_INFINITEAMMO:
            case I_AGILITY:
            case I_INVULNERABILITY:
            {
                if(powerupmillis < is.info || poweruptype == is.info)
                {
                    return true;
                }
                return false;
            }

            default:
            {
                return ammo[is.info] < is.max;
            }
        }
        return false;
    }

    void pickup(int type)
    {
        if(!validitem(type) || role == ROLE_BERSERKER || role == ROLE_ZOMBIE) return;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_MEGAHEALTH:
            case I_ULTRAHEALTH:
            {
                health = min(health+is.add, is.max);
                break;
            }

            case I_YELLOWSHIELD:
            case I_REDSHIELD:
            {
                shield = min(shield+is.add, is.max);
                break;
            }

            case I_DDAMAGE:
            case I_ARMOR:
            case I_HASTE:
            case I_INFINITEAMMO:
            case I_AGILITY:
            case I_INVULNERABILITY:
            {
                poweruptype = is.info;
                powerupmillis = min(powerupmillis+is.add, is.max);
                break;
            }

            default:
            {
                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
                break;
            }
        }
    }

    void baseammo(int gun, int k = 3)
    {
        ammo[gun] = itemstats[gun-GUN_SCATTER].add*k;
    }

    void addammo(int gun, int k = 1, int scale = 1)
    {
        itemstat &is = itemstats[gun-GUN_SCATTER];
        ammo[gun] = min(ammo[gun] + (is.add*k)/scale, is.max);
    }

    bool hasmaxammo(int type)
    {
       const itemstat &is = itemstats[type-I_AMMO_SG];
       return ammo[type-I_AMMO_SG+GUN_SCATTER]>=is.max;
    }

    void resetitems()
    {
        shield = 0;
        poweruptype = PU_NONE;
        powerupmillis = 0;
    }

    void resetWeapons()
    {
        for (int i = 0; i < NUMGUNS; i++)
        {
            ammo[i] = 0;
            lastAction[i] = 0;
            delay[i] = 0;
        }
    }

    void respawn()
    {
        maxhealth = 100;
        health = maxhealth;
        resetitems();
        resetWeapons();
        gunselect = GUN_PISTOL;
        role = ROLE_NONE;
    }

    void assignrole(int newrole)
    {
        role = newrole;
        if (role == ROLE_ZOMBIE)
        {
            maxhealth = health = 1000;
            resetitems();
            resetWeapons();
            ammo[GUN_ZOMBIE] = 1;
            gunselect = GUN_ZOMBIE;
        }
        else if (role == ROLE_BERSERKER)
        {
            maxhealth = health = maxhealth * 2;
            for (int i = 0; i < NUMGUNS; i++)
            {
                if (!ammo[i] || i == GUN_INSTA || i == GUN_ZOMBIE) continue;
                if (i == GUN_PISTOL) ammo[i] = 100;
                else ammo[i] = max(itemstats[i - GUN_SCATTER].max, itemstats[i - GUN_SCATTER].add * 5);
            }
        }
    }

    void voosh(int gun)
    {
        for (int i = 0; i < NUMGUNS; i++)
        {
            ammo[i] = 0;
        }
        baseammo(gun);
        gunselect = gun;
    }

    void spawnstate(int gamemode, int mutators, int forcegun)
    {
        if(m_insta(mutators))
        {
            gunselect = GUN_INSTA;
            ammo[GUN_INSTA] = 1;
        }
        else if(m_effic(mutators))
        {
            maxhealth = health = 200;

            // Exclude the pistol and special weapons (last three).
            for (int i = 0; i < NUMGUNS - 3; i++)
            {
                baseammo(i);
            }
            gunselect = GUN_SMG;
        }
        else if(m_tactics(mutators))
        {
            maxhealth = health = 150;
            ammo[GUN_PISTOL] = 100;
            int primaryweapon = rnd(5) + 1, secondaryweapon;
            gunselect = primaryweapon;
            int ammomultiplier = m_noitems(mutators) ? 5 : 3;
            baseammo(primaryweapon, ammomultiplier);
            do secondaryweapon = rnd(5) + 1;
            while (primaryweapon == secondaryweapon); // Make sure we do not spawn with an identical secondary and primary weapon.
            baseammo(secondaryweapon, ammomultiplier);
        }
        else if(m_voosh(mutators))
        {
            voosh(forcegun);
        }
        else
        {
            gunselect = GUN_PISTOL;
            ammo[GUN_PISTOL] = 100;
        }
    }

    int dodamage(int damage, bool isEnvironment = false)
    {
        // Subtract damage/shield points and apply damage here
        if(shield && !isEnvironment)
        { // Only if the player has shield points and damage is not caused by the environment.
            int ad = round(damage / 3.0f * 2.0f);
            if(ad > shield) ad = shield;
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

    bool hasAmmo(const int attack)
    {
        if (!validatk(attack))
        {
            return false;
        }

        // Check weapon only if it is associated with the attack.
        const int gun = attacks[attack].gun;
        if (validgun(gun))
        {
            if (ammo[gun] <= 0)
            {
                return false;
            }
            const int ammoCost = attacks[attack].use;
            if (ammoCost > 0 && ammoCost > ammo[gun])
            {
                return false;
            }
        }

        return true;
    }

    bool haspowerup(int powerup)
    {
        return powerupmillis && poweruptype == powerup;
    }
};

#include "monster.h"

const int MAXNAMELEN = 15;
const int MAXCOUNTRYCODELEN = 8;

const int MAXTEAMS = 2;
inline bool validteam(int team) { return team >= 1 && team <= MAXTEAMS; }
static const char * const teamnames[1+MAXTEAMS] = { "", "Aesir", "Vanir" };
static const char * const teamtextcode[1+MAXTEAMS] = { "\ff", "\f1", "\f3" };
static const char * const teamblipcolor[1+MAXTEAMS] = { "_neutral", "_blue", "_red" };
inline const char *teamname(int team) { return teamnames[validteam(team) ? team : 0]; }
static inline int teamnumber(const char *name) { loopi(MAXTEAMS) if(!strcmp(teamnames[1+i], name)) return 1+i; return 0; }
static const int teamtextcolor[1+MAXTEAMS] = { 0xFFFFFF, 0x6496FF, 0xFF4B19 };
static const int teamscoreboardcolor[1+MAXTEAMS] = { 0, 0x3030C0, 0xC03030 };
static const int teameffectcolor[1+MAXTEAMS] = { 0xFFFFFF, 0x2020FF, 0xFF2020 };

const int TAUNT_DELAY = 1000;
const int VOICECOM_DELAY = 2800;

const int SPAWN_DURATION = 1000; // Spawn effect/shield duration.
const int SPAWN_DELAY = 1500; // Spawn is possible after a specific number of milliseconds has elapsed.

enum
{
    // Reserved sound channels.
    Chan_Idle = 0,
    Chan_Attack,
    Chan_Weapon,
    Chan_PowerUp,
    Chan_LowHealth,
    Chan_Slide,
    Chan_Num
};

enum Interaction
{
    Available = 0,
    Active,
    Count
};

struct gameent : dynent, gamestate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain, lasthurt, lastspawn, lastthrow;
    int lastattack, lastattacker, lasthit, lastkill;
    int lastWeaponUsed;
    int deathstate;
    int attacking;
    int lasttaunt, lastfootstep, lastyelp, lastswitch, lastswitchattempt, lastroll;
    int lastpickup, lastpickupmillis, flagpickup;
    int frags, flags, deaths, points, totaldamage, totalshots, lives, holdingflag;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll;
    int smoothmillis;
    int respawnPoint;

    int chan[Chan_Num], chansound[Chan_Num];

    struct Recoil
    {
        // Maximum number of shots to apply recoil for (available patterns).
        const int maxShots = 8;

        int amount = 0;
        int index = 0;
        int time = 0;
        int lastShot = 0;
        int shots = 0;

        float recoilPitch = 0.0f;
        float recoilYaw = 0.0f;
        float recovery = 0.0f;

        vec2 kick = vec2(0, 0);

        void reset()
        {
            amount = 0;
            index = 0;
            time = lastShot = 0;
            shots = 0;
            recoilPitch = recoilYaw = 0.0f;
            recovery = 0.0f;
            kick = vec2(0, 0);
        }
    };
    Recoil recoil;

    struct crouchSlide
    {
        const int slideDuration = 450;

        int startTime = 0;
        int last = 0;
        float reduction = 0;
        float yaw = 0;
        bool queued = false;

        void initiate(const int time)
        {
            startTime = time;
            queued = false;
        }

        void reset()
        {
            startTime = last = 0;
            reduction = yaw = 0;
            queued = false;
        }

        void cancel()
        {
            startTime = last = 0;
        }

        bool isChained(const int time)
        {
            const int coolDown = slideDuration * 3;
            return last && time - last <= coolDown;
        }

        bool isInProgress(const int time) const
        {
            return startTime && time - startTime <= slideDuration;
        }
    };
    crouchSlide slide;

    bool shouldKeepSliding() const
    {
        return slide.startTime > 0 && physstate == PHYS_RAMP;
    }

    bool sliding(const int time) const
    {
        return state == CS_ALIVE && crouching && !blocked && onfloor() && (shouldKeepSliding() || slide.isInProgress(time));
    }

    float transparency;

    string name, info;
    int team, playermodel, playercolor;
    ai::aiinfo *ai;
    int ownernum, lastnode;
    bool respawnqueued, ghost;
    char country_code[MAXCOUNTRYCODELEN+1], preferred_flag[MAXCOUNTRYCODELEN+1];
    string country_name;

    vec muzzle, eject, hand;
    bool interacting[Interaction::Count];

    enum Ability
    {
        lastAttempt = 0,
        lastUse,
        Count
    };

    int lastAbility[Ability::Count];

    gameent() : weight(100),
                clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0),
                lifesequence(0), respawned(-1), suicided(-1),
                lastpain(0), lasthurt(0), lastspawn(0), lastthrow(0),
                lastfootstep(0), lastyelp(0), lastswitch(0), lastswitchattempt(0), lastroll(0),
                frags(0), flags(0), deaths(0), points(0), totaldamage(0), totalshots(0), lives(3), holdingflag(0),
                edit(NULL), smoothmillis(-1), respawnPoint(-1),
                transparency(1),
                team(0), playermodel(-1), playercolor(0), ai(NULL), ownernum(-1),
                muzzle(-1, -1, -1), eject(-1, -1, -1), hand(-1, -1, -1)
    {
        loopi(Chan_Num)
        {
            chan[i] = chansound[i] = -1;
        }
        resetInteractions();
        name[0] = info[0] = 0;
        ghost = false;
        country_code[0] = country_name[0] = preferred_flag[0] = 0;
        respawn();
    }
    ~gameent()
    {
        freeeditinfo(edit);
        if(ai) delete ai;
        loopi(Chan_Num)
        {
            if (chan[i] >= 0)
            {
                stopsound(chansound[i], chan[i]);
            }
        }
    }

    void hitpush(int damage, const vec &dir, gameent *actor, int atk)
    {
        vec push(dir);
        bool istrickjump = actor == this && isattackprojectile(attacks[atk].projectile) && attacks[atk].exprad;
        if (istrickjump)
        {
            // Projectiles reset gravity while falling so trick jumps are more rewarding and players are pushed further.
            falling.z = 1;
        }
        if (actor != this && physstate < PHYS_SLOPE)
        {
            // While in mid-air, push is stronger.
            damage *= GUN_AIR_PUSH;
        }
        if (role == ROLE_ZOMBIE)
        {
            // Zombies are pushed "a bit" more.
            damage *= GUN_ZOMBIE_PUSH;
        }
        push.mul((istrickjump ? EXP_SELFPUSH : 1.0f) * attacks[atk].hitpush * damage / weight);
        vel.add(push);
    }

    void startgame()
    {
        frags = flags = deaths = points = 0;
        totaldamage = totalshots = 0;
        lives = 3;
        holdingflag = 0;
        maxhealth = 100;
        shield = 0;
        lifesequence = -1;
        respawned = suicided = -2;
        lasthit = lastkill = 0;
        ghost = false;
        resetInteractions();
    }

    void respawn()
    {
        dynent::reset();
        gamestate::respawn();
        respawned = suicided = -1;
        lastattack = lastattacker = -1;
        deathstate = Death_Default;
        attacking = ACT_IDLE;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        flagpickup = 0;
        lastnode = -1;
        lasthit = lastkill = 0;
        lastWeaponUsed = GUN_INVALID;
        respawnqueued = false;
        for (int i = 0; i < Ability::Count; i++)
        {
            lastAbility[i] = 0;
        }
        for (int i = 0; i < Chan_Num; i++)
        {
            stopchannelsound(i);
        }
        resetInteractions();
        recoil.reset();
        slide.reset();
    }

    void halt()
    {
        move = strafe = 0;
        resetinterp();
        smoothmillis = 0;
    }

    void playchannelsound(int type, int sound, int fade = 0, bool isloop = false)
    {
        chansound[type] = sound;
        chan[type] = playsound(chansound[type], this, NULL, NULL, 0, isloop ? -1 : 0, fade, chan[type]);
    }

    void stopchannelsound(int type, int fade = 0)
    {
        if (chan[type] >= 0)
        {
            stopsound(chansound[type], chan[type], fade);
        }
        chansound[type] = chan[type] = -1;
    }

    void stopSounds()
    {
        stopownedsounds(this);
    }

    void kill(const bool shouldCountDeath)
    {
        state = CS_DEAD;
        lastpain = lastmillis;
        for (int i = 0; i < Chan_Num; i++)
        {
            // Free up sound channels used for player actions.
            stopchannelsound(i);
        }
        stopSounds();

        // Add deaths to player stats.
        if (shouldCountDeath)
        {
            deaths++;
        }
    }

    void resetInteractions()
    {
        for (int i = 0; i < Interaction::Count; i++)
        {
            interacting[i] = false;
        }
    }

    void updateWeaponDelay(const int time)
    {
        if (state != CS_ALIVE)
        {
            return;
        }
        resetLastAction(time);
    }

    void prepareThrow(const int time)
    {
        lastthrow = time;
    }

    void cancelAttack()
    {
        attacking = ACT_IDLE;
        lastthrow = 0;
    }

    bool haslowhealth()
    {
        return state == CS_ALIVE && health <= maxhealth / 4;
    }

    bool shouldgib()
    {
        return health <= THRESHOLD_GIB;
    }
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
    extern void preload();
    extern void preloadWorld();
    extern void render();
    extern void checkItems(gameent *d);
    extern void updatePowerups(int time, gameent *d);
    extern void resetSpawn();
    extern void spawnItems(bool force = false);
    extern void sendItems(packetbuf &p);
    extern void setSpawn(int i, bool shouldspawn, bool isforced = false);
    extern void teleport(int n, gameent *d);
    extern void doPickupEffects(const int n, gameent *d);
    extern void doHudPickupEffects(const int type, gameent* d, const bool shouldCheck = true);
    extern void doEntityEffects(const gameent *d, const int sourceEntityId, const bool local, const int targetEntityId = -1);
    extern void resetTriggers();
    extern void onMapStart();
    extern void onPlayerDeath(const gameent *d, const gameent *actor);
    extern void onPlayerSpectate(const gameent *d);
    extern void onPlayerUnspectate(const gameent *d);

    extern vector<extentity*> ents;
}

namespace physics
{
    extern void moveplayer(gameent* pl, int moveres, bool local);
    extern void crouchplayer(gameent* pl, int moveres, bool local);
    extern void physicsframe();
    extern void updatephysstate(gameent* d);
    extern void addroll(gameent* d, float amount);
    extern void pushRagdoll(dynent* d, const vec& position, const int damage);

    extern bool canmove(gameent* d);
    extern bool hasbounced(ProjEnt* proj, float secs, float elasticity, float waterfric, float gravity);
    extern bool isbouncing(ProjEnt* proj, float elasticity, float waterfric, float gravity);
    extern bool allowVerticalMovement(gameent* player);

    extern int physsteps;
    extern int liquidtransition(physent* d, int material, bool isinwater);
}

namespace game
{
    extern int gamemode, mutators;

    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual void drawhud(gameent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned(gameent *d) {}
        virtual void setup() {}
        virtual void checkitems(gameent *d) {}
        virtual void pickspawn(gameent *d) { findplayerspawn(d, -1, m_teammode ? d->team : 0); }
        virtual void senditems(packetbuf &p) {}
        virtual void removeplayer(gameent *d) {}
        virtual void gameover() {}
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        virtual bool hidefrags() { return false; }
        virtual bool canfollow(gameent* s, gameent* f) { return true; }
        virtual bool aicheck(gameent *d, ai::aistate &b) { return false; }
        virtual bool aidefend(gameent *d, ai::aistate &b) { return false; }
        virtual bool aipursue(gameent *d, ai::aistate &b) { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual int respawnwait() { return 0; }
        virtual float ratespawn(gameent* d, const extentity& e) { return 1.0f; }
        virtual float clipconsole(float w, float h) { return 0; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // game.cpp
    extern void taunt(gameent *d);
    extern void stopfollowing();
    extern void checkfollow();
    extern void nextfollow(int dir = 1);
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void cleargame();
    extern void startgame();
    extern void pickgamespawn(gameent* d);
    extern void spawnplayer(gameent *d);
    extern void spawneffect(gameent *d);
    extern void respawn();
    extern void setdeathstate(gameent *d, const bool isRestoringState = false);
    extern void printkillfeedannouncement(int announcement, gameent* actor);
    extern void writeobituary(gameent *d, gameent *actor, int atk, const int flags = 0);
    extern void kill(gameent *d, gameent *actor, int atk, int flags = 0);
    extern void updatetimer(int time, int type);
    extern void sendsound(const int sound, physent *d = NULL, const int team = 0);
    extern void doaction(int act);
    extern void hurt(gameent* d);
    extern void suicide(gameent* d);
    extern void managedeatheffects(gameent* d);

    extern bool clientoption(const char* arg);
    extern bool gamewaiting, betweenrounds, hunterchosen;
    extern bool isally(const gameent* a, const gameent* b);
    extern bool isinvulnerable(gameent* target, gameent* actor);

    extern int vooshgun;
    extern int maptime, maprealtime, maplimit, lastmap;
    extern int lastspawnattempt;
    extern int following, specmode;
    extern int smoothmove, smoothdist;
    extern int deathscream;
    extern int getdeathstate(gameent* d, int atk, int flags);
    extern const int getrespawndelay(gameent* d);
    extern const int getrespawnwait(gameent* d);

    extern const char* colorname(gameent* d, const char* name = NULL, const char* alt = NULL, const char* color = "");
    extern const char* teamcolorname(gameent* d, const char* alt = NULL);
    extern const char* teamcolor(const char* prefix, const char* suffix, int team, const char* alt);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);

    extern string clientmap;

    extern gameent* findPointedPlayer(const float margin = 0);
    extern gameent* hudplayer();
    extern gameent* followingplayer(gameent* fallback = NULL);
    extern gameent* getclient(int cn);
    extern gameent* newclient(int cn);
    extern gameent* self;

    extern vector<gameent*> players, clients, ragdolls;
    extern vector<gameent*> bestplayers;
    extern vector<int> bestteams;

    // client.cpp
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern void switchname(const char *name);
    extern void switchteam(const char *name);
    extern void switchplayermodel(int playermodel);
    extern void switchplayercolor(int playercolor);
    extern void sendmapinfo();
    extern void stopdemo();
    extern void changemap(const char *name, int mode, int muts);
    extern void c2sinfo(bool force = false);
    extern void sendposition(const gameent *d, const bool reliable = false);
    extern void forceintermission();

    extern bool connected, remote, demoplayback;
    extern bool addmsg(int type, const char* fmt = NULL, ...);
    extern bool isignored(int cn);

    extern int parseplayer(const char* arg);
    extern int gamespeed;

    extern string servdesc;

    extern vector<uchar> messages;

    namespace projectiles
    {
        // projectile.cpp
        extern vector<ProjEnt*> Projectiles, AttackProjectiles;

        extern void update(const int time);
        extern void updatelights();
        extern void clear(gameent* owner = nullptr);
        extern void render();
        extern void preload();
        extern void make(gameent* owner, const vec& from, const vec& to, const bool isLocal, const int id, const int attack, const int type, const int lifetime, const int speed, const float gravity = 0, const float elasticity = 0);
        extern void spawnbouncer(const vec& from, gameent* d, const int type);
        extern void bounce(physent* d, const vec& surface);
        extern void collidewithentity(physent* bouncer, physent* collideEntity);
        extern void destroy(ProjEnt& proj, const vec& position, const bool isLocal = true, const int attack = ATK_INVALID);
        extern void detonate(gameent* d, const int gun);
        extern void avoid(ai::avoidset& obstacles, const float radius);
        extern void triggerExplosion(gameent* owner, const int attack, const vec& position, const vec& velocity);
        extern void registerhit(dynent* target, gameent* actor, const int attack, const float dist, const int rays);
        extern void damage(ProjEnt* proj, gameent* actor, const int attack);

        ProjEnt* get(const int id, const gameent* owner);
    }

    // weapon.cpp
    enum
    {
        SwayEvent_Jump = 0,
        SwayEvent_Land,
        SwayEvent_LandHeavy,
        SwayEvent_Crouch,
        SwayEvent_Switch
    };

    struct swayinfo
    {
        float fade, speed, dist, verticalVelocity;
        float yaw, pitch, roll;
        vec o, dir;

        struct hudent : dynent
        {
            hudent()
            {
                type = ENT_CAMERA;
            }
        } interpolation;

        struct swayEvent
        {
            int type, millis, duration;
            float factor;
        };
        vector<swayEvent> events;

        swayinfo() : fade(0), speed(0), dist(0), verticalVelocity(0), yaw(0), pitch(0), roll(0), o(0, 0, 0), dir(0, 0, 0)
        {
        }
        ~swayinfo()
        {
        }

        void updatedirection(gameent* owner);
        void updatePosition(const gameent* owner, vec& position);
        void update(gameent* owner, vec& position);
        void addevent(const gameent* owner, int type, int duration, float factor);
        void processevents();
    };

    extern swayinfo sway;

    extern void shoot(gameent *d, const vec &targ);
    extern void updateRecoil(gameent* d, const int curtime);
    extern void updateThrow(gameent* player);
    extern void applyShotEffects(const int attack, vec& from, vec& to, gameent* player, const int id, const bool isHit, const bool isLocal);
    extern void updateShotEvent(gameent* player, const int attack);
    extern void scanhit(vec& from, vec& to, gameent* d, int atk);
    extern void gibeffect(int damage, const vec &vel, gameent *d);
    extern void updateWeaponDelay(gameent* player);
    extern void updatePlayerWeapons(gameent* d, const vec& to, const int curtime);
    extern void updateweapons(const int curtime);
    extern void clearweapons();
    extern void gunselect(int gun, gameent* d);
    extern void doweaponchangeffects(gameent* d, int gun = GUN_INVALID);
    extern void weaponswitch(gameent* d);
    extern void autoswitchweapon(gameent* d, int type);
    extern void dodamage(const int damage, gameent* target, gameent* actor, const vec& position, const int atk, const int flags, const bool isLocal);
    extern void hit(dynent* target, gameent* actor, const vec& hitPosition, const vec& velocity, int damage, const int atk, const float dist, const int rays = 1, const int flags = Hit_Torso);

    extern float intersectdist;

    extern bool isIntersectingEntity(dynent *entity, const vec &from, const vec &to, const float margin = 0, float &dist = intersectdist);

    extern int getweapon(const char* name);
    extern int calculatedamage(int damage, gameent* target, gameent* actor, int atk, int flags = Hit_Torso);
    extern int checkweaponzoom();
    extern int blood;

    extern vec hudgunorigin(int attack, const vec& from, const vec& to, gameent* d);

    extern dynent* intersectClosest(gameent* actor, const vec& from, const vec& to, const float margin = 0, const int flags = DYN_PLAYER | DYN_AI, float& intersectDistance = intersectdist);

    extern vector<hitmsg> hits;

    // monster.cpp
    struct monster : gameent
    {
        int monsterstate; // one of MS_*, MS_NONE means it's not an NPC

        int mtype; // see monstertypes table
        gameent *enemy; // monster wants to kill this entity
        float targetyaw; // monster wants to look in this direction
        int trigger; // millis at which transition to another monsterstate takes place
        vec attacktarget;
        int anger; // how many times already hit by fellow monster
        physent *stacked;
        vec stackpos, orient;
        bool halted, canmove, exploding;
        int lastunblocked, detonating;
        int bursting, shots;

        int id;
        char *label;

        monster(int _type, int _yaw, int _canmove, int _state, int _trigger, int _move, const char *_label);
        ~monster();
        void normalize_yaw(float angle);
        void transition(int _state, int _moving, int n, int r);
        void burst(bool on);
        void emitattacksound();
        void alert(bool on);
        void monsteraction(int curtime);
        void preparedetonation();
        void detonate();
        void monsterdeath(int forcestate = -1, int atk = ATK_INVALID, int flags = 0);
        void heal();
        void monsterpain(int damage, gameent *d, int atk, int flags);
    };
    extern vector<monster *> monsters;

    extern void clearmonsters();
    extern void preloadmonsters();
    extern void stackmonster(monster *d, physent *o);
    extern void updatemonsters(int curtime);
    extern void rendermonsters();
    extern void suicidemonster(monster *m);
    extern void healmonsters();
    extern void hitmonster(int damage, monster *m, gameent *at, int atk, const vec& velocity, int flags = 0);
    extern void monsterkilled(gameent* monster, int flags = 0);
    extern void checkmonsterinfight(monster *that, gameent *enemy);
    extern void endsp(bool allkilled);
    extern void spsummary(int accuracy);

    extern int getbloodcolor(dynent *d);

    // scoreboard.cpp
    extern void getbestplayers(vector<gameent *> &best);
    extern void getbestteams(vector<int> &best);
    extern void clearteaminfo();
    extern void setteaminfo(int team, int frags);
    extern void removegroupedplayer(gameent *d);

    // render.cpp
    struct playermodelinfo
    {
        const char *directory, *armdirectory, *powerup[5];
        bool ragdoll;
        const int bloodcolor, painsound, diesound[Death_Num], tauntsound;
    };

    extern void saveragdoll(gameent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern void syncplayer();
    extern void rendermonster(dynent* d, const char* mdlname, modelattach* attachments, const int attack, const int attackdelay, const int lastAction, const int lastpain, const float fade = 1, const bool ragdoll = false);

    extern int getplayercolor(const gameent* d, const int team);
    extern int chooserandomplayermodel(int seed);
    extern int getplayermodel(gameent* d);

    extern const playermodelinfo &getplayermodelinfo(gameent *d);

    // hud.cpp
    extern void drawradar(const float x, const float y, const float s);
    extern void setbliptex(const int team, const char* type = "");
    extern void managelowhealthscreen();
    extern void damageblend(const int damage, const int factor = 0);
    extern void setdamagehud(const int damage, gameent* d, gameent* actor);
    extern void addbloodsplatter(const int amount, const int color);
    extern void checkentity(int type);

    // worlddata.cpp
    extern void postWorldLoad();

    extern int lowhealthscreen;

    namespace announcer
    {
        extern void parseannouncements(const gameent* d, gameent* actor, const int flags);
        extern void update();
        extern void reset();

        extern bool announce(const int announcement, const bool shouldQueue = true);
        extern bool playannouncement(const int sound, const bool shouldQueue = true);
    }

    namespace camera
    {
        enum
        {
            CameraEvent_Land = 0,
            CameraEvent_Shake,
            CameraEvent_Spawn,
            CameraEvent_Teleport
        };

        struct camerainfo
        {
            bool isdetached;
            float yaw, pitch, roll, fov;
            float bobfade, bobspeed, bobdist;
            vec direction;
            vec2 velocity;
            vec2 parallax, parallaxVelocity;

            struct CameraEvent
            {
                int type, millis, duration;
                float factor;
            };
            vector<CameraEvent> events;

            struct ShakeEvent
            {
                int factor, millis, duration, elapsed;
                float intensity;
            };
            vector<ShakeEvent> shakes;

            struct zoominfo
            {
                float progress;

                zoominfo() : progress(0)
                {
                }
                ~zoominfo()
                {
                }

                void update();
                void disable();

                bool isenabled();

                bool isinprogress()
                {
                    return progress > 0;
                }
            };
            zoominfo zoomstate;

            camerainfo() : isdetached(false), yaw(0), pitch(0), roll(0), fov(1), bobfade(0), bobspeed(0), bobdist(0), direction(0, 0, 0), velocity(0, 0), parallax(0, 0), parallaxVelocity(0, 0)
            {
            }
            ~camerainfo()
            {
            }

            void update();
            void updateFov();
            void updateMovementFov();
            void addevent(const gameent* owner, int type, int duration, float factor = 0);
            void processevents();
            void addshake(int factor);
            void updateshake();

            vec getposition();
        };

        extern camerainfo camera;

        extern void update();
        extern void reset();
        extern void restore(const bool shouldIgnorePitch = false);
        extern void fixrange();
        extern void toggleWeaponZoom();

        extern bool allowthirdperson();
        extern bool isUnderwater();

        extern int thirdperson;
        extern int zoom;
    }

    namespace shaders
    {
        enum Fade
        {
            Out = -1,
            In = 0
        };

        struct PostFxEvent
        {
            const char* name;
            int time;
            int duration;
            int direction;
            float parameter;
        };
        extern vector<PostFxEvent> postFxEvents;

        extern void prepareWorld();
        extern void update();
        extern void cleanUp();
        extern void toggleZoomEffects(const int toggle);
        extern void updateZoomEffects(const float progress);
        extern void addPostFxEvent(const char* name, const int duration, const int direction, const float parameter, const gameent* owner);
    }
}

namespace server
{
    extern const char *modename(int n, const char *unknown = "unknown");
    extern const char *modedesc(int n, const char *unknown = "unknown");
    extern const char *modeprettyname(int n, const char *unknown = "unknown");
    extern const char *mastermodename(int n, const char *unknown = "unknown");
    extern const char *getdemofile(const char *file, bool init);

    extern void startintermission();
    extern void stopdemo();
    extern void timeupdate(int secs);
    extern void forcemap(const char *map, int mode, int muts);
    extern void forcepaused(bool paused);
    extern void forcegamespeed(int speed);
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern void updateroundstate(int state, bool send = true);
    extern void checkroundwait();
    extern void endround();
    extern void checkplayers(bool timeisup = false);

    extern bool serveroption(const char *arg);
    extern bool delayspawn(int type);
    extern bool checkovertime(bool timeisup = false);
    extern bool gamewaiting;

    extern int msgsizelookup(int msg);
}

#endif
