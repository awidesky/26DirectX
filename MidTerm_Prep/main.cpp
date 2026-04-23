
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>


#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") // 하위 시스템을 콘솔로 설정하고, 진입점은 WinMain으로 강제 지정
//#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:windows") // 하위 시스템을 윈도우로 설정하고, 진입점은 WinMain으로 강제 지정
//#pragma comment(linker, "/subsystem:console")

// # 원래는 하나의 라이브러리였음
#pragma comment(lib, "d3d11.lib") // # 컴팩트한 기능 제공
#pragma comment(lib, "dxgi.lib")  // # 하드웨어 추상화
#pragma comment(lib, "d3dcompiler.lib") // # programmable pipeline, shader, gpgpu 등등



//#define SIXSTAR
//#define FIVESTAR
#define VSSAMPLE

#ifdef SIXSTAR

#include <windows.h>
#include <stdio.h>
#include <cmath>

#include <d3d11.h>
#include <d3dcompiler.h>

/*
 * [하위 시스템과 진입점]
 * - /subsystem:console -> 창을 띄우되, 배후에 콘솔(검은 창)을 함께 띄움 (printf 디버깅용).
 * - /entry:WinMainCRTStartup -> 윈도우 프로그램의 시작점인 WinMain을 호출하라고 링커에게 명령함.
 */
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

 // # 원래는 하나의 라이브러리였음
#pragma comment(lib, "d3d11.lib") // # 컴팩트한 기능 제공
#pragma comment(lib, "dxgi.lib")  // # 하드웨어 추상화
#pragma comment(lib, "d3dcompiler.lib") // # programmable pipeline, shader, gpgpu 등등


// ---------- 전역 객체 설정 ----------
struct {
    float posX, posY;
    bool moveR, moveL, moveU, moveD;
    bool isRunning;
} g_gameContext;
ID3D11Device* g_pd3dDevice = nullptr;          // 리소스 생성자 (공장)
// # 명령을 즉시 GPU로 보냄(Immediate <-> Deferred)
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // 그리기 명령 수행 (일꾼)
IDXGISwapChain* g_pSwapChain = nullptr;          // 화면 전환 (더블 버퍼링)
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // 그림을 그릴 도화지(View)


