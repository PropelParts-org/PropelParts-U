#include <propelpartsu/map_obj/ItemKey.h>
#include <propelpartsu/PropelPartsU.h>
#include <player/PlayerBase.h>
#include <game/FlagCtrl.h>
#include <game_info/CourseInfo.h>
#include <map/SwitchFlagMgr.h>
#include <effect/EffectCreateUtil.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(ItemKey, ActorState);

CREATE_STATE_ID(ItemKey, Wait)
CREATE_STATE_ID(ItemKey, Hide)
CREATE_STATE_ID(ItemKey, GetUpDemo)
CREATE_STATE_ID(ItemKey, UpDemoWait)
CREATE_STATE_ID(ItemKey, UIJumpDemo)

// Profile
Profile* ItemKey::sProfile = getRegistrar()->newProfile<ItemKey>("itemkey")
    .resources<"ship_key">(ProfileInfo::cResType_Course)
    .createInfo(cCreateInfo)
    .flag(Profile::cFlag_DrawCullCheck)
    .build();

// Create parameters
const ActorCreateInfo ItemKey::cCreateInfo = {
    .offset_x = 8, .offset_y = -16,
    .spawn_range = {
        .offset_x = 0, .offset_y = 8,
        .half_size_x = 16, .half_size_y = 16
    },
    .cull_range = { 
        .up = 0, .down = 0, .left = 0, .right = 0
    },
    .flag = ActorCreateInfo::cFlag_MapObj
};

// Main collider
const ActorCollisionCheck::CollisionData ItemKey::cCollisionData = {
    .center_offset = {0.0f, 8.0f},
    .half_size = {7.0f, 8.0f}, 
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Item,
    .attack = ActorCollisionCheck::cAttack_None,
    .vs_kind = ActorCollisionCheck::TargetKind(
        ActorCollisionCheck::cTargetKind_PlayerBody |
        ActorCollisionCheck::cTargetKind_Yoshi
    ),
    .vs_damage = ActorCollisionCheck::cDamageFrom_None,
    .status = ActorCollisionCheck::cStatus_None,
    .callback = &ItemKey::collcheck,
};

// Class constants
static const ActorBgCollisionCheck::Sensor cBcSensorFoot = { 0.0f, 0.0f,  0.0f };

void ItemKey::collcheck(ActorCollisionCheck *cc_self, ActorCollisionCheck *cc_other) {
    ItemKey* self = cc_self->getOwner<ItemKey>();
    PlayerBase* player = cc_other->getOwner<PlayerBase>();
    if (self != nullptr && player != nullptr) {
        self->mCollectPlayerNo = player->getPlayerNo();
    }
}

ItemKey::ItemKey(const ActorCreateParam& param)
    : ActorState(param)
    , mModel()
    , mCollectEffect()
    , mUpDemoWaitCountdown(0)
    , mCollectPlayerNo(-1)
    , mHasLanded(true)
{ }

ActorBase::Result ItemKey::create() {
    // Check if we've been collected already
    u16 flagData = FlagCtrl::instance()->getFlagData(CourseInfo::instance()->getAreaNo(), mPos.x, mPos.y);
    if (flagData == 0) {
        // Uncollected key

        // Set collider
        mCollisionCheck.set(this, cCollisionData);
        reviveCollisionCheck();

        // Model setup
        mModel = AnimModel::create("ship_key", "I_ship_key");

        mInitialPos = mPos;
        mSpeedMax.set(1.0f, -8.0f, 0.0f);
        mGravity = ACTOR_DEFAULT_GRAVITY;

        // Tile sensors
        mBgCheckObj.set(this, &cBcSensorFoot, nullptr, nullptr);
        bgCheck_();

        // Water check
        mCheckWaterNeeded = true;
        mWaterCalcType = cWaterCalcType_EnablePreCheck;

        if (SwitchFlagMgr::instance()->isActivated(mSwitchFlag1-1) || mSwitchFlag1 == 0) {
            changeState(StateID_Wait);
        } else {
            changeState(StateID_Hide);
        }

        mModel->update(mPos, mAngle, mScale);
        return cResult_Success;
    }
    return cResult_Failed;
}

bool ItemKey::execute() {
    landonEffect();

    executeState();

    // Delete actor if offscreen
    screenOutCheck(cScreenOutFlag_SkipNone);

    mCollectPlayerNo = -1;

    return true;
}

bool ItemKey::draw() {
    if (!isState(StateID_Hide)) {
        mModel->update(mPos, mAngle, mScale);
        mModel->draw();
    }

    return true;
}

