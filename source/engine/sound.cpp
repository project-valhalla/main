// sound.cpp: basic positional sound using sdl_mixer

#include "engine.h"
#include "SDL_mixer.h"

bool nosound = true;

struct soundsample
{
    char *name;
    Mix_Chunk *chunk;

    soundsample() : name(NULL), chunk(NULL) {}
    ~soundsample() { DELETEA(name); }

    void cleanup() { if(chunk) { Mix_FreeChunk(chunk); chunk = NULL; } }
    bool load(const char *dir, bool msg = false);
};

struct soundslot
{
    soundsample *sample;
    int volume;
};

struct soundconfig
{
    int slots, numslots;
    int maxuses;

    bool hasslot(const soundslot *p, const vector<soundslot> &v) const
    {
        return p >= v.getbuf() + slots && p < v.getbuf() + slots+numslots && slots+numslots < v.length();
    }

    int chooseslot(int flags) const
    {
        if(flags&SND_NO_ALT || numslots <= 1) return slots;
        if(flags&SND_USE_ALT) return slots + 1 + rnd(numslots - 1);
        return slots + rnd(numslots);
    }
};

struct soundchannel
{
    int id;
    bool inuse;
    vec loc;
    soundslot *slot;
    physent *owner;
    extentity *ent;
    int radius, volume, pan, flags;
    bool dirty, allowstereo;

    soundchannel(int id) : id(id) { reset(); }

    bool hasloc() const { return loc.x >= -1e15f; }
    void clearloc() { loc = vec(-1e16f, -1e16f, -1e16f); }

    void reset()
    {
        inuse = false;
        clearloc();
        slot = NULL;
        ent = NULL;
        owner = NULL;
        radius = 0;
        volume = -1;
        pan = -1;
        flags = 0;
        dirty = false;
        allowstereo = true;
    }
};
vector<soundchannel> channels;
int maxchannels = 0;

soundchannel &newchannel(int n, soundslot *slot, physent *owner = NULL, const vec *loc = NULL, extentity *ent = NULL, int flags = 0, int radius = 0)
{
    if(ent)
    {
        loc = &ent->o;
        ent->flags |= EF_SOUND;
    }
    while(!channels.inrange(n)) channels.add(channels.length());
    soundchannel &chan = channels[n];
    chan.reset();
    chan.inuse = true;
    if(owner) chan.owner = owner;
    else if(loc) chan.loc = *loc;
    chan.slot = slot;
    chan.ent = ent;
    chan.flags = flags;
    chan.radius = radius;
    return chan;
}

void freechannel(int n)
{
    if(!channels.inrange(n) || !channels[n].inuse) return;
    soundchannel &chan = channels[n];
    chan.inuse = false;
    if(chan.ent) chan.ent->flags &= ~EF_SOUND;
}

void syncchannel(soundchannel &chan)
{
    if(!chan.dirty) return;
    if(!Mix_FadingChannel(chan.id)) Mix_Volume(chan.id, chan.volume);
    Mix_SetPanning(chan.id, 255-chan.pan, chan.pan);
    chan.dirty = false;
}

void stopchannels(int flags = 0)
{
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(!chan.inuse || (flags && !(chan.flags&flags))) continue;
        Mix_HaltChannel(i);
        freechannel(i);
    }
}

extern int musicvol;
void setmusicvol(int musicvol);
VARFP(mastervol, 0, 255, 255,
{
    if(!mastervol)
    {
        stopchannels();
        setmusicvol(0);
    }
    else setmusicvol(musicvol);
});
VARFP(gamevol, 0, 200, 255,
{
    if(!gamevol) stopchannels(SND_GAME|SND_MAP);
});
VARFP(announcervol, 0, 250, 255,
{ 
    if(!announcervol) stopchannels(SND_ANNOUNCER);
});
VARFP(uivol, 0, 180, 255,
{ 
    if(!uivol) stopchannels(SND_UI);
});
VARFP(musicvol, 0, 35, 255,
{
    setmusicvol(mastervol ? musicvol : 0);
});

VARFP(ambientsounds, 0, 1, 1,
{
    if (!ambientsounds) stopmapsounds();
});

char *musicfile = NULL, *musicdonecmd = NULL;

Mix_Music *music = NULL;
SDL_RWops *musicrw = NULL;
stream *musicstream = NULL;

