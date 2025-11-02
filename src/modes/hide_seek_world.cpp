//  SuperTuxKart - a fun racing game with go-kart
//  Hide and Seek mode (Fully Implemented)
//  GPLv3-or-later

#include "modes/hide_seek_world.hpp"

#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "items/powerup_manager.hpp"
#include "race/race_manager.hpp"
#include "tracks/track.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "network/network_config.hpp"
#include "network/network_string.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"

#include <algorithm>
#include <sstream>
#include <limits>

HideAndSeekWorld::HideAndSeekWorld() : WorldWithRank()
{
    WorldStatus::setClockMode(CLOCK_CHRONO);
    m_phase = PHASE_HIDE;
    m_phase_start_ticks = 0;
    m_game_start_ticks = 0;
    m_hide_phase_seconds_current = 180;
    m_total_cap_seconds = 900;
    m_saved_prev_hide_time = -1;
    m_hint_max_uses_this_round = 0;
    m_hint_unlock_seconds = 0;
    m_seekers_win = false;
    m_race_over_set = false;
}

HideAndSeekWorld::~HideAndSeekWorld()
{
    // Restore hide time back to server_config.xml current value kept at init
    if (m_saved_prev_hide_time >= 0)
        ServerConfig::m_hs_hide_time = m_saved_prev_hide_time;
}

void HideAndSeekWorld::init()
{
    WorldWithRank::init();
    m_display_rank = false;     // no positions list
    m_use_highscores = false;   // no highscores in this mode

    // Snapshot server config values for this round
    m_saved_prev_hide_time = (int)ServerConfig::m_hs_hide_time;
    // Use current configured time for this round
    m_hide_phase_seconds_current = (int)ServerConfig::m_hs_hide_time;
    m_total_cap_seconds          = (int)ServerConfig::m_hs_total_time_cap;

    // Phase 5: copy hint settings
    m_hint_max_uses_this_round = (int)ServerConfig::m_hs_hint_default_uses;
    m_hint_unlock_seconds      = (int)ServerConfig::m_hs_hint_unlock_seconds;

    // Use a countdown clock from total cap (like FFA)
    WorldStatus::setClockMode(WorldStatus::CLOCK_COUNTDOWN, (float)m_total_cap_seconds);

    m_game_start_ticks = getTimeTicks();
    m_phase_start_ticks = m_game_start_ticks;

    // Establish roles
    determineRoles();
    m_hider_confirmed.assign(getNumKarts(), false);

    // Clear per-seeker state
    m_hint_uses_left.clear();
    m_hint_next_tick.clear();
    m_fire_cooldown_next_tick.clear();

    // Init found-time vector for hiders
    m_hider_found_time_sec.assign(getNumKarts(), -1.0f);

    Log::info("HideSeekWorld", "Initialized Hide and Seek (hide=%ds, cap=%ds).",
              m_hide_phase_seconds_current, m_total_cap_seconds);
}

void HideAndSeekWorld::reset(bool restart)
{
    WorldWithRank::reset(restart);
}

bool HideAndSeekWorld::isRaceOver()
{
    // Ends if all hiders eliminated or total cap reached
    const int elapsed_ticks = getTimeTicks() - m_game_start_ticks;
    const int cap_ticks = stk_config->time2Ticks((float)m_total_cap_seconds);

    if (elapsed_ticks >= cap_ticks)
        return true;

    if (allHidersEliminated())
        return true;

    if (m_schedule_interrupt_race)
        return true;

    return false;
}

