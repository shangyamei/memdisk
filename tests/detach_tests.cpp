#include "detach.h"
#include "privilege.h"
#include <stdexcept>
#include <vector>
namespace {
class Fake final:public md::DetachActions {
public:
    md::DetachStep FailAt=md::DetachStep::Complete;
    std::vector<md::DetachStep> Calls;
    DWORD Apply(md::DetachStep step) override {
        Calls.push_back(step);return step==FailAt?ERROR_ACCESS_DENIED:ERROR_SUCCESS;
    }
};
void Require(bool value){if(!value)throw std::runtime_error("Safe detach invariant failed");}
}
void DetachTests(){
    using md::DetachStep;
    for(const auto step:{DetachStep::Privilege,DetachStep::Open,DetachStep::Identity,
        DetachStep::Lock,DetachStep::Dismount,DetachStep::RemoveMount}){
        Fake fake;fake.FailAt=step;
        const auto result=md::RunSafeDetach(true,true,fake);
        Require(result.Error==ERROR_ACCESS_DENIED&&result.Step==step);
        Require(fake.Calls.back()==step); // No destruction after a failed prerequisite.
    }
    Fake unmount;Require(!md::RunSafeDetach(true,false,unmount).Error);
    Require(unmount.Calls.back()==DetachStep::RemoveMount);
    Fake destroy;Require(!md::RunSafeDetach(true,true,destroy).Error);
    Require(destroy.Calls.size()==7&&destroy.Calls.back()==DetachStep::Destroy);
    Fake already;Require(!md::RunSafeDetach(false,true,already).Error);
    Require(already.Calls.size()==1&&already.Calls[0]==DetachStep::Destroy);
    HANDLE before=nullptr;const BOOL hadThread=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&before);
    if(hadThread)CloseHandle(before);
    {
        md::ScopedPrivilege privilege(SE_CHANGE_NOTIFY_NAME);Require(!privilege.Error());
        HANDLE enabled=nullptr;Require(OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&enabled)!=FALSE);
        CloseHandle(enabled);
    }
    HANDLE after=nullptr;const BOOL hasThread=OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,TRUE,&after);
    if(hasThread)CloseHandle(after);
    Require(hadThread==hasThread);
    md::ScopedPrivilege invalid(L"MemDiskDoesNotExistPrivilege");
    Require(invalid.Error()==ERROR_NO_SUCH_PRIVILEGE);
}