void setmusicvol(int musicvol)
{
    if(nosound) return;
    if(music) Mix_VolumeMusic((musicvol * mastervol * MIX_MAX_VOLUME) / (MAX_VOLUME * MAX_VOLUME));
}

void stopmusic(int fade = 0)
{
    if(nosound) return;
    DELETEA(musicfile);
    DELETEA(musicdonecmd);
    if(music && (!fade || !Mix_FadeOutMusic(fade)))
    {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = NULL;
    }
    if(musicrw) { SDL_FreeRW(musicrw); musicrw = NULL; }
    DELETEP(musicstream);
}

#ifdef WIN32
#define AUDIODRIVER "directsound winmm"
#else
#define AUDIODRIVER ""
#endif
bool shouldinitaudio = true;
SVARF(audiodriver, AUDIODRIVER, { shouldinitaudio = true; initwarning("sound configuration", INIT_RESET, CHANGE_SOUND); });
VARF(sound, 0, 1, 1, { shouldinitaudio = true; initwarning("sound configuration", INIT_RESET, CHANGE_SOUND); });
VARF(soundchans, 1, 64, 128, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(soundfreq, 0, 44100, 48000, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(soundbufferlen, 128, 1024, 4096, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

bool initaudio()
{
    static string fallback = "";
    static bool initfallback = true;
    static bool restorefallback = false;
    if(initfallback)
    {
        initfallback = false;
        if(char *env = SDL_getenv("SDL_AUDIODRIVER")) copystring(fallback, env);
    }
    if(!fallback[0] && audiodriver[0])
    {
        vector<char*> drivers;
        explodelist(audiodriver, drivers);
        loopv(drivers)
        {
            restorefallback = true;
            SDL_setenv("SDL_AUDIODRIVER", drivers[i], 1);
            if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0)
            {
                drivers.deletearrays();
                return true;
            }
        }
        drivers.deletearrays();
    }
    if(restorefallback)
    {
        restorefallback = false;
    #ifdef WIN32
        SDL_setenv("SDL_AUDIODRIVER", fallback, 1);
    #else
        unsetenv("SDL_AUDIODRIVER");
    #endif
    }
    if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0) return true;
    conoutf(CON_ERROR, "sound init failed: %s", SDL_GetError());
    return false;
}

void initsound()
{
    SDL_version version;
    SDL_GetVersion(&version);
    if(version.major == 2 && version.minor == 0 && version.patch == 6)
    {
        nosound = true;
        if(sound) conoutf(CON_ERROR, "audio is broken in SDL 2.0.6");
        return;
    }

    if(shouldinitaudio)
    {
        shouldinitaudio = false;
        if(SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
        if(!sound || !initaudio())
        {
            nosound = true;
            return;
        }
    }

    if(Mix_OpenAudio(soundfreq, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
    {
        nosound = true;
        conoutf(CON_ERROR, "sound init failed (SDL_mixer): %s", Mix_GetError());
        return;
    }
    Mix_AllocateChannels(soundchans);
    maxchannels = soundchans;
    nosound = false;
}

void musicdone()
{
    if(music) { Mix_HaltMusic(); Mix_FreeMusic(music); music = NULL; }
    if(musicrw) { SDL_FreeRW(musicrw); musicrw = NULL; }
    DELETEP(musicstream);
    DELETEA(musicfile);
    if(!musicdonecmd) return;
    char *cmd = musicdonecmd;
    musicdonecmd = NULL;
    execute(cmd);
    delete[] cmd;
}

Mix_Music *loadmusic(const char *name)
{
    if(!musicstream) musicstream = openzipfile(name, "rb");
    if(musicstream)
    {
        if(!musicrw) musicrw = musicstream->rwops();
        if(!musicrw) DELETEP(musicstream);
    }
    if(musicrw) music = Mix_LoadMUSType_RW(musicrw, MUS_NONE, 0);
    else music = Mix_LoadMUS(findfile(name, "rb"));
    if(!music)
    {
        if(musicrw) { SDL_FreeRW(musicrw); musicrw = NULL; }
        DELETEP(musicstream);
    }
    return music;
}

void playmusic(const char *name, int *fade, const char *cmd)
{
    if(nosound) return;
    stopmusic(*name ? 0 : *fade);
    if(!musicvol || !mastervol || !*name) return;
    string file;
    static const char * const exts[] = { "", ".wav", ".ogg" };
    loopk(sizeof(exts)/sizeof(exts[0]))
    {
        formatstring(file, "data/audio/music/%s%s", name, exts[k]);
        if(loadmusic(file))
        {
            DELETEA(musicfile);
            DELETEA(musicdonecmd);
            musicfile = newstring(name);
            if(cmd && *cmd) musicdonecmd = newstring(cmd);
            if(*fade) Mix_FadeInMusic(music, cmd && *cmd ? 0 : -1, *fade);
            else Mix_PlayMusic(music, cmd && *cmd ? 0 : -1);
            Mix_VolumeMusic((musicvol * mastervol * MIX_MAX_VOLUME) / (MAX_VOLUME * MAX_VOLUME));
            intret(1);
            break;
        }
    }
    if(!music)
    {
        conoutf(CON_ERROR, "could not play music file: %s", name);
        intret(0);
    }
}
COMMANDN(music, playmusic, "sis");

static Mix_Chunk *loadwav(const char *name)
{
    Mix_Chunk *c = NULL;
    stream *z = openzipfile(name, "rb");
    if(z)
    {
        SDL_RWops *rw = z->rwops();
        if(rw)
        {
            c = Mix_LoadWAV_RW(rw, 0);
            SDL_FreeRW(rw);
        }
        delete z;
    }
    if(!c) c = Mix_LoadWAV(findfile(name, "rb"));
    return c;
}

bool soundsample::load(const char *dir, bool msg)
{
    if(chunk) return true;
    if(!name[0]) return false;

    static const char * const exts[] = { "", ".wav", ".ogg" };
    string filename;
    loopi(sizeof(exts)/sizeof(exts[0]))
    {
        formatstring(filename, "data/audio/%s%s%s", dir, name, exts[i]);
        if(msg && !i) renderprogress(0, filename);
        path(filename);
        chunk = loadwav(filename);
        if(chunk) return true;
    }

    conoutf(CON_ERROR, "failed to load sample: data/audio/%s%s", dir, name);
    return false;
}

static struct soundtype
{
    hashnameset<soundsample> samples;
    vector<soundslot> slots;
    vector<soundconfig> configs;
    const char *dir;

    soundtype(const char *dir) : dir(dir) {}

    int findsound(const char *name, int vol)
    {
        loopv(configs)
        {
            soundconfig &s = configs[i];
            loopj(s.numslots)
            {
                soundslot &c = slots[s.slots+j];
                if(!strcmp(c.sample->name, name) && (!vol || c.volume==vol)) return i;
            }
        }
        return -1;
    }

    int addslot(const char *name, int vol)
    {
        soundsample *s = samples.access(name);
        if(!s)
        {
            char *n = newstring(name);
            s = &samples[n];
            s->name = n;
            s->chunk = NULL;
        }
        soundslot *oldslots = slots.getbuf();
        int oldlen = slots.length();
        soundslot &slot = slots.add();
        // soundslots.add() may relocate slot pointers
        if(slots.getbuf() != oldslots) loopv(channels)
        {
            soundchannel &chan = channels[i];
            if(chan.inuse && chan.slot >= oldslots && chan.slot < &oldslots[oldlen])
                chan.slot = &slots[chan.slot - oldslots];
        }
        slot.sample = s;
        slot.volume = vol ? vol : 100;
        return oldlen;
    }

    int addsound(const char *name, int vol, int maxuses = 0)
    {
        soundconfig &s = configs.add();
        s.slots = addslot(name, vol);
        s.numslots = 1;
        s.maxuses = maxuses;
        return configs.length()-1;
    }

    void addalt(const char *name, int vol)
    {
        if(configs.empty()) return;
        addslot(name, vol);
        configs.last().numslots++;
    }

    void clear()
    {
        slots.setsize(0);
        configs.setsize(0);
    }

    void reset()
    {
        loopv(channels)
        {
            soundchannel &chan = channels[i];
            if(chan.inuse && slots.inbuf(chan.slot))
            {
                Mix_HaltChannel(i);
                freechannel(i);
            }
        }
        clear();
    }

    void cleanupsamples()
    {
        enumerate(samples, soundsample, s, s.cleanup());
    }

    void cleanup()
    {
        cleanupsamples();
        slots.setsize(0);
        configs.setsize(0);
        samples.clear();
    }

    void preloadsound(int n)
    {
        if(nosound || !configs.inrange(n)) return;
        soundconfig &config = configs[n];
        loopk(config.numslots) slots[config.slots+k].sample->load(dir, true);
    }

    bool playing(const soundchannel &chan, const soundconfig &config) const
    {
        return chan.inuse && config.hasslot(chan.slot, slots);
    }
} gamesounds(""), mapsounds("ambient/");

void registersound(char *name, int *vol) { intret(gamesounds.addsound(name, *vol, 0)); }
COMMAND(registersound, "si");

void mapsound(char *name, int *vol, int *maxuses) { intret(mapsounds.addsound(name, *vol, *maxuses < 0 ? 0 : max(1, *maxuses))); }
COMMAND(mapsound, "sii");

void altsound(char *name, int *vol) { gamesounds.addalt(name, *vol); }
COMMAND(altsound, "si");

void altmapsound(char *name, int *vol) { mapsounds.addalt(name, *vol); }
COMMAND(altmapsound, "si");

ICOMMAND(numsounds, "", (), intret(gamesounds.configs.length()));
ICOMMAND(nummapsounds, "", (), intret(mapsounds.configs.length()));

void soundreset()
{
    gamesounds.reset();
}
COMMAND(soundreset, "");

void mapsoundreset()
{
    mapsounds.reset();
}
COMMAND(mapsoundreset, "");

void resetchannels()
{
    loopv(channels) if(channels[i].inuse) freechannel(i);
    channels.shrink(0);
}

void clear_sound()
{
    closemumble();
    if(nosound) return;
    stopmusic();

    gamesounds.cleanup();
    mapsounds.cleanup();
    Mix_CloseAudio();
    resetchannels();
}

void stopmapsounds()
{
    loopv(channels) if(channels[i].inuse && channels[i].ent)
    {
        Mix_HaltChannel(i);
        freechannel(i);
    }
}

void clearmapsounds()
{
    stopmapsounds();
    mapsounds.clear();
}

void stopmapsound(extentity *e)
{
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(chan.inuse && chan.ent == e)
        {
            Mix_HaltChannel(i);
            freechannel(i);
        }
    }
}

void checkmapsounds()
{
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type!=ET_SOUND) continue;
        if(camera1->o.dist(e.o) < e.attr2 && e.isactive()) // if inside entity radius (attr2)
        {
            if(!(e.flags&EF_SOUND))
            {
                if(!e.attr4) playsound(e.attr1, NULL, NULL, &e, SND_MAP, -1);
                else if(totalmillis-e.lasttrigger >= e.attr4 * 1000) // delay map sound (attr4)
                {
                    playsound(e.attr1, NULL, NULL, &e, SND_MAP);
                    e.lasttrigger = totalmillis;
                }
            }
        }
        else if(e.flags & EF_SOUND || !e.isactive())
        {
            stopmapsound(&e);
        }
    }
}

VAR(stereo, 0, 1, 1);

bool updatechannel(soundchannel &chan)
{
    if(!chan.slot) return false;
    float volf = 1.0f, panf = 0.5f;
    if(chan.owner) chan.loc = chan.owner->o;
    if(chan.hasloc())
    {
        vec v;
        float dist = chan.loc.dist(camera1->o, v);
        int rad = 0;
        if(chan.ent)
        {
            rad = chan.ent->attr2;
            if(chan.ent->attr3)
            {
                rad -= chan.ent->attr3;
                dist -= chan.ent->attr3;
            }
        }
        else if(chan.radius > 0) rad = chan.radius;
        if(rad > 0) volf -= clamp(dist/rad, 0.0f, 1.0f); // simple mono distance attenuation
        if(stereo && chan.allowstereo && (v.x != 0 || v.y != 0) && dist>0)
        {
            v.rotate_around_z(-camera1->yaw*RAD);
            panf = 0.5f - 0.5f*v.x/v.magnitude2(); // range is from 0 (left) to 1 (right)
        }
    }
    const int volumetype = chan.flags&SND_ANNOUNCER ? announcervol : (chan.flags&SND_UI ? uivol : gamevol);
    const int volume = volumetype * chan.slot->volume * mastervol;
    const int volumeclamped = clamp(int(volf * volume * (MIX_MAX_VOLUME / powf(MAX_VOLUME, 3)) + 0.5f), 0, MIX_MAX_VOLUME);
    const int pan = clamp(int(panf*255.9f), 0, 255);
    if(volumeclamped == chan.volume && pan == chan.pan) return false;
    chan.volume = volumeclamped;
    chan.pan = pan;
    chan.dirty = true;
    return true;
}

void reclaimchannels()
{
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(chan.inuse && !Mix_Playing(i)) freechannel(i);
    }
}

