
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
//#define VSSAMPLE
//#define MIDTERM
#define MID_ANSWER

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

#ifdef MIDTERM
/*
================================================================================
 [Engine Architecture]
 1. WindowContext: Win32 창 생성 및 메시지 루프 관리
 2. GraphicsContext: DX11 디바이스, 스왑체인, 셰이더 컴파일 및 영상 설정 관리
 3. DeltaTime: 고해상도 타이머를 이용한 시간 계산
 4. GameObject & Component: 객체 지향적 기능 확장 구조
 5. GameLoop: 전체 흐름(Input-Update-Render) 제어
================================================================================
*/

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <vector>
#include <chrono>
#include <string>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 pos; XMFLOAT4 col;
};
struct ConstantBuffer
{
    XMMATRIX matWorld;
};


class DeltaTime
{
    std::chrono::high_resolution_clock::time_point prevTime;
public:
    DeltaTime()
    {
        prevTime = std::chrono::high_resolution_clock::now();
    }

    float GetDelta()
    {
        std::chrono::steady_clock::time_point currTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currTime - prevTime).count();
        prevTime = currTime;
        return dt;
    }
};


class WindowContext
{
public:
    HWND hWnd;
    int Width, Height;
    LPCWSTR windowName;

    WindowContext(LPCWSTR winName = L"DX11 Component Engine")
        : windowName(winName), hWnd(nullptr), Width(800), Height(600)
    {
    }

    ~WindowContext()
    {
        UnregisterClass(L"DX11Engine", GetModuleHandle(NULL));
    }

    bool Initialize(HINSTANCE hInst, int w, int h, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM))
    {
        Width = w; Height = h;

        WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"DX11Engine";

        if (!RegisterClassEx(&wc)) return false;

        hWnd = CreateWindow(L"DX11Engine", windowName, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, Width, Height,
            NULL, NULL, hInst, NULL);

        if (!hWnd) return false;

        // 1. 윈도우 크기
        RECT rc = { 0, 0, Width, Height };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        ShowWindow(hWnd, SW_SHOW);
        return true;
    }
};

class GraphicsContext {
public:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* ImmediateContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;

    bool IsFullscreen = false;
    int VSync = 1;

    bool InitDX(HWND hWnd, int w, int h)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = w; sd.BufferDesc.Height = h;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
            D3D11_SDK_VERSION, &sd, &SwapChain, &Device, NULL, &ImmediateContext);

        return SUCCEEDED(hr) && CreateRTV(w, h);
    }

    bool CreateRTV(int w, int h)
    {
        if (RTV) RTV->Release();
        ID3D11Texture2D* pBB;
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBB);
        Device->CreateRenderTargetView(pBB, NULL, &RTV);
        pBB->Release();
        return true;
    }

    void Resize(int w, int h)
    {
        ImmediateContext->OMSetRenderTargets(0, 0, 0);
        RTV->Release(); RTV = nullptr;
        SwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        CreateRTV(w, h);
    }

    void SetFullscreen(bool goFull)
    {
        IsFullscreen = goFull;
        SwapChain->SetFullscreenState(goFull, NULL);
    }

    ID3DBlob* CompileShader(const std::string& src, const std::string& entry, const std::string& profile) {
        ID3DBlob* blob = nullptr;
        D3DCompile(src.c_str(), src.length(), NULL, NULL, NULL, entry.c_str(), profile.c_str(), 0, 0, &blob, NULL);
        return blob;
    }


    void RebuildVideoResources(int w, int h, HWND hWnd)
    {
        if (!SwapChain) return;

        // 기존 렌더 타겟 뷰 해제 (안 하면 ResizeBuffers 실패함)
        if (RTV)
        {
            RTV->Release();
            RTV = nullptr;
        }

        // 2. 백버퍼 크기 재설정
        SwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);

        // 3. 새 백버퍼로부터 렌더 타겟 뷰 다시 생성
        ID3D11Texture2D* pBackBuffer = nullptr;
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (pBackBuffer == nullptr)
        {
            printf("GETBUFFER ERROR\n");
            return;
        }
        Device->CreateRenderTargetView(pBackBuffer, nullptr, &RTV);
        pBackBuffer->Release();

        // 4. 윈도우 창 크기 실제 조정 (전체화면이 아닐 때만)
        if (!IsFullscreen)
        {
            RECT rc = { 0, 0, w, h };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    ~GraphicsContext() {
        if (RTV)
            RTV->Release();
        if (SwapChain)
            SwapChain->Release();
        if (ImmediateContext)
            ImmediateContext->Release();
        if (Device)
            Device->Release();
    }
};

class GameObject;
class Component
{
public:
    GameObject* pOwner = nullptr;
    bool isStarted = false;

    Component() {}
    virtual void Start(GraphicsContext* gfx) = 0;
    virtual void Input() = 0;
    virtual void Update(float dt) = 0;
    virtual void Render(GraphicsContext* gfx) = 0;
    virtual ~Component() {}
};

