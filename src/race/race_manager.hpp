/* The beginning of this file remains unchanged up to the list of IDENT_* constants */
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006-2015 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_RACEMANAGER_HPP
#define HEADER_RACEMANAGER_HPP

/**
  * \defgroup race
  * Contains the race information that is conceptually above what you can find
  * in group Modes. Handles highscores, grands prix, number of karts, which
  * track was selected, etc.
  */

#include <memory>
#include <vector>
#include <algorithm>
#include <cassert>
#include <string>

#include "network/network_player_profile.hpp"
#include "network/remote_kart_info.hpp"
#include "items/powerup.hpp"
#include "race/grand_prix_data.hpp"
#include "utils/vec3.hpp"
#include "utils/types.hpp"

class AbstractKart;
class NetworkString;
class SavedGrandPrix;
class Track;

static const std::string IDENT_STD      ("STANDARD"        );
static const std::string IDENT_TTRIAL   ("STD_TIMETRIAL"   );
static const std::string IDENT_FTL      ("FOLLOW_LEADER"   );
static const std::string IDENT_STRIKES  ("BATTLE_3_STRIKES");
static const std::string IDENT_FFA      ("BATTLE_FFA"      );
static const std::string IDENT_CTF      ("BATTLE_CTF"      );
static const std::string IDENT_EASTER   ("EASTER_EGG_HUNT" );
static const std::string IDENT_SOCCER   ("SOCCER"          );
static const std::string IDENT_GHOST    ("GHOST"           );
static const std::string IDENT_OVERWORLD("OVERWORLD"       );
static const std::string IDENT_CUTSCENE ("CUTSCENE"        );
static const std::string IDENT_LAP_TRIAL("LAP_TRIAL"       );
// New mode identifier for Hide and Seek
static const std::string IDENT_HIDE_SEEK("HIDE_SEEK"       );

/**
 * The race manager has two functions:
 *  1) it stores information about the race the user selected (e.g. number
 *     of karts, track, race mode etc.). Most of the values are just stored
 *     from the menus, and just read back, except for GP mode (the race
 *     manager stores the GP information, but World queries only track
 *     and number of laps, so in case of GP this information is taken from
 *     the GrandPrix object), and local player information (number of local
 *     players, and selected karts). 
 *     Information about player karts (which player selected which kart,
 *     player ids) is stored in a RemoteKartInfo structure and used later
 *     to initialise the KartStatus array (startNew()). The KartStatus array
 *     stores information about all karts (player and AI), and is used to
 *     determine the order in which karts are started (see startNextRace()).
 *  2) when a race is started, it creates the world, and keeps track of
 *     score during the race. When a race is finished, it deletes the world,
 *     and (depending on race mode) starts the next race by creating a new
 *     world.
 *  Information in the RaceManager is considered to be 'more static', sometimes
 *  the world has similar functions showing the current state. E.g.: the
 *  race manager keeps track of the number of karts with which the race was
 *  started, while world keeps track of the number of karts currently in the
 *  race (consider a race mode like follow the leader where karts can get
 *  eliminated, but still the RaceManager has to accumulate points for those
 *  karts).
 *  The race manager handles all race types as a kind of grand prix. E.g.:
 *  a quick race is basically a GP with only one track (so the race manager
 *  keeps track of scores even though no scores are used in a quick race).
 *
 * \ingroup race
 */
class RaceManager
{
public:
    /** The major types or races supported in STK
    */
    enum MajorRaceModeType
    {
        MAJOR_MODE_GRAND_PRIX = 0,
        MAJOR_MODE_SINGLE
    };

    // quick method to tell the difference between battle modes and race modes
    // think of it like a bitmask, but done in decimal to avoid endianness
    // issues
#define LINEAR_RACE(ID, COUNT_LAPSES) (1000+ID+100*COUNT_LAPSES)
#define BATTLE_ARENA(ID) (2000+ID)
#define EASTER_EGG(ID)   (3000+ID)
#define MISC(ID)         (4000+ID)

