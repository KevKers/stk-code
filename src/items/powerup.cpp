//
//  SuperTuxKart - a fun racing game with go-kart
//  (patched gating for Hide & Seek seekers only for CAKE)

#include "items/powerup.hpp"
// ... keep existing includes ...

void Powerup::use()
{
    const int ticks = World::getWorld()->getTicksSinceStart();
    bool has_played_sound = false;
    auto it = m_played_sound_ticks.find(ticks);
    if (it != m_played_sound_ticks.end())
        has_played_sound = true;
    else
        m_played_sound_ticks.insert(ticks);

    const KartProperties *kp = m_kart->getKartProperties();

    if (m_type != PowerupManager::POWERUP_NOTHING      &&
        m_kart->getController()->canGetAchievements()    )
    {
        PlayerManager::increaseAchievement(AchievementsStatus::POWERUP_USED, 1);
        if (RaceManager::get()->isLinearRaceMode())
            PlayerManager::increaseAchievement(AchievementsStatus::POWERUP_USED_1RACE, 1);
    }

    if (m_type != PowerupManager::POWERUP_NOTHING &&
        m_type != PowerupManager::POWERUP_SWATTER &&
        m_type != PowerupManager::POWERUP_ZIPPER)
        m_kart->playCustomSFX(SFXManager::CUSTOM_SHOOT);

    if (!has_played_sound && m_sound_use == NULL)
    {
        m_sound_use = SFXManager::get()->createSoundSource("shoot");
    }

    m_number--;

    // HS Phase 7: Only apply smart gating to cake in Hide & Seek
    if (m_type == PowerupManager::POWERUP_CAKE)
    {
        World* world = World::getWorld();
        if (world && RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_HIDE_SEEK)
        {
            HideAndSeekWorld* hs = dynamic_cast<HideAndSeekWorld*>(world);
            if (hs)
            {
                int wid = m_kart->getWorldKartId();
                if (world->getKartTeam(wid) == KART_TEAM_BLUE)
                {
                    if (!hs->shouldAllowSeekerFire(wid))
                    {
                        setNum(m_number + 1);
                        hs->applySeekerRefireCooldown(wid, 1.5f);
                        return;
                    }
                }
            }
        }
    }

    World *world = World::getWorld();
    ItemManager* im = Track::getCurrentTrack()->getItemManager();
    switch (m_type)
    {
        // ... keep the original switch content unchanged ...
    }

    if ( m_number <= 0 )
    {
        m_number = 0;
        m_type   = PowerupManager::POWERUP_NOTHING;
    }
}