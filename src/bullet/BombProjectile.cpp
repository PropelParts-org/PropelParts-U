#include <propelpartsu/bullet/BombProjectile.h>
#include <propelpartsu/PropelPartsU.h>
#include <player/PlayerBase.h>
#include <player/Yoshi.h>
#include <map_obj/ChibiYoshiBase.h>
#include <actor/ActorMgr.h>
#include <propelpartsu/enemy/BombBros.h>
#include <utility/Mtx.h>
#include <effect/EffectCreateUtil.h>
#include <graphics/LightType.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(BombProjectile, ActorState);

// States
CREATE_STATE_ID(BombProjectile, ThrowWait)
CREATE_STATE_ID(BombProjectile, EatWait)
CREATE_STATE_ID(BombProjectile, Throw)
CREATE_STATE_ID(BombProjectile, Explode)

// Profile
Profile* BombProjectile::sProfile = getRegistrar()->newProfile<BombProjectile>("brosbomb")
    .flag(Profile::cFlag_DrawCullCheck | Profile::cFlag_WinKill)
    .build();

// Main collider
const ActorCollisionCheck::CollisionData BombProjectile::cCollisionData = {
    .center_offset = {0.0f, 8.0f},
    .half_size = {8.0f, 8.0f},
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Enemy,
    .attack = ActorCollisionCheck::cAttack_None,
    .vs_kind = ActorCollisionCheck::TargetKind(
        ActorCollisionCheck::cTargetKind_Player |
        ActorCollisionCheck::cTargetKind_Enemy |
        ActorCollisionCheck::cTargetKind_Item |
        ActorCollisionCheck::cTargetKind_Tama |
        ActorCollisionCheck::cTargetKind_ChibiYoshi |
        ActorCollisionCheck::cTargetKind_DrcTouch
    ),
    .vs_damage = ActorCollisionCheck::DamageFrom(
        ActorCollisionCheck::cAttack_Explode |
        ActorCollisionCheck::cDamageFrom_YoshiEat |
        ActorCollisionCheck::cAttack_ChibiYoshiAwa

    ),
    .status = ActorCollisionCheck::cStatus_None,
    .callback = &BombProjectile::collcheck,
};

void BombProjectile::collcheck(ActorCollisionCheck *cc_self, ActorCollisionCheck *cc_other) {
    BombProjectile* bomb = cc_self->getOwner<BombProjectile>();
    Actor* other = cc_other->getOwner();
    if (bomb && other) {
        if (bomb->mPlayerBomb) {
            // Spat by a yoshi
            if (other->getKind() == cActorKind_Enemy) {
                bomb->changeState(BombProjectile::StateID_Explode);
            }
        } else {
            // Thrown by a bro
            if (other->getKind() == cActorKind_Player) {
                bomb->changeState(BombProjectile::StateID_Explode);
            } else if (other->getKind() == cActorKind_Yoshi) {
                if (cc_other->getAttack() == ActorCollisionCheck::cAttack_YoshiEat) {
                    // Colliding with yoshi's tongue
                    bomb->changeState(BombProjectile::StateID_EatWait);
                } else {
                    if (other->getPlayerNo() > -1) {
                        // Yoshi has a player riding him
                        bomb->changeState(BombProjectile::StateID_Explode);
                    }
                }
            } else if (other->getKind() == cActorKind_ChibiYoshi) {
                if (static_cast<ChibiYoshiBase*>(other)->getCarryPlayerNo() > -1) {
                    bomb->changeState(BombProjectile::StateID_EatWait);
                }
            }
        }
    }
}

// Explosion collider
const ActorCollisionCheck::CollisionData BombProjectile::cExplodeCollisionData = {
    .center_offset = {0.0f, 8.0f},
    .half_size = {18.0f, 18.0f},
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Enemy,
    .attack = ActorCollisionCheck::cAttack_Shell,
    .vs_kind = ActorCollisionCheck::TargetKind(
        ActorCollisionCheck::cTargetKind_Player |
        ActorCollisionCheck::cTargetKind_Enemy |
        ActorCollisionCheck::cTargetKind_Tama |
        ActorCollisionCheck::cTargetKind_Killer
    ),
    .vs_damage = ActorCollisionCheck::cDamageFrom_None,
    .status = ActorCollisionCheck::Status(
        ActorCollisionCheck::cStatus_NoRevision |
        ActorCollisionCheck::cStatus_BurnerKill |
        ActorCollisionCheck::cStatus_MoveKill
    ),
    .callback = &BombProjectile::explode_collcheck,
};

