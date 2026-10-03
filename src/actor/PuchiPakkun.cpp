#include <propelpartsu/actor/PuchiPakkun.h>
#include <propelpartsu/PropelPartsU.h>
#include <game/FlagCtrl.h>
#include <game_info/CourseInfo.h>
#include <collision/ActorCollisionCheckMgr.h>
#include <effect/EffectCreateUtil.h>
#include <utility/Mtx.h>
#include <game/CourseTask.h>
#include <collision/BgCollisionCheckResult.h>
#include <player/PlayerObject.h>
#include <player/PlayerMgr.h>
#include <actor/ActorUtil.h>
#include <input/InputMgr.h>
#include <actor/ActorMgr.h>
#include <propelpartsu/actor/FireBallPuchiPakkun.h>
#include <propelpartsu/actor/IceBallPuchiPakkun.h>
#include <map_obj/Freezer.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(PuchiPakkun, Enemy);

// States
CREATE_STATE_ID(PuchiPakkun, Idle)
CREATE_STATE_ID(PuchiPakkun, Walk)
CREATE_STATE_ID(PuchiPakkun, Turn)
CREATE_STATE_ID(PuchiPakkun, Jump)
CREATE_STATE_ID(PuchiPakkun, FireSpit)
CREATE_STATE_ID(PuchiPakkun, IceWait)

// Profile
Profile* PuchiPakkun::sProfile = getRegistrar()->newProfile<PuchiPakkun>("puchipakkun")
    .resources<"pakkun_puchi">(ProfileInfo::cResType_Course)
    .createInfo(cCreateInfo)
    .flag(Profile::cFlag_DrawCullCheck | Profile::cFlag_WinKill)
    .build();

// Create parameters
const ActorCreateInfo PuchiPakkun::cCreateInfo = {
    .offset_x = 8, .offset_y = -16,
    .spawn_range = {
        .offset_x = 0, .offset_y = 8,
        .half_size_x = 8, .half_size_y = 8
    },
    .cull_range = { 
        .up = 0, .down = 0, .left = 0, .right = 0
    },
    .flag = ActorCreateInfo::cFlag_None
};

// Main collider
const ActorCollisionCheck::CollisionData PuchiPakkun::cCollisionData = {
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
        ActorCollisionCheck::cTargetKind_Unk10 |
        ActorCollisionCheck::cTargetKind_DrcTouch
    ),
    .vs_damage = ActorCollisionCheck::DamageFrom(
        ~(ActorCollisionCheck::cDamageFrom_Slip |
        ActorCollisionCheck::cDamageFrom_HipAttack |
        ActorCollisionCheck::cDamageFrom_PenguinSlip |
        ActorCollisionCheck::cDamageFrom_Spin |
        ActorCollisionCheck::cDamageFrom_SpinFall |
        ActorCollisionCheck::cDamageFrom_YoshiMouth |
        ActorCollisionCheck::cDamageFrom_SpinLiftUp)
    ),
    .status = ActorCollisionCheck::cStatus_None,
    .callback = &Enemy::normal_collcheck,
};

// Gamepad touch collider
const ActorCollisionCheck::CollisionData PuchiPakkun::cCollisionData_DRC = {
    .center_offset = {0.0f, 4.0f},
    .half_size = {4.0f, 4.0f}, 
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Enemy,
    .attack = ActorCollisionCheck::cAttack_None,
    .vs_kind = ActorCollisionCheck::cTargetKind_DrcTouch,
    .vs_damage = ActorCollisionCheck::cDamageFrom_None,
    .status = ActorCollisionCheck::cStatus_None,
    .callback = nullptr
};

// Gamepad touch callback
bool PuchiPakkun::DrcTouchCB::ccSetTouchNormal(ActorCollisionCheck* cc, const sead::Vector2f& pos) {
    PuchiPakkun* nipper = cc->getOwner<PuchiPakkun>();
    if (nipper != nullptr) {
        // Start the boyon when tapped
        nipper->mBoyoMgr.begin();
    }
    return true;
}

