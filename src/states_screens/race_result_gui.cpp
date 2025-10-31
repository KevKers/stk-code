//
#include "states_screens/race_result_gui.hpp"
#include "network/network_config.hpp"
#include "network/stk_host.hpp"
#include "states_screens/state_manager.hpp"
#include "states_screens/main_menu_screen.hpp"

//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2010-2015 Joerg Henrichs
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

#include "states_screens/race_result_gui.hpp"

#include "audio/music_manager.hpp"
#include "audio/sfx_manager.hpp"
#include "audio/sfx_base.hpp"
#include "challenges/challenge_status.hpp"
#include "challenges/story_mode_timer.hpp"
#include "challenges/unlock_manager.hpp"
#include "config/player_manager.hpp"
#include "config/stk_config.hpp"
#include "config/user_config.hpp"
#include "graphics/2dutils.hpp"
#include "graphics/material.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/message_queue.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/scalable_font.hpp"
#include "guiengine/screen_keyboard.hpp"
#include "guiengine/widget.hpp"
#include "guiengine/widgets/icon_button_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/ribbon_widget.hpp"
#include "io/file_manager.hpp"
#include "karts/controller/controller.hpp"
#include "karts/controller/end_controller.hpp"
#include "karts/controller/local_player_controller.hpp"
#include "karts/ghost_kart.hpp"
#include "karts/kart_properties.hpp"
#include "karts/kart_properties_manager.hpp"
#include "modes/cutscene_world.hpp"
#include "modes/demo_world.hpp"
#include "modes/capture_the_flag.hpp"
#include "modes/overworld.hpp"
#include "modes/soccer_world.hpp"
#include "modes/hide_seek_world.hpp"
#include "network/network_config.hpp"
#include "network/stk_host.hpp"
#include "network/protocols/client_lobby.hpp"
#include "race/highscores.hpp"
#include "race/highscore_manager.hpp"
#include "replay/replay_play.hpp"
#include "replay/replay_recorder.hpp"
#include "scriptengine/property_animator.hpp"
#include "states_screens/cutscene_general.hpp"
#include "states_screens/feature_unlocked.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/online/networking_lobby.hpp"
#include "states_screens/options/options_screen_video.hpp"
#include "states_screens/race_setup_screen.hpp"
#include "tips/tips_manager.hpp"
#include "tracks/track.hpp"
#include "tracks/track_manager.hpp"
#include "utils/profiler.hpp"
#include "utils/random_generator.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"
#include "main_loop.hpp"

#include <algorithm>

/** Constructor, initialises internal data structures. */
RaceResultGUI::RaceResultGUI() : Screen("race_result.stkgui",
    /*is_dialog*/false)
{
    m_started_race_over_music = false;
    m_time_between_rows = stk_config->m_time_between_rows;
    m_time_single_scroll = stk_config->m_time_single_scroll;
    m_time_rotation = stk_config->m_time_rotation;
    m_time_for_points = stk_config->m_time_for_points;
    m_time_overall_scroll = stk_config->m_time_overall_scroll;
    m_extra_scroll_time = stk_config->m_extra_scroll_time;
    m_distance_between_rows = stk_config->m_distance_between_rows;
    m_distance_between_meta_rows = stk_config->m_distance_between_meta_rows;
    m_width_kart_name = stk_config->m_width_kart_name;
    m_width_finish_time = stk_config->m_width_finish_time;
    m_font = GUIEngine::getFont();
    m_highscore_rank = -1000; // no highscore
}

// ... rest of file unchanged until init(), enableAllButtons(), renderGlobal() and helpers

// Keep existing content; we append only where necessary below

