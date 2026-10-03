#include <propelpartsu/actor/FireBallPuchiPakkun.h>
#include <propelpartsu/PropelPartsU.h>
#include <bullet/FireBallBros.h>
#include <collision/ActorCollisionCheckMgr.h>
#include <graphics/LightType.h>
#include <collision/BgCollision.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(FireBallPuchiPakkun, FireBallBase);

// Profile
Profile* FireBallPuchiPakkun::sProfile = getRegistrar()->newProfile<FireBallPuchiPakkun>("fireballpuchipakkun")
    .flag(Profile::cFlag_DrawCullCheck | Profile::cFlag_WinKillNoScore)
    .build();

// Main collider
const ActorCollisionCheck::CollisionData FireBallPuchiPakkun::cCollisionData = {
    .center_offset = {0.0f, 0.0f},
    .half_size = {3.0f, 3.0f}, 
    .shape_type = ActorCollisionCheck::cShapeType_Box,
    .kind = ActorCollisionCheck::cKind_Tama,
    .attack = ActorCollisionCheck::cAttack_None,
    .vs_kind = ActorCollisionCheck::TargetKind(
        ActorCollisionCheck::cTargetKind_Player |
        ActorCollisionCheck::cTargetKind_Enemy |
        ActorCollisionCheck::cTargetKind_Tama |
        ActorCollisionCheck::cTargetKind_ChibiYoshi
    ),
    .vs_damage = ActorCollisionCheck::DamageFrom(
        ActorCollisionCheck::cDamageFrom_IceBall |
        ActorCollisionCheck::cDamageFrom_Spin |
        ActorCollisionCheck::cDamageFrom_YoshiEat
    ),
    .status = ActorCollisionCheck::cStatus_None,
    // Reuse the Fire Bro fireball collision callback
    .callback = &FireBallBros::collcheck,
};

FireBallPuchiPakkun::FireBallPuchiPakkun(const ActorCreateParam& param)
    : FireBallBase(param)
{ }

void FireBallPuchiPakkun::executeState_Move() {
    // Calculate speed, pos, and tile colliders
    calcSpeedY_();
    posMove_();
    bgCheck_();

    // Increase fireball scale slowly until full size
    mScale.setLerp(mScale, sead::Vector3f::ones, 0.1f);

    // Kill the fireball if touching anything
    if (mBgCheckObj.checkFoot() || mBgCheckObj.checkHead() || mBgCheckObj.checkWall(mDirection)) {
        kill();
        playDisappearSound();
    }
}

bool FireBallPuchiPakkun::initialize() {
    // Set the fireball scale to 0
    mScale.set(0.0f, 0.0f, 0.0f);
    // Initialize direction
    mDirection = static_cast<DirType>(mParam0 & 1);

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

    mChibiEatData.setEatType(ChibiYoshiEatData::cEatType_Drink);
    return true;
}

// Set collider
void FireBallPuchiPakkun::setCollisionCheck() {
    mCollisionCheck.set(this, cCollisionData);
    ActorCollisionCheckMgr::instance()->entry(mCollisionCheck);
}

// Resize the fireball effect with the actor scale
void FireBallPuchiPakkun::fireEffect() {
    mEffectAngle.y() = 0;
    if (mSpeed.x > 0.0f) {
        mEffectAngle.y() = -0x80000000;
    }
    mEffect.createEffect(RP_Cmn_Fireball, &mPos, &mEffectAngle, &mScale);
    mLight.update(LightType::cLightType_FireBall, &mPos);
    mLight.calcLayerOverlap();
}

} // namespace propelpartsu
