//  SuperTuxKart - a fun racing game with go-kart
//  Hide and Seek mode (Phases 1-7)
//  GPLv3-or-later

#include "modes/hide_seek_world.hpp"

#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "items/powerup_manager.hpp"
#include "race/race_manager.hpp"
#include "tracks/track.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"

#include <algorithm>
#include <sstream>

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

    m_game_start_ticks = getTimeTicks();
    m_phase_start_ticks = m_game_start_ticks;

    // Establish roles
    determineRoles();
    m_hider_confirmed.assign(getNumKarts(), false);

    // Clear per-seeker state
    m_hint_uses_left.clear();
    m_hint_next_tick.clear();
    m_fire_cooldown_next_tick.clear();

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

    const int when = getTimeTicks() + stk_config->time2Ticks(5.0f);
    m_pending_elim_ticks[kart_id] = when;
    return true;
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

bool HideAndSeekWorld::manualFoundByName(const std::string& seeker_name,
                                         const std::string& target_name,
                                         float max_distance_m)
{
    // Find seeker and target by display name
    int seeker_id = -1;
    int target_id = -1;
    for (unsigned i = 0; i < getNumKarts(); ++i)
    {
        const std::string nm = StringUtils::wideToUtf8(
            getKart(i)->getController()->getName());
        if (seeker_id < 0 && nm == seeker_name) seeker_id = (int)i;
        if (target_id < 0 && nm == target_name) target_id = (int)i;
    }
    if (seeker_id < 0 || target_id < 0) return false;
    if (!m_is_seeker[seeker_id] || !m_is_hider[target_id]) return false;
    if (m_phase != PHASE_SEEK) return false;
    if (getKart(target_id)->isEliminated()) return false;

    const float dist = (getKart(seeker_id)->getXYZ() - getKart(target_id)->getXYZ()).length();
    if (dist > max_distance_m) return false;

    const std::string victim_name = StringUtils::wideToUtf8(
        getKart(target_id)->getController()->getName());
    broadcastAll(StringUtils::insertValues("Player %s has been found", victim_name.c_str()));

    const int when = getTimeTicks() + stk_config->time2Ticks(5.0f);
    m_pending_elim_ticks[target_id] = when;
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