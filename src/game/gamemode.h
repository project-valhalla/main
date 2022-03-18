
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
    { "jugg", "Juggernaut", M_JUGGERNAUT, "\f0Juggernaut\ff: kill the juggernaut to become the juggernaut and score points" },
    { "infect", "Infection", M_INFECTION, "\f0Infection\ff: survive the infection or infect survivors to score points" }
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

#define sameteam(a,b)     (m_teammode && a==b)

enum
{
    MUT_CLASSIC = 1<<0, MUT_INSTAGIB = 1<<1, MUT_EFFIC = 1<<2, MUT_RANDOMWEAPON = 1<<3, MUT_ONEWEAPON = 1<<4,
    MUT_VAMPIRE = 1<<5, MUT_MAYHEM = 1<<6, MUT_NOPOWERUP = 1<<7, MUT_NOITEMS = 1<<8
};


static struct mutatorinfo
{
    const char *name, *prettyname;
    int flags, exclude;
    const char *info;
} mutator[] =
{
    { "classic", "Classic", MUT_CLASSIC, MUT_INSTAGIB|MUT_EFFIC|MUT_NOITEMS, "\f6Classic\ff: collect items for ammo, shield and health" },
    { "instagib", "Instagib", MUT_INSTAGIB, MUT_CLASSIC|MUT_EFFIC|MUT_VAMPIRE|MUT_RANDOMWEAPON|MUT_ONEWEAPON, "\f6Instagib\ff: you spawn with unlimited railgun ammo and die instantly from one shot" },
    { "effic", "Efficiency", MUT_EFFIC, MUT_CLASSIC|MUT_INSTAGIB|MUT_RANDOMWEAPON|MUT_ONEWEAPON, "\f6Efficiency\ff: you spawn with shield and all the main weapons" },
    { "voosh", "Voosh", MUT_RANDOMWEAPON, MUT_CLASSIC|MUT_INSTAGIB|MUT_EFFIC|MUT_ONEWEAPON, "\f6Voosh\ff: all players switch to a random weapon every 15 seconds" },
    { "weapon-arena", "Weapon Arena", MUT_ONEWEAPON, MUT_INSTAGIB|MUT_EFFIC|MUT_RANDOMWEAPON, "\f6Weapon Arena\ff: only one weapon is available (set in server/offline game settings)" },
    { "vamp", "Vampire", MUT_VAMPIRE, MUT_INSTAGIB, "\f6Vampire\ff: your health slowly decreases, deal damage to regenerate it" },
    { "mayhem", "Mayhem", MUT_MAYHEM, NULL, "\f6Mayhem\ff: headshots landed with hitscan weapons instantly kill opponents" },
    { "no-power", "No Power-ups", MUT_NOPOWERUP, NULL, "\f6No Power-ups\ff: power-ups do not spawn" },
    { "no-items", "No Items", MUT_NOITEMS, MUT_CLASSIC, "\f6No items\ff: items do not spawn" }
};

#define NUMMUTATORS              ((int)(sizeof(mutator)/sizeof(mutator[0])))
#define m_classic(b)             (b&MUT_CLASSIC)
#define m_insta(b)               (b&MUT_INSTAGIB)
#define m_effic(b)               (b&MUT_EFFIC)
#define m_randomweapon(b)        (b&MUT_RANDOMWEAPON)
#define m_oneweapon(b)           (b&MUT_ONEWEAPON)
#define m_vampire(b)             (b&MUT_VAMPIRE)
#define m_mayhem(b)              (b&MUT_MAYHEM)
#define m_nopowerups(b)          (b&MUT_NOPOWERUP)
#define m_noitems(b)             (b&MUT_NOITEMS)

#define m_regen(b)               (!(b&MUT_CLASSIC) && !(b&MUT_INSTAGIB) && !(b&MUT_EFFIC) && !(b&MUT_VAMPIRE))