/*
 *
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)    //윈도우의 이벤트는 메시지에 담김
    {
        // --- [키보드 메시지 처리] ---
        // WM_KEYDOWN: 키가 눌릴 때.
    case WM_KEYDOWN:
        if (wParam == VK_LEFT || wParam == 'A') g_gameContext.moveL = true;
        else if (wParam == VK_RIGHT || wParam == 'D') g_gameContext.moveR = true;
        else if (wParam == VK_UP || wParam == 'W') g_gameContext.moveU = true;
        else if (wParam == VK_DOWN || wParam == 'S') g_gameContext.moveD = true;
        else if (wParam == 'Q' || wParam == VK_ESCAPE) {
            printf("  >> 로직: Q / ESC 입력 감지, 프로그램 종료 요청!\n");
            g_gameContext.isRunning = false;
        }
        else {
            printf("[EVENT] Unsupported Key Pressed: %c (Virtual Key: %lld)\n", (char)wParam, wParam);
        }
        break;
        // WM_KEYUP: 키가 떨어질 때.
    case WM_KEYUP:
        if (wParam == VK_LEFT || wParam == 'A') g_gameContext.moveL = false;
        else if (wParam == VK_RIGHT || wParam == 'D') g_gameContext.moveR = false;
        else if (wParam == VK_UP || wParam == 'W') g_gameContext.moveU = false;
        else if (wParam == VK_DOWN || wParam == 'S') g_gameContext.moveD = false;
        break;
        // --- [시스템 메시지 처리] ---
    case WM_DESTROY:
        // 사용자가 'X' 버튼을 눌러 창을 닫으려 할 때 호출됨.
        printf("[SYSTEM] 윈도우 파괴 메시지 수신.\n");
        g_gameContext.isRunning = false;
        break;

    default:
        // 우리가 관심 없는 메시지(창 크기 조절, 포커스 변경 등)는 OS가 기본값으로 처리함.
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};
// HLSL (High-Level Shading Language) 소스
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f); // 3D 좌표를 4D로 확장
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 정점에서 계산된 색상을 픽셀에 그대로 적용
}
)";

constexpr int width = 800;
constexpr int height = 600;
constexpr float aspect = (float)width / height;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 1. 윈도우 등록 및 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11GameLoopClass";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"DX11GameLoopClass", L"과제: 움직이는 육망성 만들기 (12211723 홍성민)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);

    // 2. DX11 디바이스 및 스왑 체인(이중 버퍼링) 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width; sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; // 생성한 Win32 창 핸들 연결 ＃윈도우 핸들러를 swapchane discriptor에 등록하고, 
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // GPU와 통신할 통로(Device, &g_pd3dDevice)와 화면(SwapChain, &g_pSwapChain)을 생성함.
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 렌더 타겟 설정 (도화지 준비)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 뷰를 생성했으므로 원본 텍스트는 바로 해제 (중요!)

    // 3. 셰이더 컴파일 및 생성
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    // # 쉐이더 생성 등은 모두 디바이스에서
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    // 정점의 데이터 형식을 정의 (IA 단계에 알려줌)
    // # D3D11_INPUT_PER_VERTEX_DATA는 정점마다, D3D11_INPUT_PER_INSTANCE_DATA는 인스턴스마다 데이터가 바뀐다
    // # 인스턴스마다 데이터 바뀌지 않으니없으니 InstanceDataStepRate 쓴다
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // 3개 들어가니 size는 12바이트
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

    // 4. 정점 버퍼 생성, 삼각형 두개를 그린다.
    constexpr float r = 0.5f;
    constexpr float pi = 3.14159265f;
    struct {
        float x, y, z;
    } relativeVertexPos[6] = { // x값은 0.5(길이) * cos30'
        {  r * cos(pi / 6),  r / 2 * aspect, 0.5f },   // 30°
        {  0.0f           , -r * aspect, 0.5f },   // 270°
        { -r * cos(pi / 6),  r / 2 * aspect, 0.5f },   // 150°
        { -r * cos(pi / 6), -r / 2 * aspect, 0.5f },   // 210°
        {  0.0f           ,  r * aspect, 0.5f },   // 90°
        {  r * cos(pi / 6), -r / 2 * aspect, 0.5f }    // 330°
    };
    Vertex vertices[] = {
        { relativeVertexPos[0].x, relativeVertexPos[0].y, relativeVertexPos[0].z, 1,0,0,1 },
        { relativeVertexPos[1].x, relativeVertexPos[1].y, relativeVertexPos[1].z, 0,0,1,1 },
        { relativeVertexPos[2].x, relativeVertexPos[2].y, relativeVertexPos[2].z, 0,1,0,1 },

        { relativeVertexPos[3].x, relativeVertexPos[3].y, relativeVertexPos[3].z, 1,0,0,1 },
        { relativeVertexPos[4].x, relativeVertexPos[4].y, relativeVertexPos[4].z, 0,0,1,1 },
        { relativeVertexPos[5].x, relativeVertexPos[5].y, relativeVertexPos[5].z, 0,1,0,1 },
    };
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);
    /*
     * [게임 루프]
     */
    g_gameContext = { 0,0,0,0,0,0,1 };
    MSG msg = { 0 };
    while (g_gameContext.isRunning) {

        // 1. 입력: 사용자가 무엇을 했는가? 있다면 처리
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // # 가상 키 메시지(Virtual-key messages)를 문자 메시지(Character messages)로 변환
            // # 키보드에서 문자가 입력될 때 발생하는 WM_KEYDOWN과 WM_KEYUP 메시지를 분석하여 WM_CHAR 메시지를 생성하고 메시지 큐에 추가.
            // # 이 함수를 호출하지 않으면 WM_CHAR 메시지가 생성되지 않아, 문자 입력(텍스트 입력창 등)을 처리할 수 없
            TranslateMessage(&msg);
            // # 메시지를 윈도우 프로시저(WndProc)로 전달.
            DispatchMessage(&msg);
        }

        // 2. 업데이트: 그 결과 세상은 어떻게 변했는가?
        // WndProc가 전역 게임 컨텍스트를 주무르므로, GetAsyncKeyState를 쓰지 않아도 된다.
        constexpr float moveStep = 0.001f;
        if (g_gameContext.moveL || g_gameContext.moveR || g_gameContext.moveD || g_gameContext.moveU) { //위치 변경 없으면 굳이 버퍼 새로 만들지 않는다
            if (g_gameContext.moveL) g_gameContext.posX -= moveStep;
            if (g_gameContext.moveR) g_gameContext.posX += moveStep;
            if (g_gameContext.moveU) g_gameContext.posY += moveStep;
            if (g_gameContext.moveD) g_gameContext.posY -= moveStep;

            // 화면의 아예 바깥으로 나가지 못하게
            g_gameContext.posX = max(-1.0f, min(1.0f, g_gameContext.posX));
            g_gameContext.posY = max(-1.0f, min(1.0f, g_gameContext.posY));

            for (int i = 0; i < sizeof(vertices) / sizeof(Vertex); i++) {
                vertices[i].x = relativeVertexPos[i].x + g_gameContext.posX;
                vertices[i].y = relativeVertexPos[i].y + g_gameContext.posY;
            }

            // 먼저 만든 거 지우고
            if (pVBuffer) pVBuffer->Release();
            // 버퍼 속성과 데이터의 주소는 바뀌지 않으니, bd도 initData도 바뀌지 않는다.
            g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);
        }


        // 3. 출력: 변한 세상을 화면에 그려라!
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor); // backbuffer를 지워버림

        // 렌더링 파이프라인 상태 설정
        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, width, height, 0.0f, 1.0f };
        g_pImmediateContext->RSSetViewports(1, &vp);

        g_pImmediateContext->IASetInputLayout(pInputLayout);
        UINT stride = sizeof(Vertex), offset = 0;

        // # 여기의 StartSlot이 D3D11_INPUT_ELEMENT_DESC layout[]의 InputSlot(0)이다
        // # 여러 버퍼 쓸 때 의미 있음 (예: 위치 버퍼 / 색상 버퍼 분리)
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

        // Primitive Topology 설정: 삼각형 리스트로 연결하라!
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

        // 최종 그리기, 버텍스 6개
        g_pImmediateContext->Draw(6, 0);

        // 화면 교체 (프론트 버퍼와 백 버퍼 스왑)
        g_pSwapChain->Present(0, 0);
    }

    printf("\n게임 루프가 종료되었습니다. 프로그램 끝.\n");
    if (pVBuffer) pVBuffer->Release();
    if (pInputLayout) pInputLayout->Release();
    if (vShader) vShader->Release();
    if (pShader) pShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}

