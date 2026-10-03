#pragma once

#include <bullet/IceBallBase.h>
#include <actor/Profile.h>

/*************************************************************************
               IceBall actor for PropelParts's Nipper Plant
      Puchi Pakkun is Nipper Plant in Japanese, hence the class name 
*************************************************************************/

namespace propelpartsu {

class IceBallPuchiPakkun : public IceBallBase {
    SEAD_RTTI_OVERRIDE(IceBallPuchiPakkun, IceBallBase)
public:
    static Profile* sProfile;

    IceBallPuchiPakkun(const ActorCreateParam& param);
    ~IceBallPuchiPakkun() override = default;

    void executeState_Move() override;

    bool initialize() override;

    void setInitialSpeed() override;

    bool iceEffect() override
    {
        return mEffect.createEffect(RP_Cmn_Iceball_0, &mPos, nullptr, &mScale);
    }

    static void collcheck(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other);

    static const ActorCollisionCheck::CollisionData cCollisionData;
};

} // namespace propelpartsu