class GameObject
{
public:
    XMFLOAT3 pos = { 0, 0, 0 };
    XMFLOAT3 rot = { 0, 0, 0 };
    XMFLOAT3 scale = { 1, 1, 1 };
    std::vector<Component*> components;

    GameObject(float x, float y, float z)
    {
        pos.x = x;
        pos.y = y;
        pos.z = z;
    }
    ~GameObject()
    {
        for (int i = 0; i < (int)components.size(); i++)
            delete components[i];
    }

    void AddComponent(Component* c)
    {
        c->pOwner = this;
        components.push_back(c);
    }

    void Input()
    {
        int componentCount = (int)components.size();
        for (int i = 0; i < componentCount; i++)
        {
            if (components[i] != nullptr)
            {
                components[i]->Input();
            }
        }
    }

    void Update(float dt, GraphicsContext* gfx)
    {
        for (int i = 0; i < (int)components.size(); i++)
        {
            // Start()가 호출된 적 없다면 여기서 호출
            if (components[i]->isStarted == false) {
                components[i]->Start(gfx);
                components[i]->isStarted = true;
            }
        }

        // 업데이트 단계 (Update Phase)
        for (int i = 0; i < (int)components.size(); i++) {
            components[i]->Update(dt);
        }
    }
    void Render(GraphicsContext* gfx)
    {
        for (int i = 0; i < components.size(); i++)
        {
            if (components[i] != nullptr)
            {
                components[i]->Render(gfx);
            }
        }
    }
};


struct Mesh
{
    ID3D11Buffer* vBuffer;
    ID3D11InputLayout* pInputLayout;
    ID3D11VertexShader* pVS;
    ID3D11PixelShader* pPS;
    UINT vertexCount;
    XMFLOAT4 color;

    Mesh()
        : vBuffer(NULL), pInputLayout(NULL), pVS(NULL), pPS(NULL), vertexCount(0)
    {
        color = { 1, 1, 1, 1 };
    }

    ~Mesh()
    {
        if (vBuffer) vBuffer->Release();
        if (pInputLayout) pInputLayout->Release();
        if (pVS) pVS->Release();
        if (pPS) pPS->Release();
    }
};


class MeshRenderer : public Component
{
    Mesh* pMeshData = nullptr;
    ID3D11Buffer* cBuffer = nullptr;

public:
    MeshRenderer(Mesh* mesh) : Component(), pMeshData(mesh)
    {
    }

    ~MeshRenderer()
    {
        if (cBuffer) cBuffer->Release();
        if (pMeshData) delete pMeshData;
    }

    void Start(GraphicsContext* gfx) override
    {
        D3D11_BUFFER_DESC cbd = { 0 };
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ConstantBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        gfx->Device->CreateBuffer(&cbd, nullptr, &cBuffer);
    }

    void Render(GraphicsContext* gfx) override
    {
        if (pMeshData == nullptr || pMeshData->vBuffer == nullptr) return;

        gfx->ImmediateContext->IASetInputLayout(pMeshData->pInputLayout);
        gfx->ImmediateContext->VSSetShader(pMeshData->pVS, nullptr, 0);
        gfx->ImmediateContext->PSSetShader(pMeshData->pPS, nullptr, 0);

        float s = 1.0f / (pOwner->pos.z + 1.0f);
        XMMATRIX world = XMMatrixScaling(s, s, s) * XMMatrixRotationZ(pOwner->rot.z) * XMMatrixTranslation(pOwner->pos.x, pOwner->pos.y, 0.0f);
        ConstantBuffer cb;
        cb.matWorld = XMMatrixTranspose(world);
        gfx->ImmediateContext->UpdateSubresource(cBuffer, 0, nullptr, &cb, 0, 0);

        UINT stride = sizeof(Vertex), offset = 0;
        gfx->ImmediateContext->IASetVertexBuffers(0, 1, &pMeshData->vBuffer, &stride, &offset);
        gfx->ImmediateContext->VSSetConstantBuffers(0, 1, &cBuffer);
        gfx->ImmediateContext->Draw(pMeshData->vertexCount, 0);
    }
    void Input() override {}
    void Update(float dt) override {}
};

class PlayerController : public Component
{
    // 입력 상태를 저장하기 위한 멤버 변수 (내부용)
    XMFLOAT2 moveDir;  // x: 좌우, y: 상하
    float    rotDir;   // 회전 방향
    float    zoomDir;  // 확대/축소 방향

public:
    PlayerController() : Component()
    {
        moveDir = { 0, 0 };
        rotDir = 0.0f;
        zoomDir = 0.0f;
    }

    ~PlayerController()
    {
    }

    void Start(GraphicsContext* gfx) override
    {
        printf("[Component] 플레이어 컨트롤러 로드 완료\n");
    }

