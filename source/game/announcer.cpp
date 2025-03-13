#include "game.h"

namespace game
{
    namespace announcer
    {
        struct Announcer
        {
            int lastAnnouncement = 0;
            int fade = 0;

            struct Queue
            {
                int sound = S_INVALID;
                int type = Announcements::INVALID;
            };
            vector<Queue> queueItems;

            void queueAnnouncement(int announcement);
            void queueSound(int sound);
            void checkQueue();
        };
        Announcer announcerInfo;

        void Announcer::queueAnnouncement(const int announcement)
        {
            if (announcement <= Announcements::INVALID)
            {
                return;
            }

            Queue& newEntry = queueItems.add();
            newEntry.sound = announcements[announcement].sound;
            newEntry.type = announcement;
        }

        void Announcer::queueSound(int sound)
        {
            if (!validsound(sound))
            {
                return;
            }

            Queue& newEntry = queueItems.add();
            newEntry.sound = sound;
            newEntry.type = Announcements::INVALID;
        }

        bool announce(const int announcement, const bool shouldQueue)
        {
            if (announcement <= Announcements::INVALID)
            {
                return false;
            }

            const int sound = announcements[announcement].sound;
            if (validsound(sound))
            {
                if (!ischannelinuse(SND_ANNOUNCER) && playsound(sound, NULL, NULL, NULL, SND_ANNOUNCER) >= 0)
                {
                    announcerInfo.lastAnnouncement = announcement;
                    execident("on_announcement");
                    return true;
                }
                else if (shouldQueue)
                {
                    announcerInfo.queueAnnouncement(announcement);
                    return false;
                }
            }
            return false;
        }
        ICOMMAND(getlastannouncement, "", (), intret(announcerInfo.lastAnnouncement));

        bool playannouncement(const int sound, const bool shouldQueue)
        {
            if (validsound(sound))
            {
                if (!ischannelinuse(SND_ANNOUNCER) && playsound(sound, NULL, NULL, NULL, SND_ANNOUNCER) >= 0)
                {
                    return true;
                }
                else if (shouldQueue)
                {
                    announcerInfo.queueSound(sound);
                    return false;
                }
            }
            return false;
        }

        void Announcer::checkQueue()
        {
            if (!queueItems.length())
            {
                return;
            }

            if (!ischannelinuse(SND_ANNOUNCER))
            {
                loopv(queueItems)
                {
                    Queue& firstEntry = queueItems[i];
                    if ((firstEntry.type <= -1 && playannouncement(firstEntry.sound, false)) || announce(firstEntry.type, false))
                    {
                        /*
                         * Okay, we managed to announce the first announcement in the queue.
                         * Shift the remaining announcements down in the queue.
                         */
                        queueItems.remove(i--);
                        break;
                    }
                }
            }
        }

        void checkannouncement(const int announcement, const bool shouldPrint = true, gameent* actor = NULL)
        {
            if (!actor)
            {
                return;
            }
            if (announcement > Announcements::INVALID && shouldPrint)
            {
                // Avoid confusing players in certain circumstances by triggering console messages.
                if (announcements[announcement].type == AnnouncementTypes::STREAK || announcements[announcement].announcement == Announcements::FIRST)
                {
                    printkillfeedannouncement(announcement, actor);
                }
            }
            if (actor != followingplayer(self))
            {
                /* Now time to proccess the announcements further (play sounds, HUD interactions).
                 * Unless we are the player who achieved these (or spectating them), we do not care.
                 */
                return;
            }
            announce(announcement);
        }

        void parseannouncements(const gameent* d, gameent* actor, const int flags)
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

            if (flags & KILL_HEADSHOT)
            {
                checkannouncement(Announcements::HEADSHOT, false, actor);
            }

            if (d->type == ENT_AI)
            {
                /* NPCs are not players!
                 * Announcing kills (kill streaks, multi-kills, etc.) is unnecessary.
                 */
                return;
            }

            const bool shouldPrint = !(flags & KILL_TRAITOR);
            int announcement = Announcements::INVALID;
            if (flags & KILL_FIRST)
            {
                announcement = Announcements::FIRST;
            }
            else if (flags & KILL_SPREE)
            {
                announcement = Announcements::SPREE;
            }
            else if (flags & KILL_SAVAGE)
            {
                announcement = Announcements::SAVAGE;
            }
            else if (flags & KILL_UNSTOPPABLE)
            {
                announcement = Announcements::UNSTOPPABLE;
            }
            else if (flags & KILL_LEGENDARY)
            {
                announcement = Announcements::LEGENDARY;
            }
            if (announcement > Announcements::INVALID)
            {
                checkannouncement(announcement, shouldPrint, actor);
            }
        }

        void update()
        {
            announcerInfo.checkQueue();
        }

        void reset()
        {
            announcerInfo.queueItems.shrink(0);
        }
    }
}
