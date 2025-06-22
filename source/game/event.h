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
            Manual,       // can only be fired manually with the `emittriggerevent` command
            NUMEVENTTYPES
        };

        static const struct EventTypeInfo { const char* name; } eventtypes[NUMEVENTTYPES] =
        {
            { "use" }, { "proximity" }, { "distance" }, { "manual" }
        };

        struct EventHandler
        {
            const Emitter source; // type of object that emits the event
            const char* query;    // listen for events on these items
            const int event;      // listen for this type of event
            unsigned int* code;   // CubeScript code to run

            EventHandler(Emitter source_, const char* query_, int event_, unsigned int* code_);
            ~EventHandler();
        };

        void clearEventHandlers();
        template<Emitter t> void emit(const int id, const Type event);

        void onMapStart();
        void onPlayerDeath(const gameent* d, const gameent* actor);
        void onPlayerSpectate(const gameent* d);
        void onPlayerUnspectate(const gameent* d);
    } // namespace event
}