// Class constants
const Angle PuchiPakkun::cBaseAngleY[cDirType_NumX] = { 0x40000000, -0x40000000 };
const Angle PuchiPakkun::cBaseAngleYTurn[cDirType_NumX] = { 0x39999999, -0x39999999 };

const f32 PuchiPakkun::cMaxSpeedX = 1.0f;
const f32 PuchiPakkun::cMaxSpeedY = -4.0f;

const f32 PuchiPakkun::cWalkSpeed[cDirType_NumX] = { 0.5f, -0.5f };

static const ActorBgCollisionCheck::Sensor cBcSensorFoot = { -4.0f, 4.0f,  0.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorHead = {  0.0f, 0.0f, 16.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorWall = {  3.0f, 8.0f,  8.0f };

PuchiPakkun::PuchiPakkun(const ActorCreateParam& param)
    : Enemy(param)
    , mModel()
    , mYoshiEatData(mActorUniqueID)
    , mBabyYoshiEatData(mActorUniqueID)
    , mBoyoMgr(this)
    , mHasLanded(true)
    , mSpatFireCount(0)
    , mFireCooldown(0)
{ }

ActorBase::Result PuchiPakkun::create() {
    // Model setup
    mModel = AnimModel::create("pakkun_puchi", "pakkun_puchi", 3, 1);
    mModel->playTexAnim("pakkun_puchi");
    mModel->getTexAnim(0)->getFrameCtrl().setRate(0.0f);

    // Set max speed for gravity
    mSpeedMax.y = cMaxSpeedY;

    // Set collider
    mCollisionCheck.set(this, cCollisionData);
    mCollisionCheckDrcTouch.set(this, cCollisionData_DRC, &mDrcTouchCallback);
    reviveCollisionCheck();

    // Assign parameters
    // Bit 31
    mWalks = mParam0 >> 17 & 1;
    // Nybble 9, lowest 2 bits
    mJumpHeight = mParam0 >> 12 & 2;
    // Nybble 12 mask 2
    mSpitsFire = mParam0 >> 1 & 1;
    // Bit 50
    mSpitsIce = mParam1 >> 30 & 1;
    // Nybble 14
    mFireHeight = (mParam1 >> 24 & 0xF) + 2;

    // Center of the actor, for dieFall and yoshi tongue
    mCenterOffset.set(0.0f, 8.0f, 0.0f);

    // Set culling distance and size
    mVisibleAreaSize.set(16.0f, 16.0f);
    mVisibleAreaOffset.set(0.0f, 8.0f);
    mSize.set(16.0f, 16.0f);

    // Tile sensors
    mBgCheckObj.set(this, &cBcSensorFoot, &cBcSensorHead, &cBcSensorWall);
    bgCheck_();

    // Yoshi eat ability
    mEatDataPtr = &mYoshiEatData;
    // Yoshi eats the Nipper
    EatData::EatType eatType = EatData::cEatType_Drink;
    if (mSpitsFire) {
        if (mSpitsIce) {
            // Yoshi turns the Nipper into an iceball
            eatType = EatData::cEatType_YoshiFire_Ice;
        } else {
            // Yoshi turns the Nipper into a fireball
            eatType = EatData::cEatType_YoshiFire;
        }
    }
    mYoshiEatData.setEatType(eatType);
    
    // Baby Yoshi eat ability
    mChibiYoshiEatDataPtr = &mBabyYoshiEatData;
    mBabyYoshiEatData.setEatType(ChibiYoshiEatData::cEatType_Drink);

    // Set spawn direction
    if (mWalks) {
        mDirection = getPlayerDirLR();
    } else {
        mDirection = cDirType_Left;
    }
    mAngle.y() = PuchiPakkun::cBaseAngleY[mDirection];

    // Water check
    mCheckWaterNeeded = true;
    mWaterCalcType = cWaterCalcType_EnablePreCheck;

    // Set starting state
    u16 flagData = FlagCtrl::instance()->getFlagData(CourseInfo::instance()->getAreaNo(), mPos.x, mPos.y);
    // Set frozen if the frozen bit (nybble 12 mask 1) is set AND this Nipper hasn't been unfrozen before
    if (mParam0 & 1 && flagData == 0) {
        changeState(StateID_IceWait);
    } else if (mWalks) {
        changeState(StateID_Walk);
    } else {
        changeState(StateID_Idle);
    }

    calcMdl_();

    return cResult_Success;
}

bool PuchiPakkun::execute() {
    // Update boyon
    mBoyoMgr.execute();

    // Play effect if landing on the ground
    landonEffect();

    // Kill if crushed
    if (hasamareBgCheck_() || checkBgIn()) {
        setDeathInfo_Hasami();
    }

    executeState();
    calcMdl_Normal();

    // Delete actor if offscreen
    screenOutCheck(cScreenOutFlag_SkipNone);

    return true;
}

bool PuchiPakkun::draw() {
    mModel->draw();

    return true;
}

// Adjusted to account for both hitboxes
void PuchiPakkun::reviveCollisionCheck() {
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheck);
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheckDrcTouch);
}