    // [Step 1] 입력 감지 및 상태 저장
    void Input() override
    {
        // 매 프레임 입력 상태 초기화
        moveDir = { 0, 0 };
        rotDir = 0.0f;
        zoomDir = 0.0f;

        // 방향키 입력 (이동)
        if (GetAsyncKeyState(VK_UP) & 0x8000)    moveDir.y += 1.0f;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  moveDir.y -= 1.0f;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  moveDir.x -= 1.0f;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) moveDir.x += 1.0f;

        // AD 키 입력 (회전)
        if (GetAsyncKeyState('A') & 0x8000) rotDir += 1.0f;
        if (GetAsyncKeyState('D') & 0x8000) rotDir -= 1.0f;

        // WS 키 입력 (줌)
        if (GetAsyncKeyState('W') & 0x8000) zoomDir -= 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) zoomDir += 1.0f;
    }

    // [Step 2] 저장된 상태를 바탕으로 데이터 갱신
    void Update(float dt) override
    {
        // 1. 속도 정의 (사이즈 비례 속도 적용 가능)
        float speedFactor = pOwner->scale.x;
        float moveSpeed = 2.0f * speedFactor;
        float rotateSpeed = 3.0f * speedFactor;
        float zoomSpeed = 5.0f * speedFactor;

        // 2. 위치 업데이트
        pOwner->pos.x += moveDir.x * moveSpeed * dt;
        pOwner->pos.y += moveDir.y * moveSpeed * dt;

        // 3. 회전 업데이트
        pOwner->rot.z += rotDir * rotateSpeed * dt;

        // 4. 줌(Z축) 업데이트 및 제한
        pOwner->pos.z += zoomDir * zoomSpeed * dt;

        if (pOwner->pos.z < -0.9f)
        {
            pOwner->pos.z = -0.9f;
        }
    }

    void Render(GraphicsContext* gfx) override
    {
    }
};

class GameLoop
{
public:
    WindowContext win;
    GraphicsContext gfx;
    DeltaTime timer;
    std::vector<GameObject*> world;
    bool isRunning = true;

    ID3D11VertexShader* pDefaultVS = nullptr;
    ID3D11PixelShader* pDefaultPS = nullptr;
    ID3D11InputLayout* pDefaultLayout = nullptr;

    GameLoop() : isRunning(true)
    {
        world.clear();
        printf("[Engine] GameLoop Created.\n");
    }

    ~GameLoop()
    {
        for (int i = 0; i < (int)world.size(); i++)
        {
            if (world[i])
            {
                delete world[i];
                world[i] = nullptr;
            }
        }
        world.clear();

        if (pDefaultLayout) pDefaultLayout->Release();
        if (pDefaultVS) pDefaultVS->Release();
        if (pDefaultPS) pDefaultPS->Release();

        printf("[Engine] GameLoop Destroyed. All resources released.\n");
    }

    void Initialize(HINSTANCE hInst, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM))
    {
        win.Initialize(hInst, 800, 800, wndProc);
        gfx.InitDX(win.hWnd, 800, 800);
    }

    void Input()
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            isRunning = false;
        if (GetAsyncKeyState('F') & 0x0001)
            gfx.SetFullscreen(!gfx.IsFullscreen);
        if (GetAsyncKeyState('c') & 0x0001)
            gfx.RebuildVideoResources(600, 600, win.hWnd);

        // 2. 월드 내 모든 오브젝트에 입력 전파
        int objectCount = (int)world.size();
        for (int i = 0; i < objectCount; i++)
        {
            if (world[i] != nullptr)
            {
                world[i]->Input();
            }
        }
    }

    void Update()
    {
        float dt = timer.GetDelta();
        for (int i = 0; i < (int)world.size(); i++)
        {
            if (world[i] != nullptr)
            {
                world[i]->Update(dt, &gfx);
            }
        }
    }

    void Render()
    {
        float col[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        gfx.ImmediateContext->ClearRenderTargetView(gfx.RTV, col);

        D3D11_VIEWPORT vp = { 0, 0, (float)win.Width, (float)win.Height, 0, 1 };
        gfx.ImmediateContext->RSSetViewports(1, &vp);
        gfx.ImmediateContext->OMSetRenderTargets(1, &gfx.RTV, NULL);

        if (pDefaultLayout)
        {
            gfx.ImmediateContext->IASetInputLayout(pDefaultLayout);
        }
        gfx.ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (int i = 0; i < (int)world.size(); i++)
        {
            if (world[i] != nullptr)
            {
                world[i]->Render(&gfx);
            }
        }
        gfx.SwapChain->Present(gfx.VSync, 0);
    }

    void Run()
    {
        MSG msg = {};
        while (msg.message != WM_QUIT && isRunning)
        {
            if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg); DispatchMessage(&msg);
            }
            else
            {
                Input();
                Update();
                Render();
            }
        }
    }
};

