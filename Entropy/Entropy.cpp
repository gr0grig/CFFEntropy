#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstdio>
#include <new>
#include <memory>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#include "../sdk/UPX_Utility/UPX Utility/Extension.h"
using QWORD = unsigned __int64;
#include "../sdk/UPX_Utility/UPX Utility/CFFExplorerSDK.h"
#include "resource.h"
#include "entropy_core.h"

static HINSTANCE instance;
static ULONG_PTR graphicsToken = 0;
static constexpr size_t blockOptions[] = {16, 32, 64, 128, 256, 512, 1024};
static constexpr UINT WM_ENTROPY_MAP = WM_APP + 19;
struct PanelState {
    std::vector<double> values;
    std::vector<entropy::Row> rows;
    size_t blocks = 256;
    uint64_t mapOffset = 0, mapSize = 0;
    std::wstring region = L"whole file";
    bool valid = false, refreshing = false;
    bool hovering = false;
    double hoverPosition = 0;
    HBITMAP chartCache = nullptr;
    int cacheWidth = 0, cacheHeight = 0;
    bool cacheDirty = true;
    uint64_t chartBuilds = 0;
    uint64_t mapBuilds = 0;
    bool mapQueued = false;
    ~PanelState() { if (chartCache) DeleteObject(chartCache); }
};
static PanelState* state(HWND dialog) {
    return reinterpret_cast<PanelState*>(GetWindowLongPtrW(dialog, DWLP_USER));
}
static struct Api {
    d_eaGetObjectAddress address;
    d_eaGetObjectSize size;
} api{};
static UINT mask[] = {m_eaGetObjectAddress, m_eaGetObjectSize, 0};
static void offsetCaption(HWND dialog, const wchar_t* text) {
    wchar_t current[96]{};
    GetDlgItemTextW(dialog, IDC_OFFSET, current, static_cast<int>(std::size(current)));
    if (wcscmp(current, text)) SetDlgItemTextW(dialog, IDC_OFFSET, text);
}
static void invalidateCursor(HWND chart, double position) {
    RECT bounds{}; GetClientRect(chart, &bounds);
    const int x = 38 + static_cast<int>(position * std::max(0L, bounds.right - 54));
    RECT stripe{x - 2, 52, x + 3, bounds.bottom - 14};
    InvalidateRect(chart, &stripe, FALSE);
}
static LRESULT CALLBACK chartProc(HWND chart, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    HWND dialog = GetParent(chart);
    if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN) {
        RECT bounds{}; GetClientRect(chart, &bounds);
        const int x = static_cast<short>(LOWORD(lParam));
        const int y = static_cast<short>(HIWORD(lParam));
        const int left = 38, right = bounds.right - 16, top = 54, bottom = bounds.bottom - 16;
        auto panel = state(dialog);
        if (panel && panel->valid && panel->mapSize && right > left && bottom > top
            && x >= left && x <= right && y >= top && y <= bottom) {
            const uint64_t relative = std::min(panel->mapSize - 1,
                static_cast<uint64_t>(x - left) * panel->mapSize / static_cast<uint64_t>(right - left));
            const double position = static_cast<double>(x - left) / static_cast<double>(right - left);
            if (!panel->hovering) {
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, chart, 0};
                TrackMouseEvent(&track);
            }
            if (!panel->hovering || panel->hoverPosition != position) {
                if (panel->hovering) invalidateCursor(chart, panel->hoverPosition);
                panel->hoverPosition = position;
                panel->hovering = true;
                invalidateCursor(chart, position);
            }
            wchar_t text[80];
            swprintf_s(text, L"File offset: 0x%08llX", static_cast<unsigned long long>(panel->mapOffset + relative));
            offsetCaption(dialog, text);
            if (message == WM_LBUTTONDOWN) {
                wchar_t offset[32];
                swprintf_s(offset, L"0x%08llX", static_cast<unsigned long long>(panel->mapOffset + relative));
                const size_t bytes = (wcslen(offset) + 1) * sizeof(wchar_t);
                HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
                bool copied = false;
                if (memory) {
                    if (void* destination = GlobalLock(memory)) {
                        memcpy(destination, offset, bytes);
                        GlobalUnlock(memory);
                        if (OpenClipboard(dialog)) {
                            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory)) {
                                copied = true;
                                memory = nullptr; // Ownership transferred to Windows.
                            }
                            CloseClipboard();
                        }
                    }
                    if (memory) GlobalFree(memory);
                }
                swprintf_s(text, L"File offset: %ls  %ls", offset, copied ? L"(copied)" : L"(copy failed)");
                offsetCaption(dialog, text);
                return 0;
            }
        } else {
            if (panel && panel->hovering) { invalidateCursor(chart, panel->hoverPosition); panel->hovering = false; }
            offsetCaption(dialog, L"File offset: -");
        }
    } else if (message == WM_MOUSELEAVE) {
        if (auto panel = state(dialog)) {
            if (panel->hovering) invalidateCursor(chart, panel->hoverPosition);
            panel->hovering = false;
        }
        offsetCaption(dialog, L"File offset: -");
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(chart, chartProc, 1);
    }
    return DefSubclassProc(chart, message, wParam, lParam);
}

