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

    HBRUSH hbr;
    if (state & ODS_SELECTED) {
        hbr = CreateSolidBrush(DARK_CONTROL_PUSH);
    } else {
        hbr = CreateSolidBrush(DARK_CONTROL_BACK);
    }

    FillRect(hdc, &rect, hbr);
    DeleteObject(hbr);

    HPEN hPen = CreatePen(PS_SOLID, 1, DARK_BORDER);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

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

inline void DrawDarkTab(HWND hTab, LPDRAWITEMSTRUCT lpDrawItem) {
    HDC hdc = lpDrawItem->hDC;
    RECT rect = lpDrawItem->rcItem;
    int iItem = lpDrawItem->itemID;
    BOOL bSelected = (iItem == TabCtrl_GetCurSel(hTab));

    // Fill the tab background
    HBRUSH hbr = CreateSolidBrush(bSelected ? DARK_CONTROL_BACK : DARK_BACKGROUND);
    FillRect(hdc, &rect, hbr);
    DeleteObject(hbr);

    // Border
    HPEN hPen = CreatePen(PS_SOLID, 1, DARK_BORDER);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rect.left, rect.top, NULL);
    LineTo(hdc, rect.right, rect.top);
    LineTo(hdc, rect.right, rect.bottom);
    if (!bSelected) {
        LineTo(hdc, rect.left, rect.bottom);
    }
    LineTo(hdc, rect.left, rect.top);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    WCHAR szText[256];
    tie.pszText = szText;
    tie.cchTextMax = 256;
    TabCtrl_GetItem(hTab, iItem, &tie);

    SetTextColor(hdc, DARK_TEXT);
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = (HFONT)SendMessage(hTab, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, szText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
}
