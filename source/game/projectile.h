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
    Projectile_Melee,
    Projectile_Gib,
    Projectile_Debris,
    Projectile_Casing,
    Projectile_Casing2,
    Projectile_Casing3,
    Projectile_Tracer,
    Projectile_Max
};
inline bool isvalidprojectile(int type)
{ 
    return type >= 0 && type < Projectile_Max;
}

#include "weapon.h"

enum
{
    ProjFlag_Weapon       = 1 << 0,  // Related to a weapon.
    ProjFlag_Junk         = 1 << 1,  // Lightweight projectiles for cosmetic effects.
    ProjFlag_Bounce       = 1 << 2,  // Bounces off surfaces.
    ProjFlag_Linear       = 1 << 3,  // Follows a linear trajectory.
    ProjFlag_Track        = 1 << 4,  // Tracks specific body tags.
    ProjFlag_Impact       = 1 << 5,  // Detonates on collision with geometry or entities.
    ProjFlag_Explosive    = 1 << 6,  // Let me guess, it triggers an explosion?
    ProjFlag_Quench       = 1 << 7,  // Destroyed upon contact with water.
    ProjFlag_Eject        = 1 << 8,  // Can be ejected as a spent casing by weapons.
    ProjFlag_Loyal        = 1 << 9,  // Only responds to the weapon that fired it.
    ProjFlag_Invincible   = 1 << 10, // Cannot be destroyed.
    ProjFlag_AdjustSpeed  = 1 << 11, // Adjusts speed based on distance.
    ProjFlag_MultiHit	  = 1 << 12, // Hits multiple targets directly during lifetime.
    ProjFlag_DieWithOwner = 1 << 13  // Until death do us part?
};

