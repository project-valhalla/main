#include "game.h"

struct gameent;

static const int OFFSET_MILLIS = 500;

enum
{
    Projectile_Invalid = -1,
    Projectile_Grenade,
    Projectile_Rocket,
    Projectile_Pulse,
    Projectile_Plasma,
    Projectile_Gib,
    Projectile_Debris,
    Projectile_Casing,
    Projectile_Casing2,
    Projectile_Casing3,
    Projectile_Bullet,
    Projectile_Item,
    Projectile_Max
};
inline bool isvalidprojectile(int type)
{ 
    return type > Projectile_Invalid && type < Projectile_Max;
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
    ProjFlag_Eject       = 1 << 6, // Ejected as a spent casing by weapons.
    ProjFlag_Loyal       = 1 << 7, // Only responds to the weapon that fired it.
    ProjFlag_Invincible  = 1 << 8, // Cannot be destroyed.
    ProjFlag_AdjustSpeed = 1 << 9, // Adjusts speed based on distance.
    ProjFlag_Item        = 1 << 10 // Pickable projectile.
};

static const struct projectileinfo
{
    const int type, flags, attack, lifespan;
    const int bounceSound, loopSound, maxBounces, variants;
    const float gravity, elasticity, speed, maxSpeed, radius;
    const char* directory;
}
projs[Projectile_Max] =
{
    {
        Projectile_Grenade,
        ProjFlag_Weapon | ProjFlag_Bounce,
        ATK_GRENADE2,
        1500,
        S_BOUNCE_ROCKET,
        S_INVALID,
        0,
        0,
        0.6f,
        0.9f,
        200.0f,
        0,
        1.4f,
        "projectile/grenade",
    },
    {
        Projectile_Rocket,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Impact,
        ATK_ROCKET2,
        5000,
        S_INVALID,
        S_ROCKET_LOOP,
        0,
        0,
        0,
        0,
        300.0f,
        0,
        1.4f,
        "projectile/rocket",
    },
    {
        Projectile_Pulse,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Invincible,
        ATK_INVALID,
        5000,
        S_INVALID,
        S_PULSE_LOOP,
        0,
        0,
        0,
        0,
        1000.0f,
        0,
        1.0f,
        NULL,
    },
    {
        Projectile_Plasma,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Loyal,
        ATK_PISTOL3,
        2000,
        S_INVALID,
        S_PISTOL_LOOP,
        0,
        0,
        0,
        0,
        400,
        0,
        5.0f,
        NULL
    },
    {
        Projectile_Gib,
        ProjFlag_Junk | ProjFlag_Bounce,
        ATK_INVALID,
        0, // Randomise lifespan.
        S_INVALID,
        S_INVALID,
        2,
        5,
        0, // Randomise gravity, if the projectile has a bounce flag.
        0.6f,
        0, // Randomise speed.
        0,
        1.5f,
        "projectile/gib",
    },
    {
        Projectile_Debris,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Quench,
        ATK_INVALID,
        450,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0, // Randomise gravity, if the projectile has a bounce flag.
        0.6f,
        0, // Randomise speed.
        0,
        1.8f,
        NULL
    },
    {
        Projectile_Casing,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        0, // Randomise lifespan.
        S_BOUNCE_EJECT1,
        S_INVALID,
        2,
        0,
        0, // Randomise gravity.
        0.4f,
        0, // Randomise speed.
        0,
        0.3f,
        "projectile/eject/00"
    },
    {
        Projectile_Casing2,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        0, // Randomise lifespan.
        S_BOUNCE_EJECT2,
        S_INVALID,
        2,
        0,
        0, // Randomise gravity.
        0.3f,
        0, // Randomise speed.
        0,
        0.4f,
        "projectile/eject/01"
    },
    {
        Projectile_Casing3,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        0, // Randomise lifespan.
        ATK_INVALID,
        S_BOUNCE_EJECT3,
        S_INVALID,
        2,
        0,
        0, // Randomise gravity.
        0.4f,
        0, // Randomise speed.
        0,
        0.5f,
        "projectile/eject/02"
    },
    {
        Projectile_Bullet,
        ProjFlag_Weapon | ProjFlag_Junk | ProjFlag_Linear | ProjFlag_AdjustSpeed,
        ATK_INVALID,
        500,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0,
        0,
        2000.0f,
        0.25f,
        0.4f,
        NULL
    },
    {
        Projectile_Item,
        ProjFlag_Bounce | ProjFlag_Item,
        ATK_INVALID,
        8000,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0.8f,
        0.4f,
        100.0f,
        0,
        1.5f,
        "item/trigger"
    }
};
inline bool isattackprojectile(const int projectile)
{ 
    return isvalidprojectile(projectile) && projs[projectile].flags & ProjFlag_Weapon && !(projs[projectile].flags & ProjFlag_Junk);
}