void HideAndSeekWorld::update(int ticks)
{
    WorldWithRank::update(ticks);
    WorldWithRank::updateTrack(ticks);

    // Enforce freezes in hide phase
    if (m_phase == PHASE_HIDE)
    {
        // Freeze all seekers
        for (unsigned i = 0; i < getNumKarts(); ++i)
        {
            if (m_is_seeker[i] && !getKart(i)->isEliminated())
                freezeKart(getKart(i));
            if (m_is_hider[i] && m_hider_confirmed[i] && !getKart(i)->isEliminated())
                freezeKart(getKart(i));
        }

        // Early transition if all hiders confirmed
        if (allHidersConfirmed())
            transitionToSeek();
        else
        {
            const int elapsed = getTimeTicks() - m_phase_start_ticks;
            const int hide_ticks = stk_config->time2Ticks((float)m_hide_phase_seconds_current);
            if (elapsed >= hide_ticks)
                transitionToSeek();
        }
    }

    // Process pending eliminations
    if (!m_pending_elim_ticks.empty())
    {
        const int now = getTimeTicks();
        std::vector<int> to_remove;
        to_remove.reserve(m_pending_elim_ticks.size());
        for (auto& kv : m_pending_elim_ticks)
        {
            if (now >= kv.second)
            {
                eliminateKart(kv.first, false /*silent*/);
                // Move peer to lobby if networked
                if (auto sl = LobbyProtocol::get<ServerLobby>())
                {
                    auto name = getKart(kv.first)->getController()->getName();
                    auto peer = STKHost::get()->findPeerByName(name);
                    if (peer)
                    {
                        NetworkString* back_lobby = sl->getNetworkString(2);
                        back_lobby->setSynchronous(true);
                        back_lobby->addUInt8(LobbyProtocol::LE_BACK_LOBBY)
                                  .addUInt8(LobbyProtocol::BLR_SPECTATING_NEXT_GAME);
                        peer->sendPacket(back_lobby, /*reliable*/true);
                        delete back_lobby;
                    }
                }
                to_remove.push_back(kv.first);
            }
        }
        for (int k : to_remove) m_pending_elim_ticks.erase(k);
    }
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

bool HideAndSeekWorld::kartHit(int kart_id, int hitter)
{
    if (kart_id < 0 || hitter < 0) return false;
    if (kart_id >= (int)getNumKarts() || hitter >= (int)getNumKarts()) return false;
    if (m_phase != PHASE_SEEK) return false;

    AbstractKart* victim = getKart(kart_id);
    AbstractKart* attacker = getKart(hitter);
    if (!victim || !attacker) return false;

    if (victim->isEliminated()) return false;

    const bool victim_is_hider = m_is_hider[kart_id];
    const bool attacker_is_seeker = m_is_seeker[hitter];

    // Only process special HS logic for seekers using cake
    if (attacker_is_seeker && attacker->getLastUsedPowerup() == PowerupManager::POWERUP_CAKE)
    {
        if (!victim_is_hider)
        {
            // Refund on non-hider hit and apply cooldown
            int count = attacker->getPowerup()->getNum();
            attacker->setPowerup(PowerupManager::POWERUP_CAKE, count + 1);
            applySeekerRefireCooldown(hitter, 1.5f);
            return false;
        }
    }

    if (!victim_is_hider || !attacker_is_seeker) return false;

    // Only valid if the last used by attacker is CAKE
    if (attacker->getLastUsedPowerup() != PowerupManager::POWERUP_CAKE)
        return false;

    // Announce globally and schedule elimination in 5s
    const std::string victim_name = StringUtils::wideToUtf8(
        victim->getController()->getName());
    broadcastAll(StringUtils::insertValues("Player %s has been found", victim_name.c_str()));

    // Record found time once
    if (m_hider_found_time_sec[kart_id] < 0.0f)
    {
        float elapsed_sec = stk_config->ticks2Time(getTimeTicks() - m_game_start_ticks);
        m_hider_found_time_sec[kart_id] = elapsed_sec;
    }

    float delay = (float)ServerConfig::m_hs_elimination_delay;
    const int when = getTimeTicks() + stk_config->time2Ticks(delay);
    m_pending_elim_ticks[kart_id] = when;
    return true;
}

void HideAndSeekWorld::enterRaceOverState()
{
    // Determine winner and store for GUI
    const int elapsed_ticks = getTimeTicks() - m_game_start_ticks;
    const int cap_ticks = stk_config->time2Ticks((float)m_total_cap_seconds);
    if (allHidersEliminated())
        m_seekers_win = true;
    else if (elapsed_ticks >= cap_ticks)
        m_seekers_win = false; // hiders survive till cap
    else
        m_seekers_win = false; // default safety
    m_race_over_set = true;

    // Set race result per kart so win/lose animations/music work
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        bool win = false;
        if (m_is_hider[i]) win = !m_seekers_win;
        else if (m_is_seeker[i]) win = m_seekers_win;
        AbstractKart* k = getKart(i);
        if (k && !k->isGhostKart())
            k->setRaceResult();
    }

    WorldWithRank::enterRaceOverState();
}

int HideAndSeekWorld::getElapsedSeconds() const
{
    return stk_config->ticks2Time(getTimeTicks() - m_game_start_ticks);
}

bool HideAndSeekWorld::confirmHiderKart(int kart_id)
{
    if (kart_id < 0 || kart_id >= (int)getNumKarts()) return false;
    if (!m_is_hider[kart_id]) return false;
    if (m_phase != PHASE_HIDE) return false;
    if (m_hider_confirmed[kart_id]) return false;

    m_hider_confirmed[kart_id] = true;

    // Minimal VFX without SFX
    getKart(kart_id)->showZipperFire();

    // Broadcast globally
    const std::string name = StringUtils::wideToUtf8(
        getKart(kart_id)->getController()->getName());
    broadcastAll(StringUtils::insertValues("Player %s is ready!", name.c_str()));
    return true;
}

