#include <propelpartsu/actor/IceBallPuchiPakkun.h>
#include <propelpartsu/PropelPartsU.h>
#include <bullet/IceBallBros.h>
#include <collision/ActorCollisionCheckMgr.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(IceBallPuchiPakkun, IceBallBase);

// Profile
Profile* IceBallPuchiPakkun::sProfile = getRegistrar()->newProfile<IceBallPuchiPakkun>("iceballpuchipakkun")
    .flag(Profile::cFlag_DrawCullCheck | Profile::cFlag_WinKillNoScore)
    .build();

// Main collider
const ActorCollisionCheck::CollisionData IceBallPuchiPakkun::cCollisionData = {
    .center_offset = {0.0f, 0.0f},
    .half_size = {6.0f, 6.0f}, 
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Tama,
    .attack = ActorCollisionCheck::cAttack_IceBall,
    .vs_kind = ActorCollisionCheck::TargetKind(
        ActorCollisionCheck::cTargetKind_Player |
        ActorCollisionCheck::cTargetKind_Enemy |
        ActorCollisionCheck::cTargetKind_Tama |
        ActorCollisionCheck::cTargetKind_Killer |
        ActorCollisionCheck::cTargetKind_ChibiYoshi
    ),
    .vs_damage = ActorCollisionCheck::DamageFrom(
        ActorCollisionCheck::cDamageFrom_FireBall |
        ActorCollisionCheck::cDamageFrom_Spin |
        ActorCollisionCheck::cDamageFrom_YoshiEat
    ),
    .status = ActorCollisionCheck::cStatus_None,
    .callback = &IceBallPuchiPakkun::collcheck,
};

// Do not freeze the Nipper Plant that we came from
void IceBallPuchiPakkun::collcheck(ActorCollisionCheck *cc_self, ActorCollisionCheck *cc_other) {
    Actor* other = cc_other->getOwner();
    IceBallPuchiPakkun* self = cc_self->getOwner<IceBallPuchiPakkun>();
    if (other->getActorUniqueID().getValue() != self->mParam1) {
        IceBallBros::collcheck(cc_self, cc_other);
    }
}

// Class constants
static const ActorBgCollisionCheck::Sensor cBcSensorFoot = { 0.0f, 0.0f,  0.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorHead = {  0.0f, 0.0f, 8.0f };
static const ActorBgCollisionCheck::Sensor cBcSensorWall = {  4.0f, 4.0f,  4.0f };

IceBallPuchiPakkun::IceBallPuchiPakkun(const ActorCreateParam& param)
    : IceBallBase(param)
{ }

void IceBallPuchiPakkun::executeState_Move() {
    // IceBallBase::executeState_Move doesn't do any speed calculations, add some
    calcSpeedY_();
    // Increase iceball scale slowly until full size
    mScale.setLerp(mScale, sead::Vector3f::ones, 0.1f);
    return IceBallBase::executeState_Move();
}

bool IceBallPuchiPakkun::initialize() {
    // Set the iceball scale to 0
    mScale.set(0.0f, 0.0f, 0.0f);
    mDirection = static_cast<DirType>(mParam0 & 1);

    // Set collider
    mCollisionCheck.set(this, cCollisionData);
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheck);

    // Set tile sensors
    mBgCheckObj.set(this, &cBcSensorFoot, &cBcSensorHead, &cBcSensorWall);

    mChibiEatData.setEatType(ChibiYoshiEatData::cEatType_Drink);

    changeState(StateID_Move);
    return true;
}

// Set initial iceball speed
void IceBallPuchiPakkun::setInitialSpeed() {
    // Set starting speed based on actor parameters
    f32 distance = (mParam0 >> 4 & 0xF) * 16.0f;
    f32 height   = (mParam0 >> 8 & 0xF) * 16.0f;

    mGravity = ACTOR_DEFAULT_GRAVITY;
    mSpeedMax.y = ACTOR_DEFAULT_MAX_FALL_SPEED;

    // Find the amount of time it takes to reach the peak of the arc
    u32 riseTime = (1.0f + sqrtf(1.0f + 8.0f * height / -mGravity)) / 2.0f;

    // Launch speed so that the iceball peaks at the height of the arc
    f32 vy0 = height / riseTime - mGravity * (riseTime + 1) / 2.0f;

    // Find how many frames it takes to come back down to the ground
    f32 y = 0.0f, v = vy0, prevY = 0.0f;
    u32 frames = 0;
    do {
        prevY = y;
        v += mGravity;
        if (v < mSpeedMax.y) {
            v = mSpeedMax.y;
        }
        y += v;
        frames++;
    } while (!(v < 0.0f && y <= 0.0f) && frames < 1000);

    f32 T = (frames - 1) + prevY / (prevY - y);
    f32 baseXSpeed = distance / T;

    mSpeed.x = (mDirection) ? -baseXSpeed : baseXSpeed;
    mSpeed.y = vy0;
}

} // namespace propelpartsu
