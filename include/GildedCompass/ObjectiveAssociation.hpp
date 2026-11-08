#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace gc {
struct ObservedMapIcon {
    uint16_t type;
    float x,y,z;
    uint8_t state273,state274;
};
struct AssociatedObjective {
    double bearingDegrees;
    unsigned distanceMetres;
};
// Build-specific selected navigation destination observed in the captures.
// Revised scope includes selected quest objectives and manual pins. Retained
// quest-title text and other quest icons do not determine the selected target.
inline std::optional<AssociatedObjective> AssociateObjective(std::span<const ObservedMapIcon> icons) {
    const ObservedMapIcon *player=nullptr,*destination=nullptr,*quest=nullptr;
    for(const auto& i:icons) {
        if(!std::isfinite(i.x)||!std::isfinite(i.y)||!std::isfinite(i.z)) return std::nullopt;
        if(i.type==0) { if(player)return std::nullopt; player=&i; }
        if(i.type==3) { if(destination)return std::nullopt; destination=&i; }
    }
    // Revised user scope includes selected manual pins. Use the live selected
    // destination for all navigation; retained quest text/icons do not gate it.
    if(!player||!destination||destination->state274!=1)return std::nullopt;
    quest=destination;
    const double dx=double(quest->x)-player->x,dz=double(quest->z)-player->z;
    // The inspected native distance path ignores vertical delta for zero-height targets.
    const double dy=quest->y==0.0f?0.0:double(quest->y)-player->y;
    const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(distance)||distance>1000000 || std::hypot(dx,dz)<0.01)return std::nullopt;
    return AssociatedObjective{std::fmod(std::atan2(dx,dz)*180.0/3.141592653589793+360.0,360.0),
                               static_cast<unsigned>(std::floor(distance))};
}
inline std::optional<unsigned> ObjectiveScreenPosition(double bearing,double heading) {
    if(!std::isfinite(bearing)||!std::isfinite(heading))return std::nullopt;
    const double delta=std::remainder(bearing-heading,360.0);
    if(std::abs(delta)>60.0)return std::nullopt;
    return static_cast<unsigned>(std::lround(300.0+5.0*delta));
}
}
