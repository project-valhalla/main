#include "game.h"

struct gameent;

static const int OFFSET_MILLIS = 500;

enum
{
    ProjFlag_Weapon     = 1 << 0, // Related to a weapon.
    ProjFlag_Junk       = 1 << 1, // Lightweight projectiles for cosmetic effects.
    ProjFlag_Bounce     = 1 << 2, // Bounces off surfaces.
    ProjFlag_Linear     = 1 << 3, // Follows a linear trajectory.
    ProjFlag_Impact     = 1 << 4, // Detonates on collision with geometry or entities.
    ProjFlag_Quench     = 1 << 5, // Destroyed upon contact with water.
    ProjFlag_Eject      = 1 << 6, // Can be ejected as a spent casing by weapons.
    ProjFlag_Loyal      = 1 << 7, // Only responds to the weapon that fired it.
    ProjFlag_Invincible = 1 << 8  // Cannot be destroyed.
};

enum
{
    Projectile_Invalid = -1,
    Projectile_Grenade,
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
    int type, flags, bounceSound, loopsound;
    int maxbounces, variants;
    float radius;
    const char* directory;
}
projs[Projectile_Max] =
{
    {
        Projectile_Grenade,
        ProjFlag_Weapon | ProjFlag_Bounce,
        S_BOUNCE_GRENADE,
        -1,
        0,
        0,
        1.4f,
        "projectile/grenade",
    },
    {
        Projectile_Grenade2,
        ProjFlag_Weapon | ProjFlag_Bounce | ProjFlag_Impact,
        S_BOUNCE_GRENADE,
        -1,
        0,
        0,
        1.4f,
        "projectile/grenade",
    },
    {
        Projectile_Rocket,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Impact,
        -1,
        S_ROCKET_LOOP,
        0,
        0,
        2.0f,
        "projectile/rocket/00",
    },
    {
        Projectile_Rocket2,
        ProjFlag_Weapon | ProjFlag_Bounce,
        S_BOUNCE_ROCKET,
        -1,
        2,
        0,
        2.0f,
        "projectile/rocket/01"
    },
    {
        Projectile_Pulse,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Invincible,
        -1,
        S_PULSE_LOOP,
        0,
        0,
        1.0f,
        NULL,
    },
    {
        Projectile_Plasma,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Loyal,
        -1,
        S_PISTOL_LOOP,
        0,
        0,
        5.0f,
        NULL
    },
    {
        Projectile_Gib,
        ProjFlag_Junk | ProjFlag_Bounce,
        -1,
        -1,
        2,
        5,
        1.5f,
        "projectile/gib",
    },
    {
        Projectile_Debris,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Quench,
        -1,
        -1,
        0,
        0,
        1.8f,
        NULL
    },
    {
        Projectile_Casing,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        S_BOUNCE_EJECT1,
        -1,
        2,
        0,
        0.3f,
        "projectile/eject/00"
    },
    {
        Projectile_Casing2,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        S_BOUNCE_EJECT2,
        -1,
        2,
        0,
        0.4f,
        "projectile/eject/01"
    },
    {
        Projectile_Casing3,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        S_BOUNCE_EJECT3,
        -1,
        2,
        0,
        0.5f,
        "projectile/eject/02"
    },
    {
        Projectile_Bullet,
        ProjFlag_Weapon | ProjFlag_Junk | ProjFlag_Linear,
        -1,
        -1,
        0,
        0,
        0.4f,
        NULL
    }
};
inline bool isattackprojectile(const int projectile)
{ 
    return isvalidprojectile(projectile) && projs[projectile].flags & ProjFlag_Weapon && !(projs[projectile].flags & ProjFlag_Junk);
}
inline bool isejectedprojectile(const int projectile)
{
    return isvalidprojectile(projectile) && projs[projectile].flags & ProjFlag_Eject;
}

struct ProjEnt : dynent
{
    bool isLocal;

    int id, attack, projectile, flags, lifetime, health, weight;
    int variant, bounces, offsetMillis;
    int millis, lastBounce, bounceSound, loopChannel, loopSound;

    float lastYaw, gravity, elasticity, offsetHeight, dist;

    string model;

    vec offset, lastPosition, dv, from, to;

    gameent* owner;

    ProjEnt() : variant(0), bounces(0), millis(0), lastBounce(0), bounceSound(-1), loopChannel(-1), loopSound(-1)
    {
        state = CS_ALIVE;
        type = ENT_PROJECTILE;
        collidetype = COLLIDE_ELLIPSE;
        roll = 0;
        model[0] = 0;
        offset = lastPosition = dv = from = to = vec(0, 0, 0);
        owner = NULL;
    }
    ~ProjEnt()
    {
        if (loopChannel >= 0)
        {
            stopsound(loopSound, loopChannel);
        }
        loopSound = loopChannel = -1;
    }

    void setVariant()
    {
        const int variants = projs[projectile].variants;
        if (variants > 0)
        {
            variant = rnd(variants);
        }
    }

    void limitOffset()
    {
        if (flags & ProjFlag_Weapon && offsetMillis > 0 && offset.z < 0)
        {
            offsetHeight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
        }
        else offsetHeight = -1;
    }

    void setradius()
    {
        const float radius = projs[projectile].radius;
        this->radius = radius;
        xradius = yradius = eyeheight = aboveeye = this->radius;
    }

    void setsounds()
    {
        const int bounceSound = projs[projectile].bounceSound;
        if (validsound(bounceSound))
        {
            this->bounceSound = bounceSound;
        }
        const int loopsound = projs[projectile].loopsound;
        if (validsound(loopsound))
        {
            this->loopSound = loopsound;
        }
    }

    void set(const int type)
    {
        projectile = type;
        this->flags = projs[projectile].flags;
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
        if (offsetMillis > 0)
        {
            pos.add(vec(offset).mul(offsetMillis / float(OFFSET_MILLIS)));
            if (offsetHeight >= 0)
            {
                pos.z = max(pos.z, o.z - max(offsetHeight - eyeheight, 0.0f));
            }
        }
        return pos;
    }

    vec updateposition(const int time)
    {
        if (flags & ProjFlag_Linear)
        {
            offsetMillis = max(offsetMillis - time, 0);
            dist = to.dist(o, dv);
            dv.mul(time / max(dist * 1000 / speed, float(time)));
            vec v = vec(o).add(dv);
            return v;
        }
        else
        {
            vec pos(o);
            pos.add(vec(offset).mul(offsetMillis / float(OFFSET_MILLIS)));
            return pos;
        }
    }
};