static std::wstring wide(const std::string& s) { return std::wstring(s.begin(), s.end()); }
static std::wstring readableSize(uint64_t bytes) {
    static const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
    wchar_t text[64];
    if (unit == 0) swprintf_s(text, L"%llu B", static_cast<unsigned long long>(bytes));
    else swprintf_s(text, L"%.2f %ls", value, units[unit]);
    return text;
}
static void cell(HWND list, int row, int column, const std::wstring& value) {
    LVITEMW item{};
    item.iItem = row; item.iSubItem = column;
    item.pszText = const_cast<wchar_t*>(value.c_str());
    if (!column) { item.mask = LVIF_TEXT; ListView_InsertItem(list, &item); }
    else ListView_SetItemText(list, row, column, item.pszText);
}
static void layout(HWND dialog) {
    RECT r{}; GetClientRect(dialog, &r);
    HWND list = GetDlgItem(dialog, IDC_TABLE);
    RECT header{}; GetWindowRect(ListView_GetHeader(list), &header);
    int headerHeight = header.bottom > header.top ? header.bottom - header.top : 24;
    RECT item{};
    int rowHeight = ListView_GetItemRect(list, 0, &item, LVIR_BOUNDS) ? item.bottom - item.top : 20;
    rowHeight = std::max(1, rowHeight);
    const int count = ListView_GetItemCount(list);
    // Limit large tables to complete rows, retaining space for the plot.
    int capacity = std::max(1, (static_cast<int>(r.bottom) - 300 - headerHeight) / rowHeight);
    int height = headerHeight + std::min(count, capacity) * rowHeight;
    const int width = std::max(1L, r.right - 16);
    int columnsWidth = 0;
    for (int i = 0; i < Header_GetItemCount(ListView_GetHeader(list)); ++i)
        columnsWidth += ListView_GetColumnWidth(list, i);
    const int tableWidth = std::min(width, columnsWidth + (count > capacity ? GetSystemMetrics(SM_CXVSCROLL) : 0));
    MoveWindow(GetDlgItem(dialog, IDC_REFRESH), std::max(8L, r.right - 94), 6, 86, 25, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_FULL), std::max(8L, r.right - 166), 6, 64, 25, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_BLOCKS), std::max(8L, r.right - 266), 6, 92, 220, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_BLOCKS_LABEL), std::max(8L, r.right - 316), 11, 46, 20, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_HEADING), 8, 11, std::max(1L, r.right - 332), 20, TRUE);
    MoveWindow(list, 8, 40, std::max(1, tableWidth), height, TRUE);
    if (GetWindowLongPtrW(list, GWL_STYLE) & WS_HSCROLL) {
        height += GetSystemMetrics(SM_CYHSCROLL);
        MoveWindow(list, 8, 40, std::max(1, tableWidth), height, TRUE);
    }
    int chartTop = 40 + height + 12;
    MoveWindow(GetDlgItem(dialog, IDC_CHART), 8, chartTop, width,
        std::max(1, static_cast<int>(r.bottom) - chartTop - 82), TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_OFFSET), 12, std::max(chartTop, static_cast<int>(r.bottom) - 72), width - 4, 20, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_STATUS), 8, std::max(chartTop, static_cast<int>(r.bottom) - 48), width, 44, TRUE);
    InvalidateRect(GetDlgItem(dialog, IDC_CHART), nullptr, FALSE);
}
static void paintChart(HWND dialog, const DRAWITEMSTRUCT& draw) {
    using namespace Gdiplus;
    const int width = draw.rcItem.right - draw.rcItem.left;
    const int height = draw.rcItem.bottom - draw.rcItem.top;
    if (width < 1 || height < 1) return;
    auto panel = state(dialog);
    if (!panel) return;
    if (panel->cacheDirty || !panel->chartCache || panel->cacheWidth != width || panel->cacheHeight != height) {
    auto buffer = std::make_unique<Bitmap>(width, height, PixelFormat32bppARGB);
    if (buffer->GetLastStatus() != Ok) return;
    {
    Graphics g(buffer.get());
    g.Clear(Color(255, 250, 252, 255));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    Font title(L"Segoe UI", 11, FontStyleBold, UnitPixel);
    Font label(L"Segoe UI", 11, FontStyleRegular, UnitPixel);
    SolidBrush ink(Color(255, 45, 60, 80));
    SolidBrush muted(Color(255, 108, 122, 143));
    g.DrawString(L"ENTROPY MAP", -1, &title, PointF(14, 10), &ink);
    const auto subtitle = std::to_wstring(panel ? panel->blocks : 256) + L" blocks  /  " + (panel ? panel->region : L"whole file");
    g.DrawString(subtitle.c_str(), -1, &label, PointF(14, 27), &muted);
    const float left = 38, top = 54;
    const float right = static_cast<float>(width - 16), bottom = static_cast<float>(height - 16);
    if (right > left && bottom > top) {
        Pen grid(Color(255, 226, 233, 242), 1);
        for (int value = 0; value <= 8; ++value) {
            float y = bottom - (bottom - top) * value / 8.0f;
            g.DrawLine(&grid, left, y, right, y);
            wchar_t text[4]; swprintf_s(text, L"%d", value);
            g.DrawString(text, -1, &label, PointF(17, y - 7), &muted);
        }
        if (panel && panel->valid && !panel->values.empty()) {
            const size_t count = panel->values.size();
            const float step = (right - left) / static_cast<float>(count);
            LinearGradientBrush fill(PointF(0, top), PointF(0, bottom), Color(125, 30, 140, 225), Color(15, 30, 140, 225));
            Pen line(Color(255, 20, 116, 205), 1.8f);
            // Each interval occupies its full horizontal share; empty intervals stay blank.
            for (size_t start = 0; start < count;) {
                if (panel->values[start] < 0) { ++start; continue; }
                size_t end = start;
                std::vector<PointF> points;
                while (end < count && panel->values[end] >= 0) {
                    float y = bottom - static_cast<float>(panel->values[end]) * (bottom - top) / 8.0f;
                    points.emplace_back(left + step * static_cast<float>(end), y);
                    points.emplace_back(left + step * static_cast<float>(end + 1), y);
                    ++end;
                }
                auto area = points;
                area.emplace_back(points.back().X, bottom);
                area.emplace_back(points.front().X, bottom);
                g.FillPolygon(&fill, area.data(), static_cast<INT>(area.size()));
                g.DrawLines(&line, points.data(), static_cast<INT>(points.size()));
                start = end;
            }
        } else {
            g.DrawString(L"No data", -1, &label, PointF(left + 12, top + 12), &muted);
        }
    }
    }
    HBITMAP native = nullptr;
    if (buffer->GetHBITMAP(Color(255, 250, 252, 255), &native) != Ok || !native) return;
    if (panel->chartCache) DeleteObject(panel->chartCache);
    panel->chartCache = native;
    panel->cacheWidth = width; panel->cacheHeight = height;
    panel->cacheDirty = false;
    ++panel->chartBuilds;
    }
    HDC memory = CreateCompatibleDC(draw.hDC);
    if (!memory) return;
    HGDIOBJ oldBitmap = SelectObject(memory, panel->chartCache);
    BitBlt(draw.hDC, draw.rcItem.left, draw.rcItem.top, width, height, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteDC(memory);
    if (panel->hovering && panel->valid && !panel->values.empty() && width > 54 && height > 70) {
        const size_t block = std::min(panel->values.size() - 1, static_cast<size_t>(panel->hoverPosition * panel->values.size()));
        if (panel->values[block] >= 0) {
            const int x = draw.rcItem.left + 38 + static_cast<int>(panel->hoverPosition * (width - 54));
            HPEN cursor = CreatePen(PS_SOLID, 1, RGB(224, 44, 58));
            HGDIOBJ oldPen = SelectObject(draw.hDC, cursor);
            POINT oldPoint{};
            MoveToEx(draw.hDC, x, draw.rcItem.top + 54, &oldPoint);
            LineTo(draw.hDC, x, draw.rcItem.top + height - 15);
            MoveToEx(draw.hDC, oldPoint.x, oldPoint.y, nullptr);
            SelectObject(draw.hDC, oldPen);
            DeleteObject(cursor);
        }
    }
}
static void updateMap(HWND dialog) {
    auto panel = state(dialog);
    if (!panel || panel->refreshing) return;
    panel->mapQueued = false;
    panel->valid = false;
    panel->cacheDirty = true;
    panel->values.clear();
    panel->mapOffset = panel->mapSize = 0;
    panel->hovering = false;
    SetDlgItemTextW(dialog, IDC_OFFSET, L"File offset: -");
    panel->region = L"whole file";
    try {
        auto data = static_cast<const uint8_t*>(api.address(dialog));
        uint64_t size = api.size(dialog);
        const int selected = ListView_GetNextItem(GetDlgItem(dialog, IDC_TABLE), -1, LVNI_SELECTED);
        if (selected >= 0 && static_cast<size_t>(selected) < panel->rows.size()) {
            const auto& row = panel->rows[static_cast<size_t>(selected)];
            panel->mapOffset = row.offset;
            panel->region = wide(row.name);
            if (row.number) panel->region = L"Section " + std::to_wstring(row.number) + L": " + panel->region;
            if (row.offset <= size && data) {
                size = std::min(row.available, size - row.offset);
                data += row.offset;
            } else size = 0;
        }
        if (data && size) {
            panel->mapSize = size;
            panel->values = entropy::map(data, static_cast<size_t>(size), panel->blocks);
            ++panel->mapBuilds;
            panel->valid = true;
        }
    } catch (...) { panel->valid = false; }
    InvalidateRect(GetDlgItem(dialog, IDC_CHART), nullptr, FALSE);
}
static void refresh(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_TABLE);
    const int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    auto panel = state(dialog);
    if (panel) { panel->refreshing = true; panel->valid = false; panel->rows.clear(); }
    ListView_DeleteAllItems(list);
    try {
        const auto data = static_cast<const uint8_t*>(api.address(dialog));
        const auto size = api.size(dialog);
        if (!data) throw std::runtime_error("No document is available.");
        const auto report = entropy::analyze(data, size);
        if (panel) panel->rows = report.rows;
        int index = 0;
        for (const auto& row : report.rows) {
            wchar_t text[64];
            cell(list, index, 0, row.number ? std::to_wstring(row.number) : L"");
            cell(list, index, 1, wide(row.name));
            swprintf_s(text, L"0x%08llX", static_cast<unsigned long long>(row.offset));
            cell(list, index, 2, text);
            cell(list, index, 3, std::to_wstring(row.declared));
            cell(list, index, 4, readableSize(row.declared));
            if (size) {
                swprintf_s(text, L"%.2f%%", 100.0 * static_cast<double>(row.declared) / static_cast<double>(size));
                cell(list, index, 5, text);
            } else cell(list, index, 5, L"N/A");
            if (row.available) { swprintf_s(text, L"%.6f", row.value); cell(list, index, 6, text); }
            else cell(list, index, 6, L"N/A");
            ++index;
        }
        SetDlgItemTextW(dialog, IDC_STATUS, report.truncated
            ? L"Warning: truncated PE data. Entropy uses only available bytes. Click Refresh after edits.\nClick the map to copy the file offset."
            : L"Overlay = bytes after headers and all raw sections (including certificates, if present). Click Refresh after edits.\nClick the map to copy the file offset.");
    } catch (const std::exception& e) {
        SetDlgItemTextW(dialog, IDC_STATUS, wide(e.what()).c_str());
    } catch (...) {
        SetDlgItemTextW(dialog, IDC_STATUS, L"Could not calculate entropy.");
    }
    if (selected >= 0 && selected < ListView_GetItemCount(list))
        ListView_SetItemState(list, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    if (panel) panel->refreshing = false;
    updateMap(dialog);
    layout(dialog);
}
static LRESULT CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(new (std::nothrow) PanelState));
        SetWindowSubclass(GetDlgItem(dialog, IDC_CHART), chartProc, 1, 0);
        HWND blocks = GetDlgItem(dialog, IDC_BLOCKS);
        for (size_t count : blockOptions) {
            const auto text = std::to_wstring(count);
            SendMessageW(blocks, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
        SendMessageW(blocks, CB_SETCURSEL, 4, 0);
        HWND list = GetDlgItem(dialog, IDC_TABLE);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        const wchar_t* names[] = {L"#", L"Section", L"File offset", L"Raw size", L"Size", L"Percent", L"Entropy"};
        int widths[] = {45, 150, 110, 120, 100, 80, 140};
        for (int i = 0; i < 7; ++i) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            column.pszText = const_cast<wchar_t*>(names[i]);
            column.cx = widths[i]; column.fmt = LVCFMT_LEFT;
            ListView_InsertColumn(list, i, &column);
        }
        refresh(dialog);
        return TRUE;
    }
    case WM_SIZE:
        layout(dialog);
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_CHART) {
            try { paintChart(dialog, *reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); } catch (...) {}
            return TRUE;
        }
        break;
    case WM_NOTIFY: {
        const auto notification = reinterpret_cast<NMHDR*>(lParam);
        if (notification && notification->idFrom == IDC_TABLE && notification->code == LVN_ITEMCHANGED) {
            const auto change = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((change->uChanged & LVIF_STATE) && ((change->uOldState ^ change->uNewState) & LVIS_SELECTED)) {
                auto panel = state(dialog);
                // One selection gesture emits deselect + select. Do not scan the
                // entire file for the temporary "nothing selected" state.
                if (panel && !panel->refreshing && !panel->mapQueued) {
                    panel->mapQueued = PostMessageW(dialog, WM_ENTROPY_MAP, 0, 0) != FALSE;
                }
            }
        }
        break;
    }
    case WM_ENTROPY_MAP:
        if (auto panel = state(dialog); panel && panel->mapQueued) updateMap(dialog);
        return TRUE;
    case WM_DESTROY:
        delete state(dialog);
        SetWindowLongPtrW(dialog, DWLP_USER, 0);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_REFRESH) { refresh(dialog); return TRUE; }
        if (LOWORD(wParam) == IDC_FULL) {
            ListView_SetItemState(GetDlgItem(dialog, IDC_TABLE), -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            updateMap(dialog);
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_BLOCKS && HIWORD(wParam) == CBN_SELCHANGE) {
            const auto selected = SendDlgItemMessageW(dialog, IDC_BLOCKS, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && selected < static_cast<LRESULT>(std::size(blockOptions))) {
                if (auto panel = state(dialog)) panel->blocks = blockOptions[selected];
                updateMap(dialog);
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) instance = module;
    return TRUE;
}
extern "C" __declspec(dllexport) BOOL __cdecl ExtensionLoad(EXTINITDATA* init) {
    if (!init || !init->RetrieveExtensionApi) return FALSE;
    init->RetrieveExtensionApi(mask, &api);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    Gdiplus::GdiplusStartupInput graphicsInput;
    return api.address && api.size && InitCommonControlsEx(&controls)
        && Gdiplus::GdiplusStartup(&graphicsToken, &graphicsInput, nullptr) == Gdiplus::Ok;
}
extern "C" __declspec(dllexport) void __cdecl ExtensionUnload() {
    if (graphicsToken) { Gdiplus::GdiplusShutdown(graphicsToken); graphicsToken = 0; }
}
extern "C" __declspec(dllexport) WCHAR* __cdecl ExtensionName() { static WCHAR name[] = L"Entropy"; return name; }
extern "C" __declspec(dllexport) WCHAR* __cdecl ExtensionDescription() {
    static WCHAR description[] = L"Shannon entropy of each PE section and overlay."; return description;
}
extern "C" __declspec(dllexport) BOOL __cdecl ExtensionNeeded(void* data, UINT size) {
    // Only inspect the DOS signature here; full validation happens on opening the panel.
    return data && size >= 64 && entropy::u16(static_cast<const uint8_t*>(data)) == 0x5a4d;
}
extern "C" __declspec(dllexport) void* __cdecl ExtensionExecute(LPARAM) {
    static EXTEVENTSDATA events{sizeof(EXTEVENTSDATA), nullptr, IDD_ENTROPY, dialogProc};
    events.hInstance = instance;
    return &events;
}
