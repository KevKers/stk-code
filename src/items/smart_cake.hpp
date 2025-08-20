//
//  SuperTuxKart - Hide & Seek Smart Cake (derived from Cake)
//  GPLv3-or-later

#ifndef HEADER_SMART_CAKE_HPP
#define HEADER_SMART_CAKE_HPP

#include "items/cake.hpp"

class SmartCake : public Cake
{
public:
    explicit SmartCake(AbstractKart* kart) : Cake(kart) {}
    virtual void onFireFlyable() OVERRIDE;
};

#endif // HEADER_SMART_CAKE_HPP