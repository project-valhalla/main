#include "game.h"

// game-specific data stored in the `extras` field of map files

namespace game
{
    enum WorldDataTag
    {
        Invalid = -1,
        DisabledEnts = 0,
        NUMDATATAGS
    };
    static inline WorldDataTag dataTag(int t)
    {
        return t <= Invalid || t >= NUMDATATAGS ? Invalid : (WorldDataTag)t;
    }

    static vector<int> ents_to_disable;

    static void writeDisabledEnts(vector<uchar>& block, bool nolms)
    {
        vector<int> disabled_ents;
        const auto& ents = entities::getents();
        int skip = 0;
        // find disabled ents
        loopv(ents)
        {
            if(!nolms && ents[i]->type == ET_EMPTY)
            {
                ++skip;
                continue;
            }
            if(!ents[i]->isactive()) disabled_ents.add(i - skip);
        }
        // write ent id's to the buffer
        if(const int count = disabled_ents.length())
        {
            putint(block, count);
            loopv(disabled_ents)
            {
                putint(block, disabled_ents[i]);
            }
        }
    }

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writeGameData(vector<uchar> &extras, bool nolms)
    {
        // write DisabledEnts block
        vector<uchar> disabledEntsBlock;
        writeDisabledEnts(disabledEntsBlock, nolms);
        if(const int len = disabledEntsBlock.length())
        {
            putint(extras, DisabledEnts);
            putint(extras, len);
            extras.put(disabledEntsBlock.buf, len);
        }
    }

    static void readDisabledEnts(ucharbuf& block)
    {
        ents_to_disable.setsize(0);
        const int count = getint(block);
        loopi(count)
        {
            ents_to_disable.add(getint(block));
        }
    }

    static void readBlock(WorldDataTag tag, ucharbuf& block)
    {
        switch(tag)
        {
            case DisabledEnts:
                readDisabledEnts(block);
                return;
            case Invalid:
                conoutf(CON_WARN, "Invalid data block: %d", (int)tag);
                // fall through
            default: return;
        }
    }

    void readGameData(vector<uchar> &extras)
    {
        const int totallen = extras.length();
        if(!totallen) return;
        ucharbuf buf(extras.buf, totallen);
        while(buf.remaining())
        {
            // read each block
            const WorldDataTag tag = dataTag(getint(buf));
            const int blocklen = getint(buf);

            uchar* vblock = new uchar[blocklen];
            ucharbuf block(vblock, blocklen);
            buf.get(vblock, blocklen);

            readBlock(tag, block);
            delete[] vblock;
        }
    }

    // called after the map has been loaded
    void postWorldLoad()
    {
        const auto& ents = entities::getents();
        loopv(ents_to_disable)
        {
            const int id = ents_to_disable[i];
            if(!ents.inrange(id)) continue;
            ents[id]->setactivity(false);
        }
        ents_to_disable.setsize(0);
    }
} // namespace game