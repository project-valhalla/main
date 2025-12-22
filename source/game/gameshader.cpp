#include "engine.h"
#include "game.h"

namespace game
{
    namespace shaders
    {
        /*
            Time (in milliseconds) for map fade effects.
            Range: 0 (disabled), up to 10 seconds.
        */
        VARP(mapfadetime, 0, 2500, 10000);

        // Define fade types for post-processing effects.
        enum Fade
        {
            Out = -1,
            In = 0
        };

        // Initialise post-processing effects when preparing the world.
        void prepareWorld()
        {
            if (mapfadetime > 0)
            {
                // Start with full progress.
                const float initialProgress = 1.0f;

                enablepostfx("fade_black", vec4(initialProgress, static_cast<float>(mapfadetime), static_cast<float>(Fade::In), 0));
            }

            // Timestamp.
            lastmap = lastmillis;
        }

        // Update post-processing effects if connected.
        void updateWorld()
        {
            if (!lastmap)
            {
                return;
            }
            if (mapfadetime > 0)
            {
                const float progress = static_cast<float>(lastmillis - lastmap);
                updatepostfx("fade_black", vec4(progress, static_cast<float>(mapfadetime), static_cast<float>(Fade::In), 0));
            }
            else
            {
                disablepostfx("fade_black");
            }
        }

        // Clean up post-processing effects when the world is unloaded.
        void cleanUpWorld()
        {
            disablepostfx("fade_black");
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