void PuchiPakkun::removeCollisionCheck() {
    ActorCollisionCheckMgr::instance()->release(mCollisionCheck);
    ActorCollisionCheckMgr::instance()->release(mCollisionCheckDrcTouch);
}

// Play death effect when dying to the goal pole
void PuchiPakkun::allEnemyDeathEffSet() {
    sead::Vector3f effectPos;
    effectPos.setAdd(mPos, mCenterOffset);
    EffectCreateUtil::createEffect(RP_Cmn_EnemyBurst_00, &effectPos);
}

// Change the ice cube size when frozen
bool PuchiPakkun::createIceActor() {
    IceInfo info;
    if (mWalks) {
        sead::Vector3f pos(
            mPos.x,
            mPos.y - 1.0f,
            mPos.z
        );
        info = { 0, pos, sead::Vector3f(1.0f, 1.0f, 1.0f), nullptr };
    } else {
        sead::Vector3f pos(
            mPos.x,
            mPos.y,
            mPos.z + 5.0f
        );
        info = { 0x1000, pos, sead::Vector3f(0.8f, 0.8f, 0.8f), nullptr };
    }
    return mIceMgr.createIce(info);
}

// Turn the Nipper Plant if touching another actor or baby yoshi
void PuchiPakkun::vsEnemyHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    Actor* actor = cc_other->getOwner();
    // The Maruta (Giant Rolling Log) check here is copied from Kuribo, not sure how important it is
    if (actor != nullptr && actor->getProfileID() != ProfileInfo::cProfileID_Maruta) {
        // Save the "Revision X" of the Nipper Plant
        f32 enemyHitRevX;
        if (actor->getKind() == cActorKind_Enemy) {
            enemyHitRevX = cc_self->getRevisionX(ActorCollisionCheck::cKind_Enemy);
        } else if (actor->getKind() == cActorKind_ChibiYoshi) {
            enemyHitRevX = cc_self->getRevisionX(ActorCollisionCheck::cKind_ChibiYoshi);
        }
        if (mDirection == cDirType_Left && enemyHitRevX > 0.0f || mDirection == cDirType_Right && enemyHitRevX < 0.0f) {
            if (isState(StateID_Walk)) {
                changeState(StateID_Turn);
            }
        }
    }
}

// Treat colliding with baby yoshis as colliding with a normal actor
void PuchiPakkun::vsChibiYoshiHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    vsEnemyHitCheck_Normal(cc_self, cc_other);
}

bool PuchiPakkun::hitCallback_Ice(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    Actor* other = cc_other->getOwner();
    PuchiPakkun* self = cc_self->getOwner<PuchiPakkun>();
    if (self != nullptr) {
        // If the iceball is one we've spat do not collide with it
        if (other->getParam1() == self->getActorUniqueID().getValue()) {
            return false;
        }
    }
    return Enemy::hitCallback_Ice(cc_self, cc_other);
}