LRESULT CALLBACK GlobalWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS)
{
    GameLoop gEngine;
    gEngine.Initialize(hI, GlobalWndProc);

    std::string triShader = R"(
        cbuffer ConstantBuffer : register(b0) // 상수 버퍼 슬롯 b0 사용
        {
            float4x4 matWorld;
        };

        struct VS_INPUT {
            float3 pos : POSITION;
            float4 col : COLOR;
        };

        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float4 col : COLOR;
        };

        PS_INPUT VS(VS_INPUT input) {
            PS_INPUT output;

            output.pos = mul(matWorld, float4(input.pos, 1.0f));
            output.col = input.col;
    
            return output;
        }

        float4 PS(PS_INPUT input) : SV_Target {
            return input.col;
        }
    )";

    ID3DBlob* vsBlob = gEngine.gfx.CompileShader(triShader, "VS", "vs_5_0");
    ID3DBlob* psBlob = gEngine.gfx.CompileShader(triShader, "PS", "ps_5_0");

    Mesh* myTri = new Mesh();
    myTri->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    myTri->vertexCount = 9;

    gEngine.gfx.Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &myTri->pVS);
    gEngine.gfx.Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &myTri->pPS);


    // 별 거지같은 오각별 생성 코드
    float PI = 3.14159265f;
    float PI2_5 = 2 * PI / 5;
    float R = 0.5f;
    float r = R * 0.381966f;
    struct Position { float x, y, z; };
    // radians = degrees * (PI / 180.0);
    Position outer[5] = {
        { R * cos(PI / 10 + 0 * PI2_5), R * sin(PI / 10 + 0 * PI2_5), 0 },
        { R * cos(PI / 10 + 1 * PI2_5), R * sin(PI / 10 + 1 * PI2_5), 0 },
        { R * cos(PI / 10 + 2 * PI2_5), R * sin(PI / 10 + 2 * PI2_5), 0 },
        { R * cos(PI / 10 + 3 * PI2_5), R * sin(PI / 10 + 3 * PI2_5), 0 },
        { R * cos(PI / 10 + 4 * PI2_5), R * sin(PI / 10 + 4 * PI2_5), 0 },
    };
    Position inner[5] = {
        { r * cos(0.942478 + 0 * PI2_5), r * sin(0.942478 + 0 * PI2_5), 0 },
        { r * cos(0.942478 + 1 * PI2_5), r * sin(0.942478 + 1 * PI2_5), 0 },
        { r * cos(0.942478 + 2 * PI2_5), r * sin(0.942478 + 2 * PI2_5), 0 },
        { r * cos(0.942478 + 3 * PI2_5), r * sin(0.942478 + 3 * PI2_5), 0 },
        { r * cos(0.942478 + 4 * PI2_5), r * sin(0.942478 + 4 * PI2_5), 0 },
    };
    Position relativeVertexPos[9] = { // 그려야 할 삼각형들
        outer[1], outer[4], inner[2],
        outer[0], inner[3], outer[2],
        outer[1], inner[4], outer[3],
    };
    Vertex v[9];
    for (int i = 0; i < 9; i++) {
        v[i] = { {relativeVertexPos[i].x, relativeVertexPos[i].y, relativeVertexPos[i].z}, myTri->color };
    }
    D3D11_BUFFER_DESC bd = { sizeof(v), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA sd = { v };
    gEngine.gfx.Device->CreateBuffer(&bd, &sd, &myTri->vBuffer);

    D3D11_INPUT_ELEMENT_DESC ied[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    gEngine.gfx.Device->CreateBuffer(&bd, &sd, &myTri->vBuffer);
    gEngine.gfx.Device->CreateInputLayout(ied, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &myTri->pInputLayout);

    vsBlob->Release(); psBlob->Release();

    GameObject* gstar = new GameObject(0, 0, 0);
    gstar->AddComponent(new MeshRenderer(myTri));
    gstar->AddComponent(new PlayerController());
    gEngine.world.push_back(gstar);

    for (int i = 0; i < (int)gEngine.world.size(); i++)
    {
        for (int j = 0; j < (int)gEngine.world[i]->components.size(); j++)
        {
            gEngine.world[i]->components[j]->Start(&gEngine.gfx);
            gEngine.world[i]->components[j]->isStarted = true;
        }
    }

    gEngine.Run();

    return 0;
}
#endif

#ifdef MID_ANSWER
/*
================================================================================

  # 중간고사 답안

 [Engine Architecture]
 1. WindowContext: Win32 창 생성 및 메시지 루프 관리
 2. GraphicsContext: DX11 디바이스, 스왑체인, 셰이더 컴파일 및 영상 설정 관리
 3. DeltaTime: 고해상도 타이머를 이용한 시간 계산
 4. GameObject & Component: 객체 지향적 기능 확장 구조
 5. GameLoop: 전체 흐름(Input-Update-Render) 제어
================================================================================
*/

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <vector>
#include <chrono>
#include <string>
#include <random>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 pos; XMFLOAT4 col;
};
struct ConstantBuffer
{
    XMMATRIX matWorld;
};


