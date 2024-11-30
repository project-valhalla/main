// hardcoded sounds, defined in sound.cfg
enum
{
    // player
    S_JUMP1 = 0, S_JUMP2, S_LAND, S_LAND_WATER,
    S_FOOTSTEP, S_FOOTSTEP_DIRT, S_FOOTSTEP_METAL, S_FOOTSTEP_WOOD,
    S_FOOTSTEP_DUCT, S_FOOTSTEP_SILKY, S_FOOTSTEP_SNOW, S_FOOTSTEP_ORGANIC,
    S_FOOTSTEP_GLASS, S_FOOTSTEP_WATER,
    S_WATER_IN, S_WATER_OUT, S_UNDERWATER, S_LAVA_IN,

    S_SPAWN, S_PLAYER_DAMAGE,

    S_PAIN_MALE, S_DIE_MALE1, S_DIE_MALE2, S_DIE_MALE3, S_DIE_MALE4, S_TAUNT_MALE,
    S_PAIN_FEMALE, S_DIE_FEMALE1, S_DIE_FEMALE2, S_DIE_FEMALE3, S_DIE_FEMALE4, S_TAUNT_FEMALE,

    S_PAIN_ZOMBIE_MALE, S_DIE_ZOMBIE_MALE, S_TAUNT_ZOMBIE_MALE,
    S_PAIN_ZOMBIE_FEMALE, S_DIE_ZOMBIE_FEMALE, S_TAUNT_ZOMBIE_FEMALE,

    S_CORPSE, S_GIB, S_DEATH, S_LOW_HEALTH,

    S_VC_ATTACK, S_VC_BASE, S_VC_DEFEND, S_VC_COVER, S_VC_DROPFLAG, S_VC_GOODJOB,
    S_VC_HELLO, S_VC_HELP, S_VC_LOWHEALTH, S_VC_NICESHOT, S_VC_NO, S_VC_SORRY, S_VC_SUCK,
    S_VC_TAKEFLAG, S_VC_THANKYOU, S_VC_YEAH,

    S_VC_ATTACK2, S_VC_BASE2, S_VC_DEFEND2, S_VC_COVER2, S_VC_DROPFLAG2, S_VC_GOODJOB2,
    S_VC_HELLO2, S_VC_HELP2, S_VC_LOWHEALTH2, S_VC_NICESHOT2, S_VC_NO2, S_VC_SORRY2, S_VC_SUCK2,
    S_VC_TAKEFLAG2, S_VC_THANKYOU2, S_VC_YEAH2,

    // item / entity
    S_TELEPORT, S_TELEDEST, S_JUMPPAD,

    S_ITEM_SPAWN,

    S_AMMO_SG, S_AMMO_SMG, S_AMMO_PULSE, S_AMMO_ROCKET, S_AMMO_RAIL, S_AMMO_GRENADE,
    S_HEALTH, S_SUPERHEALTH, S_MEGAHEALTH,
    S_SHIELD_LIGHT, S_SHIELD_HEAVY, S_SHIELD_HIT,

    S_POWERUP, S_POWERUP_SPAWN,
    S_DAMAGE, S_HASTE, S_ARMOR, S_INFINITEAMMO, S_AGILITY, S_INVULNERABILITY,
    S_ACTION_DAMAGE, S_ACTION_HASTE, S_ACTION_ARMOR, S_ACTION_INFINITEAMMO, S_ACTION_INVULNERABILITY,
    S_LOOP_DAMAGE, S_LOOP_HASTE, S_LOOP_ARMOR, S_LOOP_INFINITEAMMO, S_LOOP_AGILITY, S_LOOP_INVULNERABILITY,
    S_TIMEOUT_DAMAGE, S_TIMEOUT_HASTE, S_TIMEOUT_ARMOR, S_TIMEOUT_INFINITEAMMO, S_TIMEOUT_AGILITY, S_TIMEOUT_INVULNERABILITY,
    S_ACTIVATION_AGILITY, S_ACTIVATION_INVULNERABILITY,

    S_TRIGGER,

    // weapon
    S_MELEE,
    S_SG_A, S_SG_B, S_SG_SWITCH,
    S_SMG,
    S_PULSE1, S_PULSE2_A, S_PULSE2_B, S_PULSE_LOOP, S_PULSE_EXPLODE, S_PULSE_SWITCH,
    S_ROCKET1, S_ROCKET2, S_ROCKET_LOOP, S_ROCKET_EXPLODE, S_ROCKET_SWITCH,
    S_RAIL_A, S_RAIL_B, S_RAIL_SWITCH,
    S_GRENADE, S_GRENADE_EXPLODE, S_GRENADE_SWITCH,
    S_PISTOL1, S_PISTOL2, S_PISTOL_LOOP, S_PISTOL_SWITCH,
    S_INSTAGUN,
    S_ZOMBIE, S_ZOMBIE_IDLE,