// Don't change the initial angle of the Nipper Plant in the DieFall state
void PuchiPakkun::initializeState_DieFall() {
    Enemy::initializeState_DieFall();
    mAngle.y() = PuchiPakkun::cBaseAngleY[mDirection];
    return;
}

void PuchiPakkun::initializeState_Ice() {
    if (!mWalks) {
        // Munchers are slightly smaller when encased in ice, so we replicate that here
        mScale.set(0.89f, 0.89f, 0.89f);
        // Set animation to open mouth
        mModel->getSklAnim(0)->getFrameCtrl().setFrame(0.0f);
    }
    // New to NSMBU: Muncher have a different texture for when encased in ice
    mModel->getTexAnim(0)->getFrameCtrl().setFrame(1.0f);
    return Enemy::initializeState_Ice();
}

void PuchiPakkun::finalizeState_Ice() {
    // Restore everything
    if (!mWalks) {
        mScale.set(1.0f, 1.0f, 1.0f);
    }
    syncAnim();
    mModel->getTexAnim(0)->getFrameCtrl().setFrame(0.0f);
    return Enemy::finalizeState_Ice();
}

// Calculate the model's matricies
void PuchiPakkun::calcMdl_() {
    Mtxf mtx;
    mtx.makeT(mPos);

    mtx.YrotM(mAngle.y());

    mtx.multTranslationLocal(sead::Vector3f(0.0f, mCenterOffset.y, 0.0f));
    mtx.XrotM(mAngle.x());
    mtx.multTranslationLocal(sead::Vector3f(0.0f, -mCenterOffset.y, 0.0f));

    mtx.ZrotM(mAngle.z());

    mModel->getModel()->setBaseModelMtx(mtx);
    mModel->getModel()->setLocalScale(mBoyoMgr.getScale());

    // Don't animate if frozen
    if (!isState(StateID_Ice) && !isState(StateID_IceWait)) {
        mModel->playAnmFrameCtrl();
    }

    mModel->calc();
}

// Synchronize the Nipper Plant animation with all other Nipper Plants and Munchers in the zone
void PuchiPakkun::syncAnim() {
    u32 frameMax = static_cast<u32>(mModel->getSklAnim(0)->getFrameCtrl().getFrameEnd());
    u32 exeFrame = CourseTask::instance()->getExeFrame();
    SkeletalAnimation *anm = mModel->getSklAnim(0);
    anm->getFrameCtrl().setFrame(exeFrame % frameMax);
}

// Set the walking speed based on direction
void PuchiPakkun::setWalkSpeed() {
    mSpeed.x = cWalkSpeed[mDirection];
}

// Returns true if the Nipper Plant is approacing a ledge
bool PuchiPakkun::checkLedge() {
    // Setup tile collision check data
    BgCollisionCheckParam param = {
        ._0 = 0,
        .ignore_quicksand = false,
        .layer = mLayer,
        .collision_mask = mCollisionMask,
        .type = cBgCollisionCheckType_Solid,
        .callback = nullptr
    };
    BasicBgCollisionCheck bgChk(param);
    
    BgCollisionCheckResultArea res;
    res.hit_angle = 0;
    res._10 = 0;
    
    sead::Vector3f p0 = mPos + sead::Vector3f(4.0f * cEnMuki[mDirection], 0.0f, 0.0f);
    sead::Vector3f p1 = p0 + sead::Vector3f(16.0f * cEnMuki[mDirection], 2.0f * -16.0f, 0.0f);
    
    return !bgChk.checkArea(&res, p0, p1, 1 << cDirType_Down);
}

