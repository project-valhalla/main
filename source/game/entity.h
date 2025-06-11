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

const int ENTITY_COLLECT_RADIUS = 12;
const int ENTITY_TELEPORT_RADIUS = 16;

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

static struct itemstat
{ 
    int add, max, respawntime, info;
    bool isspawndelayed;
    int sound, announcersound;
    const char* name;
} itemstats[] =
{
    { 12,    60,    15,  GUN_SCATTER,        false, S_AMMO_SG,         -1,                          "Scattergun shells"  }, // Scattergun ammo.
    { 40,    200,   15,  GUN_SMG,            false, S_AMMO_SMG,        -1,                          "SMG cartridges"     }, // SMG ammo.
    { 80,    400,   15,  GUN_PULSE,          false, S_AMMO_PULSE,      -1,                          "Pulse cells"        }, // Pulse rifle battery.
    { 6,     30,    15,  GUN_ROCKET,         false, S_AMMO_ROCKET,     -1,                          "Rockets"            }, // Rocket launcher ammo.
    { 8,     40,    15,  GUN_RAIL,           false, S_AMMO_RAIL,       -1,                          "Railgun ammunition" }, // Rocket launcher ammo.
    { 6,     30,    15,  GUN_GRENADE,        false, S_AMMO_GRENADE,    -1,                          "Grenades"           }, // Grenade Launcher ammo.
    { 25,    100,   25,  -1,                 false, S_HEALTH,          -1,                          "Health"             }, // Regular health.
    { 50,    200,   35,  -1,                 true,  S_SHIELD_LIGHT,    -1,                          "Shield"             }, // Small shield.
    { 100,   200,   35,  -1,                 true,  S_SHIELD_HEAVY,    -1,                          "Shield"             }, // Big shield.
    { 50,    200,   60,  -1,                 true,  S_MEGAHEALTH,      -1,                          "Health"             }, // Mega Health.
    { 100,   200,   80,  -1,                 true,  S_ULTRAHEALTH,     -1,                          "Health"             }, // Ultra health.
    { 30000, 60000, 100, PU_DAMAGE,          true,  S_DAMAGE,          S_ANNOUNCER_DAMAGE,          "Power-up"           }, // Double damage power-up.
    { 30000, 60000, 100, PU_HASTE,           true,  S_HASTE,           S_ANNOUNCER_HASTE,           "Power-up"           }, // Haste power-up.
    { 30000, 60000, 100, PU_ARMOR,           true,  S_ARMOR,           S_ANNOUNCER_ARMOR,           "Power-up"           }, // Armor power-up.
    { 30000, 60000, 100, PU_AMMO,            true,  S_INFINITEAMMO,    S_ANNOUNCER_INFINITEAMMO,    "Power-up"           }, // Infinite ammo power-up.
    { 30000, 60000, 100, PU_AGILITY,         true,  S_AGILITY,         S_ANNOUNCER_AGILITY,         "Power-up"           }, // Agility power-up.
    { 15000, 30000, 100, PU_INVULNERABILITY, true,  S_INVULNERABILITY, S_ANNOUNCER_INVULNERABILITY, "Power-up"           }  // Invulnerability power-up.
};

enum TriggerType
{
    Item = 0,   // an item that can be picked up
    UsableItem, // an item that can be used by pressing a button
    Marker      // a location marked on the HUD
};

// mapmodel trigger states
enum TriggerState
{
    Null = -1,
    Reset,
    Triggering,
    Triggered,
    Resetting
};

enum TriggerEvent
{
    Use = 0,   // fires when the player is close and presses the "Use" button
    Proximity, // fires when the player is close
    Distance,  // fires then the player was close and moves away or dies
    Manual,    // can only be fired manually with the `emittriggerevent` command
    NUMTRIGGEREVENTS
};

static const struct TriggerEventInfo { const char *name; } triggerevents[NUMTRIGGEREVENTS] = {
    { "use" }, { "proximity" }, { "distance" }, { "manual" }
};

struct TriggerEventHandler
{
    char *query; // listen for events on these items
    int event;   // listen for this type of event
    uint *code;  // cubescript code to run
    
    TriggerEventHandler(const char *query_, int event_, uint *code_) : event(event_), code(code_)
    {
        query = newstring(query_);
    };
    ~TriggerEventHandler()
    {
        if(query) delete[] query;
        if(code)  delete[] code;
    }
};