class DeltaTime
{
    std::chrono::high_resolution_clock::time_point prevTime;
public:
    DeltaTime()
    {
        prevTime = std::chrono::high_resolution_clock::now();
    }

    float GetDelta()
    {
        std::chrono::steady_clock::time_point currTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currTime - prevTime).count();
        prevTime = currTime;
        return dt;
    }
};


class WindowContext
{
public:
    HWND hWnd;
    int Width, Height;
    LPCWSTR windowName;

    WindowContext(LPCWSTR winName = L"DX11 Component Engine")
        : windowName(winName), hWnd(nullptr), Width(800), Height(600)
    {
    }

    ~WindowContext()
    {
        UnregisterClass(L"DX11Engine", GetModuleHandle(NULL));
    }

    bool Initialize(HINSTANCE hInst, int w, int h, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM))
    {
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

class GraphicsContext {
public:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* ImmediateContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;

    bool IsFullscreen = false;
    int VSync = 1;

    bool InitDX(HWND hWnd, int w, int h)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = w; sd.BufferDesc.Height = h;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
            D3D11_SDK_VERSION, &sd, &SwapChain, &Device, NULL, &ImmediateContext);

        return SUCCEEDED(hr) && CreateRTV(w, h);
    }

    bool CreateRTV(int w, int h)
    {
        if (RTV) RTV->Release();
        ID3D11Texture2D* pBB;
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBB);
        Device->CreateRenderTargetView(pBB, NULL, &RTV);
        pBB->Release();
        return true;
    }

    void Resize(int w, int h)
    {
        ImmediateContext->OMSetRenderTargets(0, 0, 0);
        RTV->Release(); RTV = nullptr;
        SwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        CreateRTV(w, h);
    }

    void SetFullscreen(bool goFull)
    {
        IsFullscreen = goFull;
        SwapChain->SetFullscreenState(goFull, NULL);
    }

    ID3DBlob* CompileShader(const std::string& src, const std::string& entry, const std::string& profile) {
        ID3DBlob* blob = nullptr;
        D3DCompile(src.c_str(), src.length(), NULL, NULL, NULL, entry.c_str(), profile.c_str(), 0, 0, &blob, NULL);
        return blob;
    }

    ~GraphicsContext() {
        if (RTV)
            RTV->Release();
        if (SwapChain)
            SwapChain->Release();
        if (ImmediateContext)
            ImmediateContext->Release();
        if (Device)
            Device->Release();
    }
};

class GameObject;
class Component
{
public:
    GameObject* pOwner = nullptr;
    bool isStarted = false;

    Component() {}
    virtual void Start(GraphicsContext* gfx) = 0;
    virtual void Input() = 0; // 컴포넌트 레벨의 입력 처리
    virtual void Update(float dt) = 0;
    virtual void Render(GraphicsContext* gfx) = 0;
    virtual ~Component() {}
};

class GameObject
{
public:
    XMFLOAT3 pos = { 0, 0, 0 };
    XMFLOAT3 rot = { 0, 0, 0 };
    XMFLOAT3 scale = { 1, 1, 1 };
    std::vector<Component*> components;

    GameObject(float x, float y, float z)
    {
        pos.x = x;
        pos.y = y;
        pos.z = z;
    }
    ~GameObject()
    {
        for (int i = 0; i < (int)components.size(); i++)
            delete components[i];
    }

    void AddComponent(Component* c)
    {
        c->pOwner = this;
        components.push_back(c);
    }

    void Input()
    {
        // 인덱스 기반 루프로 하위 컴포넌트의 Input 호출
        int componentCount = (int)components.size();
        for (int i = 0; i < componentCount; i++)
        {
            if (components[i] != nullptr)
            {
                components[i]->Input();
            }
        }
    }

    void Update(float dt, GraphicsContext* gfx)
    {
        for (int j = 0; j < (int)components.size(); j++)
        {
            if (components[j] != nullptr)
            {
                if (components[j]->isStarted == false)
                {
                    components[j]->Start(gfx);
                    components[j]->isStarted = true;
                }

                components[j]->Update(dt);
            }
        }
    }
    void Render(GraphicsContext* gfx)
    {
        for (int i = 0; i < components.size(); i++)
        {
            if (components[i] != nullptr)
            {
                components[i]->Render(gfx);
            }
        }
    }
};


struct Mesh
{
    ID3D11Buffer* vBuffer;
    ID3D11InputLayout* pInputLayout;
    ID3D11VertexShader* pVS;
    ID3D11PixelShader* pPS;
    UINT vertexCount;
    XMFLOAT4 color;

    Mesh()
        : vBuffer(NULL), pInputLayout(NULL), pVS(NULL), pPS(NULL), vertexCount(0)
    {
        color = { 1, 1, 1, 1 };
    }

