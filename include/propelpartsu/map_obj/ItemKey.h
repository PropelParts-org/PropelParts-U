#pragma once

#include <actor/ActorState.h>
#include <actor/Profile.h>
#include <graphics/AnimModel.h>
#include <effect/EffectObj.h>

namespace propelpartsu {

class ItemKey : public ActorState {
    SEAD_RTTI_OVERRIDE(ItemKey, ActorState)
protected:

public:
    static Profile* sProfile;

    ItemKey(const ActorCreateParam& param);
    ~ItemKey() override = default;

    Result create() override;
    bool execute() override;
    bool draw() override;

    void landonEffect();

    static void collcheck(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other);

    static const ActorCreateInfo cCreateInfo;
    static const ActorCollisionCheck::CollisionData cCollisionData;

    DECLARE_STATE_ID(ItemKey, Wait);
    DECLARE_STATE_ID(ItemKey, Hide);
    DECLARE_STATE_ID(ItemKey, GetUpDemo);
    DECLARE_STATE_ID(ItemKey, UpDemoWait);
    DECLARE_STATE_ID(ItemKey, UIJumpDemo);

private:
    AnimModel* mModel;
    sead::Vector3f mInitialPos;
    EffectObj mCollectEffect;
    u16 mUpDemoWaitCountdown;
    s8 mCollectPlayerNo;
    bool mHasLanded;
};

} // namespace propelpartsu
