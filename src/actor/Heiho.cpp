#include <propelpartsu/actor/Heiho.h>
#include <propelpartsu/PropelPartsU.h>
#include <collision/ActorCollisionCheckMgr.h>
#include <utility/Mtx.h>
#include <collision/BgCollisionCheckResult.h>
#include <player/PlayerBase.h>
#include <effect/EffectCreateUtil.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(Heiho, Enemy);

// States
CREATE_STATE_ID(Heiho, Walk)
CREATE_STATE_ID(Heiho, Turn)
CREATE_STATE_ID(Heiho, Sleep)
CREATE_STATE_ID(Heiho, Jump)
CREATE_STATE_ID(Heiho, Dizzy)
CREATE_STATE_ID(Heiho, Touch)

// Profile
Profile* Heiho::sProfile = getRegistrar()->newProfile<Heiho>("heiho")
    .resources<"heiho">(ProfileInfo::cResType_Course)
    .createInfo(cCreateInfo)
    .build();

// Create parameters
const ActorCreateInfo Heiho::cCreateInfo = {
    .offset_x = 8, .offset_y = -16,
    .spawn_range = {
        .offset_x = 0, .offset_y = 12,
        .half_size_x = 8, .half_size_y = 12
    },
    .cull_range = { 
        .up = 0, .down = 0, .left = 0, .right = 0
    },
    .flag = ActorCreateInfo::cFlag_None
};

// Main collider
const ActorCollisionCheck::CollisionData Heiho::cCollisionData = {
    .center_offset = {0.0f, 10.0f},
    .half_size = {8.0f, 10.0f}, 
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
    .vs_damage = ActorCollisionCheck::cDamageFrom_All,
    .status = ActorCollisionCheck::cStatus_None,
    .callback = &Enemy::normal_collcheck,
};

// Gamepad touch collider
const ActorCollisionCheck::CollisionData Heiho::cCollisionData_DRC = {
    .center_offset = {0.0f, 14.0f},
    .half_size = {10.0f, 14.0f}, 
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Enemy,
    .attack = ActorCollisionCheck::cAttack_None,
    .vs_kind = ActorCollisionCheck::cTargetKind_DrcTouch,
    .vs_damage = ActorCollisionCheck::cDamageFrom_None,
    .status = ActorCollisionCheck::cStatus_None,
    .callback = nullptr
};

// Gamepad touch callback
bool Heiho::DrcTouchCB::ccSetTouchNormal(ActorCollisionCheck* cc, const sead::Vector2f& pos) {
    Heiho* heiho = cc->getOwner<Heiho>();
    if (heiho != nullptr) {
        // Call onDrcTouch when tapped
        heiho->onDrcTouch();
    }
    return true;
}

const f32 Heiho::cMaxSpeedX = 1.0f;
const f32 Heiho::cMaxSpeedY = -4.0f;

const f32 Heiho::cWalkSpeed[cDirType_NumX] = { 0.6f, -0.6f };