void syncchannels()
{
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(chan.inuse && chan.hasloc() && updatechannel(chan)) syncchannel(chan);
    }
}

VARP(minimizedsounds, 0, 0, 1);

void updatesounds()
{
    updatemumble();
    if(nosound) return;
    if(minimized && !minimizedsounds) stopsounds();
    else
    {
        reclaimchannels();
        const int material = lookupmaterial(camera1->o);
        const bool inwater = player->state != CS_EDITING && isliquidmaterial(material & MATF_VOLUME);
        const bool firstpersondeath = player->state == CS_DEAD && game::camera::isfirstpersondeath();
        if(mainmenu || inwater || firstpersondeath)
        {
            stopmapsounds();
        }
        else if(ambientsounds) checkmapsounds();
        syncchannels();
    }
    if(music)
    {
        if(!Mix_PlayingMusic()) musicdone();
        else if(Mix_PausedMusic()) Mix_ResumeMusic();
    }
}

VARP(maxsoundsatonce, 0, 0, 100);

VAR(dbgsound, 0, 0, 1);

void preloadsound(int n)
{
    gamesounds.preloadsound(n);
}

void preloadmapsound(int n)
{
    mapsounds.preloadsound(n);
}

void preloadmapsounds()
{
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type==ET_SOUND) mapsounds.preloadsound(e.attr1);
    }
}

