#include "telkin/Hooks.h"
#include <telkin/Telkin.h>
#include <bullet/FireBallBase.h>

// TODO: Move this into RedCore when FireBallBase is merged

bool isActorFireBall(Actor* actor) tRegSave{
    FireBallBase* fireball = sead::DynamicCast<FireBallBase>(actor);
    return fireball != nullptr;
}

#include <telkin/DefineRegisters.h>

void addFireBallCastCheck() tAssembly(
    mr r3, r31;

    tSaveLR;

    bl _Z15isActorFireBallP5Actor;
    mr r11, r3;

    tRestoreLR;

    mr r3, r31; // Replaced instruction
    blr;
);

#include <telkin/UndefineRegisters.h>

using namespace tk::ppc;

tBranch(0x02779FCC, addFireBallCastCheck, tk::BranchType::bl);
tPatchNop(0x02779FD4);
tPatchNop(0x02779FD8);
tPatch32u(0x02779FDC, cmpwi(R::r11, 1));