static const ActorBgCollisionCheck::Sensor cBcSensorFoot = { -4.0f, 4.0f,  0.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorHead = {  0.0f, 0.0f, 20.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorWall = {  3.0f, 8.0f,  8.0f };

Heiho::Heiho(const ActorCreateParam& param)
    : Enemy(param)
    , mModel()
    , mYoshiEatData(mActorUniqueID)
    , mBabyYoshiEatData(mActorUniqueID)
    , mEnemyHitRevX(0.0f)
    , mDizzyEffect()
    , mBoyoMgr(this)
    , mTimer(0)
    , mBaseline(0.0f)
    , mJumpCounter(0)
{ }

ActorBase::Result Heiho::create() {
    // Model setp
    setupModel();

    // Set max speed for gravity
    mSpeedMax.y = cMaxSpeedY;

    // Set collider
    mCollisionCheck.set(this, Heiho::cCollisionData);
    mCollisionCheckDrcTouch.set(this, Heiho::cCollisionData_DRC);
    reviveCollisionCheck();

    // Assign parameters
    // Nybble 5
    mType = static_cast<HeihoType>(mParam0 >> 28 & 0xF);
    // Nybble 6
    mColor = mParam0 >> 24 & 0xF;
    // Nybble 8 mask 2
    mHealth = mParam0 >> 17 & 1;
    // Nybble 9
    mDistance = mParam0 >> 12 & 0xF;
    // Bit 40
    mSpawnDir = static_cast<DirType>((mParam0 >> 8 & 1)^1);

    mModel->getTexAnim(0)->getFrameCtrl().setFrame(mColor);

    // Center of the actor, for dieFall and yoshi tongue
    mCenterOffset.set(0.0f, 12.0f, 0.0f);

    // Set culling distance and size
    mVisibleAreaSize.set(16.0f, 24.0f);
    mVisibleAreaOffset.set(0.0f, 12.0f);
    mSize.set(16.0f, 24.0f);

    // Tile sensors
    mBgCheckObj.set(this, &cBcSensorFoot, &cBcSensorHead, &cBcSensorWall);

    // Yoshi eat ability
    mEatDataPtr = &mYoshiEatData;
    mYoshiEatData.setEatType(EatData::cEatType_Drink);
    
    // Baby Yoshi eat ability
    mChibiYoshiEatDataPtr = &mBabyYoshiEatData;
    mBabyYoshiEatData.setEatType(ChibiYoshiEatData::cEatType_Drink);

    // Gamepad touch
    mCollisionCheckDrcTouch.setDrcTouchCallback(&mDrcTouchCallback);

    // Set spawn direction
    DirType direction;
    if (mType < cHeihoType_Jump || mType > cHeihoType_Pace) {
        direction = getPlayerDirLR();
    } else {
        direction = mSpawnDir;
    }
    mDirection = direction;
    mAngle.y() = cBaseAngleY[mDirection];

    // Set starting state
    setInitialState();

    calcMdl_();

    return cResult_Success;
}

bool Heiho::execute() {
    // Update boyon
    mBoyoMgr.execute();

    executeState();
    calcMdl_Normal();

    // Delete actor if offscreen
    screenOutCheck(cScreenOutFlag_SkipNone);

    return true;
}

bool Heiho::draw() {
    drawModel();

    return true;
}

// Adjusted to account for both hitboxes
void Heiho::reviveCollisionCheck() {
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheck);
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheckDrcTouch);
}

void Heiho::removeCollisionCheck() {
    ActorCollisionCheckMgr::instance()->release(mCollisionCheck);
    ActorCollisionCheckMgr::instance()->release(mCollisionCheckDrcTouch);
}

// If the Shyguy is facing opposite of the player on contact, flip the shyguy
bool Heiho::setDamage(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    PlayerBase* player = cc_other->getOwner<PlayerBase>();
    if (player != nullptr) {
        if (player->setNormalDamage(cc_self)) {
            setTurnByPlayerHit(player);
            return true;
        }
    }
    return false;
}

// Change the ice cube size when frozen
bool Heiho::createIceActor() {
    sead::Vector3f pos(
        mPos.x,
        mPos.y - 3.8f,
        mPos.z
    );
    IceInfo info = { 0, pos, sead::Vector3f(1.3f, 1.5f, 1.5f), nullptr };
    return mIceMgr.createIce(info);
}

// Turn the Shyguy if touching another actor or baby yoshi
void Heiho::vsEnemyHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    Actor* actor = cc_other->getOwner();
    // The Maruta (Giant Rolling Log) check here is copied from Kuribo, not sure how important it is
    if (actor != nullptr && actor->getProfileID() != ProfileInfo::cProfileID_Maruta) {
        // Save the "Revision X" of the Shyguy
        if (actor->getKind() == cActorKind_Enemy) {
            mEnemyHitRevX = cc_self->getRevisionX(ActorCollisionCheck::cKind_Enemy);
        } else if (actor->getKind() == cActorKind_ChibiYoshi) {
            mEnemyHitRevX = cc_self->getRevisionX(ActorCollisionCheck::cKind_ChibiYoshi);
        }
        setTurnByEnemyHit(cc_self->getOwner(), actor);
    }
}

// Process Shyguy to player collisions
void Heiho::vsPlayerHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    Actor* actor = cc_other->getOwner();
    // Determines the hit type of the enemy
    FumiType fumi_type = fumiCheck(cc_self, cc_other, cFumiSeType_Normal);
    if (fumi_type == cFumiType_Fumi) { // Jumped on
        reactFumiProc(actor);
    } else if (fumi_type == cFumiType_MameFumi) { // Jumped on (Mini-Mushroom)
        return;
    } else if (fumi_type == cFumiType_SpinFumi) { // Spin Jumped on
        reactSpinFumiProc(actor);
    } else { // Player should take damage
        Enemy::vsPlayerHitCheck_Normal(cc_self, cc_other);
    }
}