void PuchiPakkun::landonEffect() {
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
        // Don't play the landing effect if hopping during a walk/turn state
        if (!isState(StateID_Walk) && !isState(StateID_Turn)) {
            // Currently in the air, set mHasLanded to false
            mHasLanded = false;
        }

        // Check if we're in a liquid (but not touching the ground), and set mHasLanded to true
        sead::Vector3f check_pos = mPos;
        check_pos.y -= 2.0f;
        WaterType water_type = ActorBgCollisionCheck::checkWater(nullptr, check_pos, mLayer);
        if (water_type != cWaterType_None) {
            mHasLanded = true;
        }
    }
}

bool PuchiPakkun::isPlayerAbove() {
    for (int i = 0; i < cPlayerNum; i++) {
        // Grab any active players
        PlayerObject* player = PlayerMgr::instance()->getPlayerObject(i);
        if (player) {
            // Are we in the same x range as the nipper?
            if (10.0f >= std::fabs(player->getPos().x - mPos.x)) {
                // Are we in the y range?
                if (mPos.y + 8.0f <= player->getPos().y && player->getPos().y <= mPos.y + 104.0f) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool PuchiPakkun::isPlayerInFireRange() {
    for (int i = 0; i < cPlayerNum; i++) {
        // Grab any active players
        PlayerObject* player = PlayerMgr::instance()->getPlayerObject(i);
        if (player) {
            if ((88.0f >= std::fabs(player->getPos().x - mPos.x))  && (96.0f >= std::fabs(player->getPos().y - mPos.y))) {
                mFireDirection = ActorUtil::getTrgToSrcDir(*player, *this);
                setFireDistance(std::fabs(player->getPos().x - mPos.x));
                return true;
            }
        }
    }
    return false;
}

// Set the distance for the fireball to travel based on Player distance
void PuchiPakkun::setFireDistance(float distance) {
    if (distance >= 65.0f) {
        mFireDist = 4;
    } else if (distance >= 49.0f) {
        mFireDist = 3;
    } else if (distance >= 30.0f) {
        mFireDist = 2;
    } else {
        mFireDist = 1;
    }
}

// Check if inside a collider
u8 PuchiPakkun::checkBgIn() {
    bool pressUpDdown = false;
    if (mBgCheckObj.checkFoot() && mBgCheckObj.checkHead()) {
        pressUpDdown = true;
    }

    const sead::Vector3f& centerPos = getCenterPos();

    const BgCollisionCheckParam param = {
        ._0 = 0,
        .ignore_quicksand = false,
        .layer = mLayer,
        .collision_mask = mCollisionMask,
        .type = cBgCollisionCheckType_Solid,
        .callback = nullptr
    };
    const sead::Vector2f checkPos(
        centerPos.x,
        centerPos.y
    );
    BasicBgCollisionCheck bgCheck(param);
    bool bgIn = bgCheck.checkPoint(nullptr, checkPos);

    s32 type = 0;
    if (pressUpDdown && bgIn) {
        type = 1;
    }

    return type;
}

// Kill the Nipper Plant due to being crushed
void PuchiPakkun::setDeathInfo_Hasami() {
    u8 dir = mPos.x - mPosPrev.x < 0.0f ? cDirType_Left : cDirType_Right;

    hitdamageEffect(getPos2D());
    GameAudio::getAudioObjEmy()->startSound("SE_EMY_DOWN", mPos);

    ENEMY_MAKE_DEATH_INFO_ARG_FALL_NO_SCORE_NO_PLAYER(arg);
    arg.speed.x = cDieFallInitSpeedX[dir];
    arg.speed.y = cDieFallInitSpeedY;
    arg.max_fall_speed = cDieFallMaxFallSpeed;
    arg.gravity = cDieFallGravity;
    arg.direction = dir;
    mDeathInfo.kill(arg);
}

// Idle state
void PuchiPakkun::initializeState_Idle() {
    // Play animation
    mModel->playSklAnim("attack");
    syncAnim();

    // Set gravity
    mGravity = cDefaultGravity;
    mSpeedMax.set(0.0f, cMaxSpeedY, 0.0f);
    mSpeed.y = 0.0f;
}

void PuchiPakkun::executeState_Idle() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    if (mBgCheckObj.checkFoot()) { // If touching the ground
        // Do a small jump during a bah
        if (InputMgr::instance()->isBgmAccentSign(1 << 0)) {
            mIsBahJump = true;
            changeState(StateID_Jump);
            return;
        }
        if (isPlayerAbove()) {
            changeState(StateID_Jump);
            return;
        }
        if (mSpitsFire && isPlayerInFireRange() && mFireCooldown == 0) {
            changeState(StateID_FireSpit);
        }
    }
    if (mFireCooldown > 0) {
        mFireCooldown--;
    }
}

void PuchiPakkun::finalizeState_Idle() { }

// Walk state
void PuchiPakkun::initializeState_Walk() {
    // Play jump animation if not coming from turn state
    if (!isOldState(StateID_Turn)) {
        mModel->playSklAnim("jump");
        syncAnim();
    }
    setWalkSpeed();

    // Set gravity
    mGravity = cDefaultGravity;
    mSpeedMax.set(0.0f, cMaxSpeedY, 0.0f);
}

void PuchiPakkun::executeState_Walk() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();
    // Finish rotating on the offchance we're not already fully facing a direction
    mAngle.y().chaseRest(PuchiPakkun::cBaseAngleY[mDirection], cTurnSpeed);

    if (mBgCheckObj.checkFoot()) { // If touching the ground
        if (checkLedge()) {
            changeState(StateID_Turn);
            return;
        }

        // Turn if touching a wall
        if (mBgCheckObj.checkWall(mDirection)) {
            mSpeed.x = 0.0f;
            changeState(StateID_Turn);
        } else {
            mSpeed.y = 1.2f;
        }
    }

    if (isPlayerAbove()) {
        changeState(StateID_Jump);
        return;
    }

    // Do a small jump during a bah
    if (InputMgr::instance()->isBgmAccentSign(1 << 0)) {
        mIsBahJump = true;
        changeState(StateID_Jump);
    }
}

void PuchiPakkun::finalizeState_Walk() { }

// Turn state
void PuchiPakkun::initializeState_Turn() {
    // Play jump animation if not coming from walk state
    if (!isOldState(StateID_Walk)) {
        mModel->playSklAnim("jump");
        syncAnim();
    } else {
        mDirection = InvDirX(mDirection);
    }

    mSpeed.x = 0.0f;
}

void PuchiPakkun::executeState_Turn() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();


    if (mBgCheckObj.checkFoot()) { // If touching the ground
        // Set Y speed to 0
        mSpeed.y = 0.0f;
    }

    // Do a small jump during a bah
    if (InputMgr::instance()->isBgmAccentSign(1 << 0)) {
        mIsBahJump = true;
        changeState(StateID_Jump);
    }

    // Return to the Walk state if we've finished turning
    if (mAngle.y().chaseRest(cBaseAngleYTurn[mDirection], cTurnSpeed)) {
        changeState(StateID_Walk);
    }
}

void PuchiPakkun::finalizeState_Turn() { }

// Jump state
void PuchiPakkun::initializeState_Jump() {
    // Play jump animation
    mModel->playSklAnim("jump");
    syncAnim();

    // Set X & Y speeds
    mSpeed.x = 0.0f;
    if (mIsBahJump) {
        mSpeed.y = 2.0f;
    } else {
        switch (mJumpHeight) {
            default:
                mSpeed.y = 5.5f; 
                break;
            case 1:
                mSpeed.y = 4.5f;
                break;
            case 2:
                mSpeed.y = 3.5f;
                break;
        }
    }
}

void PuchiPakkun::executeState_Jump() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    if (mBgCheckObj.checkFoot()) { // If touching the ground
        if (mWalks) {
            changeState(StateID_Walk);
        } else {
            changeState(StateID_Idle);
        }
    }
}