bool ischannelinuse(int flags)
{
    loopv(channels)
    {
        if (channels[i].flags & flags && channels[i].inuse)
        {
            return true;
        }
    }
    return false;
}

bool hasvolume(int flags)
{
    if(flags & SND_GAME && !gamevol) return false;
    if(flags & SND_MAP && !ambientsounds) return false;
    if(flags & SND_ANNOUNCER && !announcervol) return false;
    if(flags & SND_UI && !uivol) return false;
    return true;
}

int playsound(int n, physent *owner, const vec *loc, extentity *ent, int flags, int loops, int fade, int chanid, int radius, int expire)
{
    if(nosound || !mastervol || (minimized && !minimizedsounds) || !hasvolume(flags)) return -1;

    soundtype &sounds = ent || flags&SND_MAP ? mapsounds : gamesounds;
    if(!sounds.configs.inrange(n)) { conoutf(CON_WARN, "unregistered sound: %d", n); return -1; }
    soundconfig &config = sounds.configs[n];

    if(owner || loc)
    {
        // cull sounds that are unlikely to be heard
        vec location = owner? owner->o: *loc;
        int maxrad = game::maxsoundradius(n);
        if(radius <= 0 || maxrad < radius) radius = maxrad;
        if(camera1->o.dist(location) > 1.5f*radius)
        {
            if(channels.inrange(chanid) && sounds.playing(channels[chanid], config))
            {
                Mix_HaltChannel(chanid);
                freechannel(chanid);
            }
            return -1;
        }
    }

    if(chanid < 0)
    {
        if(config.maxuses)
        {
            int uses = 0;
            loopv(channels) if(sounds.playing(channels[i], config) && ++uses >= config.maxuses) return -1;
        }

        if(owner || loc) // avoid bursts of sounds with heavy packetloss
        {
            static int soundsatonce = 0, lastsoundmillis = 0;
            if(totalmillis == lastsoundmillis) soundsatonce++; else soundsatonce = 1;
            lastsoundmillis = totalmillis;
            if(maxsoundsatonce && soundsatonce > maxsoundsatonce) return -1;
        }
    }

    if(channels.inrange(chanid))
    {
        soundchannel &chan = channels[chanid];
        if(sounds.playing(chan, config))
        {
            if(loc) chan.loc = *loc;
            else if(chan.hasloc()) chan.clearloc();
            return chanid;
        }
    }
    if(fade < 0) return -1;

    soundslot &slot = sounds.slots[config.chooseslot(flags)];
    if(!slot.sample->chunk && !slot.sample->load(sounds.dir)) return -1;

    if(dbgsound) conoutf(CON_DEBUG, "sound: %s%s", sounds.dir, slot.sample->name);

    chanid = -1;
    loopv(channels) if(!channels[i].inuse) { chanid = i; break; }
    if(chanid < 0 && channels.length() < maxchannels) chanid = channels.length();
    if(chanid < 0) loopv(channels) if(!channels[i].volume) { Mix_HaltChannel(i); freechannel(i); chanid = i; break; }
    if(chanid < 0) return -1;

    soundchannel &chan = newchannel(chanid, &slot, owner, loc, ent, flags, radius);

    if(!owner && !loc)
    {
        /* If a sound has no owner or location, then disable stereo for the current channel,
         * as we want announcer sounds and noises with no source to sound more "in your face" as possible.
         */
        chan.allowstereo = false;
    }
    updatechannel(chan);
    int playing = -1;
    if(fade)
    {
        Mix_Volume(chanid, chan.volume);
        playing = expire >= 0 ? Mix_FadeInChannelTimed(chanid, slot.sample->chunk, loops, fade, expire) : Mix_FadeInChannel(chanid, slot.sample->chunk, loops, fade);
    }
    else playing = expire >= 0 ? Mix_PlayChannelTimed(chanid, slot.sample->chunk, loops, expire) : Mix_PlayChannel(chanid, slot.sample->chunk, loops);
    if(playing >= 0) syncchannel(chan);
    else freechannel(chanid);
    return playing;
}

