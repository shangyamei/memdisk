#pragma once
#include <ostream>

namespace md {
// Keep JSON ASCII on the wire: Windows PowerShell and the CRT may use different
// code pages. UTF-16 escapes also preserve non-ASCII errors and surrogate pairs.
inline void WriteJsonString(std::wostream& out,const wchar_t* text){
    constexpr wchar_t hex[]=L"0123456789abcdef";
    out<<L'"';
    for(;*text;++text){
        const auto ch=static_cast<unsigned short>(*text);
        if(ch==L'"'||ch==L'\\')out<<L'\\'<<*text;
        else if(ch>=32&&ch<127)out<<*text;
        else out<<L"\\u"<<hex[(ch>>12)&15]<<hex[(ch>>8)&15]<<hex[(ch>>4)&15]<<hex[ch&15];
    }
    out<<L'"';
}
}