void BombProjectile::explode_collcheck(ActorCollisionCheck *cc_self, ActorCollisionCheck *cc_other) {
    BombProjectile* bomb = cc_self->getOwner<BombProjectile>();
    Actor* other = cc_other->getOwner();
    if (bomb && other) {
        if (!bomb->mPlayerBomb) {
            // Only collide with players if we've been thrown by a bro
            if (other->getKind() == cActorKind_Player) {
                static_cast<PlayerBase*>(other)->setDamage(bomb, PlayerBase::cDamageType_Hit);
            } else if (other->getKind() == cActorKind_Yoshi) {
                if (other->getPlayerNo() > -1) {
                    static_cast<Yoshi*>(other)->setDamage(bomb, PlayerBase::cDamageType_Hit);
                }
            }
        }
    }
}

// Yoshi spit out behavior
bool BombProjectile::BombEatData::setEatSpitOut(Actor* yoshi) {
    BombProjectile* bomb = ActorMgr::instance()->getActorPtr<BombProjectile>(mOwnerID);
    if (bomb) {
        // Coming out of Yoshi's mouth
        bomb->mDirection = yoshi->getDirection();
        bomb->mPos.y += 3.0f;
        bomb->mPos.z = 5750.0f;
        bomb->mCollisionCheck.setDamage(ActorCollisionCheck::cDamageFrom_None);
        // We own the bomb now
        bomb->mPlayerBomb = true;
        bomb->mOwnerID.setValue(yoshi->getActorUniqueID().getValue());
        bomb->changeState(StateID_Throw);
    }
    return true;
}

// Gamepad touch callback
bool BombProjectile::DrcTouchCB::ccSetTouchNormal(ActorCollisionCheck* cc, const sead::Vector2f& pos) {
    BombProjectile* bomb = cc->getOwner<BombProjectile>();
    if (bomb) {
        bomb->mPlayerBomb = true;
        bomb->changeState(BombProjectile::StateID_Explode);
    }
    return true;
}

