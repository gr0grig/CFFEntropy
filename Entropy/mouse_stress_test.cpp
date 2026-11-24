#include "Entropy.cpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
static std::vector<uint8_t> fileBytes;
static void* __cdecl addressForTest(HWND) {return fileBytes.data();}
static UINT __cdecl sizeForTest(HWND) {return static_cast<UINT>(fileBytes.size());}
static void check(bool condition, const char* error) {if(!condition) throw std::runtime_error(error);}
static void pump() {
    MSG message{}; unsigned count=0;
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        check(++count<50000,"Message queue does not settle");
        TranslateMessage(&message); DispatchMessageW(&message);
    }
}
static void saveImage(HBITMAP handle, int width, int height) {
    Gdiplus::Bitmap image(handle,nullptr);
    std::unique_ptr<Gdiplus::Bitmap> crop(image.Clone(0,0,width,height,PixelFormat32bppARGB));
    UINT count=0,size=0; Gdiplus::GetImageEncodersSize(&count,&size);
    std::vector<uint8_t> storage(size);
    auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    Gdiplus::GetImageEncoders(count,size,codecs);
    for(UINT i=0;i<count;++i) if(!wcscmp(codecs[i].MimeType,L"image/png"))
        check(crop && crop->Save(L"build\\mouse-stress-chart.png",&codecs[i].Clsid,nullptr)==Gdiplus::Ok,"PNG export failed");
}
int main() {
    try {
        std::ifstream stream("C:/Program Files/NTCore/Explorer Suite/CFF Explorer.exe",std::ios::binary);
        fileBytes.assign(std::istreambuf_iterator<char>(stream),{});
        api.address=addressForTest;api.size=sizeForTest;
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&controls);
        Gdiplus::GdiplusStartupInput input;
        check(Gdiplus::GdiplusStartup(&graphicsToken,&input,nullptr)==Gdiplus::Ok,"GDI+ startup");
        HWND parent=CreateWindowW(L"STATIC",L"Entropy regression host",WS_OVERLAPPEDWINDOW,
            0,0,1200,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
        HBITMAP bitmap=CreateCompatibleBitmap(screen,1200,800);
        HGDIOBJ original=SelectObject(dc,bitmap);
        const auto begin=std::chrono::steady_clock::now();
        DWORD baseline=0;
        for(int cycle=0;cycle<40;++cycle) {
            HWND dialog=CreateDialogParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_ENTROPY),parent,reinterpret_cast<DLGPROC>(dialogProc),0);
            check(dialog!=nullptr,"Dialog creation");
            MoveWindow(dialog,0,0,1100,700,TRUE);
            auto panel=state(dialog);
            HWND chart=GetDlgItem(dialog,IDC_CHART);
            RECT bounds{};GetClientRect(chart,&bounds);
            DRAWITEMSTRUCT draw{};draw.CtlID=IDC_CHART;draw.hwndItem=chart;draw.hDC=dc;draw.rcItem=bounds;
            auto paint=[&]{SendMessageW(dialog,WM_DRAWITEM,IDC_CHART,reinterpret_cast<LPARAM>(&draw));};
            paint();
            check(panel->chartBuilds==1,"Initial chart cache not built");
            const auto mapBuilds=panel->mapBuilds;
            for(int move=0;move<250;++move) {
                const int x=38+(move*7)%(bounds.right-54);
                SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(x,60));
                paint();
                check(GetPixel(dc,x,60)==RGB(224,44,58),"Cursor not painted at expected position");
                check(panel->chartBuilds==1 && panel->mapBuilds==mapBuilds,"Mouse move rebuilt chart or entropy");
                pump();
            }
            SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(200,60));paint();
            SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(300,60));paint();
            check(GetPixel(dc,200,60)!=RGB(224,44,58),"Old cursor trail remains");
            ValidateRect(chart,nullptr);
            SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(300,70));
            check(!GetUpdateRect(chart,nullptr,FALSE),"Vertical-only mouse motion repaints chart");
            SendMessageW(chart,WM_MOUSELEAVE,0,0);paint();
            check(GetPixel(dc,300,60)!=RGB(224,44,58) && !panel->hovering,"Mouse leave did not clear cursor");
            MoveWindow(dialog,0,0,900,650,TRUE);
            GetClientRect(chart,&draw.rcItem);paint();
            check(panel->chartBuilds==2,"Resize failed to invalidate chart cache");
            SendDlgItemMessageW(dialog,IDC_BLOCKS,CB_SETCURSEL,6,0);
            SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(IDC_BLOCKS,CBN_SELCHANGE),0);paint();
            check(panel->chartBuilds==3 && panel->values.size()==1024,"Block change failed to rebuild cache");
            if(cycle==0) {SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(300,60));paint();saveImage(bitmap,draw.rcItem.right,draw.rcItem.bottom);}
            // Destroy with a queued selection update; it must not access a dead panel.
            ListView_SetItemState(GetDlgItem(dialog,IDC_TABLE),1,LVIS_SELECTED,LVIS_SELECTED);
            DestroyWindow(dialog);pump();
            const DWORD resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
            if(cycle==1) baseline=resources;
            if(cycle>1) check(resources<=baseline+2,"GDI objects leak across dialog destruction");
        }
        std::cout<<"10,000 mouse moves/paints, 40 open/close cycles, resize, scope cancellation, pixel/queue/GDI checks passed in "
            <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<" seconds.\n";
        SelectObject(dc,original);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
        DestroyWindow(parent);Gdiplus::GdiplusShutdown(graphicsToken);
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
