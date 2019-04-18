
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
    M_JUGGERNAUT  = 1<<9,
    M_INFECTION   = 1<<10
};

static struct gamemodeinfo
{
    const char *name, *prettyname;
    int flags;
    const char *info;
} gamemodes[] =
{
    { "demo", "Demo", M_DEMO | M_LOCAL, NULL},
    { "edit", "Edit", M_EDIT, "\f0Cooperative Editing\ff: edit maps with multiple players simultaneously" },
    { "dm", "Deathmatch", M_LOBBY, "\f0Deathmatch\ff: kill everyone to score points" },
    { "tdm", "Team Deathmatch", M_TEAM, "\f0Team Deathmatch\ff: kill the enemy team to score points for your team" },
    { "ctf", "Capture The Flag", M_CTF | M_TEAM, "\f0Capture The Flag\ff: capture the enemy flag and bring it back to your flag to score points for your team" },
    { "elim", "Elimination", M_ELIMINATION | M_TEAM, "\f0Elimination\ff: eliminate the enemy team to win the round and score points for your team" },
    { "lms", "Last Man Standing", M_LASTMAN, "\f0Last Man Standing\ff: eliminate everyone to win the round and score points" },
    { "jugg", "Juggernaut", M_JUGGERNAUT, "\f0Juggernaut\ff: kill players or kill the juggernaut to become the new juggernaut and score points easier" },
    { "infect", "Infection", M_INFECTION, "\f0Infection\ff: survive the infection or infect as many survivors as you can as a zombie to score points" }
};

#define STARTGAMEMODE (-1)
#define NUMGAMEMODES ((int)(sizeof(gamemodes)/sizeof(gamemodes[0])))

#define m_valid(mode)          ((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES)
#define m_check(mode, flag)    (m_valid(mode) && gamemodes[(mode) - STARTGAMEMODE].flags&(flag))
#define m_checknot(mode, flag) (m_valid(mode) && !(gamemodes[(mode) - STARTGAMEMODE].flags&(flag)))
#define m_checkall(mode, flag) (m_valid(mode) && (gamemodes[(mode) - STARTGAMEMODE].flags&(flag)) == (flag))

#define m_ctf             (m_check(gamemode, M_CTF))
#define m_teammode        (m_check(gamemode, M_TEAM))
#define m_overtime        (m_check(gamemode, M_OVERTIME))
#define isteam(a,b)       (m_teammode && a==b)
#define m_elimination     (m_check(gamemode, M_ELIMINATION))
#define m_round           (m_check(gamemode, M_ELIMINATION|M_LASTMAN|M_INFECTION))
#define m_dm              (m_checknot(gamemode, M_EDIT|M_CTF|M_ELIMINATION|M_LASTMAN|M_INFECTION))
#define m_lms             (m_check(gamemode, M_LASTMAN))
#define m_juggernaut      (m_check(gamemode, M_JUGGERNAUT))
#define m_infection       (m_check(gamemode, M_INFECTION))

#define m_demo            (m_check(gamemode, M_DEMO))
#define m_edit            (m_check(gamemode, M_EDIT))
#define m_lobby           (m_check(gamemode, M_LOBBY))
#define m_timed           (m_checknot(gamemode, M_DEMO|M_EDIT|M_LOCAL))
#define m_botmode         (m_checknot(gamemode, M_DEMO|M_LOCAL))
#define m_mp(mode)        (m_checknot(mode, M_LOCAL))

enum
{
    MUT_DEFAULT = 1<<0,
    MUT_CLASSIC = 1<<1, MUT_INSTA = 1<<2, MUT_EFFIC = 1<<3, MUT_RANDOMWEAPON = 1<<4, MUT_ONEWEAPON = 1<<5,
    MUT_VAMPIRE = 1<<6, MUT_HEADHUNTER = 1<<7, MUT_LOCATIONALDAMAGE = 1<<8, MUT_MAYHEM = 1<<9, MUT_NOPOWERUP = 1<<10, MUT_NOITEMS = 1<<11,
    MUT_GHOST = 1<<12, MUT_SLUGGARD = 1<<13, MUT_WALLJUMP = 1<<14, MUT_FALLDAMAGE = 1<<15,
    MUT_ALL = MUT_DEFAULT|MUT_CLASSIC|MUT_INSTA|MUT_EFFIC|MUT_RANDOMWEAPON|MUT_ONEWEAPON|MUT_VAMPIRE|MUT_HEADHUNTER|MUT_LOCATIONALDAMAGE|MUT_MAYHEM|MUT_NOPOWERUP|MUT_NOITEMS|MUT_GHOST|MUT_SLUGGARD|MUT_WALLJUMP|MUT_FALLDAMAGE
};