void HideAndSeekWorld::determineRoles()
{
    const unsigned n = getNumKarts();
    m_is_hider.assign(n, false);
    m_is_seeker.assign(n, false);

    for (unsigned i = 0; i < n; ++i)
    {
        KartTeam t = getKartTeam(i);
        if (t == KART_TEAM_RED) m_is_hider[i] = true;
        else if (t == KART_TEAM_BLUE) m_is_seeker[i] = true;
    }
}

void HideAndSeekWorld::transitionToSeek()
{
    if (m_phase == PHASE_SEEK) return;
    m_phase = PHASE_SEEK;
    m_phase_start_ticks = getTimeTicks();

    giveSeekerLoadout();
}

void HideAndSeekWorld::freezeKart(AbstractKart* k)
{
    if (!k) return;
    KartControl& c = k->getControls();
    c.setAccel(0.0f);
    c.setBrake(true);
    c.setSteer(0.0f);
    c.setNitro(false);
    c.setSkidControl(KartControl::SC_NONE);
    k->setSpeed(0.0f);
}

void HideAndSeekWorld::giveSeekerLoadout()
{
    // Give each seeker N cakes where N == number of (alive) hiders
    unsigned alive_hiders = 0;
    for (unsigned i = 0; i < getNumKarts(); ++i)
        if (m_is_hider[i] && !getKart(i)->isEliminated()) alive_hiders++;

    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (!m_is_seeker[i]) continue;
        AbstractKart* k = getKart(i);
        if (!k || k->isEliminated()) continue;
        k->setPowerup(PowerupManager::POWERUP_CAKE, (int)alive_hiders);
    }
}

bool HideAndSeekWorld::allHidersConfirmed() const
{
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (m_is_hider[i] && !getKart(i)->isEliminated())
        {
            if (!m_hider_confirmed[i]) return false;
        }
    }
    return true;
}

bool HideAndSeekWorld::allHidersEliminated() const
{
    for (unsigned i = 0; i < getNumKarts(); ++i)
        if (m_is_hider[i] && !getKart(i)->isEliminated()) return false;
    return true;
}

void HideAndSeekWorld::broadcastAll(const std::string& msg)
{
    if (auto sl = LobbyProtocol::get<ServerLobby>())
        sl->sendStringToAllPeers(msg);
}

float HideAndSeekWorld::getNearestHiderDistanceFrom(int seeker_world_id, int* out_hider_world_id) const
{
    if (seeker_world_id < 0 || seeker_world_id >= (int)getNumKarts()) return 1e9f;
    const Vec3 sxyz = getKart(seeker_world_id)->getXYZ();
    float best = 1e9f;
    int best_id = -1;
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (!m_is_hider[i]) continue;
        if (getKart(i)->isEliminated()) continue;
        float d = (sxyz - getKart(i)->getXYZ()).length();
        if (d < best) { best = d; best_id = (int)i; }
    }
    if (out_hider_world_id) *out_hider_world_id = best_id;
    return best;
}

