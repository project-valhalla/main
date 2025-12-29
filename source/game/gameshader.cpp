#include "engine.h"
#include "game.h"

namespace game
{
    namespace shaders
    {
        // Shader event container
        vector<PostFxEvent> postFxEvents;

        static void updatePostFx(const char* name, const float parameter, const int time = 0, const int duration = 0, const int direction = Fade::Out)
        {
            updatepostfx(name, vec4(static_cast<float>(time), static_cast<float>(duration), static_cast<float>(direction), parameter));
        }

        // Time for map fade effect in milliseconds.
        VARP(mapfadetime, 0, 1500, 10000);

        // Initialise post-processing effects when preparing the world.
        void prepareWorld()
        {
            enablepostfx("saturation");
            if (mainmenu == MENU_MAP)
            {
                enablepostfx("vignette");
            }
            if (mapfadetime > 0)
            {
                enablepostfx("fade_fromblack");
                
                // Timestamp.
                lastmap = lastmillis;
            }
        }

        // Color saturation.
        FVARP(saturation, 0, 1.25f, 2.0f);

        // Update post-processing effects if a world is loaded.
        static void updateWorld()
        {
            // Main shaders.
            updatePostFx("saturation", saturation);

            // Update conditional shaders.
            if (camera::isUnderwater())
            {
                if (!enablepostfx("underwater"))
                {
                    updatePostFx("underwater", 0, lastmillis, 1000);
                }
            }
            else
            {
                disablepostfx("underwater");
            }

            // Update transitions.
            if (!lastmap || (mapfadetime && lastmillis - lastmap > mapfadetime))
            {
                return;
            }
            const int time = lastmillis - lastmap;
            updatePostFx("fade_fromblack", 0, time, mapfadetime, Fade::Out);
        }

        void addPostFxEvent(const char* name, const int duration, const int direction, const float parameter, const gameent* owner)
        {
            // Shader events are rendered only for ourselves or the player being spectated.
            if (owner != followingplayer(self))
            {
                return;
            }

            if (!enablepostfx(name))
            {
                return;
            }
            PostFxEvent& event = postFxEvents.add();
            event.name = name;
            event.time = lastmillis;
            event.duration = duration;
            event.direction = direction;
        }

        static void processPostFxEvents()
        {
            if (postFxEvents.empty())
            {
                return;
            }
            loopv(postFxEvents)
            {
                PostFxEvent& event = postFxEvents[i];
                const int elapsed = lastmillis - event.time;
                if (elapsed > event.duration)
                {
                    if (event.direction == Fade::Out)
                    {
                        disablepostfx(event.name);
                        postFxEvents.remove(i--);
                    }
                }
                else
                {
                    updatePostFx(event.name, event.parameter, elapsed, event.duration, event.direction);
                }
            }
        }

        void update()
        {
            // Skip if a world is not loaded.
            if (!connected)
            {
                return;
            }

            updateWorld();
            processPostFxEvents();
        }

        static void removeEvents()
        {
            if (postFxEvents.length())
            {
                loopv(postFxEvents)
                {
                    PostFxEvent& event = postFxEvents[i];
                    disablepostfx(event.name);
                }
            }
            postFxEvents.shrink(0);
        }

        // Clean up map post-fx when the world is unloaded.
        static void cleanUpWorld()
        {
            static const char* worldPostFxs[] = { "saturation", "vignette", "fade_fromblack", "underwater" };
            for (const auto& postFx : worldPostFxs)
            {
                disablepostfx(postFx);
            }
        }

        // Clean up post-processing effects.
        void cleanUp()
        {
            cleanUpWorld();
            removeEvents();
        }

        // Toggle zoom post-processing.
        void toggleZoomEffects(const int toggle)
        {
            if (toggle == 1)
            {
                const int zoomType = guns[self->gunselect].zoom;
                enablepostfx("scope", vec4(static_cast<float>(zoomType), 0, 0, 0));
            }
            else if (toggle == 0)
            {
                disablepostfx("scope");
            }
        }

        // Update zoom post-processing with progress tracking.
        void updateZoomEffects(const float progress)
        {
            const int zoomType = guns[self->gunselect].zoom;
            updatepostfx("scope", vec4(static_cast<float>(zoomType), progress, 0, 0));
        }

    } // namespace shaders
} // namespace game
