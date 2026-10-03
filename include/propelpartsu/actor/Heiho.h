#pragma once

#include <enemy/Enemy.h>
#include <actor/Profile.h>
#include <collision/ActorCollisionDrcTouchCallback.h>
#include <graphics/JointBlendModel.h>
#include <enemy/EnemyEatData.h>
#include <enemy/EnemyChibiYoshiEatData.h>
#include <effect/EffectObj.h>
#include <enemy/EnemyBoyoMgr.h>

/*************************************************************************
            Port of PropelParts's Shyguy Actor to NSMBU
            Heiho is Shyguy in Japanese, hence the class name
*************************************************************************/

namespace propelpartsu {

class Heiho : public Enemy {
    SEAD_RTTI_OVERRIDE(Heiho, Enemy)
protected:
    enum HeihoType {
        cHeihoType_Walk = 0,
        cHeihoType_Walk_Ledge,
        cHeihoType_Sleep,
        cHeihoType_Jump,
        cHeihoType_Pace
    };

    class DrcTouchCB : public ActorCollisionDrcTouchCallback {
    public:
        bool ccSetTouchNormal(ActorCollisionCheck* cc, const sead::Vector2f& pos) override;
    };

public:
    static Profile* sProfile;

    Heiho(const ActorCreateParam& param);
    ~Heiho() override = default;

    Result create() override;
    bool execute() override;
    bool draw() override;

    void removeCollisionCheck() override;
    void reviveCollisionCheck() override;

    void allEnemyDeathEffSet() override;

    bool setDamage(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;

    bool createIceActor() override;

    void calcMdl_Base() override {
        calcMdl_();
    }

    void vsEnemyHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;
    void vsPlayerHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;
    void vsYoshiHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;
    void vsChibiYoshiHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;

    void initializeState_DieFall() override;

    void initializeState_DieOther() override;
    void executeState_DieOther() override;

    virtual void setupModel();
    virtual void drawModel();
    virtual void calcMdl_();

    virtual void setInitialState(bool reset_pacer_dist = true);

    virtual void onDrcTouch();

    virtual void setTurnByEnemyHit(Actor* actor_self, Actor* actor_other);
    virtual void setTurnByPlayerHit(Actor* player);

    virtual void reactFumiProc(Actor* player);
    virtual void reactSpinFumiProc(Actor* player);
    virtual void reactYoshiFumiProc(Actor* yoshi);

    void setWalkSpeed();
    bool checkLedge();
    void landonEffect();
    u8 checkBgIn();
    void setDeathInfo_Hasami();

    static const ActorCreateInfo cCreateInfo;
    static const ActorCollisionCheck::CollisionData cCollisionData;
    static const ActorCollisionCheck::CollisionData cCollisionData_DRC;

    static const s32 cTurnSpeed = 0x6000000;

    static const f32 cMaxSpeedX;
    static const f32 cMaxSpeedY;

    static const f32 cWalkSpeed[cDirType_NumX];

    DECLARE_STATE_ID(Heiho, Walk);
    DECLARE_STATE_ID(Heiho, Turn);
    DECLARE_STATE_ID(Heiho, Sleep);
    DECLARE_STATE_ID(Heiho, Jump);
    DECLARE_STATE_ID(Heiho, Dizzy);
    DECLARE_STATE_ID(Heiho, Touch);

private:
    JointBlendModel* mModel;
    EnemyEatData mYoshiEatData;
    EnemyChibiYoshiEatData mBabyYoshiEatData;
    f32 mEnemyHitRevX;
    EffectObj mDizzyEffect;
    ActorCollisionCheck mCollisionCheckDrcTouch;
    DrcTouchCB mDrcTouchCallback;
    EnemyBoyoMgr mBoyoMgr;

    u32 mTimer;
	f32 mBaseline;
    f32 mFinalPos[2];
    u32 mJumpCounter;
    bool mHasLanded;

    HeihoType mType;
    u8 mColor;
    u8 mHealth;
    u32 mDistance;
    DirType mSpawnDir;
};

} // namespace propelpartsu