#endif // SIXSTAR


#ifdef FIVESTAR


#include <windows.h>
#include <stdio.h>
#include <cmath>

#include <d3d11.h>
#include <d3dcompiler.h>

/*
 * [하위 시스템과 진입점]
 * - /subsystem:console -> 창을 띄우되, 배후에 콘솔(검은 창)을 함께 띄움 (printf 디버깅용).
 * - /entry:WinMainCRTStartup -> 윈도우 프로그램의 시작점인 WinMain을 호출하라고 링커에게 명령함.
 */
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

 // # 원래는 하나의 라이브러리였음
#pragma comment(lib, "d3d11.lib") // # 컴팩트한 기능 제공
#pragma comment(lib, "dxgi.lib")  // # 하드웨어 추상화
#pragma comment(lib, "d3dcompiler.lib") // # programmable pipeline, shader, gpgpu 등등


// ---------- 전역 객체 설정 ----------
struct {
    float posX, posY;
    bool moveR, moveL, moveU, moveD;
    bool isRunning;
} g_gameContext;
ID3D11Device* g_pd3dDevice = nullptr;          // 리소스 생성자 (공장)
// # 명령을 즉시 GPU로 보냄(Immediate <-> Deferred)
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // 그리기 명령 수행 (일꾼)
IDXGISwapChain* g_pSwapChain = nullptr;          // 화면 전환 (더블 버퍼링)
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // 그림을 그릴 도화지(View)