    ~Mesh()
    {
        if (vBuffer) vBuffer->Release();
        if (pInputLayout) pInputLayout->Release();
        if (pVS) pVS->Release();
        if (pPS) pPS->Release();
    }
};


class MeshRenderer : public Component
{
    Mesh* pMeshData = nullptr;
    ID3D11Buffer* cBuffer = nullptr;

public:
    MeshRenderer(Mesh* mesh) : Component(), pMeshData(mesh)
    {
    }

    ~MeshRenderer()
    {
        if (cBuffer) cBuffer->Release();
        if (pMeshData) delete pMeshData;
    }

    void Start(GraphicsContext* gfx) override
    {
        D3D11_BUFFER_DESC cbd = { 0 };
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ConstantBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        gfx->Device->CreateBuffer(&cbd, nullptr, &cBuffer);
    }

    void Render(GraphicsContext* gfx) override
    {
        if (pMeshData == nullptr || pMeshData->vBuffer == nullptr) return;

        gfx->ImmediateContext->IASetInputLayout(pMeshData->pInputLayout);
        gfx->ImmediateContext->VSSetShader(pMeshData->pVS, nullptr, 0);
        gfx->ImmediateContext->PSSetShader(pMeshData->pPS, nullptr, 0);

        float s = 1.0f / (pOwner->pos.z + 1.0f);
        XMMATRIX world = XMMatrixScaling(s, s, s) * XMMatrixRotationZ(pOwner->rot.z) * XMMatrixTranslation(pOwner->pos.x, pOwner->pos.y, 0.0f);
        ConstantBuffer cb;
        cb.matWorld = XMMatrixTranspose(world);
        gfx->ImmediateContext->UpdateSubresource(cBuffer, 0, nullptr, &cb, 0, 0);

        UINT stride = sizeof(Vertex), offset = 0;
        gfx->ImmediateContext->IASetVertexBuffers(0, 1, &pMeshData->vBuffer, &stride, &offset);
        gfx->ImmediateContext->VSSetConstantBuffers(0, 1, &cBuffer);
        gfx->ImmediateContext->Draw(pMeshData->vertexCount, 0);
    }
    void Input() override {}
    void Update(float dt) override {}
};

class PlayerController : public Component
{
    // 입력 상태를 저장하기 위한 멤버 변수 (내부용)
    XMFLOAT2 moveDir;  // x: 좌우, y: 상하
    float    rotDir;   // 회전 방향
    float    zoomDir;  // 확대/축소 방향

public:
    PlayerController() : Component()
    {
        moveDir = { 0, 0 };
        rotDir = 0.0f;
        zoomDir = 0.0f;
    }

    ~PlayerController()
    {
    }

    void Start(GraphicsContext* gfx) override
    {
    }

    // [Step 1] 입력 감지 및 상태 저장
    void Input() override
    {
        // 매 프레임 입력 상태 초기화
        moveDir = { 0, 0 };
        rotDir = 0.0f;
        zoomDir = 0.0f;

        // 방향키 입력 (이동)
        if (GetAsyncKeyState(VK_UP) & 0x8000)    moveDir.y += 1.0f;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  moveDir.y -= 1.0f;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  moveDir.x -= 1.0f;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) moveDir.x += 1.0f;

        // AD 키 입력 (회전)
        if (GetAsyncKeyState('A') & 0x8000) rotDir += 1.0f;
        if (GetAsyncKeyState('D') & 0x8000) rotDir -= 1.0f;

        // WS 키 입력 (줌)
        if (GetAsyncKeyState('W') & 0x8000) zoomDir -= 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) zoomDir += 1.0f;
    }

    // [Step 2] 저장된 상태를 바탕으로 데이터 갱신
    void Update(float dt) override
    {
        // 1. 속도 정의 (사이즈 비례 속도 적용 가능)
        float speedFactor = pOwner->scale.x;
        float moveSpeed = 2.0f * speedFactor;
        float rotateSpeed = 3.0f * speedFactor;
        float zoomSpeed = 5.0f * speedFactor;

        // 2. 위치 업데이트
        pOwner->pos.x += moveDir.x * moveSpeed * dt;
        pOwner->pos.y += moveDir.y * moveSpeed * dt;

        // 3. 회전 업데이트
        pOwner->rot.z += rotDir * rotateSpeed * dt;

        // 4. 줌(Z축) 업데이트 및 제한
        pOwner->pos.z += zoomDir * zoomSpeed * dt;

        if (pOwner->pos.z < -0.9f)
        {
            pOwner->pos.z = -0.9f;
        }
    }

    void Render(GraphicsContext* gfx) override
    {
    }
};


class GameSystem {
    // # inline 변수로 선언하면 클래스 외부에 정의를 넣어 줄 필요 없음, 다만 C++17 이상이 필요함
    static inline GameSystem* m_pInstance;

