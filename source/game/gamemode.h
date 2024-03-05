
enum
{
    M_TEAM        = 1<<0,
    M_CTF         = 1<<1,
    M_OVERTIME    = 1<<2,
    M_EDIT        = 1<<3,
    M_DEMO        = 1<<4,
    M_LOCAL       = 1<<5,
    M_LOBBY       = 1<<6,
    M_ELIMINATION = 1<<7,
    M_LASTMAN     = 1<<8,
    M_BERSERKER   = 1<<9,
    M_INFECTION   = 1<<10,
    M_INVASION    = 1<<11,
    M_BETRAYAL    = 1<<12,
    M_SP          = 1<<13
};

static struct gamemodeinfo
{
    const char *name, *prettyname;
    int flags;
    const char *info;
} gamemodes[] =
{
    { "tutorial", "Tutorial",          M_SP | M_LOCAL,         NULL                                                                                                          },
    { "demo",     "Demo",              M_DEMO | M_LOCAL,       NULL                                                                                                          },
    { "edit",     "Edit",              M_EDIT,                 "\f2Cooperative Editing\ff: edit maps with multiple players simultaneously"                                   },
    { "dm",       "Deathmatch",        M_LOBBY,                "\f2Deathmatch\ff: kill everyone to score points"                                                             },
    { "tdm",      "Team Deathmatch",   M_TEAM,                 "\f2Team Deathmatch\ff: kill the enemy team to score points for your team"                                    },
    { "ctf",      "Capture The Flag",  M_CTF | M_TEAM,         "\f2Capture The Flag\ff: capture the enemy flag and bring it back to your flag to score points for your team" },
    { "elim",     "Elimination",       M_ELIMINATION | M_TEAM, "\f2Elimination\ff: eliminate the enemy team to win the round and score points for your team"                 },
    { "lms",      "Last Man Standing", M_LASTMAN,              "\f2Last Man Standing\ff: eliminate everyone to win the round and score points"                               },
    { "bers",     "Berserker",         M_BERSERKER,            "\f2Berserker\ff: kill to claim the Berserker title, score points and dominate the arena"                     },
    { "infect",   "Infection",         M_INFECTION,            "\f2Infection\ff: survive the infection or infect survivors to score points"                                  },
    { "invasion", "Invasion",          M_INVASION | M_LOCAL,   "\f2Invasion\ff: survive waves of monsters to score points"                                                   },
    { "betrayal", "Betrayal",          M_BETRAYAL,             "\f2Betrayal\ff: survive the traitor as a victim or kill victims as a traitor to score points"                }
};

#define STARTGAMEMODE (-2)
#define NUMGAMEMODES ((int)(sizeof(gamemodes)/sizeof(gamemodes[0])))

#define m_valid(mode)          ((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES)
#define m_check(mode, flag)    (m_valid(mode) && gamemodes[(mode) - STARTGAMEMODE].flags&(flag))
#define m_checknot(mode, flag) (m_valid(mode) && !(gamemodes[(mode) - STARTGAMEMODE].flags&(flag)))
#define m_checkall(mode, flag) (m_valid(mode) && (gamemodes[(mode) - STARTGAMEMODE].flags&(flag)) == (flag))

#define m_ctf             (m_check(gamemode, M_CTF))
#define m_teammode        (m_check(gamemode, M_TEAM))
#define m_overtime        (m_check(gamemode, M_OVERTIME))
#define m_elimination     (m_check(gamemode, M_ELIMINATION))
#define m_round           (m_check(gamemode, M_ELIMINATION|M_LASTMAN|M_INFECTION|M_BETRAYAL))
#define m_dm              (m_checknot(gamemode, M_EDIT|M_CTF|M_ELIMINATION|M_LASTMAN|M_INFECTION|M_BETRAYAL))
#define m_lms             (m_check(gamemode, M_LASTMAN))
#define m_berserker       (m_check(gamemode, M_BERSERKER))
#define m_infection       (m_check(gamemode, M_INFECTION))
#define m_betrayal        (m_check(gamemode, M_BETRAYAL))

#define m_invasion        (m_check(gamemode, M_INVASION))
#define m_tutorial        (m_check(gamemode, M_SP))