void stopsounds(int exclude)
{
    loopv(channels) if(channels[i].inuse)
    {
        if(channels[i].flags & exclude) continue;
        Mix_HaltChannel(i);
        freechannel(i);
    }
}

bool stopsound(int n, int chanid, int fade)
{
    if(!gamesounds.configs.inrange(n) || !channels.inrange(chanid) || !gamesounds.playing(channels[chanid], gamesounds.configs[n])) return false;
    if(dbgsound) conoutf(CON_DEBUG, "stopsound: %s%s", gamesounds.dir, channels[chanid].slot->sample->name);
    if(!fade || !Mix_FadeOutChannel(chanid, fade))
    {
        Mix_HaltChannel(chanid);
        freechannel(chanid);
    }
    return true;
}

void stopownersounds(physent *d)
{
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(chan.inuse && chan.owner == d)
        {
            Mix_HaltChannel(i);
            freechannel(i);
        }
    }
}

int playsoundname(const char *s, physent *owner, const vec *loc, int vol, int flags, int loops, int fade, int chanid, int radius, int expire)
{
    if(!vol) vol = 100;
    int id = gamesounds.findsound(s, vol);
    if(id < 0) id = gamesounds.addsound(s, vol);
    return playsound(id, owner, loc, NULL, flags, loops, fade, chanid, radius, expire);
}

