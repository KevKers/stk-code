//  SuperTuxKart - a fun racing game with go-kart
//  Hide and Seek prototype world (Phase 1 scaffolding)
//  GPLv3-or-later

#include "modes/hide_seek_world.hpp"

#include "karts/abstract_kart.hpp"
#include "network/network_config.hpp"
#include "tracks/track.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "race/race_manager.hpp"

#include <algorithm>

HideAndSeekWorld::HideAndSeekWorld() : WorldWithRank()
{
    // For scaffolding we just run a chrono timer
    WorldStatus::setClockMode(CLOCK_CHRONO);
}

HideAndSeekWorld::~HideAndSeekWorld() {}

void HideAndSeekWorld::init()
{
    WorldWithRank::init();
    m_display_rank = false;     // no positions list for now
    m_use_highscores = false;   // no highscores in this mode
    Log::info("HideSeekWorld", "Initialized Hide and Seek world scaffolding");
}

void HideAndSeekWorld::reset(bool restart)
{
    WorldWithRank::reset(restart);
    Log::info("HideSeekWorld", "Reset world (restart=%d)", restart ? 1 : 0);
}

bool HideAndSeekWorld::isRaceOver()
{
    // Phase 1: don't end automatically, allow manual interrupt
    if (m_schedule_interrupt_race)
        return true;
    return false;
}

void HideAndSeekWorld::update(int ticks)
{
    WorldWithRank::update(ticks);
    WorldWithRank::updateTrack(ticks);
}

void HideAndSeekWorld::getKartsDisplayInfo(
    std::vector<RaceGUIBase::KartIconDisplayInfo>* info)
{
    const unsigned int kart_amount = getNumKarts();
    for (unsigned int i = 0; i < kart_amount; i++)
    {
        RaceGUIBase::KartIconDisplayInfo& rank_info = (*info)[i];
        rank_info.lap = -1; // no laps
        rank_info.m_outlined_font = true;
        rank_info.m_color = GUIEngine::getSkin()->getColor("font::normal");
        rank_info.m_text = getKart(i)->getController()->getName();
    }
}

const std::string& HideAndSeekWorld::getIdent() const
{
    return IDENT_HIDE_SEEK;
}