// Class constants
static const ActorBgCollisionCheck::Sensor cBcSensorFoot = { 0.0f, 0.0f,  0.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorHead = { 0.0f, 0.0f, 16.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorWall = { 5.0f, 9.0f,  6.0f };

const Angle BombProjectile::cBaseAngleY[cDirType_NumX] = { 0x20000000, -0x20000000 };

BombProjectile::BombProjectile(const ActorCreateParam& param)
    : ActorState(param)
    , mModel()
    , mYoshiEatData(mActorUniqueID)
    , mChibiYoshiEatData(mActorUniqueID)
    , mChibiYoshiAwaData(mActorUniqueID)
    , mLightMask()
    , mExplodeLight()
    , mReadyToThrow(false)
    , mPlayerBomb(false)
    , mExplosionTimer(0)
    , mInLiquid(false)
    , mLightRadius(130.0f)
{ }

ActorBase::Result BombProjectile::create() {
    // Setup model
    mModel = AnimModel::create("bombhei", "bombhei", 6, 0, 1);

    // Collider
    mCollisionCheck.set(this, cCollisionData, &mDrcTouchCallback);
    
    mOwnerID.setValue(mParam0);

    // Center of the actor, for dieFall and yoshi tongue
    mCenterOffset.set(0.0f, 8.0f, 0.0f);

    // Set culling distance and size
    mVisibleAreaSize.set(16.0f, 16.0f);
    mVisibleAreaOffset.set(0.0f, 8.0f);
    mSize.set(16.0f, 16.0f);

    // Yoshi eat ability
    mEatDataPtr = &mYoshiEatData;
    mYoshiEatData.setEatType(EatData::cEatType_Eat);
    
    // Baby Yoshi eat ability
    mChibiYoshiEatDataPtr = &mChibiYoshiEatData;
    mChibiYoshiEatData.setEatType(ChibiYoshiEatData::cEatType_Drink);

    // Baby Yoshi bubble ability
    mChibiYoshiAwaDataPtr = &mChibiYoshiAwaData;

    // Tile sensors
    mBgCheckObj.set(this, &cBcSensorFoot, &cBcSensorHead, &cBcSensorWall);
    bgCheck_();

    // Water check
    mCheckWaterNeeded = true;
    mWaterCalcType = cWaterCalcType_EnablePreCheck;

    mLightMask.create(nullptr, Mask::cTexName_Dark);

    Actor* owner = ActorMgr::instance()->getActorPtr<Actor>(mOwnerID);
    if (owner) {
        mDirection = owner->getDirection();
        mAngle.y() = cBaseAngleY[mDirection];
    } else {
        mDirection = getPlayerDirLR();
        mAngle.y() = cBaseAngleY[mDirection];
        changeState(StateID_Throw);
        return cResult_Success;
    }

    if (owner->getKind() == Actor::cActorKind_Yoshi) {
        mPlayerBomb = true;
    }

    if (sead::DynamicCast<BombBros>(owner)) {
        changeState(StateID_ThrowWait);
    } else {
        changeState(StateID_Throw);
    }

    return cResult_Success;
}

bool BombProjectile::execute() {
    // Play effect if landing in water
    splashEffect();

    executeState();

    calcMdl();
    if (isState(StateID_Throw)) {
        Mtxf mtxEffect;
        mModel->getModel()->getBoneWorldMatrix(mModel->getModel()->searchBoneIndex("ef_bombhei"), &mtxEffect);
        mIgniteEffect.createEffect(RP_Bombhei_Ignite, mtxEffect, false);
    }

    // Delete actor if offscreen
    screenOutCheck(cScreenOutFlag_SkipNone);

    return true;
}

bool BombProjectile::draw() {
    if (!isState(StateID_Explode)) {
        mModel->draw();
    }

    if (isState(StateID_Throw) || isState(StateID_Explode)) {
        mLightMask.draw();
    }

    return true;
}

void BombProjectile::splashEffect() {
    // Check for any collisions with liquids
    f32 waterPosY;
    WaterType waterType = checkWater_(&waterPosY);
    if (cWaterType_Quicksand > waterType && waterType > cWaterType_AirWater) {
        // Submerged in poison or lava
        if (getCenterPos().y < waterPosY) {
            // Kill if more than half submerged
            deleteActor(true);
        }
    }
    if (waterType == cWaterType_None) {
        // Reset every frame we're not in liquid
        mInLiquid = false;
    } else if (mInLiquid == false) {
        // First frame touching liquid
        mInLiquid = true;
        sead::Vector3f splashPos(mPos.x, waterPosY, EFFECT_Z_POS_DEFAULT);
        switch (waterType) {
            case cWaterType_Water:
                splashEffect_(splashPos, RP_Cmn_WaterSplash_04, 6, "SE_OBJ_CMN_SPLASH");
                break;
            case cWaterType_Lava:
            case cWaterType_LavaWave:
                splashEffect_(splashPos, RP_Cmn_LavaSplash_04, 16, "SE_OBJ_CMN_SPLASH_LAVA");
                changeState(StateID_Explode);
                break;
            case cWaterType_Poison:
                splashEffect_(splashPos, RP_Cmn_PoisonSplash_04, 23, "SE_OBJ_CMN_SPLASH_POISON");
                break;
        }
    }
}

// Calculate the model's matricies
void BombProjectile::calcMdl() {
    Mtxf mtx;
    mtx.makeT(mPos);

    mtx.YrotM(mAngle.y());

    mtx.multTranslationLocal(sead::Vector3f(0.0f, mCenterOffset.y, 0.0f));
    mtx.XrotM(mAngle.x());
    mtx.multTranslationLocal(sead::Vector3f(0.0f, -mCenterOffset.y, 0.0f));

    mtx.ZrotM(mAngle.z());

    mModel->getModel()->setBaseModelMtx(mtx);
    mModel->getModel()->setLocalScale(mScale);

    mModel->playAnmFrameCtrl();

    mModel->calc();

    // Store root bone position
    Mtxf mtxRoot;
    mModel->getModel()->getBoneWorldMatrix(mModel->getModel()->searchBoneIndex("skl_root"), &mtxRoot);
    mtxRoot.multVecZero(mRootPos);

    // Update light mask
    if (isState(StateID_Throw) || isState(StateID_Explode)) {
        mLightMask.setPosition(getCenterPos());
        mLightMask.setRadius(mLightRadius);
        mLightMask.execute();
    }
}

void BombProjectile::explodeTiles() {
    sead::Vector2f tl1(mPos.x - 16.0f, mPos.y + 16.0f);
    sead::Vector2f br1(mPos.x + 16.0f, mPos.y - 16.0f);
    mBgCheckObj.checkBombBreak(tl1, br1);

    sead::Vector2f tl2(mPos.x, mPos.y + 27.0f);
    sead::Vector2f br2(mPos.x, mPos.y - 27.0f);
    mBgCheckObj.checkBombBreak(tl2, br2);

    sead::Vector2f tl3(mPos.x - 24.0f, mPos.y + 8.0f);
    sead::Vector2f br3(mPos.x + 24.0f, mPos.y - 8.0f);
    mBgCheckObj.checkBombBreak(tl3, br3);
}

void BombProjectile::updateExplodeLight() {
    f32 lightRad = mLightRadius / 130.0f;
    if (lightRad >= 0.01f) {
        if (lightRad > 1.0f) {
            lightRad = 1.0f;
        }
    } else {
        lightRad = 0.01f;
    }
    mExplodeLight.update(cLightType_Bomhei, &mPos, nullptr, &lightRad);
}

void BombProjectile::initializeState_ThrowWait() {
    mScale.set(0.5f, 0.5f, 0.5f);
    mModel->playSklAnim("stop");
    mModel->getSklAnim(0)->getFrameCtrl().setRate(0.0f);
}

void BombProjectile::executeState_ThrowWait() {
    BrosBase* owner = ActorMgr::instance()->getActorPtr<BrosBase>(mOwnerID);
    if (owner) {
        mPos = owner->getCreateWeaponPos();
        mAngle.y() = owner->getAngle().y();
        mScale.setLerp(mScale, sead::Vector3f::ones, 0.05f);

        if (mReadyToThrow) {
            changeState(StateID_Throw);
        }
    } else {
        // Owner has been deleted for whatever reason
        deleteActor(true);
    }
}

void BombProjectile::finalizeState_ThrowWait() {
    mScale.set(1.0f, 1.0f, 1.0f);
}

void BombProjectile::initializeState_EatWait() {
    mModel->getShuAnim(0)->getFrameCtrl().setFrame(0.0f);
    mModel->getShuAnim(0)->getFrameCtrl().setRate(0.0f);
    removeCollisionCheck();
}

void BombProjectile::executeState_EatWait() { }

void BombProjectile::finalizeState_EatWait() { }

void BombProjectile::initializeState_Throw() {
    mAngle.y() = cBaseAngleY[mDirection];
    reviveCollisionCheck();

    mModel->playColorAnim("bombhei");
    mModel->getShuAnim(0)->getFrameCtrl().setRate(1.0f);
    mModel->getShuAnim(0)->getFrameCtrl().setPlayMode(FrameCtrl::cMode_NoRepeat);

    mLightMask.setPosition(getCenterPos());
    mLightMask.setRadius(mLightRadius);

    Actor* owner = ActorMgr::instance()->getActorPtr<Actor>(mOwnerID);

    // Add projectile physics
    if (!mPlayerBomb) {
        mSpeed.x = (mDirection) ? -2.1f : 2.1f;
        mSpeed.y = 3.5f;
        mSpeedMax.y = -6.0f;
        mGravity = -0.2f;
        if (owner) {
            mSpeed.x = mSpeed.x + owner->getPosDelta().x * 1.1f;
        }
    } else {
        mSpeed.x = owner->getSpeedVec().x * 0.5f + ((mDirection) ? -3.85f : 3.85f);
        mSpeed.y = 2.8f;
        mGravity = -0.225f;
        mSpeedMax.x = (mDirection) ? -2.9f : 2.9f;
        mMaxFallSpeed = -4.0f;
        mSpeedMax.y = -4.0f;
        mPow = 0.04f;
    }
}

void BombProjectile::executeState_Throw() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    calcSpeedX_();
    posMove_();
    bgCheck_();

    // Ignite sound
    GameAudio::getAudioObjEmy()->holdSound("SE_EMY_BH_HIBANA", mActorUniqueID.getValue(), mPos);

    // Explode after a while
    bool shouldExplode = false;
    if (mModel->getShuAnim(0)->getFrameCtrl().isStop()) {
        shouldExplode = true;
    }

    if (mBgCheckObj.checkFoot() || mBgCheckObj.checkHead() || mBgCheckObj.checkWall(mDirection)) {
        shouldExplode = true;
    }

    if (shouldExplode) {
        changeState(StateID_Explode);
    }
}

void BombProjectile::finalizeState_Throw() { }

void BombProjectile::initializeState_Explode() {
    removeCollisionCheck();
    sead::Vector3f effectPos(mRootPos.x, mRootPos.y, getEffectZPos());
    EffectCreateUtil::createEffect(RP_Cmn_Explosion_0, &effectPos);
    GameAudio::getAudioObjEmy()->startSound("SE_EMY_BH_BOMB", mPos);

    mCollisionCheck.set(this, cExplodeCollisionData);
    reviveCollisionCheck();

    explodeTiles();
    mExplosionTimer = 0;

    mLightRadius = 300.0f;
}

void BombProjectile::executeState_Explode() {
    updateExplodeLight();
    mExplosionTimer++;

    if (mExplosionTimer == 16) {
        removeCollisionCheck();
    }

    if (mExplosionTimer > 29 && sead::Mathf::chase(&mLightRadius, 0.0f, 10.0f)) {
        deleteActor(true);
    }
}

void BombProjectile::finalizeState_Explode() { }

} // namespace propelpartsu
