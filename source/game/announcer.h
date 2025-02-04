namespace game
{
    namespace announcer
    {
        enum Announcements
        {
            INVALID = -1,
            HEADSHOT,
            FIRST,
            SPREE,
            SAVAGE,
            UNSTOPPABLE,
            LEGENDARY,
            ONE_MINUTE,
            FIVE_MINUTES,
            ONE_KILL,
            FIVE_KILLS,
            TEN_KILLS,
            OVERTIME,
            COUNT
        };

        enum AnnouncementTypes
        {
            GAME = 0,
            SPECIAL,
            MULTIKILL,
            STREAK
        };

        struct announcementType
        {
            int announcement;
            int type;
            int sound;
            const char* message;
        };
        static const announcementType announcements[Announcements::COUNT] =
        {
            { Announcements::HEADSHOT,     AnnouncementTypes::SPECIAL, S_ANNOUNCER_HEADSHOT,      "\f2scored a headshot"                   },
            { Announcements::FIRST,        AnnouncementTypes::SPECIAL, S_ANNOUNCER_FIRST_BLOOD,   "\f2drew first blood!"                   },
            { Announcements::SPREE,        AnnouncementTypes::STREAK,  S_ANNOUNCER_KILLING_SPREE, "\f2is on a killing spree!"              },
            { Announcements::SAVAGE,       AnnouncementTypes::STREAK,  S_ANNOUNCER_SAVAGE,        "\f2is on a savage killing spree!"       },
            { Announcements::UNSTOPPABLE,  AnnouncementTypes::STREAK,  S_ANNOUNCER_UNSTOPPABLE,   "\f2is on an unstoppable killing spree!" },
            { Announcements::LEGENDARY,    AnnouncementTypes::STREAK,  S_ANNOUNCER_LEGENDARY,     "\f2is on a legendary killing spree!"    },
            { Announcements::ONE_MINUTE,   AnnouncementTypes::GAME,    S_ANNOUNCER_1_MINUTE,      "\f2One minute remaining"                },
            { Announcements::FIVE_MINUTES, AnnouncementTypes::GAME,    S_ANNOUNCER_5_MINUTES,     "\f2Five minutes remaining"              },
            { Announcements::ONE_KILL,     AnnouncementTypes::GAME,    S_ANNOUNCER_1_KILL,        "\f2One kill remaining"                  },
            { Announcements::FIVE_KILLS,   AnnouncementTypes::GAME,    S_ANNOUNCER_5_KILLS,       "\f2Five kills remaining"                },
            { Announcements::TEN_KILLS,    AnnouncementTypes::GAME,    S_ANNOUNCER_10_KILLS,      "\f2Ten kills remaining"                 },
            { Announcements::OVERTIME,     AnnouncementTypes::GAME,    S_ANNOUNCER_OVERTIME,      "\f2Overtime: scores are tied"           }
        };
    }
}