#define m_demo            (m_check(gamemode, M_DEMO))
#define m_edit            (m_check(gamemode, M_EDIT))
#define m_lobby           (m_check(gamemode, M_LOBBY))
#define m_timed           (m_checknot(gamemode, M_DEMO|M_EDIT|M_LOCAL))
#define m_botmode         (m_checknot(gamemode, M_DEMO|M_LOCAL))
#define m_nobot           (m_check(gamemode, M_BETRAYAL|M_INVASION))
#define m_mp(mode)        (m_checknot(mode, M_LOCAL))

#define sameteam(a,b)     (m_teammode && a==b)

enum
{
    MUT_CLASSIC      = 1<<0,
    MUT_INSTAGIB     = 1<<1,
    MUT_EFFIC        = 1<<2,
    MUT_TACTICS      = 1<<3,
    MUT_RANDOMWEAPON = 1<<4,
    MUT_VAMPIRE      = 1<<5,
    MUT_MAYHEM       = 1<<6,
    MUT_NOPOWERUP    = 1<<7,
    MUT_NOITEMS      = 1<<8
};


static struct mutatorinfo
{
    const char *name, *prettyname;
    int flags, exclude;
    const char *info;
} mutator[] =
{
    { "classic", "Classic",       MUT_CLASSIC,      MUT_INSTAGIB|MUT_EFFIC|MUT_NOITEMS,                             "\f6Classic\ff: collect items for ammo, shield and health"                                  },
    { "instagib", "Instagib",     MUT_INSTAGIB,     MUT_CLASSIC|MUT_EFFIC|MUT_TACTICS|MUT_VAMPIRE|MUT_RANDOMWEAPON, "\f6Instagib\ff: you spawn with unlimited railgun ammo and die instantly from one shot"     },
    { "effic", "Efficiency",      MUT_EFFIC,        MUT_CLASSIC|MUT_INSTAGIB|MUT_TACTICS|MUT_RANDOMWEAPON,          "\f6Efficiency\ff: you spawn with all the main weapons and an extra 100 to your max health" },
    { "tactics", "Tactics",       MUT_TACTICS,      MUT_CLASSIC|MUT_INSTAGIB|MUT_EFFIC|MUT_RANDOMWEAPON,            "\f6Tactics\ff: you spawn with two random weapons and an extra 50 to your max health"       },
    { "voosh", "Voosh",           MUT_RANDOMWEAPON, MUT_CLASSIC|MUT_INSTAGIB|MUT_EFFIC|MUT_TACTICS,                 "\f6Voosh\ff: all players switch to a random weapon every 20 seconds"                       },
    { "vamp", "Vampire",          MUT_VAMPIRE,      MUT_INSTAGIB,                                                   "\f6Vampire\ff: your health slowly decreases, deal damage to regenerate it"                 },
    { "mayhem", "Mayhem",         MUT_MAYHEM,       NULL,                                                           "\f6Mayhem\ff: headshots landed with hitscan weapons instantly kill opponents"              },
    { "no-power", "No Power-ups", MUT_NOPOWERUP,    NULL,                                                           "\f6No Power-ups\ff: power-ups do not spawn"                                                },
    { "no-items", "No Items",     MUT_NOITEMS,      MUT_CLASSIC,                                                    "\f6No items\ff: items do not spawn"                                                        }
};

#define NUMMUTATORS            ((int)(sizeof(mutator)/sizeof(mutator[0])))
#define m_insta(mut)           (mut&MUT_INSTAGIB)
#define m_effic(mut)           (mut&MUT_EFFIC)
#define m_tactics(mut)         (mut&MUT_TACTICS)
#define m_voosh(mut)           (mut&MUT_RANDOMWEAPON)
#define m_vampire(mut)         (mut&MUT_VAMPIRE)
#define m_mayhem(mut)          (mut&MUT_MAYHEM)
#define m_nopowerups(mut)      (mut&MUT_NOPOWERUP)
#define m_noitems(mut)         (mut&MUT_NOITEMS)

inline const int maximumhealth(bool iszombie)
{
    return iszombie ? 1000 : 250;
}

enum
{
    ROLE_NONE = 0,
    ROLE_BERSERKER,
    ROLE_ZOMBIE,
    ROLE_TRAITOR
};