struct ProjEnt : dynent
{
    int id, projectile, flags, lifespan, health, weight;
    int variant, bounces, offsetMillis;
    int millis, lastBounce, bounceSound, loopChannel, loopSound;

    // Type-specific data.
    int attack, hitFlags, item;

    float lastYaw, gravity, elasticity, offsetHeight, dist;
    bool isLocal, isActive;
    string model;
    vec offset, lastPosition, dv, from, to;
    gameent* owner;

    ProjEnt() : variant(0), bounces(0), millis(0), lastBounce(0), bounceSound(-1), loopChannel(-1), loopSound(-1), attack(ATK_INVALID), hitFlags(0), item(ET_EMPTY)
    {
        state = CS_ALIVE;
        type = ENT_PROJECTILE;
        collidetype = COLLIDE_ELLIPSE;
        roll = 0;
        model[0] = 0;
        offset = lastPosition = dv = from = to = vec(0, 0, 0);
        owner = NULL;
        isActive = true;
    }
    ~ProjEnt()
    {
        if (loopChannel >= 0)
        {
            stopsound(loopSound, loopChannel);
        }
        loopSound = loopChannel = -1;
    }

    void limitOffset()
    {
        if (flags & ProjFlag_Weapon && offsetMillis > 0 && offset.z < 0)
        {
            offsetHeight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
        }
        else offsetHeight = -1;
    }

    void set(const int type)
    {
        // Essential attributes such as type and flags, used to set the projectile.
        projectile = type;
        this->flags = projs[projectile].flags;

        // Set the lifespan.
        const int lifespan = projs[projectile].lifespan;
        if (lifespan > 0)
        {
            this->lifespan = lifespan;
        }
        else
        {
            // Randomise lifespan.
            this->lifespan = rnd(1000) + 1000;
        }

        // Set the gravity.
        const float gravity = projs[projectile].gravity;
        if (gravity > 0.0f)
        {
            this->gravity = gravity;
        }
        else if (flags & ProjFlag_Bounce)
        {
            // Randomise gravity if the projectile has bounce properties.
            this->gravity = 0.3f + rndscale(0.8f);
        }

        // Set the elasticity if necessary.
        elasticity = projs[projectile].elasticity;

        // Set the speed.
        const float baseSpeed = projs[projectile].speed;
        const float maxSpeed = projs[projectile].maxSpeed;
        if (maxSpeed > 0.0f && flags & ProjFlag_AdjustSpeed)
        {
            const float distance = to.dist(from);
            this->speed = max(baseSpeed, distance / maxSpeed);
        }
        else
        {
            if (baseSpeed > 0.0f)
            {
                this->speed = baseSpeed;
            }
            else
            {
                // Randomise speed.
                this->speed = rnd(100) + 20;
            }
        }

        // Set sounds.
        const int bounceSound = projs[projectile].bounceSound;
        if (validsound(bounceSound))
        {
            this->bounceSound = bounceSound;
        }
        const int loopsound = projs[projectile].loopSound;
        if (validsound(loopsound))
        {
            this->loopSound = loopsound;
        }
    }

    void setRadius()
    {
        const float radius = projs[projectile].radius;
        this->radius = radius;
        xradius = yradius = eyeheight = aboveeye = this->radius;
    }

    void setVariant()
    {
        const int variants = projs[projectile].variants;
        if (variants > 0)
        {
            variant = rnd(variants);
        }
    }

    void checkLiquid()
    {
        const int material = lookupmaterial(o);
        const bool isInWater = isliquidmaterial(material & MATF_VOLUME);
        inwater = isInWater ? material & MATF_VOLUME : MAT_AIR;
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

    vec offsetPosition()
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

    vec updatePosition(const int time)
    {
        if (flags & ProjFlag_Linear)
        {
            // Update the position of linear projectiles like rockets.
            offsetMillis = max(offsetMillis - time, 0);
            dist = to.dist(o, dv);
            dv.mul(time / max(dist * 1000 / speed, float(time)));
            vec v = vec(o).add(dv);
            return v;
        }
        else if (flags & ProjFlag_Bounce)
        {
            // Update the position of bouncing projectiles.
            vec pos(o);
            pos.add(vec(offset).mul(offsetMillis / float(OFFSET_MILLIS)));
            return pos;
        }
    }
};
