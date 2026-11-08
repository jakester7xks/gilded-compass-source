#include "GildedCompass/ObjectiveAssociation.hpp"
#include <array>
#include <cstdio>
#include <limits>
int main() {
    std::array<gc::ObservedMapIcon,3> icons{{
        {0,-10648.369f,605.268f,-3923.949f,0,1},
        {3,-10421.584f,548.373f,-4199.342f,0,1},
        {560,-10421.584f,548.373f,-4199.342f,1,1}}};
    auto target=gc::AssociateObjective(icons);
    if(!target||target->distanceMetres!=361)return 1;
    std::array cleared{icons[0],icons[2]};
    if(gc::AssociateObjective(cleared))return 2;
    // Bounty Notice - Blix, captured session 3 snapshot 13.
    icons[0]={0,-10628.128f,600.066f,-3938.552f,0,1};
    icons[1]={3,-10115.153f,579.132f,-4699.103f,0,1};
    target=gc::AssociateObjective(icons);
    if(!target||target->distanceMetres!=917)return 3;
    // Rhett's Request; stale main-quest icon must not override selection.
    icons[0]={0,-10580.551f,585.415f,-3935.849f,0,1};
    icons[1]={3,-10174.545f,559.193f,-4055.124f,0,1};
    target=gc::AssociateObjective(icons);
    if(!target||target->distanceMetres!=423)return 4;
    // Selected manual pin, now explicitly requested. Zero-height sentinel
    // uses planar distance, consistent with the captured native 245m text.
    icons[0]={0,-10505.351f,576.905f,-3971.547f,0,1};
    icons[1]={3,-10330.357f,0,-3798.732f,0,1};
    target=gc::AssociateObjective(icons);
    if(!target||target->distanceMetres!=245)return 5;
    icons[1].state274=0;
    if(gc::AssociateObjective(icons))return 6;
    icons[1].state274=1;
    std::array ambiguous{icons[0],icons[1],icons[1]};
    if(gc::AssociateObjective(ambiguous))return 7;
    std::array noPlayer{icons[1]};
    if(gc::AssociateObjective(noPlayer))return 8;
    icons[1].x=std::numeric_limits<float>::quiet_NaN();
    if(gc::AssociateObjective(icons))return 9;
    if(gc::ObjectiveScreenPosition(1,359)!=310||gc::ObjectiveScreenPosition(359,1)!=290
       ||gc::ObjectiveScreenPosition(90,0)||gc::ObjectiveScreenPosition(60,0)!=600)return 10;
    puts("Main quest, Blix, Rhett and selected manual-pin fixtures passed; absent/inactive/ambiguous destinations rejected; seam passed.");
    return 0;
}