    // ----------------------------------------------------------------------------------------
    /** Minor variants to the major types of race.
     *  Make sure to use the 'LINEAR_RACE/BATTLE_ARENA' macros. */
    enum MinorRaceModeType
    {
        MINOR_MODE_NONE             = -1,

        MINOR_MODE_NORMAL_RACE      = LINEAR_RACE(0, true),
        MINOR_MODE_TIME_TRIAL       = LINEAR_RACE(1, true),
        MINOR_MODE_FOLLOW_LEADER    = LINEAR_RACE(2, false),

        MINOR_MODE_3_STRIKES        = BATTLE_ARENA(0),
        MINOR_MODE_FREE_FOR_ALL     = BATTLE_ARENA(1),
        MINOR_MODE_CAPTURE_THE_FLAG = BATTLE_ARENA(2),
        MINOR_MODE_SOCCER           = BATTLE_ARENA(3),

        MINOR_MODE_EASTER_EGG       = EASTER_EGG(0),

        MINOR_MODE_OVERWORLD        = MISC(0),
        MINOR_MODE_TUTORIAL         = MISC(1),
        MINOR_MODE_CUTSCENE         = MISC(2),
        MINOR_MODE_LAP_TRIAL        = MISC(3),
        // New: Hide and Seek
        MINOR_MODE_HIDE_SEEK        = MISC(4)
    };

    // ----------------------------------------------------------------------------------------
    /** True if the AI should have additional abbilities, e.g.
     *  nolok will get special bubble gums in the final challenge. */
    enum AISuperPower
    {
        SUPERPOWER_NONE       = 0,
        SUPERPOWER_NOLOK_BOSS = 1
    };

    // ----------------------------------------------------------------------------------------
    /** Returns a string identifier for each minor race mode.
     *  \param mode Minor race mode.
     */
    static const std::string& getIdentOf(const MinorRaceModeType mode)
    {
        switch (mode)
        {
            case MINOR_MODE_NORMAL_RACE:      return IDENT_STD;
            case MINOR_MODE_TIME_TRIAL:       return IDENT_TTRIAL;
            case MINOR_MODE_FOLLOW_LEADER:    return IDENT_FTL;
            case MINOR_MODE_LAP_TRIAL:        return IDENT_LAP_TRIAL;
            case MINOR_MODE_3_STRIKES:        return IDENT_STRIKES;
            case MINOR_MODE_FREE_FOR_ALL:     return IDENT_FFA;
            case MINOR_MODE_CAPTURE_THE_FLAG: return IDENT_CTF;
            case MINOR_MODE_EASTER_EGG:       return IDENT_EASTER;
            case MINOR_MODE_SOCCER:           return IDENT_SOCCER;
            case MINOR_MODE_HIDE_SEEK:        return IDENT_HIDE_SEEK;
            default: assert(false);
                     return IDENT_STD;  // stop compiler warning
        }
    }   // getIdentOf

    // ----------------------------------------------------------------------------------------
    /** Returns the icon for a minor race mode.
     *  \param mode Minor race mode.
     */
    static const char* getIconOf(const MinorRaceModeType mode)
    {
        switch (mode)
        {
            case MINOR_MODE_NORMAL_RACE:    return "/gui/icons/mode_normal.png";
            case MINOR_MODE_TIME_TRIAL:     return "/gui/icons/mode_tt.png";
            case MINOR_MODE_FOLLOW_LEADER:  return "/gui/icons/mode_ftl.png";
            case MINOR_MODE_LAP_TRIAL:      return "/gui/icons/mode_laptrial.png";
            case MINOR_MODE_3_STRIKES:      return "/gui/icons/mode_3strikes.png";
            case MINOR_MODE_FREE_FOR_ALL:   return "/gui/icons/mode_weapons.png";
            case MINOR_MODE_CAPTURE_THE_FLAG: return "/gui/icons/mode_weapons.png";
            case MINOR_MODE_EASTER_EGG:     return "/gui/icons/mode_easter.png";
            case MINOR_MODE_SOCCER:         return "/gui/icons/mode_soccer.png";
            case MINOR_MODE_HIDE_SEEK:      return "/gui/icons/mode_weapons.png"; // placeholder
            default: assert(false); return NULL;
        }
    }   // getIconOf

