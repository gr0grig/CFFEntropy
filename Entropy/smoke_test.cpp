#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
#include <stdexcept>
using QWORD = unsigned __int64;
#include "../sdk/UPX_Utility/UPX Utility/Extension.h"
#include "../sdk/UPX_Utility/UPX Utility/CFFExplorerSDK.h"
#include "resource.h"
static std::vector<char> bytes;
static void* __cdecl address(HWND) {return bytes.data();}
static UINT __cdecl size(HWND) {return static_cast<UINT>(bytes.size());}
static void __cdecl retrieve(UINT* mask, void* output) {
    auto slots=static_cast<UINT_PTR*>(output);
    for (int i=0;mask[i];++i) {
        if (mask[i]==m_eaGetObjectAddress) slots[i]=reinterpret_cast<UINT_PTR>(&address);
        else if (mask[i]==m_eaGetObjectSize) slots[i]=reinterpret_cast<UINT_PTR>(&size);
        else throw std::runtime_error("Unexpected API request");
    }
}
int main() {
    HMODULE dll=LoadLibraryW(L"build\\Entropy.dll");
    if (!dll) {std::cerr<<"LoadLibrary failed: "<<GetLastError();return 1;}
    auto load=reinterpret_cast<BOOL(__cdecl*)(EXTINITDATA*)>(GetProcAddress(dll,"ExtensionLoad"));
    auto execute=reinterpret_cast<void*(__cdecl*)(LPARAM)>(GetProcAddress(dll,"ExtensionExecute"));
    auto needed=reinterpret_cast<BOOL(__cdecl*)(void*,UINT)>(GetProcAddress(dll,"ExtensionNeeded"));
    if (!load || !execute || !needed || !GetProcAddress(dll,"ExtensionName") || !GetProcAddress(dll,"ExtensionDescription") || !GetProcAddress(dll,"ExtensionUnload")) return 2;
    std::ifstream file("C:/Program Files/NTCore/Explorer Suite/CFF Explorer.exe",std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(file),{});
    EXTINITDATA init{retrieve};
    if (!load(&init) || !needed(bytes.data(),size(nullptr))) return 3;
    auto events=static_cast<EXTEVENTSDATA*>(execute(0));
    HWND parent=CreateWindowExW(0,L"STATIC",L"Entropy test host",WS_OVERLAPPEDWINDOW,0,0,1000,650,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND dialog=CreateDialogParamW(events->hInstance,MAKEINTRESOURCEW(events->DlgID),parent,reinterpret_cast<DLGPROC>(events->DlgProc),0);
    if (!dialog) {std::cerr<<"CreateDialog failed: "<<GetLastError();return 4;}
    MoveWindow(dialog,0,0,950,600,TRUE);
    HWND table=GetDlgItem(dialog,IDC_TABLE);
    if (SendDlgItemMessageW(dialog,IDC_BLOCKS,CB_GETCOUNT,0,0)!=7) return 14;
    if (SendDlgItemMessageW(dialog,IDC_BLOCKS,CB_GETCURSEL,0,0)!=4) return 15;
    SendDlgItemMessageW(dialog,IDC_BLOCKS,CB_SETCURSEL,0,0);
    SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_BLOCKS,CBN_SELCHANGE),0);
    int count=ListView_GetItemCount(table);
    wchar_t value[128]{};
    ListView_GetItemText(table,0,6,value,128);
    if (count!=5 || !value[0]) return 5;
    ListView_GetItemText(table,0,0,value,128);
    if (wcscmp(value,L"1")) return 11;
    ListView_GetItemText(table,0,1,value,128);
    if (wcscmp(value,L".text")) return 12;
    bytes.insert(bytes.end(),256,'x');
    SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_REFRESH,BN_CLICKED),0);
    if (ListView_GetItemCount(table)!=6) return 6;
    ListView_GetItemText(table,5,1,value,128);
    if (wcscmp(value,L"Overlay")) return 7;
    ListView_GetItemText(table,5,4,value,128);
    if (wcscmp(value,L"256 B")) return 13;
    ListView_GetItemText(table,5,6,value,128);
    if (wcscmp(value,L"0.000000")) return 8;
    bytes.resize(2);
    SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_REFRESH,BN_CLICKED),0);
    if (ListView_GetItemCount(table)!=0) return 9;
    GetDlgItemTextW(dialog,IDC_STATUS,value,128);
    if (!wcsstr(value,L"invalid")) return 10;
    DestroyWindow(dialog); DestroyWindow(parent);
    reinterpret_cast<void(__cdecl*)()>(GetProcAddress(dll,"ExtensionUnload"))();
    FreeLibrary(dll);
    std::cout<<"DLL exports, API binding, hidden Win32 dialog, section table, overlay and Refresh: passed.\n";
}