/*
 *
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)    //윈도우의 이벤트는 메시지에 담김
    {
        // --- [키보드 메시지 처리] ---
        // WM_KEYDOWN: 키가 눌릴 때.
    case WM_KEYDOWN:
        if (wParam == VK_LEFT || wParam == 'A') g_gameContext.moveL = true;
        else if (wParam == VK_RIGHT || wParam == 'D') g_gameContext.moveR = true;
        else if (wParam == VK_UP || wParam == 'W') g_gameContext.moveU = true;
        else if (wParam == VK_DOWN || wParam == 'S') g_gameContext.moveD = true;
        else if (wParam == 'Q' || wParam == VK_ESCAPE) {
            printf("  >> 로직: Q / ESC 입력 감지, 프로그램 종료 요청!\n");
            g_gameContext.isRunning = false;
        }
        else {
            printf("[EVENT] Unsupported Key Pressed: %c (Virtual Key: %lld)\n", (char)wParam, wParam);
        }
        break;
        // WM_KEYUP: 키가 떨어질 때.
    case WM_KEYUP:
        if (wParam == VK_LEFT || wParam == 'A') g_gameContext.moveL = false;
        else if (wParam == VK_RIGHT || wParam == 'D') g_gameContext.moveR = false;
        else if (wParam == VK_UP || wParam == 'W') g_gameContext.moveU = false;
        else if (wParam == VK_DOWN || wParam == 'S') g_gameContext.moveD = false;
        break;
        // --- [시스템 메시지 처리] ---
    case WM_DESTROY:
        // 사용자가 'X' 버튼을 눌러 창을 닫으려 할 때 호출됨.
        printf("[SYSTEM] 윈도우 파괴 메시지 수신.\n");
        g_gameContext.isRunning = false;
        break;

    default:
        // 우리가 관심 없는 메시지(창 크기 조절, 포커스 변경 등)는 OS가 기본값으로 처리함.
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

struct Position {
    float x, y, z;
};

// HLSL (High-Level Shading Language) 소스
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f); // 3D 좌표를 4D로 확장
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 정점에서 계산된 색상을 픽셀에 그대로 적용
}
)";

constexpr int width = 800;
constexpr int height = 600;
constexpr float aspect = (float)width / height;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 1. 윈도우 등록 및 생성
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11GameLoopClass";
    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(L"DX11GameLoopClass", L"움직이는 5각별(오망성) 만들기",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return -1;
    ShowWindow(hWnd, nCmdShow);

    // 2. DX11 디바이스 및 스왑 체인(이중 버퍼링) 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width; sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; // 생성한 Win32 창 핸들 연결 ＃윈도우 핸들러를 swapchane discriptor에 등록하고, 
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // GPU와 통신할 통로(Device, &g_pd3dDevice)와 화면(SwapChain, &g_pSwapChain)을 생성함.
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    // 렌더 타겟 설정 (도화지 준비)
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 뷰를 생성했으므로 원본 텍스트는 바로 해제 (중요!)

    // 3. 셰이더 컴파일 및 생성
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    // # 쉐이더 생성 등은 모두 디바이스에서
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    // 정점의 데이터 형식을 정의 (IA 단계에 알려줌)
    // # D3D11_INPUT_PER_VERTEX_DATA는 정점마다, D3D11_INPUT_PER_INSTANCE_DATA는 인스턴스마다 데이터가 바뀐다
    // # 인스턴스마다 데이터 바뀌지 않으니없으니 InstanceDataStepRate 쓴다
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // 3개 들어가니 size는 12바이트
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

    // 4. 정점 버퍼 생성, 삼각형 두개를 그린다.
    constexpr float PI = 3.14159265f;
    constexpr float PI2_5 = 2 * PI / 5;
    constexpr float R = 0.5f;
    constexpr float r = R * 0.381966f;
    // radians = degrees * (PI / 180.0);
    Position outer[5] = {
        { R * cos(PI / 10 + 0 * PI2_5), aspect * R * sin(PI / 10 + 0 * PI2_5), 0.5f },
        { R * cos(PI / 10 + 1 * PI2_5), aspect * R * sin(PI / 10 + 1 * PI2_5), 0.5f },
        { R * cos(PI / 10 + 2 * PI2_5), aspect * R * sin(PI / 10 + 2 * PI2_5), 0.5f },
        { R * cos(PI / 10 + 3 * PI2_5), aspect * R * sin(PI / 10 + 3 * PI2_5), 0.5f },
        { R * cos(PI / 10 + 4 * PI2_5), aspect * R * sin(PI / 10 + 4 * PI2_5), 0.5f },
    };
	Position inner[5] = {
        { r * cos(0.942478  + 0 * PI2_5), aspect * r * sin(0.942478  + 0 * PI2_5), 0.5f },
        { r * cos(0.942478  + 1 * PI2_5), aspect * r * sin(0.942478  + 1 * PI2_5), 0.5f },
        { r * cos(0.942478  + 2 * PI2_5), aspect * r * sin(0.942478  + 2 * PI2_5), 0.5f },
        { r * cos(0.942478  + 3 * PI2_5), aspect * r * sin(0.942478  + 3 * PI2_5), 0.5f },
        { r * cos(0.942478  + 4 * PI2_5), aspect * r * sin(0.942478  + 4 * PI2_5), 0.5f },
    };
    Position relativeVertexPos[9] = {
        outer[1], outer[4], inner[2],
        outer[0], inner[3], outer[2],
        outer[1], inner[4], outer[3],
    };
    Vertex vertices[] = {
        { relativeVertexPos[0].x, relativeVertexPos[0].y, relativeVertexPos[0].z, 1,0,0,1 },
        { relativeVertexPos[1].x, relativeVertexPos[1].y, relativeVertexPos[1].z, 0,0,1,1 },
        { relativeVertexPos[2].x, relativeVertexPos[2].y, relativeVertexPos[2].z, 0,1,0,1 },

        { relativeVertexPos[3].x, relativeVertexPos[3].y, relativeVertexPos[3].z, 1,0,0,1 },
        { relativeVertexPos[4].x, relativeVertexPos[4].y, relativeVertexPos[4].z, 0,0,1,1 },
        { relativeVertexPos[5].x, relativeVertexPos[5].y, relativeVertexPos[5].z, 0,1,0,1 },

        { relativeVertexPos[6].x, relativeVertexPos[6].y, relativeVertexPos[6].z, 1,0,0,1 },
        { relativeVertexPos[7].x, relativeVertexPos[7].y, relativeVertexPos[7].z, 0,0,1,1 },
        { relativeVertexPos[8].x, relativeVertexPos[8].y, relativeVertexPos[8].z, 0,1,0,1 },
    };
    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);
    /*
     * [게임 루프]
     */
    g_gameContext = { 0,0,0,0,0,0,1 };
    MSG msg = { 0 };
    while (g_gameContext.isRunning) {

        // 1. 입력: 사용자가 무엇을 했는가? 있다면 처리
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // # 가상 키 메시지(Virtual-key messages)를 문자 메시지(Character messages)로 변환
            // # 키보드에서 문자가 입력될 때 발생하는 WM_KEYDOWN과 WM_KEYUP 메시지를 분석하여 WM_CHAR 메시지를 생성하고 메시지 큐에 추가.
            // # 이 함수를 호출하지 않으면 WM_CHAR 메시지가 생성되지 않아, 문자 입력(텍스트 입력창 등)을 처리할 수 없
            TranslateMessage(&msg);
            // # 메시지를 윈도우 프로시저(WndProc)로 전달.
            DispatchMessage(&msg);
        }

        // 2. 업데이트: 그 결과 세상은 어떻게 변했는가?
        // WndProc가 전역 게임 컨텍스트를 주무르므로, GetAsyncKeyState를 쓰지 않아도 된다.
        constexpr float moveStep = 0.001f;
        if (g_gameContext.moveL || g_gameContext.moveR || g_gameContext.moveD || g_gameContext.moveU) { //위치 변경 없으면 굳이 버퍼 새로 만들지 않는다
            if (g_gameContext.moveL) g_gameContext.posX -= moveStep;
            if (g_gameContext.moveR) g_gameContext.posX += moveStep;
            if (g_gameContext.moveU) g_gameContext.posY += moveStep;
            if (g_gameContext.moveD) g_gameContext.posY -= moveStep;

            // 화면의 아예 바깥으로 나가지 못하게
            g_gameContext.posX = max(-1.0f, min(1.0f, g_gameContext.posX));
            g_gameContext.posY = max(-1.0f, min(1.0f, g_gameContext.posY));

            for (int i = 0; i < sizeof(vertices) / sizeof(Vertex); i++) {
                vertices[i].x = relativeVertexPos[i].x + g_gameContext.posX;
                vertices[i].y = relativeVertexPos[i].y + g_gameContext.posY;
            }

            // 먼저 만든 거 지우고
            if (pVBuffer) pVBuffer->Release();
            // 버퍼 속성과 데이터의 주소는 바뀌지 않으니, bd도 initData도 바뀌지 않는다.
            g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);
        }


        // 3. 출력: 변한 세상을 화면에 그려라!
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor); // backbuffer를 지워버림

        // 렌더링 파이프라인 상태 설정
        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        D3D11_VIEWPORT vp = { 0, 0, width, height, 0.0f, 1.0f };
        g_pImmediateContext->RSSetViewports(1, &vp);

        g_pImmediateContext->IASetInputLayout(pInputLayout);
        UINT stride = sizeof(Vertex), offset = 0;

        // # 여기의 StartSlot이 D3D11_INPUT_ELEMENT_DESC layout[]의 InputSlot(0)이다
        // # 여러 버퍼 쓸 때 의미 있음 (예: 위치 버퍼 / 색상 버퍼 분리)
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

        // Primitive Topology 설정: 삼각형 리스트로 연결하라!
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

        // 최종 그리기, 버텍스 9개
        g_pImmediateContext->Draw(9, 0);

        // 화면 교체 (프론트 버퍼와 백 버퍼 스왑)
        g_pSwapChain->Present(0, 0);
    }

    printf("\n게임 루프가 종료되었습니다. 프로그램 끝.\n");
    if (pVBuffer) pVBuffer->Release();
    if (pInputLayout) pInputLayout->Release();
    if (vShader) vShader->Release();
    if (pShader) pShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}


