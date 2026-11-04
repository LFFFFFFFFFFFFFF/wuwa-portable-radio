#pragma once
#include <windows.h>
#include <wchar.h>
inline int parse_radio_key(const wchar_t *value,int fallback){
    if(!_wcsicmp(value,L"NONE")||!wcscmp(value,L"0"))return 0;
    if(!wcscmp(value,L"-"))return VK_OEM_MINUS;
    if(!wcscmp(value,L"="))return VK_OEM_PLUS;
    if(!wcscmp(value,L"`")||!wcscmp(value,L"·")||!_wcsicmp(value,L"OEM_3"))return VK_OEM_3;
    wchar_t *end=nullptr;
    if(value[0]==L'F'||value[0]==L'f'){long n=wcstol(value+1,&end,10);return end&&!*end&&n>=1&&n<=24?VK_F1+(int)n-1:fallback;}
    long n=wcstol(value,&end,10);return end&&end!=value&&!*end&&n>=0&&n<256?(int)n:fallback;
}
