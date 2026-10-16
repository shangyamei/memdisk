#pragma once
#include "client.h"
#include <stdexcept>

namespace probe {
inline void Check(BOOL ok,const char* stage){
    if(!ok)throw std::runtime_error(std::string(stage)+" (Win32 "+std::to_string(GetLastError())+")");
}
// Exercise the separate open/close/rename/delete sequence used by lock files.
// No delete-on-close flag: that alone does not test DeleteFile/rename access.
inline void FileOperations(const std::wstring& root){
    const auto directory=root+L".memdisk-git-probe-"+std::to_wstring(GetCurrentProcessId())+
        L"-"+std::to_wstring(GetTickCount64());
    Check(CreateDirectoryW(directory.c_str(),nullptr),"Create probe directory");
    const auto git=directory+L"\\.git";
    Check(CreateDirectoryW(git.c_str(),nullptr),"Create nested .git directory");
    const auto lock=git+L"\\config.lock",config=git+L"\\config";
    for(unsigned pass=0;pass<2;++pass){
        {
            md::Handle file(CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,
                FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
            Check(static_cast<bool>(file),"Create config.lock");
            const char contents[]="[core]\nrepositoryformatversion = 0\n";
            DWORD bytes=0;
            Check(WriteFile(file.value,contents,sizeof(contents)-1,&bytes,nullptr),"Write config.lock");
            if(bytes!=sizeof(contents)-1)throw std::runtime_error("Short config.lock write");
        }
        Check(MoveFileExW(lock.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),
            pass?"Replace config from config.lock":"Rename config.lock to config");
    }
    Check(DeleteFileW(config.c_str()),"Delete config");
    Check(RemoveDirectoryW(git.c_str()),"Remove nested .git directory");
    Check(RemoveDirectoryW(directory.c_str()),"Remove probe directory");
}
}