void ItemKey::landonEffect() {
    if (!mHasLanded) {
        // Either in the air or first frame of touching the ground
        // Effect position
        sead::Vector3f pos(
            mPos.x,
            mPos.y,
            EFFECT_Z_POS_DEFAULT
        );
        // Check if we are touching the ground
        if (mBgCheckObj.checkFoot()) {
            // If we are, that means we've only started touching the ground on this frame, so we need to play a landing effect
            pos.z = getEffectZPos();
            // Set mHasLanded to true so the effect only plays the first frame we touch the ground
            mHasLanded = true;
            // Play different land effects depending on the type of ground we're landing on
            switch (BgUnitCode::getAttr(mBgCheckObj.getBgCheckData(cDirType_Down))) {
                default: // Normal ground
                    EffectCreateUtil::createEffect(RP_Cmn_LandingSmoke_08, &pos);
                    break;
                case BgUnitCode::cNuma: // Beach sand
                case BgUnitCode::cSand: // Desert sand
                    EffectCreateUtil::createEffect(RP_Cmn_LandingSand_04, &pos);
                    break;
                case BgUnitCode::cIce:
                    EffectCreateUtil::createEffect(RP_Cmn_LandingIce_04, &pos);
                    break;
                case BgUnitCode::cSnow:
                    EffectCreateUtil::createEffect(RP_Cmn_LandingSnow_04, &pos);
                    break;
                case BgUnitCode::cWater: // Water geyser
                    EffectCreateUtil::createEffect(RP_Cmn_LandingPillarWtr_04, &pos);
                    break;
            }
        } else {
            // Not touching the ground on this frame, check if we're entering liquid
            sead::Vector3f check_pos = pos;
            check_pos.y -= 2.0f;
            // Check if we're inside liquid
            WaterType water_type = ActorBgCollisionCheck::checkWater(&pos.y, check_pos, mLayer);
            if (water_type != cWaterType_None) {
                // If we are, that means we've only started touching the liquid on this frame, so we need to play a splash effect
                // Set mHasLanded to true so the effect only plays the first frame we touch the liquid
                mHasLanded = true;
                pos.z = 6500.0f;
                // Play different splash effects depending on the type of liquid we're landing on
                switch (water_type) {
                    default:
                        break;
                    case cWaterType_Water:
                        splashEffect_(pos, RP_Cmn_WaterSplash_04, 6, "SE_OBJ_CMN_SPLASH");
                        break;
                    case cWaterType_Lava:
                    case cWaterType_LavaWave:
                        splashEffect_(pos, RP_Cmn_LavaSplash_04, 16, "SE_OBJ_CMN_SPLASH_LAVA");
                        break;
                    case cWaterType_Poison:
                        splashEffect_(pos, RP_Cmn_PoisonSplash_04, 23, "SE_OBJ_CMN_SPLASH_POISON");
                        break;
                }
            }
        }
    } else if (!mBgCheckObj.checkFoot()) {
        // Currently in the air, set mHasLanded to false
        mHasLanded = false;

        // Check if we're in a liquid (but not touching the ground), and set mHasLanded to true
        sead::Vector3f check_pos = mPos;
        check_pos.y -= 2.0f;
        WaterType water_type = ActorBgCollisionCheck::checkWater(nullptr, check_pos, mLayer);
        if (water_type != cWaterType_None) {
            mHasLanded = true;
        }
    }
}

void ItemKey::initializeState_Wait() { }

void ItemKey::executeState_Wait() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    if (mBgCheckObj.checkFoot() && mCollectPlayerNo != -1) {
        FlagCtrl::instance()->setFlagData(CourseInfo::instance()->getAreaNo(), mInitialPos.x, mInitialPos.y, 1);
        if (mParam0 & 1) {
            GameAudio::getAudioObjMap()->startSound("SE_OBJ_BROOM_KEY_GET", mPos);
        }
        SwitchFlagMgr::instance()->set(mSwitchFlag0-1, 0, true);
        changeState(StateID_GetUpDemo);
    }
}

void ItemKey::finalizeState_Wait() { }

void ItemKey::initializeState_Hide() {
    removeCollisionCheck();
}

void ItemKey::executeState_Hide() {
    if (SwitchFlagMgr::instance()->isActivated(mSwitchFlag1-1)) {
        changeState(StateID_Wait);
    }
}

void ItemKey::finalizeState_Hide() {
    reviveCollisionCheck();
}

void ItemKey::initializeState_GetUpDemo() {
    mSpeed.y = 4.0f;
}

void ItemKey::executeState_GetUpDemo() {
    // Do speed and position calculations
    calcSpeedY_();
    posMove_();

    if (mParam0 & 1) {
        sead::Vector3f effectPos(mPos.x, mPos.y + 8.0f, mPos.z);
        sead::Vector3f effectScale(2.0f, 2.0f, 2.0f);
        mCollectEffect.createEffect(RP_CoinNormalTail, &effectPos, nullptr, &effectScale);
    }

    mAngle.y() += 0x20000000;
    if (mSpeed.y <= 0.0f) {
        mPos.z = 9000.0f;
        mSpeed.y = 0.0f;
        changeState(StateID_UpDemoWait);
    }
}

void ItemKey::finalizeState_GetUpDemo() { }

void ItemKey::initializeState_UpDemoWait() {
    mUpDemoWaitCountdown = 20;
}

void ItemKey::executeState_UpDemoWait() {
    if (mParam0 & 1) {
        sead::Vector3f effectPos(mPos.x, mPos.y + 8.0f, mPos.z);
        sead::Vector3f effectScale(2.0f, 2.0f, 2.0f);
        mCollectEffect.createEffect(RP_CoinNormalTail, &effectPos, nullptr, &effectScale);
    }

    mAngle.y() += 0x20000000;
    if (mUpDemoWaitCountdown != 0) {
        mUpDemoWaitCountdown--;
        return;
    }
    changeState(StateID_UIJumpDemo);
}

void ItemKey::finalizeState_UpDemoWait() { }

void ItemKey::initializeState_UIJumpDemo() {
    mAngle.y() = 0;
}

void ItemKey::executeState_UIJumpDemo() {
    if (mParam0 & 1) {
        sead::Vector3f effectPos(mPos.x, mPos.y + 8.0f, mPos.z);
        sead::Vector3f effectScale(2.0f, 2.0f, 2.0f);
        mCollectEffect.createEffect(RP_CoinNormalTail, &effectPos, nullptr, &effectScale);
    }

    float targetX = 0.0f;
    float targetY = -16.0f;

    sead::Vector3f deltaPos(targetX - mPos.x, targetY - mPos.y, 0.0f);
    deltaPos.normalize();
    mPos.x += deltaPos.x * 12.0f;
    mPos.y += deltaPos.y * 12.0f;
}

void ItemKey::finalizeState_UIJumpDemo() { }

} // namespace propelpartsu