void RaceResultGUI::renderGlobal(float dt)
{
#ifndef SERVER_ONLY
    m_timer += dt;
    assert(World::getWorld()->getPhase() == WorldStatus::RESULT_DISPLAY_PHASE);
    unsigned int num_karts = (unsigned int)m_all_row_infos.size();
    float time_overall_scroll = m_time_overall_scroll;
    float time_for_points = m_time_for_points;

    switch (m_animation_state)
    {
    case RR_INIT:
        for (unsigned int i = 0; i < num_karts; i++)
        {
            RowInfo *ri = &(m_all_row_infos[i]);
            ri->m_start_at = m_time_between_rows * i;
            ri->m_x_pos = (float)UserConfigParams::m_width;
            ri->m_y_pos = (float)(m_top + i*m_distance_between_rows);
        }
        m_animation_state = RR_RACE_RESULT;
        break;
    case RR_RACE_RESULT:
        if (RaceManager::get()->getMajorMode() ==
            RaceManager::MAJOR_MODE_GRAND_PRIX)
            time_overall_scroll -= m_extra_scroll_time;
        if (m_timer > time_overall_scroll)
        {
            for (unsigned int i = 0; i < num_karts; i++)
            {
                RowInfo *ri = &(m_all_row_infos[i]);
                ri->m_x_pos = (float)m_leftmost_column;
            }
            if (RaceManager::get()->getMajorMode() !=
                RaceManager::MAJOR_MODE_GRAND_PRIX)
            {
                m_animation_state = RR_WAIT_TILL_END;
                enableAllButtons();
                break;
            }

            m_animation_state = RR_WAITING_GP_RESULT;
            std::vector<RowInfo> prev_infos = m_all_row_infos;
            determineGPLayout();
            m_all_row_info_waiting = m_all_row_infos;
            m_all_row_infos = prev_infos;
            GUIEngine::IconButtonWidget *middle = getWidget<GUIEngine::IconButtonWidget>("middle");
            GUIEngine::RibbonWidget *operations = getWidget<GUIEngine::RibbonWidget>("operations");
            operations->setActive(true);
            operations->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
            middle->setLabel(_("Continue"));
            middle->setImage("gui/icons/green_check.png");
            middle->setVisible(true);
            operations->select("middle", PLAYER_ID_GAME_MASTER);
        }
        break;
    case RR_WAITING_GP_RESULT:
        break;
    case RR_OLD_GP_RESULTS:
        if (m_timer > m_time_overall_scroll)
        {
            m_animation_state = RR_INCREASE_POINTS;
            m_timer = 0;
            for (unsigned int i = 0; i < num_karts; i++)
            {
                RowInfo *ri = &(m_all_row_infos[i]);
                ri->m_x_pos = (float)m_leftmost_column;
            }
        }
        break;
    case RR_INCREASE_POINTS:
        if (m_timer > 1 + time_for_points)
        {
            m_animation_state = RR_RESORT_TABLE;
            if (m_gp_position_was_changed)
                m_timer = 0;
            else
                m_timer = m_time_rotation + 1;
            for (unsigned int i = 0; i < num_karts; i++)
            {
                RowInfo *ri = &(m_all_row_infos[i]);
                ri->m_new_points = 0;
                ri->m_current_displayed_points = (float)ri->m_new_overall_points;
            }

        }
        break;
    case RR_RESORT_TABLE:
        if (m_timer > m_time_rotation)
        {
            m_animation_state = RR_WAIT_TILL_END;
            for (unsigned int i = 0; i < num_karts; i++)
            {
                RowInfo *ri = &(m_all_row_infos[i]);
                ri->m_y_pos = ri->m_centre_point - ri->m_radius;
            }
            enableAllButtons();
        }
        break;
    case RR_WAIT_TILL_END:
        if (RaceManager::get()->getMajorMode() == RaceManager::MAJOR_MODE_GRAND_PRIX)
            displayGPProgress();
        if (m_timer - m_time_rotation > 1.0f &&
            dynamic_cast<DemoWorld*>(World::getWorld()))
        {
            RaceManager::get()->exitRace();
            StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());
        }
        break;
    }   // switch

    float v = 0.9f*UserConfigParams::m_width / m_time_single_scroll;
    if (RaceManager::get()->isSoccerMode())
    {
        displaySoccerResults();
    }
    else if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_HIDE_SEEK)
    {
        displayHideSeekResults();
    }
    else if (RaceManager::get()->isCTFMode())
    {
        displayCTFResults();
    }
    else if (RaceManager::get()->isBenchmarking())
    {
        if(!UserConfigParams::m_benchmark)
        {
            displayBenchmarkSummary();
        }
        else
        {
            profiler.writeToFile();
            main_loop->requestAbort();
        }
    }
    else
    {
        for (unsigned int i = 0; i < m_all_row_infos.size(); i++)
        {
            RowInfo *ri = &(m_all_row_infos[i]);
            float x = ri->m_x_pos;
            float y = ri->m_y_pos;
            switch (m_animation_state)
            {
            case RR_INIT: break;
            case RR_RACE_RESULT:
            case RR_WAITING_GP_RESULT:
            case RR_OLD_GP_RESULTS:
                if (m_timer > ri->m_start_at)
                {   // if active
                    ri->m_x_pos -= dt*v;
                    if (ri->m_x_pos < m_leftmost_column)
                        ri->m_x_pos = (float)m_leftmost_column;
                    x = ri->m_x_pos;
                }
                break;
            case RR_INCREASE_POINTS:
            {
#ifndef NDEBUG
                WorldWithRank *wwr = dynamic_cast<WorldWithRank*>(World::getWorld());
#endif
                assert(wwr);
                ri->m_current_displayed_points += dt * m_most_points / time_for_points;
                if (ri->m_current_displayed_points > ri->m_new_overall_points)
                {
                    ri->m_current_displayed_points =
                        (float)ri->m_new_overall_points;
                }
                ri->m_new_points -= dt * m_most_points / time_for_points;
                if (ri->m_new_points < 0)
                    ri->m_new_points = 0;
                break;
            }
            case RR_RESORT_TABLE:
                x = ri->m_x_pos
                    - ri->m_radius*sinf(m_timer / m_time_rotation*M_PI);
                y = ri->m_centre_point
                    + ri->m_radius*cosf(m_timer / m_time_rotation*M_PI);
                break;
            case RR_WAIT_TILL_END:
                break;
            }   // switch
            displayOneEntry((unsigned int)x, (unsigned int)y, i, true);
        }   // for i
    }

    if (RaceManager::get()->getMajorMode() != RaceManager::MAJOR_MODE_GRAND_PRIX ||
        m_animation_state == RR_WAITING_GP_RESULT)
    {
        displayPostRaceInfo();
    }