// Process Shyguy to player riding yoshi collisions
void Heiho::vsYoshiHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    PlayerBase* player = cc_other->getOwner<PlayerBase>();
    Actor* actor_self = cc_self->getOwner();
    switch (fumiCheck(cc_self, cc_other, cFumiSeType_Normal)) {
        case cFumiType_Fumi: // Jumped on
            reactYoshiFumiProc(player);
            break;
        case cFumiType_Hit: // Player should take damage
            Enemy::vsPlayerHitCheck_Normal(cc_self, cc_other);
            break;
    }
}

// Treat colliding with baby yoshis as colliding with a normal actor
void Heiho::vsChibiYoshiHitCheck_Normal(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other)
{
    vsEnemyHitCheck_Normal(cc_self, cc_other);
}

// Play diefall animation
void Heiho::initializeState_DieFall() {
    mModel->setAnm("diefall");
    return Enemy::initializeState_DieFall();
}

// Fall off the screen if dying from a normal stomp
void Heiho::initializeState_DieOther() {
    // Remove collider
    removeCollisionCheck();
    // Play animation
    mModel->setAnm("die", 5.0f, FrameCtrl::cMode_Repeat, 1.15f);
    // Set gravity
    mAngle.y() = 0;
    mSpeed.set(0.0f, 0.0f, 0.0f);
    mGravity = cDefaultGravity;
}

void Heiho::executeState_DieOther() {
    // Calculate the Shyguy falling
    calcSpeedY_();
    posMove_();
}

// Load model and associated animations
void Heiho::setupModel() {
    mModel = JointBlendModel::create("heiho", "heiho", 13, 1, 0, 0, 0);
    mModel->playTexAnim("color");
    mModel->getTexAnim(0)->getFrameCtrl().setRate(0.0f);
}

// Register model for drawing
void Heiho::drawModel() {
    mModel->draw();
}

// Calculate the model's matricies
void Heiho::calcMdl_() {
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
    if (!isState(StateID_Ice)) {
        mModel->playAnmFrameCtrl();
    }

    mModel->calc();
}

void Heiho::setInitialState(bool reset_pacer_dist) {
    // Set initial state
    switch (mType) {
        case cHeihoType_Pace:
            if (reset_pacer_dist) {
                // Set pacer final distances
                if (mType == cHeihoType_Pace) {
                    float finalOffset = 16.0f * mDistance - 8.0f;
                    mFinalPos[0] = mPos.x + finalOffset;
                    mFinalPos[1] = mPos.x - finalOffset;
                }
            }
        default:
            changeState(StateID_Walk);
            break;
        
        case cHeihoType_Sleep:
            changeState(StateID_Sleep);
            break;

        case cHeihoType_Jump:
            changeState(StateID_Jump);
            break;
    }
}

// On Gamepad tap
void Heiho::onDrcTouch() {
    // Start boyon
    mBoyoMgr.begin();
    // If dizzy, kill the shyguy
    if (isState(StateID_Dizzy)) {
        GameAudio::getAudioObjEmy()->startSound("SE_EMY_CMN_TOUCH_POCON", mPos);
        setDeathInfo_Fall(mDirection);
    }
    // Otherwise, enter Touch state if on the ground
    if (mBgCheckObj.checkFoot()) {
        if (!isState(StateID_Touch)) {
            changeState(StateID_Touch);
        }
    }
}

// Enter the Turn state if in the Walk state, if colliding with another actor
void Heiho::setTurnByEnemyHit(Actor* actor_self, Actor* actor_other) {
    if (mDirection == cDirType_Left && mEnemyHitRevX > 0.0f || mDirection == cDirType_Right && mEnemyHitRevX < 0.0f) {
        if (!isState(StateID_Turn) &&
            !isState(StateID_Sleep) &&
            !isState(StateID_Jump) &&
            !isState(StateID_Dizzy) &&
            !isState(StateID_Touch)) {
            changeState(StateID_Turn);
        }
    }
}

// Turn if the player bumps into the back of the Shyguy
void Heiho::setTurnByPlayerHit(Actor* player) {
    if (!isState(StateID_Sleep) &&
        !isState(StateID_Dizzy)) {
        mDirection = getPlayerDirLR();
        if (isState(StateID_Turn)) {
            changeState(StateID_Walk);
        }
        mAngle.y() = cBaseAngleY[mDirection];
        if (isState(StateID_Walk)) {
            setWalkSpeed();
        }
        calcMdl_();
    }
}

