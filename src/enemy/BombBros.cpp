#include <propelpartsu/enemy/BombBros.h>
#include <propelpartsu/PropelPartsU.h>
#include <graphics/JointBlendModel.h>
#include <map_obj/ActorCoinMgr.h>
#include <map/LayerID.h>
#include <utility/Mtx.h>

namespace propelpartsu {

SEAD_RTTI_OVERRIDE_IMPL(BombBros, BrosBase);

// Profile
Profile* BombBros::sProfile = getRegistrar()->newProfile<BombBros>("bombbro")
    .resources<"bomb_bros", "bombhei">(ProfileInfo::cResType_Course)
    .createInfo(cCreateInfo)
    .flag(Profile::cFlag_DrawCullCheck | Profile::cFlag_WinKill)
    .build();

// Create parameters
const ActorCreateInfo BombBros::cCreateInfo = {
    .offset_x = 8, .offset_y = -16,
    .spawn_range = {
        .offset_x = 0, .offset_y = 16,
        .half_size_x = 8, .half_size_y = 16
    },
    .cull_range = { 
        .up = 0, .down = 0, .left = 0, .right = 0
    },
    .flag = ActorCreateInfo::cFlag_NoCoinEdit
};

BombBros::BombBros(const ActorCreateParam& param)
    : BrosBase(param)
{ }

ActorBase::Result BombBros::create() {
    BrosBase::create();

    // Load a new model, since the original BrosBase loaded it's model from cobPack
    mBlendModel = JointBlendModel::create("bomb_bros", "bros", 10);
    mBlendModel->playSklAnim("walk");
    initModel();
    changeState(StateID_Attack);
    calcMdl_Base();

    return cResult_Success;
}

bool BombBros::hitCallback_Shell(ActorCollisionCheck* cc_self, ActorCollisionCheck* cc_other) {
    BombProjectile* bomb = cc_other->getOwner<BombProjectile>();
    if (bomb) {
        // Hit with a Bomb Bro Bomb
        if (bomb->getPlayerBomb()) {
            // Player Bomb, spawn extra coins as a reward
            ActorCoinMgr::UpCoinArg coinArg = {
                .pos = getCenterPos(),
                .dir = bomb->getDirection(),
                .count = 5,
                .layer = cLayerID_Layer1,
                .spawn_height = 0,
                ._13 = 0,
                .spacing_decrease = 0,
                ._15 = 0,
                .curve_type = 0,
            };
            ActorCoinMgr::instance()->createUpCoin(coinArg);
            return Enemy::hitCallback_Shell(cc_self, cc_other);
        } else {
            // Not player bomb, do nothing
            return false;
        }
    } else {
        // Not hit with a Bomb Bro Bomb
        return Enemy::hitCallback_Shell(cc_self, cc_other);
    }
}

// Delete a held bomb if frozen
void BombBros::initializeState_Ice() {
    BombProjectile* bomb = ActorMgr::instance()->getActorPtr<BombProjectile>(mWeaponID);
    if (bomb) {
        bomb->deleteActor(true);
        mWeaponID.setValue(ActorUniqueID::cInvalidID);
    }
    return Enemy::initializeState_Ice();
}

// Get world position of the Bro's throwing hand
sead::Vector3f BombBros::getCreateWeaponPos() {
    sead::Vector3f bone6Translate = mBone6Transform.getTranslation();
    Mtxf mtxHand;
    if (mDirection == cDirType_Left) {
        mtxHand.makeT(sead::Vector3f(3.0f, -2.0f, 0.0f));
        mBone6Transform.makeIdentity();
        mtxHand.setMul(mBone6Transform, mtxHand);
    } else {
        mtxHand.makeT(sead::Vector3f(-2.0f, -2.0f, 0.0f));
        mBone6Transform.makeIdentity();
        mtxHand.setMul(mBone6Transform, mtxHand);
    }

    sead::Vector3f ret(
        bone6Translate.x + mtxHand.m[0][3],
        bone6Translate.y + mtxHand.m[1][3],
        bone6Translate.z + mtxHand.m[2][3]
    );
    return ret;
}

} // namespace propelpartsu