ICOMMAND(uisound, "si", (const char *s, int *vol), if(uivol) playsoundname(s, NULL, NULL, *vol, SND_UI));
ICOMMAND(gamesound, "si", (const char* s, int* vol), if(gamevol) playsoundname(s, NULL, NULL, *vol));
ICOMMAND(playsound, "i", (int *n), if(gamevol) playsound(*n));
ICOMMAND(voicecom, "ssi", (const char *sound, char *text, const int *team),
{
    int id = gamesounds.findsound(sound, 0);
    game::voicecom(id, text, *team);
});

void resetsound()
{
    clearchanges(CHANGE_SOUND);
    if(!nosound)
    {
        gamesounds.cleanupsamples();
        mapsounds.cleanupsamples();
        if(music)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(music);
        }
        if(musicstream) musicstream->seek(0, SEEK_SET);
        Mix_CloseAudio();
    }
    initsound();
    resetchannels();
    if(nosound)
    {
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
        music = NULL;
        gamesounds.cleanupsamples();
        mapsounds.cleanupsamples();
        return;
    }
    if(music && loadmusic(musicfile))
    {
        Mix_PlayMusic(music, musicdonecmd ? 0 : -1);
        Mix_VolumeMusic((musicvol * mastervol * MIX_MAX_VOLUME) / (MAX_VOLUME * MAX_VOLUME));
    }
    else
    {
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
    }
}

