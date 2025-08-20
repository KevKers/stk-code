//
//  SuperTuxKart - Hide & Seek Smart Cake (derived from Cake)
//  GPLv3-or-later

#include "items/smart_cake.hpp"

#include "modes/hide_seek_world.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "utils/constants.hpp"
#include "utils/log.hpp"

void SmartCake::onFireFlyable()
{
    // This is based on Cake::onFireFlyable(), with one small targeting bias
    // when in Hide & Seek and fired by a seeker: prefer nearest hider in 7–10m.
    Flyable::onFireFlyable();
    setDoTerrainInfo(false);

    float forward_offset = m_owner->getKartLength()/2.0f + getExtend().getZ()/2.0f;

    float up_velocity = m_speed/7.0f;

    // give a speed proportional to kart speed. m_speed is defined in flyable
    m_speed *= m_owner->getSpeed() / 23.0f;

    // when going backwards, decrease speed of cake by less
    if (m_owner->getSpeed() < 0) m_speed /= 3.6f;

    m_speed += 16.0f;
    if (m_speed < 1.0f) m_speed = 1.0f;

    btTransform trans = m_owner->getTrans();

    // Find closest kart in front of the current one
    const bool backwards = m_owner->getControls().getLookBack();
    const AbstractKart* closest_kart = nullptr;
    Vec3 direction;
    float kart_dist_squared = 0.0f;
    getClosestKart(&closest_kart, &kart_dist_squared, &direction,
                   m_owner /* search in front of this kart */, backwards);

    // Bias toward the nearest hider in 7–10m when in Hide & Seek and owner is seeker
    if (RaceManager::get()->getMinorMode() == RaceManager::MINOR_MODE_HIDE_SEEK)
    {
        HideAndSeekWorld* hs = dynamic_cast<HideAndSeekWorld*>(World::getWorld());
        if (hs)
        {
            int my_id = m_owner->getWorldKartId();
            if (World::getWorld()->getKartTeam(my_id) == KART_TEAM_BLUE)
            {
                int hid_id = -1;
                float d = hs->getNearestHiderDistanceFrom(my_id, &hid_id);
                if (hid_id >= 0 && d >= 7.0f && d <= 10.0f)
                {
                    closest_kart = World::getWorld()->getKart(hid_id);
                    direction = (closest_kart->getXYZ() - m_owner->getXYZ()).normalized();
                    kart_dist_squared = (closest_kart->getXYZ() - m_owner->getXYZ()).length2();
                }
            }
        }
    }

    // Aim solution copied from Cake
    if (closest_kart != nullptr && m_speed > closest_kart->getSpeed())
    {
        float fire_angle = 0.0f;
        getLinearKartItemIntersection(m_owner->getXYZ(), closest_kart,
                                      m_speed, 9.8f /*gravity*/, forward_offset,
                                      &fire_angle, &up_velocity);
        btQuaternion q;
        q = trans.getRotation() * btQuaternion(btVector3(0, 1, 0), fire_angle);
        trans.setRotation(q);
    }

    trans.setOrigin(trans.getOrigin() + trans.getBasis() * Vec3(0, 0.2f, 0));
    setTrans(trans);

    setVelocity(Vec3(sinf(m_owner->getHeading())*m_speed,
                     up_velocity,
                     cosf(m_owner->getHeading())*m_speed));
}