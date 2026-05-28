#pragma once
#include "Framework.hpp"

class WindowContext {
public:
    HWND hWnd;
    int Width, Height;
    LPCWSTR windowName;

    WindowContext(LPCWSTR winName = L"DX11 Engine")
        : windowName(winName), hWnd(nullptr), Width(800), Height(600) {
    }

    ~WindowContext() {
        UnregisterClass(L"DX11Engine", GetModuleHandle(NULL));
    }

    bool Initialize(HINSTANCE hInst, int w, int h, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM)) {
        Width = w; Height = h;

        WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"DX11Engine";

        if (!RegisterClassEx(&wc)) return false;

        RECT rc = { 0, 0, w, h };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        hWnd = CreateWindow(L"DX11Engine", windowName, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, hInst, NULL);

        if (!hWnd) return false;

        ShowWindow(hWnd, SW_SHOW);
        return true;
    }
};