bool HideAndSeekWorld::handleHintFor(const std::string& seeker_name_utf8,
                                     std::string& out_message)
{
    out_message.clear();
    // Find seeker id
    int seeker_id = -1;
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        const std::string nm = StringUtils::wideToUtf8(getKart(i)->getController()->getName());
        if (nm == seeker_name_utf8) { seeker_id = (int)i; break; }
    }
    if (seeker_id < 0) { out_message = "Unknown player."; return false; }
    if (!m_is_seeker[seeker_id]) { out_message = "Only seekers can use /hint."; return false; }
    if (m_phase != PHASE_SEEK) { out_message = "Hints available only during seek phase."; return false; }

    // Unlock gating based on server_config unlock seconds
    int elapsed = getTimeTicks() - m_game_start_ticks;
    if (elapsed < stk_config->time2Ticks((float)m_hint_unlock_seconds))
    {
        out_message = "Hints are not available yet.";
        return false;
    }

    // Per-seeker cooldown and uses
    int now = getTimeTicks();
    auto it_next = m_hint_next_tick.find(seeker_id);
    if (it_next != m_hint_next_tick.end() && now < it_next->second)
    {
        int remain = stk_config->ticks2Time(it_next->second - now);
        std::ostringstream oss; oss << "Please wait " << remain << "s.";
        out_message = oss.str();
        return false;
    }

    if (m_hint_uses_left.find(seeker_id) == m_hint_uses_left.end())
        m_hint_uses_left[seeker_id] = m_hint_max_uses_this_round;

    if (m_hint_uses_left[seeker_id] <= 0)
    {
        out_message = "No more hints left.";
        return false;
    }

    // Compute nearest and buckets
    float hot = (float)ServerConfig::m_hs_hot_threshold;
    float wmin = (float)ServerConfig::m_hs_warm_min;
    float wmax = (float)ServerConfig::m_hs_warm_max;

    const Vec3 sxyz = getKart(seeker_id)->getXYZ();
    float nearest = 1e9f;
    int hot_cnt = 0, warm_cnt = 0, cold_cnt = 0;

    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (!m_is_hider[i] || getKart(i)->isEliminated()) continue;
        float d = (sxyz - getKart(i)->getXYZ()).length();
        if (d < nearest) nearest = d;
        if (d < hot) hot_cnt++;
        else if (d >= wmin && d <= wmax) warm_cnt++;
        else cold_cnt++;
    }

    if (nearest > 1e8f)
    {
        out_message = "No hiders remaining.";
        return false;
    }

    // Build message: nearest label and counts for hot/warm if multiple
    std::ostringstream oss;
    if (nearest < hot)
    {
        if (hot_cnt > 1) oss << hot_cnt << " players are Hot";
        else oss << "Hot";
    }
    else if (nearest >= wmin && nearest <= wmax)
    {
        if (warm_cnt > 1) oss << warm_cnt << " players are Warm";
        else oss << "Warm";
    }
    else
    {
        oss << "Cold";
    }

    out_message = oss.str();

    // Spend one use and set cooldown (20s)
    m_hint_uses_left[seeker_id] -= 1;
    m_hint_next_tick[seeker_id] = now + stk_config->time2Ticks(20.0f);
    return true;
}

bool HideAndSeekWorld::shouldAllowSeekerFire(int seeker_world_id)
{
    if (seeker_world_id < 0 || seeker_world_id >= (int)getNumKarts()) return true;
    if (!m_is_seeker[seeker_world_id]) return true;
    if (m_phase != PHASE_SEEK) return false;

    int now = getTimeTicks();
    auto it = m_fire_cooldown_next_tick.find(seeker_world_id);
    if (it != m_fire_cooldown_next_tick.end() && now < it->second)
        return false;

    float nearest = getNearestHiderDistanceFrom(seeker_world_id, nullptr);
    float allow_d = (float)ServerConfig::m_hs_fire_distance_threshold;
    return nearest <= allow_d;
}

void HideAndSeekWorld::applySeekerRefireCooldown(int seeker_world_id, float seconds)
{
    m_fire_cooldown_next_tick[seeker_world_id] = getTimeTicks() + stk_config->time2Ticks(seconds);
}

void HideAndSeekWorld::getFoundHiders(std::vector<std::pair<irr::core::stringw, float>>& out) const
{
    out.clear();
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (!m_is_hider[i]) continue;
        float t = m_hider_found_time_sec[i];
        if (t >= 0.0f)
        {
            irr::core::stringw nm = getKart(i)->getController()->getName();
            out.emplace_back(nm, t);
        }
    }
}

void HideAndSeekWorld::getRemainingHiders(std::vector<irr::core::stringw>& out) const
{
    out.clear();
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (!m_is_hider[i]) continue;
        if (getKart(i)->isEliminated()) continue; // already eliminated
        if (m_hider_found_time_sec[i] < 0.0f) // never found yet
        {
            out.push_back(getKart(i)->getController()->getName());
        }
    }
}

void HideAndSeekWorld::saveCompleteState(BareNetworkString* bns, STKPeer* peer)
{
    bns->addUInt8(m_phase);
    bns->addUInt32(m_phase_start_ticks);
    bns->addUInt32(m_game_start_ticks);
    bns->addUInt32(m_hide_phase_seconds_current);
    bns->addUInt32(m_total_cap_seconds);
    bns->addUInt32(m_hint_max_uses_this_round);
    bns->addUInt32(m_hint_unlock_seconds);
    bns->addUInt8(m_seekers_win ? 1 : 0);
    bns->addUInt8(m_race_over_set ? 1 : 0);

    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        bns->addUInt8(m_is_hider[i] ? 1 : 0);
        bns->addUInt8(m_is_seeker[i] ? 1 : 0);
        bns->addUInt8(m_hider_confirmed[i] ? 1 : 0);
        bns->addFloat(m_hider_found_time_sec[i]);
    }

    bns->addUInt32((uint32_t)m_pending_elim_ticks.size());
    for (const auto& kv : m_pending_elim_ticks)
    {
        bns->addUInt32(kv.first);
        bns->addUInt32(kv.second);
    }

    bns->addUInt32((uint32_t)m_hint_uses_left.size());
    for (const auto& kv : m_hint_uses_left)
    {
        bns->addUInt32(kv.first);
        bns->addUInt32(kv.second);
    }

    bns->addUInt32((uint32_t)m_hint_next_tick.size());
    for (const auto& kv : m_hint_next_tick)
    {
        bns->addUInt32(kv.first);
        bns->addUInt32(kv.second);
    }

    bns->addUInt32((uint32_t)m_fire_cooldown_next_tick.size());
    for (const auto& kv : m_fire_cooldown_next_tick)
    {
        bns->addUInt32(kv.first);
        bns->addUInt32(kv.second);
    }
}

