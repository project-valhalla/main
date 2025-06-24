struct gameent;

namespace game
{
    namespace event
    {
        // types of objects can emit events
        enum Emitter
        {
            Trigger = 0,
            Monster
        };

        // types of events
        enum Type
        {
            Invalid = -1,
            Use = 0,      // fires when the player is close and presses the "Use" button
            Proximity,    // fires when the player is close
            Distance,     // fires when the player was close and moves away or dies

            Notice,       // a monster noticed the player
            Pain,         // a monster took damage
            Death,        // a monster died

            Manual,       // can only be fired manually with the `emittriggerevent` command
            NUMEVENTTYPES
        };

        template<class T> void emit(T* source, Type event);
        template<Emitter t> void emit(const int id, const Type event);

        void onMapStart();
        void onPlayerDeath(const gameent* d, const gameent* actor);
        void onPlayerSpectate(const gameent* d);
        void onPlayerUnspectate(const gameent* d);
    } // namespace event
}