static struct mutatorinfo
{
    const char *name, *prettyname;
    int flags, exclude;
    const char *info;
} mutator[] =
{
    { "default", "", MUT_DEFAULT, MUT_ALL, ""},
    { "classic", "Classic", MUT_CLASSIC, MUT_DEFAULT | MUT_INSTA | MUT_EFFIC | MUT_NOITEMS, "\f6Classic\ff: collect items for ammo, shield and health" },
    { "insta", "Instagib", MUT_INSTA, MUT_DEFAULT | MUT_CLASSIC | MUT_EFFIC | MUT_VAMPIRE | MUT_RANDOMWEAPON | MUT_ONEWEAPON, "\n\f6Instagib\ff: you spawn with unlimited railgun ammo and die instantly from one shot" },
    { "effic", "Efficiency", MUT_EFFIC, MUT_DEFAULT | MUT_CLASSIC | MUT_INSTA | MUT_RANDOMWEAPON | MUT_ONEWEAPON, "\f6Efficiency\ff: you spawn with shield and all weapons" },
    { "voosh", "Voosh", MUT_RANDOMWEAPON, MUT_DEFAULT|MUT_INSTA|MUT_EFFIC|MUT_ONEWEAPON, "\f6Voosh\ff: all players switch to one random weapon every 15 seconds" },
    { "one-weapon", "One Weapon", MUT_ONEWEAPON, MUT_DEFAULT|MUT_INSTA|MUT_EFFIC|MUT_RANDOMWEAPON, "\f6One Weapon\ff: only one weapon is available" },
    { "vamp", "Vampire", MUT_VAMPIRE, MUT_INSTA, "\f6Vampire\ff: deal damage to regenerate health" },
    { "head-hunter", "Head Hunter", MUT_HEADHUNTER, MUT_LOCATIONALDAMAGE, "\f6Head Hunter\ff: only headshos deal damage" },
    { "loc-damage", "Locational Damage", MUT_LOCATIONALDAMAGE, MUT_HEADHUNTER|MUT_INSTA, "\f6Locational Damage\ff: damage depends on which body area you hit" },
    { "mayhem", "Mayhem", MUT_MAYHEM, NULL, "\f6Mayhem\ff: land a headshot with any hitscan weapon to instantly kill enemies" },
    { "no-power", "No Power-ups", MUT_NOPOWERUP, 0, "\f6No Power-ups\ff: power-ups never spawn in the map" },
    { "no-items", "No Items", MUT_NOITEMS, MUT_CLASSIC, "\f6No items\ff: items never spawn" },
    { "ghost", "Ghost", MUT_GHOST, NULL, "\f6Ghost\ff: players are much harder to see" },
    { "sluggard", "Sluggard", MUT_SLUGGARD, 0, "\f6Sluggard\ff: player speed is decreased" },
    { "wall-jump", "Wall jump", MUT_WALLJUMP, NULL, "\f6Wall jump\ff: players can wall jump" },
    { "fall-damage", "Fall damage", MUT_FALLDAMAGE, 0, "\f6Fall damage\ff: damage is applied when a player exceeds the maximum jumping height threshold" }
};

#define NUMMUTATORS              ((int)(sizeof(mutator)/sizeof(mutator[0])))
#define m_default(b)             (b&MUT_DEFAULT)
#define m_classic(b)             (b&MUT_CLASSIC)
#define m_insta(b)               (b&MUT_INSTA)
#define m_effic(b)               (b&MUT_EFFIC)
#define m_randomweapon(b)        (b&MUT_RANDOMWEAPON)
#define m_oneweapon(b)           (b&MUT_ONEWEAPON)
#define m_vampire(b)             (b&MUT_VAMPIRE)
#define m_headhunter(b)          (b&MUT_HEADHUNTER)
#define m_locationaldam(b)       (b&MUT_LOCATIONALDAMAGE)
#define m_mayhem(b)              (b&MUT_MAYHEM)
#define m_nopowerups(b)          (b&MUT_NOPOWERUP)
#define m_noitems(b)             (b&MUT_INSTA || b&MUT_NOITEMS)
#define m_ghost(b)               (b&MUT_GHOST)
#define m_sluggard(b)            (b&MUT_SLUGGARD)
#define m_walljump(b)            (b&MUT_WALLJUMP)
#define m_falldamage(b)          (b&MUT_FALLDAMAGE)

#define m_headshot(b)            (b&MUT_HEADHUNTER || b&MUT_LOCATIONALDAMAGE || b&MUT_MAYHEM)
#define m_regen(b)               (!(b&MUT_CLASSIC) && !(b&MUT_INSTA) && !(b&MUT_EFFIC) && !(b&MUT_VAMPIRE))
#define m_unlimitedammo(b)       ((b&MUT_INSTA) || (b&MUT_RANDOMWEAPON) || (b&MUT_ONEWEAPON))