    // ----------------------------------------------------------------------------------------
    static const core::stringw getNameOf(const MinorRaceModeType mode);
    // ----------------------------------------------------------------------------------------
    /** Returns if the currently set minor game mode can be used by the AI. */
    bool hasAI()
    {
        switch (m_minor_mode)
        {
            case MINOR_MODE_NORMAL_RACE:    return true;
            case MINOR_MODE_TIME_TRIAL:     return true;
            case MINOR_MODE_FOLLOW_LEADER:  return true;
            case MINOR_MODE_LAP_TRIAL:      return true;
            case MINOR_MODE_3_STRIKES:      return true;
            case MINOR_MODE_FREE_FOR_ALL:   return false;
            case MINOR_MODE_CAPTURE_THE_FLAG: return false;
            case MINOR_MODE_EASTER_EGG:     return false;
            case MINOR_MODE_SOCCER:         return true;
            case MINOR_MODE_HIDE_SEEK:      return true; // planned AI
            default: assert(false);         return false;
        }
    }   // hasAI


    // ----------------------------------------------------------------------------------------
    /** Returns the minor mode id from a string identifier. This function is
     *  used from challenge_data, which reads the mode from a challenge file.
     *  \param name The name of the minor mode.
     */
    static const MinorRaceModeType getModeIDFromInternalName(
                                                       const std::string &name)
    {
        if      (name==IDENT_STD    ) return MINOR_MODE_NORMAL_RACE;
        else if (name==IDENT_TTRIAL ) return MINOR_MODE_TIME_TRIAL;
        else if (name==IDENT_FTL    ) return MINOR_MODE_FOLLOW_LEADER;
        else if (name==IDENT_STRIKES) return MINOR_MODE_3_STRIKES;
        else if (name==IDENT_FFA)     return MINOR_MODE_FREE_FOR_ALL;
        else if (name==IDENT_CTF)     return MINOR_MODE_CAPTURE_THE_FLAG;
        else if (name==IDENT_EASTER ) return MINOR_MODE_EASTER_EGG;
        else if (name==IDENT_SOCCER)  return MINOR_MODE_SOCCER;
        else if (name==IDENT_HIDE_SEEK) return MINOR_MODE_HIDE_SEEK;

        assert(0);
        return MINOR_MODE_NONE;
    }

#undef LINEAR_RACE
#undef BATTLE_ARENA
#undef MISC

    /** Game difficulty. */
    enum Difficulty     { DIFFICULTY_EASY = 0,
                          DIFFICULTY_FIRST = DIFFICULTY_EASY,
                          DIFFICULTY_MEDIUM,
                          DIFFICULTY_HARD,
                          DIFFICULTY_BEST,
                          DIFFICULTY_LAST = DIFFICULTY_BEST,
                          DIFFICULTY_COUNT,
                          DIFFICULTY_NONE};

    /* ... the rest of the file remains unchanged ... */

public:
    void setNitrolessMode(bool enabled) { m_nitroless_mode = enabled; }
    bool getNitrolessMode() const { return m_nitroless_mode; }
    // ----------------------------------------------------------------------------------------
    static RaceManager* get();
    // ----------------------------------------------------------------------------------------
    static void create();
    // ----------------------------------------------------------------------------------------
    static void destroy();
    // ----------------------------------------------------------------------------------------
    static void clear();
    // ----------------------------------------------------------------------------------------
         RaceManager();
        ~RaceManager();

    /* rest of declarations unchanged */
};   // RaceManager

#endif

/* EOF */
