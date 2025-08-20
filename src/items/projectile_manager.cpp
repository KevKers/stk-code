/* patched portion only */
#include "items/projectile_manager.hpp"
#include "items/smart_cake.hpp"
// ... other includes remain as in original file ...

// Keep the rest of file unchanged; we only patch newProjectile switch

std::shared_ptr<Flyable>
    ProjectileManager::newProjectile(AbstractKart *kart,
                                     PowerupManager::PowerupType type)
{
    const std::string& uid = getUniqueIdentity(kart, type);
    auto it = m_active_projectiles.find(uid);
    if (it != m_active_projectiles.end())
    {
        it->second->onFireFlyable();
        return it->second;
    }

    std::shared_ptr<Flyable> f;
    switch(type)
    {
        case PowerupManager::POWERUP_BOWLING:
            f = std::make_shared<Bowling>(kart);
            break;
        case PowerupManager::POWERUP_PLUNGER:
            f = std::make_shared<Plunger>(kart);
            break;
        case PowerupManager::POWERUP_CAKE:
        {
            bool use_smart = (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_HIDE_SEEK)
                             && (!NetworkConfig::get()->isNetworking())
                             && (World::getWorld()->getKartTeam(kart->getWorldKartId()) == KART_TEAM_BLUE);
            if (use_smart)
                f = std::make_shared<SmartCake>(kart);
            else
                f = std::make_shared<Cake>(kart);
            break;
        }
        case PowerupManager::POWERUP_RUBBERBALL:
            f = std::make_shared<RubberBall>(kart);
            break;
        default:
            return nullptr;
    }
    f->onFireFlyable();
    m_active_projectiles[uid] = f;
    if (RewindManager::get()->isEnabled())
        f->addForRewind(uid);

    return f;
}