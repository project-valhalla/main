#include "game.h"

struct gameent;

static const int OFFSET_MILLIS = 500;

enum
{
    ProjFlag_Weapon = 1 << 0, // Related to a weapon.
    ProjFlag_Junk   = 1 << 1, // Lightweight projectiles for cosmetic effects.
    ProjFlag_Bounce = 1 << 2, // Bounces off surfaces.
    ProjFlag_Linear = 1 << 3, // Follows a linear trajectory.
    ProjFlag_Impact = 1 << 4, // Detonates on collision with geometry or entities.
    ProjFlag_Quench = 1 << 5, // Destroyed upon contact with water.
    ProjFlag_Eject  = 1 << 6  // Can be hit and destroyed by other weapons.
};

enum
{
    Projectile_Grenade = 0,
    Projectile_Grenade2,
    Projectile_Rocket,
    Projectile_Rocket2,
    Projectile_Pulse,
    Projectile_Plasma,
    Projectile_Gib,
    Projectile_Debris,
    Projectile_Casing,
    Projectile_Casing2,
    Projectile_Casing3,
    Projectile_Bullet,
    Projectile_Max
};
inline bool isvalidprojectile(int type)
{ 
    return type >= 0 && type < Projectile_Max;
}

static const struct projectileinfo
{
    int type, flags;
    const char* directory;
    int bouncesound, loopsound, maxbounces, variants;
    float radius;
} projs[Projectile_Max] =
{
    { Projectile_Grenade,  ProjFlag_Weapon | ProjFlag_Bounce,                                     "projectile/grenade",   S_BOUNCE_GRENADE, -1,             0, 0, 1.4f },
    { Projectile_Grenade2, ProjFlag_Weapon | ProjFlag_Bounce | ProjFlag_Impact,                   "projectile/grenade",   S_BOUNCE_GRENADE, -1,             0, 0, 1.4f },
    { Projectile_Rocket,   ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Impact,                   "projectile/rocket/00", -1,               S_ROCKET_LOOP,  0, 0, 1.4f },
    { Projectile_Rocket2,  ProjFlag_Weapon | ProjFlag_Bounce,                                     "projectile/rocket/01", S_BOUNCE_ROCKET,  -1,             2, 0, 2.0f },
    { Projectile_Pulse,    ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact, NULL,                   S_BOUNCE_ROCKET,  S_PULSE_LOOP,   0, 0, 1.0f },
    { Projectile_Plasma,   ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact, NULL,                   S_BOUNCE_ROCKET,  S_PISTOL_LOOP,  0, 0, 1.0f },
    { Projectile_Gib,      ProjFlag_Junk | ProjFlag_Bounce,                                       "projectile/gib",       -1,               -1,             2, 5, 1.5f },
    { Projectile_Debris,   ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Quench,                     NULL,                   -1,               -1,             0, 0, 1.8f },
    { Projectile_Casing,   ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,                      "projectile/eject/00",  S_BOUNCE_EJECT1,  -1,             2, 0, 0.3f },
    { Projectile_Casing2,  ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,                      "projectile/eject/01",  S_BOUNCE_EJECT2,  -1,             2, 0, 0.4f },
    { Projectile_Casing3,  ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,                      "projectile/eject/02",  S_BOUNCE_EJECT3,  -1,             2, 0, 0.5f },
    { Projectile_Bullet,   ProjFlag_Weapon | ProjFlag_Junk | ProjFlag_Linear,                     NULL,                   -1,               -1,             0, 0, 0.4f }
};
inline bool isweaponprojectile(int projectile)
{ 
    return isvalidprojectile(projectile) && projs[projectile].flags & ProjFlag_Weapon && !(projs[projectile].flags & ProjFlag_Junk);
}
inline bool isejectedprojectile(int projectile)
{
    return isvalidprojectile(projectile) && projs[projectile].flags & ProjFlag_Eject;
}

struct projectile : dynent
{
    bool islocal, isdestroyed;

    int id, atk, projtype, flags, lifetime;
    int variant, bounces, offsetmillis;
    int lastbounce, bouncesound, loopchan, loopsound;

    float lastyaw, gravity, elasticity, offsetheight, dist;

    string model;

    vec offset, lastposition, dv, from, to;

    gameent* owner;

    projectile() : isdestroyed(false), variant(0), bounces(0), lastbounce(0), bouncesound(-1), loopchan(-1), loopsound(-1)
    {
        state = CS_ALIVE;
        type = ENT_PROJECTILE;
        collidetype = COLLIDE_ELLIPSE;
        roll = 0;
        model[0] = 0;
        offset = lastposition = dv = from = to = vec(0, 0, 0);
        owner = NULL;
    }
    ~projectile()
    {
        if (loopchan >= 0) stopsound(loopsound, loopchan);
        loopsound = loopchan = -1;
    }

    void setvariant()
    {
        const int variants = projs[projtype].variants;
        if (variants > 0)
        {
            variant = rnd(variants);
        }
    }

    void limitoffset()
    {
        if (flags & ProjFlag_Weapon && offsetmillis > 0 && offset.z < 0)
        {
            offsetheight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
        }
        else offsetheight = -1;
    }

    void setradius()
    {
        const float radius = projs[projtype].radius;
        this->radius = radius;
        xradius = yradius = eyeheight = aboveeye = this->radius;
    }

    void setsounds()
    {
        const int bouncesound = projs[projtype].bouncesound;
        if (validsound(bouncesound))
        {
            this->bouncesound = bouncesound;
        }
        const int loopsound = projs[projtype].loopsound;
        if (validsound(loopsound))
        {
            this->loopsound = loopsound;
        }
    }

    void setflags()
    {
        const int projectileFlags = projs[projtype].flags;
        this->flags = projectileFlags;
    }

    void checkliquid()
    {
        const int material = lookupmaterial(o);
        const bool isinwater = isliquidmaterial(material & MATF_VOLUME);
        inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;
    }

    vec offsetposition()
    {
        vec pos(o);
        if (offsetmillis > 0)
        {
            pos.add(vec(offset).mul(offsetmillis / float(OFFSET_MILLIS)));
            if (offsetheight >= 0) pos.z = max(pos.z, o.z - max(offsetheight - eyeheight, 0.0f));
        }
        return pos;
    }

    vec updateposition(int time)
    {
        if (flags & ProjFlag_Linear)
        {
            offsetmillis = max(offsetmillis - time, 0);
            dist = to.dist(o, dv);
            dv.mul(time / max(dist * 1000 / speed, float(time)));
            vec v = vec(o).add(dv);
            return v;
        }
        else
        {
            vec pos(o);
            pos.add(vec(offset).mul(offsetmillis / float(OFFSET_MILLIS)));
            return pos;
        }
    }
};