void PuchiPakkun::finalizeState_Jump() {
    mIsBahJump = false;
}

// Fire spit state
void PuchiPakkun::initializeState_FireSpit() {
    // Play spit animation
    mModel->playSklAnim("spit");
    mFireTimer = 15;
}

void PuchiPakkun::executeState_FireSpit() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    // Spawn a fireball every 15 frames
    if (mFireTimer == 15 && isPlayerInFireRange()) {
        ActorCreateParam fireBall;
        fireBall.param_0 = mFireHeight << 8 | mFireDist << 4 | mFireDirection;
        fireBall.param_1 = mActorUniqueID.getValue();
        // Create an iceball instead if bit 50 of the spritedata is set & play spit sound
        if (mSpitsIce) {
            GameAudio::getAudioObjEmy()->startSound("SE_EMY_ICE_BROS_ICE", mPos);
            fireBall.profile = IceBallPuchiPakkun::sProfile;
        } else {
            GameAudio::getAudioObjEmy()->startSound("SE_EMY_FIRE_BROS_FIRE", mPos);
            fireBall.profile = FireBallPuchiPakkun::sProfile;
        }
        fireBall.position = sead::Vector3f(mPos.x, mPos.y + 10.0f, mPos.z);
        fireBall.param_ex_0.course.layer = mLayer;
        ActorMgr::instance()->createImmediately(fireBall);

        mSpatFireCount++;
        mFireTimer = 0;
    }
    // Return to the Idle state if 4 fireballs have been spit or the player leaves the fire range
    if (mSpatFireCount > 3 || !isPlayerInFireRange()) {
        changeState(StateID_Idle);
    }
    mFireTimer++;
}

