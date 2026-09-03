#pragma once
#include <windows.h>

const COLORREF DARK_BACKGROUND = RGB(32, 32, 32);
const COLORREF DARK_CONTROL_BACK = RGB(45, 45, 45);
const COLORREF DARK_CONTROL_HOVER = RGB(60, 60, 60);
const COLORREF DARK_CONTROL_PUSH = RGB(80, 80, 80);
const COLORREF DARK_TEXT = RGB(240, 240, 240);
const COLORREF DARK_BORDER = RGB(64, 64, 64);

inline void DrawDarkButton(LPDRAWITEMSTRUCT lpDrawItem) {
    HDC hdc = lpDrawItem->hDC;
    RECT rect = lpDrawItem->rcItem;
    UINT state = lpDrawItem->itemState;

    // Determine colors based on state
    HBRUSH hbr;
    if (state & ODS_SELECTED) {
        hbr = CreateSolidBrush(DARK_CONTROL_PUSH);
    } else if (state & ODS_HOTLIGHT) { // Note: ODS_HOTLIGHT needs TrackMouseEvent, but we can simplify
        hbr = CreateSolidBrush(DARK_CONTROL_HOVER);
    } else {
        hbr = CreateSolidBrush(DARK_CONTROL_BACK);
    }

    FillRect(hdc, &rect, hbr);
    DeleteObject(hbr);

    // Border
    HPEN hPen = CreatePen(PS_SOLID, 1, DARK_BORDER);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    // Text
    WCHAR szText[256];
    GetWindowTextW(lpDrawItem->hwndItem, szText, 256);
    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
    HFONT hOldFont = nullptr;
    if (hFont) hOldFont = (HFONT)SelectObject(hdc, hFont);

    SetTextColor(hdc, DARK_TEXT);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, szText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (hOldFont) SelectObject(hdc, hOldFont);
}