void HideAndSeekWorld::restoreCompleteState(const BareNetworkString& b)
{
    m_phase = (PhaseHS)b.getUInt8();
    m_phase_start_ticks = b.getUInt32();
    m_game_start_ticks = b.getUInt32();
    m_hide_phase_seconds_current = b.getUInt32();
    m_total_cap_seconds = b.getUInt32();
    m_hint_max_uses_this_round = b.getUInt32();
    m_hint_unlock_seconds = b.getUInt32();
    m_seekers_win = b.getUInt8() != 0;
    m_race_over_set = b.getUInt8() != 0;

    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        m_is_hider[i] = b.getUInt8() != 0;
        m_is_seeker[i] = b.getUInt8() != 0;
        m_hider_confirmed[i] = b.getUInt8() != 0;
        m_hider_found_time_sec[i] = b.getFloat();
    }

    m_pending_elim_ticks.clear();
    uint32_t pending_count = b.getUInt32();
    for (uint32_t i = 0; i < pending_count; ++i)
    {
        int kart_id = b.getUInt32();
        int tick = b.getUInt32();
        m_pending_elim_ticks[kart_id] = tick;
    }

    m_hint_uses_left.clear();
    uint32_t hint_uses_count = b.getUInt32();
    for (uint32_t i = 0; i < hint_uses_count; ++i)
    {
        int seeker_id = b.getUInt32();
        int uses = b.getUInt32();
        m_hint_uses_left[seeker_id] = uses;
    }

    m_hint_next_tick.clear();
    uint32_t hint_next_count = b.getUInt32();
    for (uint32_t i = 0; i < hint_next_count; ++i)
    {
        int seeker_id = b.getUInt32();
        int tick = b.getUInt32();
        m_hint_next_tick[seeker_id] = tick;
    }

    m_fire_cooldown_next_tick.clear();
    uint32_t fire_cd_count = b.getUInt32();
    for (uint32_t i = 0; i < fire_cd_count; ++i)
    {
        int seeker_id = b.getUInt32();
        int tick = b.getUInt32();
        m_fire_cooldown_next_tick[seeker_id] = tick;
    }
}

std::pair<uint32_t, uint32_t> HideAndSeekWorld::getGameStartedProgress() const
{
    std::pair<uint32_t, uint32_t> progress(
        std::numeric_limits<uint32_t>::max(),
        std::numeric_limits<uint32_t>::max());

    const int elapsed_ticks = getTimeTicks() - m_game_start_ticks;
    const int elapsed_sec = stk_config->ticks2Time(elapsed_ticks);
    progress.first = (uint32_t)elapsed_sec;

    unsigned int total_hiders = 0;
    unsigned int found_hiders = 0;
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        if (m_is_hider[i])
        {
            total_hiders++;
            if (getKart(i)->isEliminated() || m_hider_found_time_sec[i] >= 0.0f)
                found_hiders++;
        }
    }

    if (total_hiders > 0)
    {
        progress.second = (uint32_t)((float)found_hiders / (float)total_hiders * 100.0f);
    }

    return progress;
}

void HideAndSeekWorld::addReservedKart(int kart_id)
{
    WorldWithRank::addReservedKart(kart_id);
    if (kart_id >= 0 && kart_id < (int)m_hider_found_time_sec.size())
    {
        m_hider_found_time_sec[kart_id] = -1.0f;
    }
}

void HideAndSeekWorld::terminateRace()
{
    const unsigned int kart_amount = getNumKarts();
    for (unsigned int i = 0; i < kart_amount; ++i)
    {
        getKart(i)->finishedRace(0.0f, true);
    }
    WorldWithRank::terminateRace();
}

void HideAndSeekWorld::countdownReachedZero()
{
    m_time_ticks = 0;
    m_time = 0.0f;
}