void PuchiPakkun::finalizeState_FireSpit() {
    // Reset fire spitting related values
    mFireTimer = 0;
    mSpatFireCount = 0;
    mFireCooldown = 45;
}

// Frozen state
void PuchiPakkun::initializeState_IceWait() {
    // Set model for frozen
    mModel->playSklAnim("attack");
    mScale.set(0.89f, 0.89f, 0.89f);
    mModel->getTexAnim(0)->getFrameCtrl().setFrame(1.0f);
    removeCollisionCheck();
    // Restore the touch collider, frozen Nippers can be tapped
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheckDrcTouch);

    // Spawn the ice block
    ActorCreateParam IceBlock;
    IceBlock.param_0 = mParam1 >> 31 & 1;
    IceBlock.parent_id = mActorUniqueID;
    IceBlock.profile = Profile::get(ProfileInfo::cProfileID_Freezer);
    IceBlock.position = mPos;
    IceBlock.param_ex_0.course.layer = mLayer;
    ActorMgr::instance()->createImmediately(IceBlock);
}

void PuchiPakkun::executeState_IceWait() {
    // 0 = Has fully melted
    // 1 = Not melting
    // 2 = melting
    int meltResult = 0;
    for (ActorBase& child : mChildList) {
        // Get our child actor
        Freezer* freezer = sead::DynamicCast<Freezer>(&child);
        if (freezer != nullptr) {
            // If it's the ice cube, check the melt state
            meltResult = 1;
            if (freezer->getIsMelting() != 0) {
                meltResult = 2;
            }
        }
    }
    if (meltResult == 2) {
        // Reset the size and texture animation
        mScale.set(1.0f, 1.0f, 1.0f);
        mModel->getTexAnim(0)->getFrameCtrl().setFrame(0.0f);
    } else if (meltResult == 0) {
        // Set the "has melted" flag and go to initial state
        FlagCtrl::instance()->setFlagData(CourseInfo::instance()->getAreaNo(), mPos.x, mPos.y, 1);
        reviveCollisionCheck();
        if (mWalks) {
            changeState(StateID_Walk);
        } else {
            changeState(StateID_Idle);
        }
    }
}

void PuchiPakkun::finalizeState_IceWait() { }

} // namespace propelpartsu
