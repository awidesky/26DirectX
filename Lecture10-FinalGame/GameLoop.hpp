#pragma once
#include "WindowContext.hpp"
#include "GraphicsContext.hpp"
#include "Timer.hpp"
#include "ObjectBase.hpp"

class GameLoop {
public:
    WindowContext win;
    GraphicsContext gfx;
    DeltaTime timer;
    std::vector<GameObject*> world;
    bool isRunning = true;

    GameLoop() {
        printf("[Engine] GameLoop Created.\n");
    }

    ~GameLoop() {
        for (auto obj : world) delete obj;
        world.clear();
        printf("[Engine] GameLoop Destroyed.\n");
    }

    void Initialize(HINSTANCE hInst, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM)) {
        win.Initialize(hInst, 800, 600, wndProc);
        gfx.InitDX(win.hWnd, 800, 600);
    }

    void Input() {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;

        // 창 크기 조절 토글 예시 (C키)
        if (GetAsyncKeyState('C') & 0x0001) {
            win.Width = 600; win.Height = 600;
            RECT rc = { 0, 0, win.Width, win.Height };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(win.hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
            gfx.Resize(win.Width, win.Height);
        }

        for (auto obj : world) obj->Input();
    }

    void Update() {
        float dt = timer.GetDelta();
        for (auto obj : world) obj->Update(dt, &gfx);
    }

    void Render() {
        float col[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        gfx.ImmediateContext->ClearRenderTargetView(gfx.RTV, col);

        D3D11_VIEWPORT vp = { 0, 0, (float)win.Width, (float)win.Height, 0, 1 };
        gfx.ImmediateContext->RSSetViewports(1, &vp);
        gfx.ImmediateContext->OMSetRenderTargets(1, &gfx.RTV, NULL);
        gfx.ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D11_BLEND_DESC blendDesc;
        ZeroMemory(&blendDesc, sizeof(blendDesc));

        // 독립적인 혼합(Independent Blend)을 끌 경우, RenderTarget[0]의 설정이 모든 타겟에 적용됩니다.
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        // RenderTarget[0] 설정 (기본 출력 타겟)
        blendDesc.RenderTarget[0].BlendEnable = TRUE; // ★ 알파 블렌딩 활성화!

        // 컬러(RGB) 블렌딩 공식 설정
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;  // 소스(현재 그릴 값)의 알파를 곱함
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; // 대상(이미 그려진 값)에 (1-소스알파)를 곱함
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;      // 두 값을 더함

        // 알파(A) 채널 블렌딩 공식 설정 (보통 컬러와 맞추거나 더하는 방식을 씁니다)
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        // RGB와 Alpha 전 채널의 상태를 업데이트하도록 마스크 설정
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ID3D11BlendState* pBlendState = nullptr;
        HRESULT hr = gfx.Device->CreateBlendState(&blendDesc, &pBlendState);
        if (FAILED(hr)) {
            // 에러 처리
        }

        // 블렌딩에 사용할 상수 벡터 (보통 nullptr이나 기본 배열을 넘깁니다)
        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        UINT sampleMask = 0xffffffff; // 모든 샘플 활성화

        // Output Merger 단계에 블렌드 상태 설정
        gfx.ImmediateContext->OMSetBlendState(pBlendState, blendFactor, sampleMask);


        for (auto obj : world) obj->Render(&gfx);

        gfx.SwapChain->Present(gfx.VSync, 0);
    }

    void Run() {
        MSG msg = {};
        while (msg.message != WM_QUIT && isRunning) {
            if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg); DispatchMessage(&msg);
            }
            else {
                Input();
                Update();
                Render();
            }
        }
    }
};