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
    I_SUPERHEALTH, I_MEGAHEALTH, I_DDAMAGE, I_HASTE, I_ARMOUR, I_UAMMO, I_AGILITY, I_INVULNERABILITY,
    MAXENTTYPES
};

#define validitem(n) (((n) >= I_AMMO_SG && (n) <= I_INVULNERABILITY))

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
    { "shotgun",         "Shotgun",            "item/ammo/shotgun"    },
    { "smg",             "Submachine Gun",     "item/ammo/smg"        },
    { "pulse",           "Pulse Rifle",        "item/ammo/pulse"      },
    { "rocket",          "Rocket Launcher",    "item/ammo/rocket"     },
    { "railgun",         "Railgun",            "item/ammo/railgun"    },
    { "health",          "Health",             "item/health"          },
    { "light_shield",    "Light Shield",       "item/shield/yellow"   },
    { "heavy_shield",    "Heavy Shield",       "item/shield/red"      },
    { "superhealth",     "Super Health",       "item/health/super"    },
    { "megahealth",      "Mega Health",        "item/health/mega"     },
    { "double_damage",   "Double Damage",      "item/doubledamage"    },
    { "haste",           "Haste",              "item/haste"           },
    { "armour",          "Armor",              "item/armour"          },
    { "unlimited_ammo",  "Unlimited Ammo",     "item/ammo/unlimited"  },
    { "agility",         "Agility",            "item/agility"         },
    { "invulnerability", "Invulnerability",    "item/invulnerability" }
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

static struct itemstat { int add, max, spawntime, info, sound, announcersound; } itemstats[] =
{
    { 12,    60,    15,  GUN_SG,             S_AMMO_SG,         NULL,                        }, // shotgun ammo
    { 40,    200,   15,  GUN_SMG,            S_AMMO_SMG,        NULL,                        }, // SMG ammo
    { 80,    400,   15,  GUN_PULSE,          S_AMMO_PULSE,      NULL,                        }, // pulse battery
    { 6,     30,    15,  GUN_RL,             S_AMMO_ROCKET,     NULL,                        }, // rockets
    { 8,     40,    15,  GUN_RAIL,           S_AMMO_RAIL,       NULL,                        }, // railgun ammo
    { 25,    100,   25,  NULL,               S_HEALTH,          NULL,                        }, // regular health
    { 50,    200,   35,  NULL,               S_SHIELD_LIGHT,    NULL,                        }, // light shield
    { 100,   200,   35,  NULL,               S_SHIELD_HEAVY,    NULL,                        }, // heavy shield
    { 50,    250,   60,  NULL,               S_SUPERHEALTH,     NULL,                        }, // super health
    { 100,   250,   80,  NULL,               S_MEGAHEALTH,      NULL,                        }, // mega health
    { 30000, 60000, 100, PU_DAMAGE,          S_DAMAGE,          S_ANNOUNCER_DDAMAGE,         }, // double damage
    { 30000, 60000, 100, PU_HASTE,           S_HASTE,           S_ANNOUNCER_HASTE,           }, // haste
    { 30000, 60000, 100, PU_ARMOR,           S_ARMOUR,          S_ANNOUNCER_ARMOUR,          }, // armour
    { 30000, 60000, 100, PU_AMMO,            S_UAMMO,           S_ANNOUNCER_UAMMO,           }, // unlimited ammo
    { 30000, 60000, 100, PU_AGILITY,         S_AGILITY,         S_ANNOUNCER_AGILITY,         }, // agility
    { 15000, 30000, 100, PU_INVULNERABILITY, S_INVULNERABILITY, S_ANNOUNCER_INVULNERABILITY, }  // invulnerability
};