    S_IMPACT_MELEE, S_IMPACT_SG, S_IMPACT_SMG, S_IMPACT_PULSE, S_IMPACT_RAILGUN, S_IMPACT_PISTOL,
    S_IMPACT_WATER, S_IMPACT_WATER_PROJ, S_IMPACT_GLASS,

    S_HIT_MELEE, S_HIT_RAILGUN,
    S_HIT_WEAPON, S_HIT_WEAPON_HEAD,

    S_BOUNCE_ROCKET, S_BOUNCE_GRENADE,
    S_BOUNCE_EJECT1, S_BOUNCE_EJECT2, S_BOUNCE_EJECT3,

    S_WEAPON_LOAD, S_WEAPON_NOAMMO, S_WEAPON_DETONATE, S_WEAPON_ZOOM,

    // announcer
    S_ANNOUNCER_FIGHT, S_ANNOUNCER_WIN, S_ANNOUNCER_OVERTIME,

    S_ANNOUNCER_DAMAGE, S_ANNOUNCER_HASTE, S_ANNOUNCER_ARMOR,
    S_ANNOUNCER_INFINITEAMMO, S_ANNOUNCER_AGILITY, S_ANNOUNCER_INVULNERABILITY,

    S_ANNOUNCER_FIRST_BLOOD, S_ANNOUNCER_HEADSHOT,
    S_ANNOUNCER_KILLING_SPREE, S_ANNOUNCER_SAVAGE, S_ANNOUNCER_UNSTOPPABLE, S_ANNOUNCER_LEGENDARY,

    S_ANNOUNCER_10_KILLS, S_ANNOUNCER_5_KILLS, S_ANNOUNCER_1_KILL,
    S_ANNOUNCER_5_MINUTES, S_ANNOUNCER_1_MINUTE,

    // monster
    S_OGRO_PAIN, S_OGRO_DEATH, S_OGRO_HALT,
    S_RATA_PAIN, S_RATA_DEATH, S_RATA_HALT,
    S_GUARD_PAIN, S_GUARD_DEATH, S_GUARD_HALT, S_GUARD_UNHALT, S_GUARD_ATTACK, S_GUARD_INFIGHT,
    S_BAUUL_PAIN, S_BAUUL_DEATH, S_BAUUL_HALT,
    S_SPIDER_PAIN, S_SPIDER_DEATH, S_SPIDER_HALT, S_SPIDER_ATTACK, S_SPIDER_INFIGHT,

    // mode
    S_FLAGPICKUP, S_FLAGDROP, S_FLAGRETURN, S_FLAGSCORE, S_FLAGRESET, S_FLAGFAIL, S_FLAGLOOP,
    S_BERSERKER, S_BERSERKER_LOOP, S_BERSERKER_ACTION,
    S_INFECTION, S_INFECTED, S_WIN_ZOMBIES, S_WIN_SURVIVORS,
    S_ROUND, S_ROUND_WIN,
    S_VOOSH,
    S_TRAITOR, S_TRAITOR_KILL, S_VICTIM,

    // miscellaneous
    S_HIT, S_HIT_ALLY, S_KILL, S_KILL_ALLY, S_KILL_SELF,
    S_CHAT,
    S_INTERMISSION, S_INTERMISSION_WIN
};
inline bool validsound(int sound) { return sound >= 0; }

// sound flags.
enum
{
    SND_MAP       = 1<<0,
    SND_NO_ALT    = 1<<1,
    SND_USE_ALT   = 1<<2,
    SND_ANNOUNCER = 1<<3,
    SND_UI        = 1<<4
};

const int MAX_QUEUE = 10; // queued sounds up to a maximum of 10

extern int playsound(int n, physent *owner = NULL, const vec *loc = NULL, extentity *ent = NULL, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern int playsoundname(const char *s, physent *owner = NULL, const vec *loc = NULL, int vol = 0, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1);
extern void preloadsound(int n);
extern void preloadmapsound(int n);
extern bool stopsound(int n, int chanid, int fade = 0);
extern void stopsounds(int exclude = 0);
extern void stopmapsounds();
extern void stopownersounds(physent *d);
extern void initsound();
extern void pauseaudio(int value);