// React to being stomped on
void Heiho::reactFumiProc(Actor* player) {
    if (mHealth == 1) {
        // Decrement health and enter Dizzy state if set to 2 hits mode
        mHealth = 0;
        changeState(StateID_Dizzy);
    } else {
        // Kill the Shyguy, and enter the dieOther state
        setDeathInfo_FumiOther(player, mSpeed);
    }
}

// React to spinjumps
void Heiho::reactSpinFumiProc(Actor* player)
{
    setDeathInfo_SpinFumi(player);
}

// React to yoshi stomps
void Heiho::reactYoshiFumiProc(Actor* yoshi)
{
    setDeathInfo_YoshiFumi(yoshi);
}

// Set the walking speed based on direction
void Heiho::setWalkSpeed() {
    mSpeed.x = cWalkSpeed[mDirection];
}

// Returns true if the Shyguy is approacing a ledge
bool Heiho::checkLedge() {
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
    
    sead::Vector3f p0 = mPos + sead::Vector3f(2.5f * cEnMuki[mDirection], 0.0f, 0.0f);
    sead::Vector3f p1 = p0 + sead::Vector3f(16.0f * cEnMuki[mDirection], 2.0f * -16.0f, 0.0f);
    
    return !bgChk.checkArea(&res, p0, p1, 1 << cDirType_Down);
}

// Walk state
void Heiho::initializeState_Walk() {
    // Play walk animation if not coming from Turn/Touch state
    if (!isOldState(StateID_Turn) && !isOldState(StateID_Touch)) {
        mModel->setAnm("walk", 3.0f);
    }
    // Set walk speed
    setWalkSpeed();
    // Set gravity
    mGravity = cDefaultGravity;
    mSpeedMax.set(0.0f, cMaxSpeedY, 0.0f);
}

void Heiho::executeState_Walk() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();
    // Finish rotating on the offchance we're not already fully facing a direction
    mAngle.y().chaseRest(cBaseAngleY[mDirection], cTurnSpeed);
    if (mBgCheckObj.checkFoot()) { // If touching the ground
        // Set Y speed to 0
        mSpeed.y = 0.0f;

        // Turn if near a ledge
        if (checkLedge() && mType == cHeihoType_Walk_Ledge) {
            changeState(StateID_Turn);
            return;
        }
    }
    // Turn if touching a wall
    if (mBgCheckObj.checkWall(mDirection)) {
        changeState(StateID_Turn);
    }
    // Check if we've reached final pacer distances
    if (mType == cHeihoType_Pace) {
        if ((mDirection == 0 && mPos.x >= mFinalPos[0]) || (mDirection == 1 && mPos.x <= mFinalPos[1])) {
            changeState(StateID_Turn);
        }
    }
}

void Heiho::finalizeState_Walk() { }

// Turn state
void Heiho::initializeState_Turn() {
    // Invert direction and set X speed to 0
    mDirection = InvDirX(mDirection);
    mSpeed.x = 0.0f;
}

void Heiho::executeState_Turn() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();


    if (mBgCheckObj.checkFoot()) { // If touching the ground
        // Set Y speed to 0
        mSpeed.y = 0.0f;
    }
    // Return to the Walk state if we've finished turning
    if (mAngle.y().chaseRest(cBaseAngleY[mDirection], cTurnSpeed)) {
        changeState(StateID_Walk);
    }
}

void Heiho::finalizeState_Turn() { }

// Sleep state
void Heiho::initializeState_Sleep() {
    // Play sleep animation
    mModel->setAnm("sleep", 7.5f);
    // Make the Shyguy face the screen
    mAngle.y() = 0;
    // Set gravity
    mGravity = cDefaultGravity;
    mSpeedMax.set(0.0f, cMaxSpeedY, 0.0f);
    // Remove any X speed from other states
    mSpeed.x = 0.0f;
}

void Heiho::executeState_Sleep() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    if (mBgCheckObj.checkFoot()) { // If touching the ground
        // Set Y speed to 0
        mSpeed.y = 0.0f;
    }
}

void Heiho::finalizeState_Sleep() { }

// Jump state
void Heiho::initializeState_Jump() {
    mJumpCounter = 0;

    // Set gravity
    mGravity = cDefaultGravity;
    mSpeedMax.set(0.0f, cMaxSpeedY, 0.0f);
    // Remove any X speed from other states
    mSpeed.x = 0.0f;
}

