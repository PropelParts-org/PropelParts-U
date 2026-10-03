#pragma once

#include <enemy/Enemy.h>
#include <actor/Profile.h>
#include <collision/ActorCollisionDrcTouchCallback.h>
#include <graphics/AnimModel.h>
#include <enemy/EnemyEatData.h>
#include <enemy/EnemyChibiYoshiEatData.h>
#include <enemy/EnemyBoyoMgr.h>

/*************************************************************************
            Port of PropelParts's Nipper Plant Actor to NSMBU
      Puchi Pakkun is Nipper Plant in Japanese, hence the class name
*************************************************************************/

namespace propelpartsu {

class PuchiPakkun : public Enemy {
    SEAD_RTTI_OVERRIDE(PuchiPakkun, Enemy)
protected:
    class DrcTouchCB : public ActorCollisionDrcTouchCallback {
    public:
        bool ccSetTouchNormal(ActorCollisionCheck* cc, const sead::Vector2f& pos) override;
    };
public:
    static Profile* sProfile;

    PuchiPakkun(const ActorCreateParam& param);
    ~PuchiPakkun() override = default;

    Result create() override;
    bool execute() override;
    bool draw() override;

    void removeCollisionCheck() override;
    void reviveCollisionCheck() override;

    void allEnemyDeathEffSet() override;

    bool createIceActor() override;

    void calcMdl_Base() override {
        calcMdl_();
    }

    void vsEnemyHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;
    void vsChibiYoshiHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;

    bool hitCallback_Ice(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;

    void initializeState_DieFall() override;

    void initializeState_Ice() override;
    void finalizeState_Ice() override;

    void calcMdl_();
    void syncAnim();
    void setWalkSpeed();
    bool checkLedge();
    void landonEffect();
    bool isPlayerAbove();
    bool isPlayerInFireRange();
    void setFireDistance(float distance);
    u8 checkBgIn();
    void setDeathInfo_Hasami();

    static const ActorCreateInfo cCreateInfo;
    static const ActorCollisionCheck::CollisionData cCollisionData;
    static const ActorCollisionCheck::CollisionData cCollisionData_DRC;

    static const Angle cBaseAngleY[cDirType_NumX];
    static const Angle cBaseAngleYTurn[cDirType_NumX];

    static const s32 cTurnSpeed = 0x8000000;

    static const f32 cMaxSpeedX;
    static const f32 cMaxSpeedY;

    static const f32 cWalkSpeed[cDirType_NumX];

    DECLARE_STATE_ID(PuchiPakkun, Idle);
    DECLARE_STATE_ID(PuchiPakkun, Walk);
    DECLARE_STATE_ID(PuchiPakkun, Turn);
    DECLARE_STATE_ID(PuchiPakkun, Jump);
    DECLARE_STATE_ID(PuchiPakkun, FireSpit);
    DECLARE_STATE_ID(PuchiPakkun, IceWait);

private:
    AnimModel* mModel;
    EnemyEatData mYoshiEatData;
    EnemyChibiYoshiEatData mBabyYoshiEatData;
    ActorCollisionCheck mCollisionCheckDrcTouch;
    DrcTouchCB mDrcTouchCallback;
    EnemyBoyoMgr mBoyoMgr;

    bool mHasLanded;
    bool mIsBahJump;
    DirType mFireDirection;
    u8 mFireDist;
    u32 mFireTimer;
    u32 mSpatFireCount;
    u32 mFireCooldown;

    bool mWalks;
    bool mSpitsFire;
    bool mSpitsIce;
    u8 mFireHeight;
    u32 mJumpHeight;
};

} // namespace propelpartsu