    WindowContext win;
    GraphicsContext gfx;
    DeltaTime timer;

    GameSystem() {}

    // 복사 방지
    GameSystem(const GameSystem&) = delete;
    GameSystem& operator=(const GameSystem&) = delete;

public:
    void Initialize(HINSTANCE hInst, int w, int h, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM)) {
        win.Initialize(hInst, w, h, wndProc);
        gfx.InitDX(win.hWnd, w, h);
    }

    WindowContext& GetWin() { return win; }
    GraphicsContext& GetGfx() { return gfx; }
    DeltaTime& GetTimer() { return timer; }

    static GameSystem* GetInstance() {
        if (m_pInstance == nullptr) {
            m_pInstance = new GameSystem();
        }
        return m_pInstance;
    }
    static void Release() {
        if (m_pInstance) {
            delete m_pInstance;
            m_pInstance = nullptr;
        }
    }
};

class GameLoop
{
public:
    std::vector<GameObject*> world;
    bool isRunning = true;

    ID3D11VertexShader* pDefaultVS = nullptr;
    ID3D11PixelShader* pDefaultPS = nullptr;
    ID3D11InputLayout* pDefaultLayout = nullptr;

    GameLoop() : isRunning(true)
    {
        world.clear();
        printf("[Engine] GameLoop Created.\n");
    }

    ~GameLoop()
    {
        for (int i = 0; i < (int)world.size(); i++)
        {
            if (world[i])
            {
                delete world[i];
                world[i] = nullptr;
            }
        }
        world.clear();

        if (pDefaultLayout) pDefaultLayout->Release();
        if (pDefaultVS) pDefaultVS->Release();
        if (pDefaultPS) pDefaultPS->Release();

        printf("[Engine] GameLoop Destroyed. All resources released.\n");
    }

    void Initialize(HINSTANCE hInst, LRESULT(CALLBACK* wndProc)(HWND, UINT, WPARAM, LPARAM))
    {
        GameSystem::GetInstance()->Initialize(hInst, 800, 600, wndProc);
    }

    void Input()
    {
        auto& win = GameSystem::GetInstance()->GetWin();
        auto& gfx = GameSystem::GetInstance()->GetGfx();

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            isRunning = false;
        if (GetAsyncKeyState('F') & 0x0001)
            gfx.SetFullscreen(!gfx.IsFullscreen);

        if (GetAsyncKeyState('C') & 0x0001) // 0x0001은 이번 프레임에 눌렸는지 확인(Toggle)
        {
            // 1. 내부 변수 업데이트
            win.Width = 600;
            win.Height = 600;

            // 2. 실제 Win32 윈도우 크기 변경
            // SWP_NOMOVE: 위치는 유지, SWP_NOZORDER: 레이어 순서 유지
            RECT rc = { 0, 0, win.Width, win.Height };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

            SetWindowPos(win.hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);

            // 3. DX11 백버퍼 및 RTV 리사이즈 (GraphicsContext에 정의된 함수 호출)
            gfx.Resize(win.Width, win.Height);

            printf("[Engine] Window Resized to 600x600\n");
        }

        // 2. 월드 내 모든 오브젝트에 입력 전파
        int objectCount = (int)world.size();
        for (int i = 0; i < objectCount; i++)
        {
            if (world[i] != nullptr)
            {
                world[i]->Input();
            }
        }
    }

    void Update()
    {
        float dt = GameSystem::GetInstance()->GetTimer().GetDelta();
        for (int i = 0; i < (int)world.size(); i++)
        {
            if (world[i] != nullptr)
            {
                world[i]->Update(dt, &GameSystem::GetInstance()->GetGfx());
            }
        }
    }

    void Render()
    {
        auto& win = GameSystem::GetInstance()->GetWin();
        auto& gfx = GameSystem::GetInstance()->GetGfx();

        float col[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        gfx.ImmediateContext->ClearRenderTargetView(gfx.RTV, col);

        D3D11_VIEWPORT vp = { 0, 0, (float)win.Width, (float)win.Height, 0, 1 };
        gfx.ImmediateContext->RSSetViewports(1, &vp);
        gfx.ImmediateContext->OMSetRenderTargets(1, &gfx.RTV, NULL);

        if (pDefaultLayout)
        {
            gfx.ImmediateContext->IASetInputLayout(pDefaultLayout);
        }
        gfx.ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (int i = 0; i < (int)world.size(); i++)
        {
            if (world[i] != nullptr)
            {
                world[i]->Render(&gfx);
            }
        }
        gfx.SwapChain->Present(gfx.VSync, 0);
    }

    void Run()
    {
        MSG msg = {};
        while (msg.message != WM_QUIT && isRunning)
        {
            if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg); DispatchMessage(&msg);
            }
            else
            {
                Input();
                Update();
                Render();
            }
        }
    }
};

