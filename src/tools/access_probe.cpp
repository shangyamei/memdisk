#include "client.h"
#include <array>
#include <iostream>
#include <stdexcept>

static void Require(bool value,const char* message) {
    if(!value)throw std::runtime_error(std::string(message)+" (Win32 "+std::to_string(GetLastError())+")");
}
int wmain(int argc,wchar_t** argv) {
    bool restricted=argc==3&&wcscmp(argv[1],L"--restricted")==0;
    bool hold=argc==3&&wcscmp(argv[1],L"--hold")==0;
    if((!restricted&&!hold&&argc!=2)||wcslen(argv[argc-1])!=1){
        std::cerr<<"Usage: memdisk-access-test [--restricted|--hold] R\n";return 2;}
    const WCHAR letter=argv[argc-1][0];
    if(letter<L'D'||letter>L'Z')return 2;
    // Refuse all non-MemDisk targets before creating any test file.
    const WCHAR dos[]={letter,L':',0};WCHAR target[256]{};
    if(!QueryDosDeviceW(dos,target,_countof(target))||wcsncmp(target,L"\\Device\\MemDisk",15)!=0){
        std::cerr<<"Target is not a MemDisk device\n";return 2;}
    if(hold){
        const auto path=std::wstring(1,letter)+L":\\.memdisk-held-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
        md::Handle file(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr));
        if(!file){std::cerr<<"Cannot hold test file\n";return 1;}
        std::cout<<"HOLD_READY"<<std::endl;
        std::string release;std::getline(std::cin,release);return 0;
    }
    md::Handle token;
    if(restricted){
        auto limited=md::RestrictedUserToken();token.value=limited.value;limited.value=INVALID_HANDLE_VALUE;
        if(!token||!SetThreadToken(nullptr,token.value)){std::cerr<<"Cannot create restricted token\n";return 2;}
    }else{
        HANDLE current=nullptr;TOKEN_ELEVATION elevation{};DWORD size=0;
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&current))return 2;
        md::Handle process(current);
        if(!GetTokenInformation(process.value,TokenElevation,&elevation,sizeof(elevation),&size)||elevation.TokenIsElevated){
            std::cerr<<"Run from ordinary PowerShell, or pass --restricted in the elevated verification script\n";return 2;}
    }
    try{
        const std::wstring root=std::wstring(1,letter)+L":\\";
        ULARGE_INTEGER total{},free{};
        Require(GetDiskFreeSpaceExW(root.c_str(),nullptr,&total,&free)&&total.QuadPart>0,"Read capacity denied");
        const auto path=root+L".memdisk-user-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
        {
            md::Handle file(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,0,nullptr,CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr));
            Require(static_cast<bool>(file),"Ordinary file create denied");
            std::array<unsigned char,4096> data{},read{};
            for(size_t i=0;i<data.size();++i)data[i]=static_cast<unsigned char>(i*17+29);
            DWORD bytes=0;
            Require(WriteFile(file.value,data.data(),static_cast<DWORD>(data.size()),&bytes,nullptr)&&bytes==data.size(),"File write failed");
            LARGE_INTEGER zero{};Require(SetFilePointerEx(file.value,zero,nullptr,FILE_BEGIN),"Seek failed");
            Require(ReadFile(file.value,read.data(),static_cast<DWORD>(read.size()),&bytes,nullptr)&&bytes==read.size(),"File read failed");
            Require(data==read,"File content mismatch");
        }
        const std::wstring raw=std::wstring(L"\\\\?\\GLOBALROOT")+target;
        for(const DWORD access:{DWORD(GENERIC_READ),DWORD(GENERIC_WRITE)}){
            md::Handle device(CreateFileW(raw.c_str(),access,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
            const DWORD error=GetLastError();
            Require(!device&&error==ERROR_ACCESS_DENIED,"Raw disk data access was not denied");
        }
        md::Handle control(CreateFileW(MD_CONTROL_PATH,GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        const DWORD error=GetLastError();
        Require(!control&&error==ERROR_ACCESS_DENIED,"Management access was not denied");
        std::cout<<"PASS: ordinary-token capacity + file create/write/read/delete; raw data + management denied\n";
        if(restricted)RevertToSelf();return 0;
    }catch(const std::exception& error){
        if(restricted)RevertToSelf();std::cerr<<error.what()<<"\n";return 1;
    }
}