COMMAND(resetsound, "");

void pauseaudio(int value)
{
    if (value == 1)
    {
        Mix_Pause(-1);
        if(!Mix_PausedMusic()) Mix_PauseMusic();
    }
    else
    {
        Mix_Resume(-1);
        if(Mix_PausedMusic()) Mix_ResumeMusic();
    }
}

#ifdef WIN32

#include <wchar.h>

#else

#include <unistd.h>

#ifdef _POSIX_SHARED_MEMORY_OBJECTS
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <wchar.h>
#endif

#endif

#if defined(WIN32) || defined(_POSIX_SHARED_MEMORY_OBJECTS)
struct MumbleInfo
{
    int version, timestamp;
    vec pos, front, top;
    wchar_t name[256];
};
#endif

#ifdef WIN32
static HANDLE mumblelink = NULL;
static MumbleInfo *mumbleinfo = NULL;
#define VALID_MUMBLELINK (mumblelink && mumbleinfo)
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
static int mumblelink = -1;
static MumbleInfo *mumbleinfo = (MumbleInfo *)-1;
#define VALID_MUMBLELINK (mumblelink >= 0 && mumbleinfo != (MumbleInfo *)-1)
#endif

#ifdef VALID_MUMBLELINK
VARFP(mumble, 0, 1, 1, { if(mumble) initmumble(); else closemumble(); });
#else
VARFP(mumble, 0, 0, 1, { if(mumble) initmumble(); else closemumble(); });
#endif

void initmumble()
{
    if(!mumble) return;
#ifdef VALID_MUMBLELINK
    if(VALID_MUMBLELINK) return;

    #ifdef WIN32
        mumblelink = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "MumbleLink");
        if(mumblelink)
        {
            mumbleinfo = (MumbleInfo *)MapViewOfFile(mumblelink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MumbleInfo));
            if(mumbleinfo) wcsncpy(mumbleinfo->name, L"VALHALLA", 256);
        }
    #elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
        defformatstring(shmname, "/MumbleLink.%d", getuid());
        mumblelink = shm_open(shmname, O_RDWR, 0);
        if(mumblelink >= 0)
        {
            mumbleinfo = (MumbleInfo *)mmap(NULL, sizeof(MumbleInfo), PROT_READ|PROT_WRITE, MAP_SHARED, mumblelink, 0);
            if(mumbleinfo != (MumbleInfo *)-1) wcsncpy(mumbleinfo->name, L"VALHALLA", 256);
        }
    #endif
    if(!VALID_MUMBLELINK) closemumble();
#else
    conoutf(CON_ERROR, "Mumble positional audio is not available on this platform.");
#endif
}

void closemumble()
{
#ifdef WIN32
    if(mumbleinfo) { UnmapViewOfFile(mumbleinfo); mumbleinfo = NULL; }
    if(mumblelink) { CloseHandle(mumblelink); mumblelink = NULL; }
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
    if(mumbleinfo != (MumbleInfo *)-1) { munmap(mumbleinfo, sizeof(MumbleInfo)); mumbleinfo = (MumbleInfo *)-1; }
    if(mumblelink >= 0) { close(mumblelink); mumblelink = -1; }
#endif
}

static inline vec mumblevec(const vec &v, bool pos = false)
{
    // change from X left, Z up, Y forward to X right, Y up, Z forward
    // 8 cube units = 1 meter
    vec m(-v.x, v.z, v.y);
    if(pos) m.div(8);
    return m;
}

void updatemumble()
{
#ifdef VALID_MUMBLELINK
    if(!VALID_MUMBLELINK) return;

    static int timestamp = 0;

    mumbleinfo->version = 1;
    mumbleinfo->timestamp = ++timestamp;

    mumbleinfo->pos = mumblevec(player->o, true);
    mumbleinfo->front = mumblevec(vec(player->yaw*RAD, player->pitch*RAD));
    mumbleinfo->top = mumblevec(vec(player->yaw*RAD, (player->pitch+90)*RAD));
#endif
}

