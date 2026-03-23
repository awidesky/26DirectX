#include <windows.h>
#include <stdio.h>

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
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // 3개 들어가니 size는 12바이트
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
    vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

    // 4. 정점 버퍼 생성, 삼각형 두개를 그린다.
    struct {
        float x, y, z;
    } relativeVertexPos[6] = { // x값은 0.5(길이) * cos30'
        {  0.4330127f,  0.25f * aspect, 0.5f },   // 30°
        {  0.0f,       -0.5f  * aspect,  0.5f },   // 270°
        { -0.4330127f,  0.25f * aspect, 0.5f },   // 150°
        { -0.4330127f, -0.25f * aspect, 0.5f },   // 210°
        {  0.0f,        0.5f  * aspect,  0.5f },   // 90°
        {  0.4330127f, -0.25f * aspect, 0.5f }    // 330°
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
            TranslateMessage(&msg);
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