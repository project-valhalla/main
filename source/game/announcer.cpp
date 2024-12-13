#include "game.h"

namespace game
{
    namespace announcer
    {
        enum Announcements
        {
            HEADSHOT = 0,
            FIRST,
            SPREE,
            SAVAGE,
            UNSTOPPABLE,
            LEGENDARY,
            MAX_ANNOUNCEMENTS
        };

        enum AnnouncementTypes
        {
            GAME = 0,
            SPECIAL,
            STREAK,
            MULTIKILL,
            MAX_TYPES
        };

        struct announcementType
        {
            int announcement;
            int type;
            int sound;
            const char* message;
        };
        static const announcementType announcements[Announcements::MAX_ANNOUNCEMENTS] =
        {
            { Announcements::HEADSHOT,    AnnouncementTypes::GAME,    S_ANNOUNCER_HEADSHOT,      "scored a headshot"                     },
            { Announcements::FIRST,       AnnouncementTypes::SPECIAL, S_ANNOUNCER_FIRST_BLOOD,   "\f2drew first blood!"                  },
            { Announcements::SPREE,       AnnouncementTypes::STREAK,  S_ANNOUNCER_KILLING_SPREE, "\f2is on a killing spree"              },
            { Announcements::SAVAGE,      AnnouncementTypes::STREAK,  S_ANNOUNCER_SAVAGE,        "\f2is on a savage killing spree"       },
            { Announcements::UNSTOPPABLE, AnnouncementTypes::STREAK,  S_ANNOUNCER_UNSTOPPABLE,   "\f2is on an unstoppable killing spree" },
            { Announcements::LEGENDARY,   AnnouncementTypes::STREAK,  S_ANNOUNCER_LEGENDARY,     "\f2is on a legendary killing spree"    }
        };

        struct queue
        {
            int sound;
            int type;
        };
        vector<queue> queued;

        void queueannouncement(int announcement)
        {
            queue& newqueue = queued.add();
            newqueue.type = announcement;
            newqueue.sound = announcements[announcement].sound;
        }

        bool announce(int announcement, bool shouldQueue = true)
        {
            int sound = announcements[announcement].sound;
            if (validsound(sound))
            {
                if (!ischannelinuse(SND_ANNOUNCER))
                {
                    if (playsound(sound, NULL, NULL, NULL, SND_ANNOUNCER) >= 0)
                    {
                        // Current announcement.
                        return true;
                    }
                }
                else if (shouldQueue)
                {
                    queueannouncement(announcement);
                    return false;
                }
            }
            return false;
        }

        void checkqueue()
        {
            if (!queued.length())
            {
                return;
            }

            if (!ischannelinuse(SND_ANNOUNCER))
            {
                loopv(queued)
                {
                    queue& first = queued[i];
                    if (announce(first.type, false))
                    {
                        /*
                         * Okay, we managed to announce the first announcement in the queue.
                         * Shift the remaining announcements down in the queue.
                         */
                        queued.remove(i--);
                        break;
                    }
                }
            }
        }

        void checkannouncement(int announcement, bool shouldAnnounce = true, gameent* actor = NULL)
        {
            if (shouldAnnounce)
            {
                // Avoid confusing players in certain circumstances by triggering console messages.
                if (actor)
                {
                    if (announcements[announcement].type == AnnouncementTypes::SPECIAL || announcements[announcement].type == AnnouncementTypes::STREAK)
                    {
                        conoutf(CON_GAMEINFO, "%s \fs%s\fr", colorname(actor), announcements[announcement].message);
                    }
                }
            }
            announce(announcement);
        }

        void parseannouncements(gameent* d, gameent* actor, int flags)
        {
            if (!flags || isally(d, actor))
            {
                /* No announcements to check (or even worse, we killed an ally)?
                 * Nothing to celebrate then.
                 */
                return;
            }

            if (actor->aitype == AI_BOT)
            {
                // Bots taunting players when getting extraordinary kills.
                taunt(actor);
            }
            if (actor != followingplayer(self))
            {
                /* Now: time to announce extraordinary kills.
                 * Unless we are the player who achieved these (or spectating them),
                 * we do not care.
                 */
                return;
            }

            if (flags & KILL_HEADSHOT)
            {
                checkannouncement(Announcements::HEADSHOT);
            }

            if (d->type == ENT_AI)
            {
                /* NPCs are not players!
                 * Announcing kills (kill streaks, multi-kills, etc.) is unnecessary.
                 */
                return;
            }

            bool shouldAnnounce = !(flags & KILL_TRAITOR);
            if (flags & KILL_FIRST)
            {
                checkannouncement(Announcements::FIRST, shouldAnnounce, actor);
            }
            if (flags & KILL_SPREE)
            {
                checkannouncement(Announcements::SPREE, shouldAnnounce, actor);
            }
            if (flags & KILL_SAVAGE)
            {
                checkannouncement(Announcements::SAVAGE, shouldAnnounce, actor);
            }
            if (flags & KILL_UNSTOPPABLE)
            {
                checkannouncement(Announcements::UNSTOPPABLE, shouldAnnounce, actor);
            }
            if (flags & KILL_LEGENDARY)
            {
                checkannouncement(Announcements::LEGENDARY, shouldAnnounce, actor);
            }
        }

        void update()
        {
            checkqueue();
        }

        void reset()
        {
            queued.shrink(0);
        }
    }
}
