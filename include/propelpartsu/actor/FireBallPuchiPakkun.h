#pragma once

#include <bullet/FireBallBase.h>
#include <actor/Profile.h>

/*************************************************************************
        Port of PropelParts's Nipper Plant Fireball Actor to NSMBU
      Puchi Pakkun is Nipper Plant in Japanese, hence the class name 
*************************************************************************/

namespace propelpartsu {

class FireBallPuchiPakkun : public FireBallBase {
    SEAD_RTTI_OVERRIDE(FireBallPuchiPakkun, FireBallBase)
public:
    static Profile* sProfile;

    FireBallPuchiPakkun(const ActorCreateParam& param);
    ~FireBallPuchiPakkun() override = default;

    void executeState_Move() override;

    bool initialize() override;

    void setCollisionCheck() override;

    void fireEffect() override;

    static const ActorCollisionCheck::CollisionData cCollisionData;
};

} // namespace propelpartsu
