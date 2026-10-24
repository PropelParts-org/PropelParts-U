#pragma once

#include <actor/ActorState.h>
#include <actor/Profile.h>
#include <actor/EatData.h>
#include <collision/ActorCollisionDrcTouchCallback.h>
#include <graphics/AnimModel.h>
#include <effect/EffectObj.h>
#include <actor/ChibiYoshiEatData.h>
#include <map_obj/ChibiYoshiAwaData.h>
#include <map_obj/MaskDraw.h>
#include <graphics/Light.h>

/*************************************************************************
            Port of PropelParts's Bomb Bro. Bomb Actor to NSMBU
*************************************************************************/

namespace propelpartsu {

class BombProjectile : public ActorState {
    SEAD_RTTI_OVERRIDE(BombProjectile, ActorState)
protected:
    class BombEatData : public EatData {
    public:
        BombEatData(ActorUniqueID owner_id) : EatData(owner_id) {}

        bool setEatSpitOut(Actor* yoshi) override;
    };

    class DrcTouchCB : public ActorCollisionDrcTouchCallback {
    public:
        bool ccSetTouchNormal(ActorCollisionCheck* cc, const sead::Vector2f& pos) override;
    };

public:
    static Profile* sProfile;

    BombProjectile(const ActorCreateParam& param);
    ~BombProjectile() override = default;

    Result create() override;
    bool execute() override;
    bool draw() override;

    void splashEffect();
    void calcMdl();
    void explodeTiles();
    void updateExplodeLight();

    DECLARE_STATE_ID(BombProjectile, ThrowWait);
    DECLARE_STATE_ID(BombProjectile, EatWait);
    DECLARE_STATE_ID(BombProjectile, Throw);
    DECLARE_STATE_ID(BombProjectile, Explode);

    static void collcheck(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other);
    static void explode_collcheck(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other);

    static const ActorCollisionCheck::CollisionData cCollisionData;
    static const ActorCollisionCheck::CollisionData cExplodeCollisionData;

    static const Angle cBaseAngleY[cDirType_NumX];

    void setReady(bool value) {
        mReadyToThrow = value;
    }

    bool getPlayerBomb() const {
        return mPlayerBomb;
    }

private:
    AnimModel* mModel;
    EffectObj mIgniteEffect;
    ActorUniqueID mOwnerID;
    BombEatData mYoshiEatData;
    ChibiYoshiEatData mChibiYoshiEatData;
    ChibiYoshiAwaData mChibiYoshiAwaData;
    DrcTouchCB mDrcTouchCallback;
    CircleLightMask mLightMask;
    Light mExplodeLight;

    bool mReadyToThrow;
    bool mPlayerBomb;
    int mExplosionTimer;

    sead::Vector3f mRootPos;
    bool mInLiquid;
    f32 mLightRadius;
};

} // namespace propelpartsu
