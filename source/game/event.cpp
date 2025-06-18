#include "event.h"
#include "game.h"
#include "query.h"

namespace event
{
    EventHandler::EventHandler(Emitter source_, const char *query_, int event_, unsigned int *code_) : source(source_), event(event_), code(code_)
    {
        query = newstring(query_);
    }
    EventHandler::~EventHandler()
    {
        if(query) delete[] query;
        if(code)  delete[] code;
    }
    vector<EventHandler *> event_handlers;

    Type findEventType(const char *name)
    {
        for(int i = 0; i < NUMEVENTTYPES; ++i) if(!strcmp(name, eventtypes[i].name)) return (Type)i;
        conoutf(CON_ERROR, "unknown trigger event type \"%s\"", name);
        return Invalid;
    }

    void registerEventHandler(EventHandler *handler)
    {
        event_handlers.add(handler);
    }

    void clearEventHandlers()
    {
        event_handlers.deletecontents();
    }
    ICOMMAND(cleareventhandlers, "", (), clearEventHandlers());

    // execute all the event handlers that match the label
    void executeEventHandlers(Emitter emitter, Type event, const char *label)
    {
        loopv(event_handlers)
        {
            EventHandler *handler = event_handlers[i];
            if(handler->source == emitter && handler->event == event && query::match(handler->query, label))
            {
                executeret(handler->code);
            }
        }
    }

    VAR(eventsource, 0, 0, -1); // the id of the object that emitted the event

    template<Emitter t> void emit(int, Type) = delete; // emitters must be explicitly defined for each emitter type
    // event emitter for triggers
    template<>
    void emit<Trigger>(int id, Type event)
    {
        if(!entities::ents.inrange(id) || entities::ents[id]->type != TRIGGER) return;
        eventsource = id;
        executeEventHandlers(Trigger, event, entities::ents[id]->label);
    }

    // fire events manually
    template<Emitter T>
    void emit(int id, char *event)
    {
        Type event_i = findEventType(event);
        if(event_i == Invalid) return;
        emit<T>(id, event_i);
    }
    ICOMMAND(emittriggerevent, "is", (int *id, char *event), emit<Trigger>(*id, event)); // fire trigger manually

    // register a new event handler for triggers
    ICOMMAND(trigger, "sss", (char *query, char *event, char *code),
    {
        const Type type = findEventType(event);
        if(type == Invalid) return;
        registerEventHandler(new EventHandler(Trigger, query, type, compilecode(code)));
    });

    void onMapStart()
    {
        entities::onMapStart();
        clearEventHandlers();
    }
    void onPlayerDeath(gameent *d, gameent *actor) { entities::onPlayerDeath(d, actor); }
    void onPlayerSpectate(gameent *d) { entities::onPlayerSpectate(d); }
    void onPlayerUnspectate(gameent *d) { entities::onPlayerUnspectate(d); }
} // namespace event