#endif


#ifdef VSSAMPLE
/*
================================================================================
 [HLSL 핵심 강의: 상수 버퍼(cbuffer)와 레지스터(register)]
================================================================================

 1. 왜 이런 복잡한 구조를 쓰는가? (The Necessity)
    - CPU와 GPU는 물리적으로 떨어진 별개의 장치임.
    - CPU가 관리하는 메모리(RAM)와 GPU가 관리하는 메모리(VRAM)는 서로 주소가 다름.
    - 따라서 CPU에서 계산한 값(예: 위치 오프셋)을 GPU에게 전달하려면,
      양쪽이 약속한 '특정한 입구'가 필요한데 그게 바로 [register]임.

 2. cbuffer (Constant Buffer) : "명령서 박스"
    - GPU는 데이터를 하나씩 받는 것보다 뭉텅이로 받는 것을 좋아함.
    - 관련된 변수들(위치, 색상, 시간 등)을 하나의 '박스'에 담아 보내는 규격이 cbuffer임.
    - [주의]: GPU는 데이터를 16바이트(float4) 단위로 읽기 때문에,
      박스 내부의 데이터 크기는 항상 16의 배수가 되도록 맞춰야 함 (Padding).

 3. register(b#) : "입구 번호"
    - GPU 하드웨어에는 데이터를 꽂는 여러 종류의 선반(Slot)이 있음.
    - 'b'는 Constant Buffer 전용 선반을 의미함.
    - '0'은 0번 선반이라는 뜻임.
    - [연결 고리]:
        (C++) VSSetConstantBuffers(0, ...) <---> (HLSL) register(b0)
      두 번호가 일치해야만 CPU가 보낸 박스가 셰이더의 변수로 배달됨.

 4. 현대 그래픽스의 특징 (Programmable Pipeline)
    - 과거(DX9 이전)에는 GPU가 정해진 연산만 했기에 이런 지정이 필요 없었음.
    - 현대 GPU는 우리가 시키는 대로만 계산하는 '범용 계산기'이므로,
      데이터의 입구(register)와 모양(cbuffer)을 우리가 직접 설계해줘야 함.
================================================================================


// [실습 예제용 구조]
cbuffer MoveBuffer : register(b0) // 0번 상수 버퍼 입구에서 대기
{
    float2 g_Offset;  // x, y 축으로 얼만큼 이동할지 (8바이트)
    float2 g_Padding; // 16바이트 규격을 맞추기 위한 빈 공간 (8바이트)
};


 [한 줄 요약]
 cbuffer는 CPU가 GPU에게 보내는 '편지 봉투'이고,
 register(b0)는 그 편지가 도착할 '우체통 번호'라고 생각하자.
*/

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h> // 행렬 및 벡터 연산용
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// [1. 상수 버퍼 구조체] 
// HLSL의 cbuffer와 1:1로 매칭되어야 함 (16바이트 정렬 주의)
struct ConstantData {
    XMFLOAT2 offset;    // x, y 이동값 (8바이트)
    float rotate;    // 회전값(라디안 (4바이트)
    float scale;
};

