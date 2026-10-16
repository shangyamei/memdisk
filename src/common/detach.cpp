#include "detach.h"
#include "privilege.h"
#include <optional>
namespace md {
DWORD VerifyIdentity(HANDLE volume,ULONGLONG id){
    ULONGLONG actual=0;DWORD bytes=0;
    if(!DeviceIoControl(volume,MD_IOCTL_IDENTIFY,nullptr,0,&actual,sizeof(actual),&bytes,nullptr))return GetLastError();
    return bytes==sizeof(actual)&&actual==id?ERROR_SUCCESS:ERROR_INVALID_DRIVE;
}
namespace {
class VolumeActions final:public DetachActions {
    const MD_DISK_INFO& disk_;
    std::optional<ScopedPrivilege> privilege_;
    Handle volume_;
public:
    explicit VolumeActions(const MD_DISK_INFO& disk):disk_(disk){}
    DWORD Apply(DetachStep step) override {
        DWORD bytes=0;
        switch(step){
        case DetachStep::Privilege:
            privilege_.emplace(SE_MANAGE_VOLUME_NAME);return privilege_->Error();
        case DetachStep::Open: {
            const auto path=std::wstring(L"\\\\.\\")+disk_.Letter+L":";
            volume_.value=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
                FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
            return volume_?ERROR_SUCCESS:GetLastError();
        }
        case DetachStep::Identity:return VerifyIdentity(volume_.value,disk_.Id);
        case DetachStep::Lock: {
            DWORD error=ERROR_ACCESS_DENIED;
            // Retry transient Explorer/AV metadata handles; never force a lock.
            for(unsigned attempt=0;attempt<10;++attempt){
                if(DeviceIoControl(volume_.value,FSCTL_LOCK_VOLUME,nullptr,0,nullptr,0,&bytes,nullptr))return ERROR_SUCCESS;
                error=GetLastError();
                if(error!=ERROR_ACCESS_DENIED&&error!=ERROR_SHARING_VIOLATION&&error!=ERROR_LOCK_VIOLATION)break;
                if(attempt<9)Sleep(100);
            }
            return error;
        }
        case DetachStep::Dismount:
            return DeviceIoControl(volume_.value,FSCTL_DISMOUNT_VOLUME,nullptr,0,nullptr,0,&bytes,nullptr)?
                ERROR_SUCCESS:GetLastError();
        case DetachStep::RemoveMount:return DriverCall(Request(MdUnmount,disk_.Id)).Error;
        case DetachStep::Destroy:return DriverCall(Request(MdDestroy,disk_.Id)).Error;
        default:return ERROR_INVALID_FUNCTION;
        }
    }
};
}
DetachResult SafeDetach(const MD_DISK_INFO& disk,bool destroy){
    VolumeActions actions(disk);return RunSafeDetach(disk.Mounted!=0,destroy,actions);
}
MD_REPLY DetachDirect(const MD_REQUEST& request){
    if(request.Version!=MD_VERSION||(request.Operation!=MdUnmount&&request.Operation!=MdDestroy)||!request.Id)
        return Failure(ERROR_INVALID_PARAMETER,L"Safe detach request");
    auto list=DriverCall(Request(MdList));if(list.Error)return list;
    for(ULONG i=0;i<list.Count;++i)if(list.Disks[i].Id==request.Id){
        const auto result=SafeDetach(list.Disks[i],request.Operation==MdDestroy);
        if(result.Error)return Failure(result.Error,std::wstring(DetachStage(result.Step))+
            L" [id="+std::to_wstring(request.Id)+L", "+list.Disks[i].Letter+L":]");
        return DriverCall(Request(MdList));
    }
    return Failure(ERROR_NOT_FOUND,L"Instance");
}
}
