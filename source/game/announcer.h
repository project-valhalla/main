namespace game
{
    namespace announcer
    {
        enum Announcements
        {
            INVALID = -1,
            HeadShot,
            FirstBlood,
            AirShot,
            Deadeye,
            StreakSpree,
            StreakSavage,
            StreakUnstoppable,
            StreakLegendary,
            OneMinute,
            FiveMinutes,
            OneKill,
            FiveKills,
            TenKills,
            Overtime,
            Count
        };

        enum AnnouncementTypes
        {
            Match = 0,
            Special,
            Streak
        };

        struct announcementType
        {
            int announcement;
            int type;
            int sound;
            const char* message;
        };
        static const announcementType announcements[Announcements::Count] =
        {
            { Announcements::HeadShot,          AnnouncementTypes::Special, S_ANNOUNCER_HEADSHOT,      "\f2scored a headshot"                   },
            { Announcements::FirstBlood,        AnnouncementTypes::Special, S_ANNOUNCER_FIRST_BLOOD,   "\f2drew first blood!"                   },
            { Announcements::AirShot,           AnnouncementTypes::Special, S_ANNOUNCER_AIRSHOT,       nullptr                                  },
            { Announcements::Deadeye,           AnnouncementTypes::Special, S_ANNOUNCER_DEADEYE,       nullptr                                  },
            { Announcements::StreakSpree,       AnnouncementTypes::Streak,  S_ANNOUNCER_KILLING_SPREE, "\f2is on a killing spree!"              },
            { Announcements::StreakSavage,      AnnouncementTypes::Streak,  S_ANNOUNCER_SAVAGE,        "\f2is on a savage killing spree!"       },
            { Announcements::StreakUnstoppable, AnnouncementTypes::Streak,  S_ANNOUNCER_UNSTOPPABLE,   "\f2is on an unstoppable killing spree!" },
            { Announcements::StreakLegendary,   AnnouncementTypes::Streak,  S_ANNOUNCER_LEGENDARY,     "\f2is on a legendary killing spree!"    },
            { Announcements::OneMinute,         AnnouncementTypes::Match,   S_ANNOUNCER_1_MINUTE,      "\f2One minute remaining"                },
            { Announcements::FiveMinutes,       AnnouncementTypes::Match,   S_ANNOUNCER_5_MINUTES,     "\f2Five minutes remaining"              },
            { Announcements::OneKill,           AnnouncementTypes::Match,   S_ANNOUNCER_1_KILL,        "\f2One kill remaining"                  },
            { Announcements::FiveKills,         AnnouncementTypes::Match,   S_ANNOUNCER_5_KILLS,       "\f2Five kills remaining"                },
            { Announcements::TenKills,          AnnouncementTypes::Match,   S_ANNOUNCER_10_KILLS,      "\f2Ten kills remaining"                 },
            { Announcements::Overtime,          AnnouncementTypes::Match,   S_ANNOUNCER_OVERTIME,      "\f2Overtime: scores are tied"           }
        };
    }
}
