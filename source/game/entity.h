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

    I_AMMO_SG, I_AMMO_SMG, I_AMMO_PULSE, I_AMMO_RL, I_AMMO_RAIL, I_AMMO_GRENADE,
    I_HEALTH, I_YELLOWSHIELD, I_REDSHIELD,
    I_MEGAHEALTH, I_ULTRAHEALTH, I_DDAMAGE, I_HASTE, I_ARMOR, I_INFINITEAMMO, I_AGILITY, I_INVULNERABILITY,

    TRIGGER, // attr1 = idx, attr2 = model, attr3 = radius, attr4 = delay
    TARGET, // attr1 = idx, attr2 = hostile, att3 = can move

    MAXENTTYPES
};

#define validitem(n) (((n) >= I_AMMO_SG && (n) <= I_INVULNERABILITY))

const int ENTITY_TELEPORT_RADIUS = 16;
const int ENTITY_COLLECT_RADIUS = 12;

static const struct gentityinfo { const char *name, *prettyname, *file; } gentities[MAXENTTYPES] =
{
    { "none",            "None",               NULL                   },
    { "light",           "Light",              NULL                   },
    { "mapmodel",        "Map Model",          NULL                   },
    { "playerstart",     "Spawn Point",        NULL                   },
    { "envmap",          "Environment Map",    NULL                   },
    { "particles",       "Particles",          NULL                   },
    { "sound",           "Ambience",           NULL                   },
    { "spotlight",       "Spotlight",          NULL                   },
    { "decal",           "Decal",              NULL                   },
    { "teleport",        "Portal",             "item/teleport"        },
    { "teledest",        "Portal Destination", NULL                   },
    { "jumppad",         "Jump Pad",           NULL                   },
    { "flag",            "Flag",               NULL                   },
    { "scattergun",      "Scattergun",         "item/ammo/scattergun" },
    { "smg",             "Submachine Gun",     "item/ammo/smg"        },
    { "pulse",           "Pulse Rifle",        "item/ammo/pulse"      },
    { "rocket",          "Rocket Launcher",    "item/ammo/rocket"     },
    { "railgun",         "Railgun",            "item/ammo/railgun"    },
    { "grenade",         "Grenade",            "item/ammo/grenade"    },
    { "health",          "Health",             "item/health"          },
    { "light_shield",    "Light Shield",       "item/shield/yellow"   },
    { "heavy_shield",    "Heavy Shield",       "item/shield/red"      },
    { "megahealth",      "Mega Health",        "item/health/super"    },
    { "ultrahealth",     "Ultra Health",       "item/health/mega"     },
    { "double_damage",   "Double Damage",      "item/doubledamage"    },
    { "haste",           "Haste",              "item/haste"           },
    { "armor",           "Armor",              "item/armor"           },
    { "infinite_ammo",   "Infinite Ammo",      "item/ammo/infinite"   },
    { "agility",         "Agility",            "item/agility"         },
    { "invulnerability", "Invulnerability",    "item/invulnerability" },
    { "trigger",         "Trigger",            "item/trigger"         },
    { "target",          "Target",             NULL                   }
};

enum // power-up types
{
    PU_NONE = 0,
    PU_DAMAGE,
    PU_HASTE,
    PU_ARMOR,
    PU_AMMO,
    PU_AGILITY,
    PU_INVULNERABILITY
};

static struct itemstat { int add, max, spawntime, info, sound; } itemstats[] =
{
    { 12,    60,    15,  GUN_SCATTER,        S_AMMO_SG,         }, // shotgun ammo
    { 40,    200,   15,  GUN_SMG,            S_AMMO_SMG,        }, // SMG ammo
    { 80,    400,   15,  GUN_PULSE,          S_AMMO_PULSE,      }, // pulse battery
    { 6,     30,    15,  GUN_ROCKET,         S_AMMO_ROCKET,     }, // rockets
    { 8,     40,    15,  GUN_RAIL,           S_AMMO_RAIL,       }, // railgun ammo
    { 6,     30,    15,  GUN_GRENADE,        S_AMMO_GRENADE,    }, // grenades
    { 25,    100,   25,  NULL,               S_HEALTH,          }, // regular health
    { 50,    200,   35,  NULL,               S_SHIELD_LIGHT,    }, // light shield
    { 100,   200,   35,  NULL,               S_SHIELD_HEAVY,    }, // heavy shield
    { 50,    200,   60,  NULL,               S_SUPERHEALTH,     }, // super health
    { 100,   200,   80,  NULL,               S_MEGAHEALTH,      }, // mega health
    { 30000, 60000, 100, PU_DAMAGE,          S_DAMAGE,          }, // double damage
    { 30000, 60000, 100, PU_HASTE,           S_HASTE,           }, // haste
    { 30000, 60000, 100, PU_ARMOR,           S_ARMOR,           }, // armor
    { 30000, 60000, 100, PU_AMMO,            S_INFINITEAMMO,    }, // infinite ammo
    { 30000, 60000, 100, PU_AGILITY,         S_AGILITY,         }, // agility
    { 15000, 30000, 100, PU_INVULNERABILITY, S_INVULNERABILITY, }  // invulnerability
};

enum
{
    Trigger_Item = 0,
    Trigger_Interest,
    Trigger_RespawnPoint
};

enum
{
    TriggerState_Null = -1,
    TriggerState_Reset,
    TriggerState_Triggering,
    TriggerState_Triggered,
    TriggerState_Resetting
};
