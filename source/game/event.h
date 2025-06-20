struct gameent;
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
    static const struct EventTypeInfo { const char *name; } eventtypes[NUMEVENTTYPES] =
    {
        { "use" }, { "proximity" }, { "distance" }, { "manual" }
    };

    struct EventHandler
    {
        Emitter source;      // type of object that emits the event
        char *query;         // listen for events on these items
        int event;           // listen for this type of event
        unsigned int *code;  // cubescript code to run
        
        EventHandler(Emitter source_, const char *query_, int event_, unsigned int *code_);
        ~EventHandler();
    };

    void clearEventHandlers();
    template<Emitter t> void emit(int id, Type event);

    void onMapStart();
    void onPlayerDeath(gameent *d, gameent *actor);
    void onPlayerSpectate(gameent *d);
    void onPlayerUnspectate(gameent *d);
} // namespace event