struct VideoConfig {
    int Width = 800;
    int Height = 600;
    bool IsFullscreen = false;
    bool NeedsResize = false;
    int VSync = 1;
} g_Config;

// 전역 변수
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr; // 상수 버퍼 추가

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

// [실시간 이동을 위한 오프셋 변수]
XMFLOAT2 g_CurOffset = { 0.0f, 0.0f };
float g_Rotate = 0.0f;
float g_Scale= 1.0f;

void RebuildVideoResources(HWND hWnd) {
    if (!g_pSwapChain) return;
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    g_pSwapChain->ResizeBuffers(0, g_Config.Width, g_Config.Height, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    if (!g_Config.IsFullscreen) {
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }
    g_Config.NeedsResize = false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11MoveClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindowW(L"DX11MoveClass", L"Arrows: Move | A, D: Rotate | W, S: Resize Object | 1, 2: Resize Window",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    RebuildVideoResources(hWnd);

    // [셰이더 소스: 오프셋 적용 버전]
    const char* shaderSource = R"(
        cbuffer MoveBuffer : register(b0) // 상수 버퍼 슬롯 b0 사용
        {
            float2 g_Offset; // CPU에서 보내준 x, y 이동값
            float g_Rotate;
            float g_Scale;
        };

        struct VS_INPUT {
            float3 pos : POSITION;
            float4 col : COLOR;
        };

        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float4 col : COLOR;
        };

        PS_INPUT VS_Main(VS_INPUT input) {
            PS_INPUT output;

            // # 스케일 행렬 추가
            float3x3 sMatrix = { g_Scale, 0.0f,     0.0f,  // row 1
                                 0.0f,    g_Scale,  0.0f,  // row 2
                                 0.0f,    0.0f,     1.0f}; // row 3

            // # 회전 행렬 추가
            float3x3 rMatrix = { cos(g_Rotate), -sin(g_Rotate),  0.0f,  // row 1
                                 sin(g_Rotate),  cos(g_Rotate),  0.0f,  // row 2
                                 0.0f,           0.0f,           1.0f}; // row 3

            // 입력받은 정점 위치에 오프셋을 더함 (이동 처리)
            float3 finalPos = mul(rMatrix, mul(sMatrix, input.pos));
            finalPos.x += g_Offset.x;
            finalPos.y += g_Offset.y;

            output.pos = float4(finalPos, 1.0f);
            output.col = input.col;
            return output;
        }

        float4 PS_Main(PS_INPUT input) : SV_Target {
            float halfPI = 1.57079635f;
            float4 ret = float4((float3)((sin(g_Rotate - halfPI) + 1.0f) / 2.0f), 1.0f); //((g_Rotate % pi2) / pi2);
            return ret;
        }
    )";

    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS_Main", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS_Main", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
    vsBlob->Release(); psBlob->Release();

    Vertex vertices[] = {
        {  0.0f,  0.3f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.3f, -0.3f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.3f, -0.3f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

    // [2. 상수 버퍼 생성]
    D3D11_BUFFER_DESC cbd = { 0 };
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ConstantData); // 16바이트
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // [방향키 입력 처리]
            float moveSpeed = 0.005f;
            float rotateAngle = 0.1f;
            float scaleUnit = 0.05f;
            if (GetAsyncKeyState(VK_LEFT))  g_CurOffset.x -= moveSpeed;
            if (GetAsyncKeyState(VK_RIGHT)) g_CurOffset.x += moveSpeed;
            if (GetAsyncKeyState(VK_UP))    g_CurOffset.y += moveSpeed;
            if (GetAsyncKeyState(VK_DOWN))  g_CurOffset.y -= moveSpeed;
            if (GetAsyncKeyState('A'))      g_Rotate += rotateAngle;
            if (GetAsyncKeyState('D'))      g_Rotate -= rotateAngle;
            if (GetAsyncKeyState('W'))      g_Scale += scaleUnit;
            if (GetAsyncKeyState('S'))      g_Scale -= scaleUnit;
            if (g_Scale < 0) g_Scale = 0.0f;

            if (GetAsyncKeyState('1') & 0x0001) { g_Config.Width = 800; g_Config.Height = 600; g_Config.NeedsResize = true; }
            if (GetAsyncKeyState('2') & 0x0001) { g_Config.Width = 1280; g_Config.Height = 720; g_Config.NeedsResize = true; }
            if (g_Config.NeedsResize) RebuildVideoResources(hWnd);

            // [렌더링 시작]
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // [3. 상수 버퍼 데이터 업데이트 및 전송]
            ConstantData cbData = { g_CurOffset, g_Rotate, g_Scale };
            g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cbData, 0, 0);
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer); // VS의 0번 슬롯(b0)에 바인딩
            g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer); // PS의 0번 슬롯(b0)에 바인딩 ＃ 픽셀 쉐이더에서도 써야 하므로 바인딩해야함!

            D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)g_Config.Width, (float)g_Config.Height, 0.0f, 1.0f };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
            g_pImmediateContext->Draw(3, 0);

            g_pSwapChain->Present(g_Config.VSync, 0);
        }
    }

    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}
#endif