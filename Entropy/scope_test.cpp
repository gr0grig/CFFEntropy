#include "Entropy.cpp"
#include <fstream>
#include <iterator>
#include <iostream>
static std::vector<uint8_t> document;
static void* __cdecl testAddress(HWND) { return document.data(); }
static UINT __cdecl testSize(HWND) { return static_cast<UINT>(document.size()); }
static void check(bool value) { if (!value) throw std::runtime_error("Map scope test failed"); }
static void pump() { MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); } }
int main() {
    try {
        std::ifstream file("C:/Program Files/NTCore/Explorer Suite/CFF Explorer.exe", std::ios::binary);
        document.assign(std::istreambuf_iterator<char>(file), {});
        document.insert(document.end(), 4096, 0);
        api.address = testAddress; api.size = testSize;
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        HWND parent = CreateWindowExW(0, L"STATIC", L"Scope test", WS_OVERLAPPEDWINDOW,
            0, 0, 1000, 650, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND dialog = CreateDialogParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_ENTROPY),
            parent, reinterpret_cast<DLGPROC>(dialogProc), 0);
        check(dialog != nullptr);
        auto panel = state(dialog);
        check(panel && panel->values == entropy::map(document.data(), document.size()));
        HWND table = GetDlgItem(dialog, IDC_TABLE);
        ListView_SetItemState(table, 0, LVIS_SELECTED, LVIS_SELECTED);
        pump();
        const auto row = panel->rows[0];
        wchar_t percent[64], expected[64];
        ListView_GetItemText(table, 0, 5, percent, 64);
        swprintf_s(expected, L"%.2f%%", 100.0 * static_cast<double>(row.declared) / static_cast<double>(document.size()));
        check(wcscmp(percent, expected) == 0);
        check(panel->region == L"Section 1: .text");
        MoveWindow(dialog, 0, 0, 1000, 650, TRUE);
        check(ListView_GetColumnWidth(table, 6) == 140);
        for (int columnIndex = 0; columnIndex < 7; ++columnIndex) {
            LVCOLUMNW column{}; column.mask = LVCF_FMT;
            check(ListView_GetColumn(table, columnIndex, &column) && (column.fmt & LVCFMT_JUSTIFYMASK) == LVCFMT_LEFT);
        }
        HWND chart = GetDlgItem(dialog, IDC_CHART);
        RECT bounds{}; GetClientRect(chart, &bounds);
        wchar_t offsetText[80], expectedOffset[80];
        SendMessageW(chart, WM_MOUSEMOVE, 0, MAKELPARAM(38, 60));
        check(panel->hovering && panel->hoverPosition == 0);
        GetDlgItemTextW(dialog, IDC_OFFSET, offsetText, 80);
        swprintf_s(expectedOffset, L"File offset: 0x%08llX", static_cast<unsigned long long>(row.offset));
        check(wcscmp(offsetText, expectedOffset) == 0);
        SendMessageW(chart, WM_MOUSEMOVE, 0, MAKELPARAM(bounds.right - 16, 60));
        check(panel->hovering && panel->hoverPosition == 1);
        GetDlgItemTextW(dialog, IDC_OFFSET, offsetText, 80);
        swprintf_s(expectedOffset, L"File offset: 0x%08llX", static_cast<unsigned long long>(row.offset + row.available - 1));
        check(wcscmp(offsetText, expectedOffset) == 0);
        SendMessageW(chart, WM_MOUSELEAVE, 0, 0);
        check(!panel->hovering);
        GetDlgItemTextW(dialog, IDC_OFFSET, offsetText, 80);
        check(wcscmp(offsetText, L"File offset: -") == 0);
        check(panel->values == entropy::map(document.data() + row.offset, static_cast<size_t>(row.available)));
        SendDlgItemMessageW(dialog, IDC_BLOCKS, CB_SETCURSEL, 0, 0);
        SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_BLOCKS, CBN_SELCHANGE), 0);
        check(panel->blocks == 16 && panel->values == entropy::map(document.data() + row.offset, static_cast<size_t>(row.available), 16));
        const int overlay = static_cast<int>(panel->rows.size() - 1);
        const auto buildsBeforeSelection = panel->mapBuilds;
        ListView_SetItemState(table, overlay, LVIS_SELECTED, LVIS_SELECTED);
        pump();
        check(panel->mapBuilds == buildsBeforeSelection + 1);
        check(panel->region == L"Overlay");
        ListView_GetItemText(table, overlay, 5, percent, 64);
        swprintf_s(expected, L"%.2f%%", 100.0 * 4096 / static_cast<double>(document.size()));
        check(wcscmp(percent, expected) == 0);
        for (double value : panel->values) check(value == 0);
        refresh(dialog);
        check(panel->region == L"Overlay" && panel->values.size() == 16);
        check(GetDlgItem(dialog, IDC_FULL) != nullptr);
        SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_FULL, BN_CLICKED), 0);
        check(ListView_GetNextItem(table, -1, LVNI_SELECTED) == -1);
        check(panel->region == L"whole file" && panel->values == entropy::map(document.data(), document.size(), 16));
        DestroyWindow(dialog); DestroyWindow(parent);
        std::cout << "Whole file, section, overlay, block selection, Refresh and Full button: passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
