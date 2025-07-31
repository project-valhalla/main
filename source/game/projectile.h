#include "game.h"

struct gameent;

static const int OFFSET_MILLIS = 500;

enum
{
    Projectile_Invalid = -1,
    Projectile_Grenade,
    Projectile_Grenade2,
    Projectile_Rocket,
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

#include "weapon.h"

enum
{
    ProjFlag_Weapon      = 1 << 0, // Related to a weapon.
    ProjFlag_Junk        = 1 << 1, // Lightweight projectiles for cosmetic effects.
    ProjFlag_Bounce      = 1 << 2, // Bounces off surfaces.
    ProjFlag_Linear      = 1 << 3, // Follows a linear trajectory.
    ProjFlag_Impact      = 1 << 4, // Detonates on collision with geometry or entities.
    ProjFlag_Quench      = 1 << 5, // Destroyed upon contact with water.
    ProjFlag_Eject       = 1 << 6, // Can be ejected as a spent casing by weapons.
    ProjFlag_Loyal       = 1 << 7, // Only responds to the weapon that fired it.
    ProjFlag_Invincible  = 1 << 8, // Cannot be destroyed.
    ProjFlag_AdjustSpeed = 1 << 9  // Adjusts speed based on distance.
};

static const struct projectileinfo
{
    int type, flags, attack;
    int bounceSound, loopsound, maxbounces, variants;
    float maxSpeed, radius;
    const char* directory;
}
projs[Projectile_Max] =
{
    {
        Projectile_Grenade,
        ProjFlag_Weapon | ProjFlag_Bounce,
        ATK_GRENADE3,
        S_BOUNCE_GRENADE,
        S_INVALID,
        0,
        0,
        0,
        1.4f,
        "projectile/grenade",
    },
    {
        Projectile_Grenade2,
        ProjFlag_Weapon | ProjFlag_Bounce | ProjFlag_Impact,
        ATK_GRENADE3,
        S_BOUNCE_GRENADE,
        S_INVALID,
        0,
        0,
        0,
        1.4f,
        "projectile/grenade",
    },
    {
        Projectile_Rocket,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Impact,
        ATK_ROCKET2,
        S_INVALID,
        S_ROCKET_LOOP,
        0,
        0,
        0,
        1.4f,
        "projectile/rocket",
    },
    {
        Projectile_Pulse,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Invincible,
        ATK_INVALID,
        S_INVALID,
        S_PULSE_LOOP,
        0,
        0,
        0,
        1.0f,
        NULL,
    },
    {
        Projectile_Plasma,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Loyal,
        ATK_PISTOL3,
        S_INVALID,
        S_PISTOL_LOOP,
        0,
        0,
        0,
        5.0f,
        NULL
    },
    {
        Projectile_Gib,
        ProjFlag_Junk | ProjFlag_Bounce,
        ATK_INVALID,
        S_INVALID,
        S_INVALID,
        2,
        5,
        0,
        1.5f,
        "projectile/gib",
    },
    {
        Projectile_Debris,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Quench,
        ATK_INVALID,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0,
        1.8f,
        NULL
    },
    {
        Projectile_Casing,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        S_BOUNCE_EJECT1,
        S_INVALID,
        2,
        0,
        0,
        0.3f,
        "projectile/eject/00"
    },
    {
        Projectile_Casing2,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        S_BOUNCE_EJECT2,
        S_INVALID,
        2,
        0,
        0,
        0.4f,
        "projectile/eject/01"
    },
    {
        Projectile_Casing3,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        S_BOUNCE_EJECT3,
        S_INVALID,
        2,
        0,
        0,
        0.5f,
        "projectile/eject/02"
    },
    {
        Projectile_Bullet,
        ProjFlag_Weapon | ProjFlag_Junk | ProjFlag_Linear | ProjFlag_AdjustSpeed,
        ATK_INVALID,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0.25f,
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
    int id, attack, projectile, flags, lifetime, health, weight;
    int variant, bounces, offsetMillis;
    int millis, lastBounce, bounceSound, loopChannel, loopSound, hitFlags;
    float lastYaw, gravity, elasticity, offsetHeight, dist;
    bool isLocal;
    string model;
    vec offset, lastPosition, dv, from, to;
    gameent* owner;

    ProjEnt() : variant(0), bounces(0), millis(0), lastBounce(0), bounceSound(-1), loopChannel(-1), loopSound(-1), hitFlags(0)
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

    void setSpeed(const float baseSpeed)
    {
        const float maxSpeed = projs[projectile].maxSpeed;
        if (maxSpeed > 0.0f && flags & ProjFlag_AdjustSpeed)
        {
            const float distance = to.dist(from);
            speed = max(baseSpeed, distance / maxSpeed);
        }
        else
        {
            speed = baseSpeed;
        }
    }

    void checkliquid()
    {
        const int material = lookupmaterial(o);
        const bool isinwater = isliquidmaterial(material & MATF_VOLUME);
        inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;
    }

    void kill(const bool isDestroyed = false)
    {
        if (!isLocal)
        {
            return;
        }
        if (isDestroyed)
        {
            hitFlags |= Hit_Projectile;
        }
        state = CS_DEAD;
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