static const struct projectileinfo
{
    int type, flags, attack, lifeTime;
    int bounceSound, loopSound, maxBounces, variants;
    float gravity, elasticity, speed;
    float maxSpeed, radius, fade;
    const char* directory;
}
projs[Projectile_Max] =
{
    {
        Projectile_Grenade,
        ProjFlag_Weapon | ProjFlag_Bounce | ProjFlag_Explosive,
        ATK_GRENADE2,
        1500,
        S_BOUNCE_ROCKET,
        S_INVALID,
        0,
        0,
        0.6f,
        0.8f,
        200,
        0,
        1.4f,
        0,
        "projectile/grenade",
    },
    {
        Projectile_Rocket,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Explosive | ProjFlag_Impact,
        ATK_ROCKET2,
        5000,
        S_INVALID,
        S_ROCKET_LOOP,
        0,
        0,
        0,
        0,
        300,
        0,
        1.0f,
        0,
        "projectile/rocket",
    },
    {
        Projectile_Pulse,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Explosive | ProjFlag_Invincible,
        ATK_INVALID,
        3000,
        S_INVALID,
        S_PULSE_LOOP,
        0,
        0,
        0,
        0,
        800,
        0,
        1.0f,
        50.0f,
        nullptr,
    },
    {
        Projectile_Plasma,
        ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Explosive | ProjFlag_Loyal,
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
        1000.0f,
        nullptr
    },
    {
        Projectile_Melee,
        ProjFlag_Weapon | ProjFlag_Track | ProjFlag_MultiHit | ProjFlag_Invincible | ProjFlag_DieWithOwner,
        ATK_INVALID,
        290,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0,
        0,
        0,
        0,
        1.3f,
        0,
        nullptr
    },
    {
        Projectile_Gib,
        ProjFlag_Junk | ProjFlag_Bounce,
        ATK_INVALID,
        0,
        S_INVALID,
        S_INVALID,
        2,
        5,
        0,
        0,
        0,
        0,
        2.5f,
        150.0f,
        "projectile/gib",
    },
    {
        Projectile_Debris,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Quench,
        ATK_INVALID,
        500,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0,
        0,
        0,
        0,
        1.8f,
        0,
        nullptr
    },
    {
        Projectile_Casing,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        2000,
        S_BOUNCE_EJECT1,
        S_INVALID,
        2,
        0,
        0,
        0,
        0,
        0,
        0.3f,
        0,
        "projectile/eject/00"
    },
    {
        Projectile_Casing2,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        2000,
        S_BOUNCE_EJECT2,
        S_INVALID,
        2,
        0,
        0,
        0,
        0,
        0,
        0.4f,
        0,
        "projectile/eject/01"
    },
    {
        Projectile_Casing3,
        ProjFlag_Junk | ProjFlag_Bounce | ProjFlag_Eject,
        ATK_INVALID,
        2000,
        S_BOUNCE_EJECT3,
        S_INVALID,
        2,
        0,
        0,
        0,
        0,
        0,
        0.5f,
        0,
        "projectile/eject/02"
    },
    {
        Projectile_Tracer,
        ProjFlag_Weapon | ProjFlag_Junk | ProjFlag_Linear | ProjFlag_AdjustSpeed,
        ATK_INVALID,
        500,
        S_INVALID,
        S_INVALID,
        0,
        0,
        0,
        0,
        1500,
        0.25f,
        0.4f,
        0,
        nullptr
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
    int id, attack, projectile, flags, lifetime, health, weight, trackType;
    int variant, bounces, offsetMillis, hitFlags;
    int millis, lastImpact, bounceSound, loopChannel, loopSound;
    float lastYaw, gravity, elasticity, offsetHeight, dist;
    bool isLocal;
    string model;
    vec offset, lastPosition, dv, from, to;
    gameent* owner;

    vector<dynent*> targets;

    ProjEnt() : variant(0), bounces(0), hitFlags(0), millis(0), lastImpact(0), bounceSound(-1), loopChannel(-1), loopSound(-1)
    {
        state = CS_ALIVE;
        type = ENT_PROJECTILE;
        collidetype = COLLIDE_ELLIPSE;
        roll = 0;
        model[0] = 0;
        offset = lastPosition = dv = from = to = vec(0, 0, 0);
        owner = nullptr;
        targets.setsize(0);
    }
    ~ProjEnt()
    {
        targets.shrink(0);
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

    void setSpeed(const projectileinfo& context)
    {
        float baseSpeed = context.speed;
        if (baseSpeed == 0)
        {
            baseSpeed = 20 + rnd(100);
        }
        if (flags & ProjFlag_AdjustSpeed)
        {
            const float maxSpeed = context.maxSpeed;
            const float distance = to.dist(from);
            this->speed = max(baseSpeed, distance / maxSpeed);
            return;
        }
        this->speed = baseSpeed;
    }

    void setBounce(const projectileinfo& context)
    {
        // Projectile gravity.
        float gravity = context.gravity;
        if (gravity == 0)
        {
            const float randomGravity = 0.3f + rndscale(0.8f);
            gravity = randomGravity;
        }
        this->gravity = gravity;
        
        // Projectile elasticity.
        float elasticity = context.elasticity;
        if (elasticity == 0)
        {
            const float randomElasticity = 0.4f + rndscale(0.6f);
            elasticity = randomElasticity;
        }
        this->elasticity = elasticity;
    }

    void setRadius(const projectileinfo& context)
    {
        this->radius = context.radius;
        xradius = yradius = eyeheight = aboveeye = this->radius;
    }
    
    void setLifeTime(const projectileinfo& context)
    {
        int lifeTime = context.lifeTime;
        if (lifeTime == 0)
        {
            const int randomLifeTime = 1000 + rnd(1000);
            lifeTime = randomLifeTime;
        }
        this->lifetime = lifeTime;
    }

    void setSounds(const projectileinfo& context)
    {
        const int bounceSound = context.bounceSound;
        if (validsound(bounceSound))
        {
            this->bounceSound = bounceSound;
        }
        const int loopSound = context.loopSound;
        if (validsound(loopSound))
        {
            this->loopSound = loopSound;
        }
    }

    // Set a 3D model for the projectile if necessary.
    void setModel(const projectileinfo& context)
    {
        if (context.directory == nullptr)
        {
            return;
        }
        const int variants = context.variants;
        if (variants > 0)
        {
            variant = rnd(variants);
            defformatstring(variantName, "%s/%02d", projs[projectile].directory, variant);
            copystring(model, variantName);
            return;
        }
        copystring(model, projs[projectile].directory);
    }

    void set(const int type)
    {
        projectile = type;
        const projectileinfo& projectileInfo = projs[type];
        this->flags = projectileInfo.flags;
        setSpeed(projectileInfo);
        if (this->flags & ProjFlag_Bounce)
        {
            setBounce(projectileInfo);
        }
        setLifeTime(projectileInfo);
        setRadius(projectileInfo);
        setSounds(projectileInfo);
        setModel(projectileInfo);
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

    void detach()
    {
        flags &= ~(ProjFlag_Track);
        flags |= ProjFlag_Bounce;
        resetinterp();
    }

    /*
        Check if target has been already hit by projectile (duplicate direct hits).
        If the target hasn't already been hit by this projectile, keep track.
        The target entity can be player, NPC and projectile actors.
    */
    bool registerTarget(dynent* target)
    {
        if (targets.find(target) < 0)
        {
            targets.add(target);
            return true;
        }
        return false;
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

    vec getOffset()
    {
        if (flags & ProjFlag_Bounce)
        {
            return offsetPosition();
        }
        return vec(offset).mul(offsetMillis / float(OFFSET_MILLIS)).add(o);
    }

    // Adjust projectile's model.
    vec manipulateModel()
    {
        if (!(flags & ProjFlag_Bounce))
        {
            const float dist = min(o.dist(to) / 32.0f, 1.0f);
            const vec pos = vec(o).add(vec(offset).mul(dist * offsetMillis / float(OFFSET_MILLIS)));
            
            // The amount of distance in front of the smoke trail needs to change if the model does.
            vec velocity = dist < 1e-6f ? vel : vec(to).sub(pos).normalize();
            vectoyawpitch(velocity, yaw, pitch);
            velocity.mul(3);
            velocity.add(pos);

            return velocity;
        }
        return offsetPosition();
    }
};
