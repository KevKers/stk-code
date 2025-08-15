//  SuperTuxKart - a fun racing game with go-kart
//  Hide and Seek prototype world (Phase 1 scaffolding)
//  GPLv3-or-later

#ifndef HIDE_SEEK_WORLD_HPP
#define HIDE_SEEK_WORLD_HPP

#include "modes/world_with_rank.hpp"
#include "states_screens/race_gui_base.hpp"

#include <vector>
#include <string>

class NetworkString;

// Minimal scaffolding for Hide & Seek mode: no gameplay yet, just a world shell
// so we can boot the mode, render GUI, and verify lifecycle via logs.
class HideAndSeekWorld : public WorldWithRank
{
public:
    HideAndSeekWorld();
    virtual ~HideAndSeekWorld();

    // World overrides
    virtual void init() OVERRIDE;
    virtual void reset(bool restart = false) OVERRIDE;
    virtual bool isRaceOver() OVERRIDE; // Phase 1: never ends (until interrupted)
    virtual bool raceHasLaps() OVERRIDE { return false; }
    virtual void update(int ticks) OVERRIDE;
    virtual void getKartsDisplayInfo(
        std::vector<RaceGUIBase::KartIconDisplayInfo>* info) OVERRIDE;

    virtual const std::string& getIdent() const OVERRIDE;
    virtual bool useFastMusicNearEnd() const OVERRIDE { return false; }
    virtual bool shouldDrawTimer() const OVERRIDE { return true; }
};

#endif // HIDE_SEEK_WORLD_HPP
