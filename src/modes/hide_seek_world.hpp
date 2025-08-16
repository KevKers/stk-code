//  SuperTuxKart - a fun racing game with go-kart
//  Hide and Seek mode (Phases 1-4)
//  GPLv3-or-later

#ifndef HIDE_SEEK_WORLD_HPP
#define HIDE_SEEK_WORLD_HPP

#include "modes/world_with_rank.hpp"
#include "states_screens/race_gui_base.hpp"
#include "network/protocols/lobby_protocol.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include "config/stk_config.hpp"

#include <vector>
#include <string>
#include <unordered_map>

class NetworkString;
class AbstractKart;

class HideAndSeekWorld : public WorldWithRank
{
public:
    enum PhaseHS { PHASE_HIDE = 0, PHASE_SEEK = 1 };

    HideAndSeekWorld();
    virtual ~HideAndSeekWorld();

    // World overrides
    virtual void init() OVERRIDE;
    virtual void reset(bool restart = false) OVERRIDE;
    virtual bool isRaceOver() OVERRIDE;
    virtual bool raceHasLaps() OVERRIDE { return false; }
    virtual void update(int ticks) OVERRIDE;
    virtual void getKartsDisplayInfo(
        std::vector<RaceGUIBase::KartIconDisplayInfo>* info) OVERRIDE;

    virtual const std::string& getIdent() const OVERRIDE;
    virtual bool useFastMusicNearEnd() const OVERRIDE { return false; }
    virtual bool shouldDrawTimer() const OVERRIDE { return true; }
    virtual bool haveBonusBoxes() OVERRIDE { return false; } // disable pickups

    // Projectile hit hook
    virtual bool kartHit(int kart_id, int hitter = -1) OVERRIDE;

    // Command helpers
    bool confirmHiderKart(int kart_id);
    bool manualFoundByName(const std::string& seeker_name_utf8,
                           const std::string& target_name_utf8,
                           float max_distance_m);

    // Hint handling for /hint
    bool handleHintFor(const std::string& seeker_name_utf8,
                       std::string& out_message);

private:
    // Phase/time
    PhaseHS m_phase;
    int     m_phase_start_ticks;
    int     m_game_start_ticks;

    // Per-mode config snapshot
    int     m_hide_phase_seconds_current; // snapshot for this round
    int     m_total_cap_seconds;
    int     m_saved_prev_hide_time; // to restore one-shot admin change

    // Roles and state
    std::vector<bool> m_is_hider;          // by world kart id
    std::vector<bool> m_is_seeker;         // by world kart id
    std::vector<bool> m_hider_confirmed;   // during hide phase

    // Pending eliminations (kart id -> ticks when to eliminate)
    std::unordered_map<int,int> m_pending_elim_ticks;

    // Helpers
    void determineRoles();
    void transitionToSeek();
    void freezeKart(AbstractKart* k);
    void giveSeekerLoadout();
    bool allHidersConfirmed() const;
    bool allHidersEliminated() const;
    void broadcastAll(const std::string& msg);

    // Per-seeker hint usage/cooldown
    std::unordered_map<int,int> m_hint_uses_left;
    std::unordered_map<int,int> m_hint_next_tick;
};

#endif // HIDE_SEEK_WORLD_HPP