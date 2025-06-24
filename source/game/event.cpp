#include "event.h"
#include "game.h"
#include "query.h"

namespace game
{
    namespace event
    {
        // use binary search to find instances of objects by their id
        // `T` must have an `id` field such that if `i < j` then `v[i]->id < v[j]->id`
        template<class T>
        T *findInstance(vector<T*> v, int val)
        {
            int low = 0, high = v.length() - 1;
            while(low <= high)
            {
                const int mid = low + (high - low) / 2;
                const int inst = v[mid]->id;
                if(inst < val)
                {
                    low = mid + 1;
                }
                else if(inst > val)
                {
                    high = mid - 1;
                }
                else return v[mid];
            }
            return nullptr;
        }

        static const struct EventTypeInfo { const char* name; } eventtypes[NUMEVENTTYPES] =
        {
            { "use" }, { "proximity" }, { "distance" },
            { "notice" }, { "pain" }, { "death" },
            { "manual" }
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

        EventHandler::EventHandler(Emitter source_, const char* query_, int event_, unsigned int* code_) : source(source_), event(event_), code(code_)
        {
            query = newstring(query_);
        }
        EventHandler::~EventHandler()
        {
            if (query)
            {
                delete[] query;
            }
            if (code)
            {
                delete[] code;
            }
        }
        vector<EventHandler*> event_handlers;

        Type findEventType(const char* name)
        {
            for (int i = 0; i < NUMEVENTTYPES; ++i)
            {
                if (!strcmp(name, eventtypes[i].name))
                {
                    return (Type)i;
                }
            }
            conoutf(CON_ERROR, "unknown trigger event type \"%s\"", name);
            return Invalid;
        }

        void registerEventHandler(EventHandler* handler)
        {
            event_handlers.add(handler);
        }

        void clearEventHandlers()
        {
            event_handlers.deletecontents();
        }
        ICOMMAND(eventhandlerreset, "", (), clearEventHandlers());

        // execute all the event handlers that match the label
        void executeEventHandlers(Emitter emitter, Type event, const char* label)
        {
            loopv(event_handlers)
            {
                EventHandler* handler = event_handlers[i];
                if (handler->source == emitter && handler->event == event && query::match(handler->query, label))
                {
                    executeret(handler->code);
                }
            }
        }

        VAR(eventsource, 0, 0, -1); // the id of the object that emitted the event

        // emitters must be explicitly defined for each emitter type
        template<class T> void emit(T*, Type)    = delete; // for structs that contain an `id` field
        template<Emitter t> void emit(int, Type) = delete; // for `extentity`'s
        
        // event emitter for triggers
        template<>
        void emit<Trigger>(const int id, Type event)
        {
            if (!entities::ents.inrange(id) || entities::ents[id]->type != TRIGGER)
            {
                return;
            }
            eventsource = id;
            executeEventHandlers(Trigger, event, entities::ents[id]->label);
        }

        // event emitters for monsters
        template<>
        void emit(monster *m, Type event)
        {
            if(!m) return;
            eventsource = m->id;
            executeEventHandlers(Monster, event, m->label);
        }
        template<>
        void emit<Monster>(int id, Type event)
        {
            emit(findInstance(monsters, id), event);
        }

        // fire events manually
        template<Emitter T>
        void emit(const int id, const char* event)
        {
            Type event_i = findEventType(event);
            if (event_i == Invalid)
            {
                return;
            }
            emit<T>(id, event_i);
        }
        ICOMMAND(triggeremit, "is", (int* id, char* event), emit<Trigger>(*id, event)); // fire trigger manually
        ICOMMAND(monsteremit, "is", (int* id, char* event), emit<Monster>(*id, event));

        // register a new event handler for triggers
        ICOMMAND(trigger, "sss", (char* query, char* event, char* code),
        {
            const Type type = findEventType(event);
            if (type == Invalid) return;
            registerEventHandler(new EventHandler(Trigger, query, type, compilecode(code)));
        });
        ICOMMAND(monster, "sss", (char* query, char* event, char* code),
        {
            const Type type = findEventType(event);
            if (type == Invalid) return;
            registerEventHandler(new EventHandler(Monster, query, type, compilecode(code)));
        });

        void onMapStart()
        {
            entities::onMapStart();
            clearEventHandlers();
        }

        void onPlayerDeath(const gameent* d, const gameent* actor)
        { 
            entities::onPlayerDeath(d, actor);
        }

        void onPlayerSpectate(const gameent* d)
        { 
            entities::onPlayerSpectate(d);
        }

        void onPlayerUnspectate(const gameent* d)
        { 
            entities::onPlayerUnspectate(d);
        }
    } // namespace event
} // namespace game