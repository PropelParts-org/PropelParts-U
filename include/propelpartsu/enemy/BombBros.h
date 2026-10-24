#pragma once

#include <enemy/BrosBase.h>
#include <actor/Profile.h>
#include <input/InputMgr.h>
#include <propelpartsu/bullet/BombProjectile.h>
#include <actor/ActorMgr.h>

/*************************************************************************
              Port of PropelParts's Bomb Bro. Actor to NSMBU
*************************************************************************/

namespace propelpartsu {

class BombBros : public BrosBase {
    SEAD_RTTI_OVERRIDE(BombBros, BrosBase)
public:
    static Profile* sProfile;

    BombBros(const ActorCreateParam& param);
    ~BombBros() override = default;

    Result create() override;

    f32 getCreateWeaponFrm() const override {
        return 1.875f;
    }

    bool hitCallback_Shell(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) override;

    void initializeState_Ice() override;

    sead::Vector3f getCreateWeaponPos() override;

    f32 getAttackFrm() const override {
        return 30.0f;
    }

    void setSpeed() override {
        mSpeed.x = (mMoveDir) ? -0.6f : 0.6f;
        mSpeed.y = 0.0f;
    }

    void setAtkTimer() override {
        mAtkTimer = GAME_RANDOM.getU32(50) + 10;
    }

    void weaponCreate() override {
        ActorCreateParam bomb;
        bomb.profile = BombProjectile::sProfile;
        bomb.param_0 = mActorUniqueID.getValue();
        bomb.position = getCreateWeaponPos();
        bomb.param_ex_0.course.layer = mLayer;
        mWeaponID = ActorMgr::instance()->createImmediately(bomb)->getActorUniqueID();
    }

    void weaponAttack() override {
        BombProjectile* bomb = ActorMgr::instance()->getActorPtr<BombProjectile>(mWeaponID);
        if (bomb) {
            bomb->setReady(true);
            mWeaponID.setValue(ActorUniqueID::cInvalidID);
        }
    }

    void setJumpCnt() override {
        mJumpCnt = (GAME_RANDOM.getU32(8) + 1) * 10 + 60;
    }

    void setJump() override {
        if (GAME_RANDOM.getU32(7) == 0) {
            mSpeed.y = 3.5f;
        } else {
            mSpeed.y = 2.5f;
        }
    }

    void vf654() override{
        // Delete a held bomb if touched
        BombProjectile* bomb = ActorMgr::instance()->getActorPtr<BombProjectile>(mWeaponID);
        if (bomb) {
            bomb->deleteActor(true);
            mWeaponID.setValue(ActorUniqueID::cInvalidID);
        }
    }

    static const ActorCreateInfo cCreateInfo;
};

} // namespace propelpartsu