void Heiho::executeState_Jump() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    // Stop moving upwards if touching a ceiling
    if (mBgCheckObj.checkHead()) {
        mSpeed.y = 0.0f;
    }

    if (mBgCheckObj.checkFoot()) { // Touching the ground
        // Landing effect
        sead::Vector3f effectPos(mPos.x, mPos.y, 4500.0f);
        EffectCreateUtil::createEffect(RP_Cmn_LandingSmoke_01, &effectPos);

        // Reset jump counter if landing from the 3rd bounce
        if (mJumpCounter == 3) {
            mJumpCounter = 0;
        }
        // Increment jump counter
        mJumpCounter++;

        // Play animation, set speed, and play sound
        if (mJumpCounter == 3) {
            mModel->setAnm("jump2", 0.0f, FrameCtrl::cMode_NoRepeat, 0.6f);
            mSpeed.y = 6.0f;
            GameAudio::getAudioObjEmy()->startSound("SE_PLY_JUMPDAI_HIGH", mPos);
        } else {
            mModel->setAnm("jump", 3.0f, FrameCtrl::cMode_NoRepeat, 0.6f);
            mSpeed.y = 4.5f;
            GameAudio::getAudioObjEmy()->startSound("SE_PLY_JUMPDAI", mPos);
        }
    }
}

void Heiho::finalizeState_Jump() { }

// Dizzy state
void Heiho::initializeState_Dizzy() {
    // Play the associated dizzy animation
    if (isOldState(StateID_Sleep)) {
        mModel->setAnm("dizzy_sleep", 0.0f);
    } else {
        mModel->setAnm("dizzy", 10.0f);
    }

    // Set gravity & reset speed values
    mGravity = cDefaultGravity;
    mSpeedMax.set(0.0f, cMaxSpeedY, 0.0f);
    mSpeed.set(0.0f, cMaxSpeedY, 0.0f);

    mTimer = 0;
}

void Heiho::executeState_Dizzy() {
    // Do speed, position, and tile collision calculations
    calcSpeedY_();
    posMove_();
    bgCheck_();

    if (mBgCheckObj.checkFoot()) { // If touching the ground
        // Set Y speed to 0
        mSpeed.y = 0.0f;
    }

    // Play a dizzy star effect above the Shyguy
    f32 effectYOffset = mCenterOffset.y - (isOldState(StateID_Sleep) ? 56.0f : 53.0f);
    sead::Vector3f effectPos(mPos.x, mPos.y + effectYOffset, 4500.0f);
    sead::Vector3f effectScale(0.5f, 0.5f, 0.5f);
    mDizzyEffect.createEffect(RP_BossKK_Piyori, &effectPos, nullptr, &effectScale);

    // Go back to previous state after 600 frames and restore health
    if (mTimer > 600) {
        setInitialState(false);
        mHealth = 1;
    }
    // Increment timer
    mTimer++;
}

void Heiho::finalizeState_Dizzy() { }

// Touch state
void Heiho::initializeState_Touch() {
    // Set X & Y speeds for the backwards bounce
    const f32 c_speed[cDirType_NumX] = { -0.5f, 0.5f };
    mSpeed.x = c_speed[mDirection];
    mSpeedMax.x = mSpeed.x;
    mSpeed.y = 3.0f;
}

void Heiho::executeState_Touch() {
    // Calculate X & Y speeds and position
    calcSpeedX_();
    calcSpeedY_();
    posMove_();
    // Finish rotating on the offchance we're not already fully facing a direction (unless coming from Sleep state)
    if (!isOldState(StateID_Sleep)) {
        mAngle.y().chaseRest(cBaseAngleY[mDirection], cTurnSpeed);
    }
    // Calculate tile collisions
    mBgCheckObj.checkBg();

    // Invert Y speed if we hit a ceiling
    if (mBgCheckObj.checkHead()) {
        mSpeed.y *= -1.0f;
    }
    // Kill our X speed if we touch a wall
    if (mBgCheckObj.checkWall(cDirType_Right) || mBgCheckObj.checkWall(cDirType_Left)) {
        mSpeed.x = 0.0f;
    }
    // Reset state if we've reached terminal velocity
    if (mSpeed.y <= mSpeedMax.y) {
        setInitialState();
        return;
    }
}

void Heiho::finalizeState_Touch() { }

} // namespace propelpartsu