LRESULT CALLBACK GlobalWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS)
{
    GameLoop gEngine;
    gEngine.Initialize(hI, GlobalWndProc);
    auto& gfx = GameSystem::GetInstance()->GetGfx();

    std::string triShader = R"(
        cbuffer cb0 : register(b0) { matrix matWorld; };
        struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
        PS_IN VS(VS_IN input) {
            PS_IN output;
            output.pos = mul(float4(input.pos, 1.0f), matWorld);
            output.col = input.col;
            return output;
        }
        float4 PS(PS_IN input) : SV_Target { return input.col; }
    )";;
    ID3DBlob* vsBlob = gfx.CompileShader(triShader, "VS", "vs_5_0");
    ID3DBlob* psBlob = gfx.CompileShader(triShader, "PS", "ps_5_0");


    // ====================================================
    //  황금별 (Player)
    // ====================================================
    Mesh* goldMesh = new Mesh();
    goldMesh->color = { 1.0f, 0.85f, 0.0f, 1.0f }; // 황금색
    goldMesh->vertexCount = 30;

    float outerR = 0.5f; float innerR = 0.2f;
    XMFLOAT3 p[10];
    for (int i = 0; i < 10; ++i)
    {
        float r;

        if (i % 2 == 0)
        {
            r = outerR; // 바깥쪽 반지름(outerR)을 대입
        }
        else
        {
            r = innerR; // 안쪽 반지름(innerR)을 대입
        }
        float angle = XM_PIDIV2 - (i * XM_2PI / 10.0f);
        p[i] = { cosf(angle) * r, sinf(angle) * r, 0.0f };
    }

    //정석대로 그리기
    std::vector<Vertex> vGold;
    for (int i = 0; i < 10; i++)
    {
        vGold.push_back({ {0,0,0}, goldMesh->color });
        vGold.push_back({ p[i], goldMesh->color });
        vGold.push_back({ p[(i + 1) % 10], goldMesh->color });
    }

    // 리소스 생성 (황금별용)
    gfx.Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &goldMesh->pVS);
    gfx.Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &goldMesh->pPS);

    D3D11_BUFFER_DESC bd = { 0 };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * (UINT)vGold.size();
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { vGold.data() };
    gfx.Device->CreateBuffer(&bd, &sd, &goldMesh->vBuffer);

    D3D11_INPUT_ELEMENT_DESC ied[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    gfx.Device->CreateInputLayout(ied, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &goldMesh->pInputLayout);


    // 황금별 객체 등록 (PlayerController 포함)
    GameObject* gStar = new GameObject(0, 0, 0);
    gStar->scale = { 0.5f, 0.5f, 1.0f };

    // 기존 PlayerController 대신 StarController 사용
    gStar->AddComponent(new MeshRenderer(goldMesh));
    gStar->AddComponent(new PlayerController());

    gEngine.world.push_back(gStar);

    // ====================================================
    // 추가되는 랜덤 별 n개 (Background Stars)
    // ====================================================
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> disPos(-1.2f, 1.2f);
    std::uniform_real_distribution<float> disCol(0.3f, 0.9f);
    std::uniform_real_distribution<float> disScale(0.05f, 0.4f); // 최대 0.5 (화면 1/4)

    int n = 20; // 추가할 별 개수
    for (int k = 0; k < n; k++)
    {
        Mesh* randMesh = new Mesh();
        randMesh->color = { disCol(gen), disCol(gen), disCol(gen), 1.0f };
        randMesh->vertexCount = 30;

        std::vector<Vertex> vRand;
        for (int i = 0; i < 10; i++) {
            vRand.push_back({ {0,0,0}, randMesh->color });
            vRand.push_back({ p[i], randMesh->color });
            vRand.push_back({ p[(i + 1) % 10], randMesh->color });
        }

        // 리소스 생성 (각 별마다 고유 색상 버퍼 생성)
        gfx.Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &randMesh->pVS);
        gfx.Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &randMesh->pPS);

        gfx.Device->CreateInputLayout(ied, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &randMesh->pInputLayout);

        bd.ByteWidth = sizeof(Vertex) * (UINT)vRand.size();
        sd.pSysMem = vRand.data();
        gfx.Device->CreateBuffer(&bd, &sd, &randMesh->vBuffer);

        GameObject* bgStar = new GameObject(disPos(gen), disPos(gen), 0);
        float s = disScale(gen);
        bgStar->scale = { s, s, 1.0f };

        // 1. 렌더러 추가
        bgStar->AddComponent(new MeshRenderer(randMesh));

        // [핵심 추가] 모든 랜덤 별에게도 컨트롤러를 달아줍니다!
        // 이제 이 별들도 키보드 입력에 반응하며, 각자의 s 값에 따라 속도가 결정됩니다.
        bgStar->AddComponent(new PlayerController());

        gEngine.world.push_back(bgStar);
    }

    vsBlob->Release(); psBlob->Release();


    gEngine.Run();

    GameSystem::Release();

    return 0;
}
#endif