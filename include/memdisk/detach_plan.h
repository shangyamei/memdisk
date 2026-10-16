#pragma once
#include "protocol.h"
#include <initializer_list>
namespace md {
enum class DetachStep { Privilege,Open,Identity,Lock,Dismount,RemoveMount,Destroy,Complete };
struct DetachResult { DWORD Error=0; DetachStep Step=DetachStep::Complete; };
class DetachActions {
public:
    virtual ~DetachActions()=default;
    virtual DWORD Apply(DetachStep step)=0;
};
inline const WCHAR* DetachStage(DetachStep step){
    switch(step){
    case DetachStep::Privilege:return L"Enable SeManageVolumePrivilege";
    case DetachStep::Open:return L"Open volume";
    case DetachStep::Identity:return L"Verify instance identity";
    case DetachStep::Lock:return L"FSCTL_LOCK_VOLUME (volume in use or lock denied)";
    case DetachStep::Dismount:return L"FSCTL_DISMOUNT_VOLUME";
    case DetachStep::RemoveMount:return L"Remove mount link";
    case DetachStep::Destroy:return L"Destroy driver instance";
    default:return L"Complete";
    }
}
inline DetachResult RunSafeDetach(bool mounted,bool destroy,DetachActions& actions){
    if(mounted){
        for(const auto step:{DetachStep::Privilege,DetachStep::Open,DetachStep::Identity,
            DetachStep::Lock,DetachStep::Dismount,DetachStep::RemoveMount}){
            const auto error=actions.Apply(step);
            if(error)return {error,step};
        }
    }
    if(destroy){const auto error=actions.Apply(DetachStep::Destroy);if(error)return {error,DetachStep::Destroy};}
    return {};
}
}
