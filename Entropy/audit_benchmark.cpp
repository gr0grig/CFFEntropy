#include "Entropy.cpp"
#include <chrono>
#include <iostream>
static std::vector<uint8_t> sample;
static void* __cdecl testAddress(HWND) {return sample.data();}
static UINT __cdecl testSize(HWND) {return static_cast<UINT>(sample.size());}
static void put16(size_t off, uint16_t v) {memcpy(sample.data()+off,&v,2);}
static void put32(size_t off, uint32_t v) {memcpy(sample.data()+off,&v,4);}
static void pump() {MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) DispatchMessageW(&message);}
template<class F> static void measure(const char* name, F f) {
    auto start=std::chrono::steady_clock::now(); f();
    std::cout<<name<<": "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
}
int main() {
    sample.resize(256*1024*1024);
    put16(0,0x5a4d);put32(60,64);put32(64,0x4550);put16(70,2);put16(84,64);put16(88,0x20b);put32(148,512);
    memcpy(sample.data()+152,".first",6);put32(168,128*1024*1024-512);put32(172,512);
    memcpy(sample.data()+192,".second",7);put32(208,128*1024*1024);put32(212,128*1024*1024);
    api.address=testAddress;api.size=testSize;
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&graphicsToken,&input,nullptr);
    HWND parent=CreateWindowW(L"STATIC",L"Benchmark",WS_OVERLAPPEDWINDOW,0,0,1200,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND dialog=nullptr;
    measure("Open 256 MiB",[&]{dialog=CreateDialogParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_ENTROPY),parent,reinterpret_cast<DLGPROC>(dialogProc),0);});
    MoveWindow(dialog,0,0,1100,700,TRUE);
    HWND list=GetDlgItem(dialog,IDC_TABLE);
    measure("Select section 1",[&]{ListView_SetItemState(list,0,LVIS_SELECTED,LVIS_SELECTED);pump();});
    measure("Switch to section 2",[&]{ListView_SetItemState(list,1,LVIS_SELECTED,LVIS_SELECTED);pump();});
    measure("Full",[&]{SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_FULL,BN_CLICKED),0);});
    HDC screen=GetDC(nullptr), dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,1100,500); auto old=SelectObject(dc,bitmap);
    DRAWITEMSTRUCT draw{};draw.hDC=dc;draw.rcItem={0,0,1100,500};
    measure("100 chart repaints",[&]{for(int i=0;i<100;++i)paintChart(dialog,draw);});
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
    DestroyWindow(dialog);DestroyWindow(parent);Gdiplus::GdiplusShutdown(graphicsToken);
}