#endif
}   // renderGlobal

void RaceResultGUI::displayHideSeekResults()
{
#ifndef SERVER_ONLY
    HideAndSeekWorld* hs = dynamic_cast<HideAndSeekWorld*>(World::getWorld());
    if (!hs) return;

    // Header: which team wins
    core::stringw result_text;
    video::SColor header_color = video::SColor(255, 255, 255, 255);
    if (hs->didSeekersWin())
    {
        result_text = _("Blue Team Wins"); // Seekers = Blue
        header_color = video::SColor(255, 100, 150, 255);
    }
    else if (hs->didHidersWin())
    {
        result_text = _("Red Team Wins"); // Hiders = Red
        header_color = video::SColor(255, 255, 120, 120);
    }
    else
    {
        result_text = _("Game Over");
    }

    gui::IGUIFont* title_font = GUIEngine::getTitleFont();
    gui::IGUIFont* font = GUIEngine::getFont();
    int center_x = UserConfigParams::m_width / 2;
    int top_y = m_top; // near top
    core::rect<s32> title_pos(center_x, top_y, center_x, top_y);
    title_font->draw(result_text.c_str(), title_pos, header_color, true, true);

    // Timer: show elapsed/total and remaining using mm:ss.xx (FFA style precision=2)
    float elapsed_f = (float)hs->getElapsedSeconds();
    float total_f = (float)hs->getTotalCapSeconds();
    float remain_f = std::max(0.0f, total_f - elapsed_f);
    core::stringw timer_text = StringUtils::toWString(
        StringUtils::timeToString(elapsed_f, 2, true, false)) +
        core::stringw(L" / ") +
        StringUtils::toWString(StringUtils::timeToString(total_f, 2, true, false)) +
        core::stringw(L"  (") +
        StringUtils::toWString(StringUtils::timeToString(remain_f, 2, true, false)) +
        core::stringw(L" ") + _("remaining") + core::stringw(L")");
    int subtitle_y = top_y + title_font->getDimension(L"Ag").Height + 10;
    core::rect<s32> subtitle_pos(center_x, subtitle_y, center_x, subtitle_y);
    font->draw(timer_text.c_str(), subtitle_pos, video::SColor(255, 230, 230, 230), true, true);

    // Columns similar to soccer
    int col_left_x  = (int)(UserConfigParams::m_width * 0.18f);
    int col_right_x = (int)(UserConfigParams::m_width * 0.82f);
    int list_y = subtitle_y + font->getDimension(L"Ag").Height + 25;

    std::vector<std::pair<irr::core::stringw, float>> found;
    std::vector<irr::core::stringw> remaining;
    hs->getFoundHiders(found);
    hs->getRemainingHiders(remaining);

    // Headers per your spec
    core::stringw left_header = _("Found");
    core::stringw right_header = _("Remaining");

    gui::ScalableFont* header_font = GUIEngine::getTitleFont();
    core::rect<s32> left_header_pos(col_left_x, list_y, col_left_x, list_y);
    core::rect<s32> right_header_pos(col_right_x, list_y, col_right_x, list_y);
    header_font->draw(left_header.c_str(), left_header_pos, video::SColor(255, 255, 255, 255), true, true);
    header_font->draw(right_header.c_str(), right_header_pos, video::SColor(255, 255, 255, 255), true, true);

    int line_h = font->getDimension(L"Aj").Height + 6;
    int y_left = list_y + header_font->getDimension(L"Ag").Height + 10;
    int y_right = y_left;

    // Left column: Found list, with mm:ss.xx times
    for (const auto& p : found)
    {
        core::stringw line = p.first + core::stringw(L"  ") +
            StringUtils::toWString(StringUtils::timeToString(p.second, 2, true, false));
        core::rect<s32> pos(col_left_x, y_left, col_left_x, y_left);
        font->draw(line.c_str(), pos, video::SColor(255, 240, 240, 240), true, true);
        y_left += line_h;
    }

    // Right column: Remaining names only
    for (const auto& nm : remaining)
    {
        core::rect<s32> pos(col_right_x, y_right, col_right_x, y_right);
        font->draw(nm.c_str(), pos, video::SColor(255, 240, 240, 240), true, true);
        y_right += line_h;
    }

    // Online client footer buttons
    if (NetworkConfig::get()->isClient())
    {
        if (GUIEngine::RibbonWidget* ops = getWidget<GUIEngine::RibbonWidget>("operations"))
        {
            if (auto left = getWidget<GUIEngine::IconButtonWidget>("left"))
            {
                left->setLabel(_("Quit server"));
                left->setImage("gui/icons/main_quit.png");
                left->setVisible(true);
            }
            if (auto right = getWidget<GUIEngine::IconButtonWidget>("right"))
            {
                right->setLabel(_("Continue"));
                right->setImage("gui/icons/green_check.png");
                right->setVisible(true);
            }
        }
    }
#endif
}

void RaceResultGUI::eventCallback(GUIEngine::Widget* widget, const std::string& name, const int playerID)
{
#ifndef SERVER_ONLY
    // Handle online HS footer buttons
    if (NetworkConfig::get()->isClient())
    {
        if (name == "left")
        {
            // Quit server: disconnect and return to online menu (main menu is acceptable)
            try
            {
                if (STKHost::existHost())
                    STKHost::get()->disconnectAllPeers(true /*timeout_waiting*/);
            }
            catch(...)
            {
                // Ignore any exception, continue cleanup
            }
            RaceManager::get()->exitRace();
            StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());
            return;
        }
        else if (name == "right")
        {
            // Continue: default behavior (let existing logic handle)
            ;
        }
    }
#endif
    Screen::eventCallback(widget, name, playerID);
}