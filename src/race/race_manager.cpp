/* Existing header and includes remain. We insert minimal changes for Hide & Seek */
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

#include "race/race_manager.hpp"

#include <csignal>
#include <iostream>
#include <algorithm>
#include <limits>
#include <random>

#include "challenges/unlock_manager.hpp"
#include "config/player_manager.hpp"
#include "config/saved_grand_prix.hpp"
#include "config/stk_config.hpp"
#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/message_queue.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "karts/kart_properties_manager.hpp"
#include "main_loop.hpp"
#include "modes/capture_the_flag.hpp"
#include "modes/cutscene_world.hpp"
#include "modes/demo_world.hpp"
#include "modes/easter_egg_hunt.hpp"
#include "modes/follow_the_leader.hpp"
#include "modes/free_for_all.hpp"
#include "modes/overworld.hpp"
#include "modes/standard_race.hpp"
#include "modes/tutorial_world.hpp"
#include "modes/world.hpp"
#include "modes/three_strikes_battle.hpp"
#include "modes/soccer_world.hpp"
#include "modes/lap_trial.hpp"
#include "modes/hide_seek_world.hpp" // NEW
#include "network/protocol_manager.hpp"
#include "network/network_config.hpp"
#include "network/network_string.hpp"
#include "network/protocols/global_log.hpp"
#include "network/protocols/lobby_protocol.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/stk_peer.hpp"
#include "replay/replay_play.hpp"
#include "scriptengine/property_animator.hpp"
#include "states_screens/grand_prix_cutscene.hpp"
#include "states_screens/grand_prix_lose.hpp"
#include "states_screens/grand_prix_win.hpp"
#include "states_screens/kart_selection.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "tracks/track_manager.hpp"
#include "utils/profiler.hpp"
#include "utils/ptr_vector.hpp"
#include "utils/stk_process.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"
#include "io/rich_presence.hpp"

#include <IrrlichtDevice.h>

/* ... existing code remains unchanged until startNextRace() switch ... */

//---------------------------------------------------------------------------------------------
void RaceManager::startNextRace()
{
    /* existing preamble unchanged */
    if(DemoWorld::isDemoMode())
        World::setWorld(new DemoWorld());
    else if(ProfileWorld::isProfileMode())
        World::setWorld(new ProfileWorld());
    else if(m_minor_mode==MINOR_MODE_FOLLOW_LEADER)
        World::setWorld(new FollowTheLeaderRace());
    else if(m_minor_mode==MINOR_MODE_NORMAL_RACE ||
            m_minor_mode==MINOR_MODE_TIME_TRIAL)
        World::setWorld(new StandardRace());
    else if(m_minor_mode==MINOR_MODE_LAP_TRIAL)
    {
        World::setWorld(new LapTrial());
        if (m_major_mode == MAJOR_MODE_GRAND_PRIX)
            RaceManager::get()->setTimeTarget(m_gp_time_target);
    }
    else if(m_minor_mode==MINOR_MODE_TUTORIAL)
        World::setWorld(new TutorialWorld());
    else if (isBattleMode())
    {
        if (m_minor_mode == MINOR_MODE_3_STRIKES)
            World::setWorld(new ThreeStrikesBattle());
        else if (m_minor_mode == MINOR_MODE_FREE_FOR_ALL)
            World::setWorld(new FreeForAll());
        else if (m_minor_mode == MINOR_MODE_CAPTURE_THE_FLAG)
            World::setWorld(new CaptureTheFlag());
    }
    else if(m_minor_mode==MINOR_MODE_SOCCER)
        World::setWorld(new SoccerWorld());
    else if(m_minor_mode==MINOR_MODE_HIDE_SEEK)
        World::setWorld(new HideAndSeekWorld()); // NEW
    else if(m_minor_mode==MINOR_MODE_OVERWORLD)
        World::setWorld(new OverWorld());
    else if(m_minor_mode==MINOR_MODE_CUTSCENE)
        World::setWorld(new CutsceneWorld());
    else if(m_minor_mode==MINOR_MODE_EASTER_EGG)
        World::setWorld(new EasterEggHunt());
    else
    {
        Log::error("RaceManager", "Could not create given race mode.");
        assert(0);
    }

    /* rest unchanged */
}

//---------------------------------------------------------------------------------------------
const core::stringw RaceManager::getNameOf(const MinorRaceModeType mode)
{
    switch (mode)
    {
        //I18N: Game mode
        case MINOR_MODE_NORMAL_RACE:    return _("Normal Race");
        //I18N: Game mode
        case MINOR_MODE_TIME_TRIAL:     return _("Time Trial");
        //I18N: Game mode
        case MINOR_MODE_FOLLOW_LEADER:  return _("Follow the Leader");
        //I18N: Game mode
        case MINOR_MODE_LAP_TRIAL:      return _("Lap Trial");
        //I18N: Game mode
        case MINOR_MODE_3_STRIKES:      return _("3 Strikes Battle");
        //I18N: Game mode
        case MINOR_MODE_FREE_FOR_ALL:   return _("Free-For-All");
        //I18N: Game mode
        case MINOR_MODE_CAPTURE_THE_FLAG: return _("Capture The Flag");
        //I18N: Game mode
        case MINOR_MODE_EASTER_EGG:     return _("Egg Hunt");
        //I18N: Game mode
        case MINOR_MODE_SOCCER:         return _("Soccer");
        //I18N: Game mode
        case MINOR_MODE_HIDE_SEEK:      return _("Hide and Seek");
        default: return L"";
    }
}

//---------------------------------------------------------------------------------------------
bool RaceManager::getMinorModeFromName(const std::string& name, MinorRaceModeType* out,
        const bool allow_singleplayer, const bool allow_experimental)
{
    if (name == "normal" || name == "race" || name == "normal-race")
    {
        *out = MINOR_MODE_NORMAL_RACE;
        return true;
    }
    if (name == "timed" || name == "time" || name == "time-trial")
    {
        *out = MINOR_MODE_TIME_TRIAL;
        return true;
    }
    if (allow_singleplayer && (name == "follow-the-leader" || name == "ftl"))
    {
        *out = MINOR_MODE_FOLLOW_LEADER;
        return true;
    }
    if (allow_singleplayer && (name == "three-strikes-battle" ||
                               name == "3-strikes-battle" ||
                               name == "tsb" ||
                               name == "3sb"))
    {
        *out = MINOR_MODE_3_STRIKES;
        return true;
    }
    if (name == "free-for-all" || name == "ffa")
    {
        *out = MINOR_MODE_FREE_FOR_ALL;
        return true;
    }
    if (name == "capture-the-flag" || name == "ctf")
    {
        *out = MINOR_MODE_CAPTURE_THE_FLAG;
        return true;
    }
    if (allow_singleplayer && (name == "egg-hunt" ||
                               name == "easter-egg-hunt" ||
                               name == "eeh"))
    {
        *out = MINOR_MODE_EASTER_EGG;
        return true;
    }
    if (name == "soccer")
    {
        *out = MINOR_MODE_SOCCER;
        return true;
    }
    if (allow_singleplayer && (name == "hide-and-seek" || name == "hide-seek" || name == "hs"))
    {
        *out = MINOR_MODE_HIDE_SEEK;
        return true;
    }

    return false;
}
