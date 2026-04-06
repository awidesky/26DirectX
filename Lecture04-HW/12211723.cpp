/*
================================================================================
 [가이드: 게임 엔진의 뼈대 만들기]
================================================================================
 1. Component (기능): 캐릭터가 할 수 있는 '일' (이동, 시간 재기 등)
 2. GameObject (객체): 게임에 존재하는 '물체' (플레이어, 타이머 등)
 3. GameWorld (세계): 모든 물체를 담고 있는 '바구니'

 * 구조: Component -> GameObject -> GameWorld 순으로 확장됨.
         (루프 한 번 돌 때 [입력 -> 업데이트 -> 렌더링] 순서로 모든 객체를 훑음.)
 [작동 원리]
 - Start(): 물체가 태어날 때 딱 한 번 실행되는 초기화 코드
 - Input(): 키보드/마우스 상태를 확인.
 - Update(): 수치(좌표 등)를 계산.
 - Render(): 화면에 결과를 출력.

================================================================================
*/

#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <vector>
#include <string>
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
struct VideoConfig {
    int Width = 800;
    int Height = 600;
    bool IsFullscreen = false;
} g_Config;
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

ID3D11Device* g_pd3dDevice = nullptr;          // 리소스 생성자 (공장)
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // 그리기 명령 수행 (일꾼)
IDXGISwapChain* g_pSwapChain = nullptr;          // 화면 전환 (더블 버퍼링)
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // 그림을 그릴 도화지(View)


// 콘솔의 특정 좌표로 커서를 이동시키는 함수
void MoveCursor(int x, int y)
{
    // \033[y;xH  (y와 x는 1부터 시작함)
    printf("\033[%d;%dH", y, x);
}

// [1단계: 컴포넌트 기저 클래스]
// 모든 기능(이동, 렌더링 등)은 이 클래스를 상속받아야 함.
class Component
{
public:
    class GameObject* pOwner = nullptr; // 이 기능이 누구의 것인지 저장
    bool isStarted = 0;           // Start()가 실행되었는지 체크

    virtual void Start() = 0;              // 초기화
    virtual void Input() {}                // 입력 (선택사항)
    virtual void Update(float dt) = 0;     // 로직 (필수)
    virtual void Render() {}               // 그리기 (선택사항)

    virtual ~Component() {}
};

// [2단계: 게임 오브젝트 클래스]
// 컴포넌트들을 담는 바구니 역할을 함.
class GameObject {
public:
    std::string name;
    float x, y;
    std::vector<Component*> components;

    GameObject(std::string n, float x = 0.0f, float y = 0.0f) : name(n), x(x), y(y) {}

    // 객체가 죽을 때 담고 있던 컴포넌트들도 메모리에서 해제함
    ~GameObject() {
        for (int i = 0; i < (int)components.size(); i++)
        {
            delete components[i];
        }
    }

    // 새로운 기능을 추가하는 함수
    void AddComponent(Component* pComp)
    {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

// --- [3단계: 실제 구현할 기능 컴포넌트들] ---

// 기능 1: 플레이어 조종 및 이동
class TriangleControl : public Component {
public:
    float speed = 10.0f;
    bool wasd;
    bool moveUp, moveDown, moveLeft, moveRight;
    Vertex vertices[3], initpos[3] = {
        {  0.0f,  0.3f, 0.3f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.3f, -0.3f, 0.3f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.3f, -0.3f, 0.3f, 0.0f, 0.0f, 1.0f, 1.0f },
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA bufferData = { vertices, 0, 0 };
    ID3D11Buffer* pVBuffer = nullptr;
    
	TriangleControl(bool wasd) : wasd(wasd) {
        moveUp = moveDown = moveLeft = moveRight = false;
    }
    ~TriangleControl() {
        if (pVBuffer) pVBuffer->Release();
    }

    void Start() override {
        if (wasd) {
            initpos[0].b = 1.0f;
            initpos[1].r = 1.0f;
            initpos[2].g = 1.0f;
        }
        for (int i = 0; i < 3; i++) {
            vertices[i] = initpos[i];
            vertices[i].x += pOwner->x;
            vertices[i].y += pOwner->y;
        }
        g_pd3dDevice->CreateBuffer(&bd, &bufferData, &pVBuffer);
        printf("[%s] TriangleControl 기능 시작!\n", pOwner->name.c_str());
    }

    // [입력 단계] 키 상태만 체크함
    void Input() override
    {
        moveUp = (GetAsyncKeyState(wasd ? 'W' : VK_UP) & 0x8000);
        moveDown = (GetAsyncKeyState(wasd ? 'S' : VK_DOWN) & 0x8000);
        moveLeft = (GetAsyncKeyState(wasd ? 'A' : VK_LEFT) & 0x8000);
        moveRight = (GetAsyncKeyState(wasd ? 'D' : VK_RIGHT) & 0x8000);
    }

    // [업데이트 단계] 체크된 키 상태로 좌표만 계산함
    void Update(float dt) override
    {
        if (moveUp)    pOwner->y += speed * dt;
        if (moveDown)  pOwner->y -= speed * dt;
        if (moveLeft)  pOwner->x -= speed * dt;
        if (moveRight) pOwner->x += speed * dt;

        // 화면의 아예 바깥으로 나가지 못하게
        pOwner->x = max(-1.0f, min(1.0f, pOwner->x));
        pOwner->y = max(-1.0f, min(1.0f, pOwner->y));
    }

    // [렌더링 단계] 계산된 좌표를 화면에 그림
    void Render() override
    {
        if (moveUp || moveDown || moveLeft || moveRight) { //위치 변경 없으면 굳이 버퍼 새로 만들지 않는다
            for (int i = 0; i < sizeof(vertices) / sizeof(Vertex); i++) {
                vertices[i].x = initpos[i].x + pOwner->x;
                vertices[i].y = initpos[i].y + pOwner->y;
            }

            // 먼저 만든 거 지우고
            if (pVBuffer) pVBuffer->Release();
            // 버퍼 속성과 데이터의 주소는 바뀌지 않으니, bd도 initData도 바뀌지 않는다.
            g_pd3dDevice->CreateBuffer(&bd, &bufferData, &pVBuffer);
        }

        UINT stride = sizeof(Vertex), offset = 0;
        g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

        // 최종 그리기, 버텍스 3개
        g_pImmediateContext->Draw(3, 0);
    }
};

// 기능 2: 시스템 정보 출력 및 윈도우 관련 설정
class InfoDisplay : public Component {
public:
    float measuredTime = 0.0f;
    int frames = 0;

    void Start() override
    {
        measuredTime = 0.0f;
        printf("[%s] InfoDisplay 기능 시작!\n", pOwner->name.c_str());
    }

    void Input() {
        // [입력 처리: GetAsyncKeyState의 0x0001 플래그로 1회성 입력 감지]
        if (GetAsyncKeyState('F') & 0x0001) {
            g_Config.IsFullscreen = !g_Config.IsFullscreen;
            g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
        }
    }

    void Update(float dt) override { measuredTime += dt; }

    void Render() override {
        // 화면 최상단에 정보 출력
        frames++;
        if (measuredTime > 3) {
            printf("=======================================\n");
            printf("fps: %.2f\n", frames / measuredTime);
            printf("Control: W, A, S, D, Arrows | Exit: ESC\n");
            printf("=======================================\n");
            measuredTime = 0.0f;
            frames = 0;
        }
    }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

class GameLoop {
public:
    bool isRunning = false; //before init
    std::vector<GameObject*> gameWorld;
    std::chrono::high_resolution_clock::time_point prevTime;
    float deltaTime = 0.0f;   //delta time;

    HWND hWnd = nullptr;
    ID3D11VertexShader* vShader = nullptr;
    ID3D11PixelShader* pShader = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;

    //초기화
    void Initialize(HINSTANCE hInstance, HINSTANCE hPrevInstance, int nCmdShow)
    {
        //초기화시 동작준비됨
        isRunning = true;

        gameWorld.clear();

        // --- [1. 윈도우 생성 단계] ---
        WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.lpszClassName = L"DX11EngineClass";
        RegisterClassExW(&wcex);

        // [중요] AdjustWindowRect: 우리가 원하는 '그림 영역'이 g_Config 크기가 되도록 
        // 타이틀바와 테두리 두께를 계산하여 전체 창 크기를 역산함.
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        hWnd = CreateWindowW(L"DX11EngineClass", L"과제: 컴포넌트 기반 게임 오브젝트 시스템 구현 (12211723 홍성민)",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, // 계산된 창 크기 적용
            nullptr, nullptr, hInstance, nullptr);

        ShowWindow(hWnd, nCmdShow);

        // --- [DX11 리소스 생성 단계] ---
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        // [중요] 스왑체인의 버퍼 크기는 곧 '백버퍼'의 물리 해상도임.
        // 중앙 설정값(g_Config)을 사용하여 데이터 일관성을 유지함.
        sd.BufferDesc.Width = g_Config.Width;
        sd.BufferDesc.Height = g_Config.Height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;


        // GPU와 통신할 통로(Device)와 화면(SwapChain)을 생성함.
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

        g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
        g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

        /*
         typedef struct D3D11_INPUT_ELEMENT_DESC {
             LPCSTR                     SemanticName;         // 1. 의미 (이름)
             UINT                       SemanticIndex;        // 2. 번호 (인덱스)
             DXGI_FORMAT                Format;               // 3. 데이터 형식 (크기/타입)
             UINT                       InputSlot;            // 4. 입력 슬롯 (통로)
             UINT                       AlignedByteOffset;    // 5. 오프셋 (시작 지점)
             D3D11_INPUT_CLASSIFICATION InputSlotClass;       // 6. 클래스 (데이터 성격)
             UINT                       InstanceDataStepRate; // 7. 인스턴싱 스텝
         } D3D11_INPUT_ELEMENT_DESC;
         */
         //정점의 데이터 형식을 정의 (IA 단계에 알려줌)
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
        vsBlob->Release(); psBlob->Release(); // 컴파일용 임시 메모리 해제

        // 시간 측정 준비
        prevTime = std::chrono::high_resolution_clock::now();
        deltaTime = 0.0f;
    }

    void Input()
    {
        // esc 누르면 종료
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;

        // B. 입력 단계 (Input Phase)
        for (int i = 0; i < (int)gameWorld.size(); i++)
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
                gameWorld[i]->components[j]->Input();
    }

    void Update() {
        // C. 스타트 실행
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                // Start()가 호출된 적 없다면 여기서 호출 (유니티 방식)
                if (gameWorld[i]->components[j]->isStarted == false) {
                    gameWorld[i]->components[j]->Start();
                    gameWorld[i]->components[j]->isStarted = true;
                }
            }
        }

        // D. 업데이트 단계 (Update Phase)
        for (int i = 0; i < (int)gameWorld.size(); i++)
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
                gameWorld[i]->components[j]->Update(deltaTime);
    }

    void Render()
    {
        // E. 렌더링 단계 (Render Phase)
        float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

        // [중요] 뷰포트 설정
        // 뷰포트는 백버퍼라는 메모리 공간 중 '어디에' 픽셀을 채울지 결정함.
        // 일반적으로 백버퍼 전체를 사용하도록 g_Config 값을 그대로 할당함.
        // 만약 멀티뷰(분할화면)를 한다면 이 Width/Height를 절반으로 줄여서 사용함.

        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;                                //뷰포트 시작 X 좌표
        vp.TopLeftY = 0;                                // 뷰포트 시작 Y 좌표
        vp.Width = (float)g_Config.Width;               // 뷰포트 가로 폭
        vp.Height = (float)g_Config.Height;             // 뷰포트 세로 폭
        vp.MinDepth = 0.0f;                             // 깊이 버퍼의 최소값 (0.0 ~ 1.0)
        vp.MaxDepth = 1.0f;                             // 깊이 버퍼의 최대값
        g_pImmediateContext->RSSetViewports(1, &vp);

        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

        g_pImmediateContext->IASetInputLayout(pInputLayout);

        // Primitive Topology 설정: 삼각형 리스트로 연결하라!
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(pShader, nullptr, 0);


        // # 각 컴포넌트 먼저 그리기
		for (int i = 0; i < (int)gameWorld.size(); i++) {
			for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
				gameWorld[i]->components[j]->Render();
			}
		}

        // [중요] Present의 역할
        // 다 그려진 백버퍼(g_Config 크기)를 실제 윈도우 창으로 전송함.
        // 윈도우 창 크기와 백버퍼 크기가 다르면 여기서 '강제 스케일링'이 발생하여 화질이 깨짐.
        g_pSwapChain->Present(0, 0);
        /* Present(a,b)
        * 첫 번째 인자 (a): SyncInterval (V-Sync 설정)
        *      - 0:     즉시 출력. 모니터가 화면을 갱신하든 말든 상관없이 다 그렸으면 바로 화면을 교체함. (FPS가 무제한으로 올라가며 화면 찢어짐 발생 가능)
        *      - 1~4:   수직 동기화(V-Sync) 활성화.
        *               1은 모니터 주사율(60Hz 등)에 맞춰서 기다렸다가 화면 스왑.
        *               2/3/4 는 모니터가 화면을 2/3/4번 그릴 때마다 스왑
        * 두 번째 인자 (b): Flags (출력 옵션)
        *      - 0:                         일반적인 출력임.
        *        DXGI_PRESENT_TEST:         실제로 화면을 바꾸지는 않고, 장치가 준비되었는지 테스트만 할 때 씀.
        *        DXGI_PRESENT_DO_NOT_WAIT:  만약 GPU가 이전 프레임을 처리하느라 바쁘면 기다리지 않고 바로 에러를 뱉게 함.
        */
    }



    void Run()
    {
        // --- [무한 게임 루프] ---
        MSG msg = {};
        while (msg.message != WM_QUIT && isRunning) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    isRunning = false;
                    continue;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // A. 시간 관리 (DeltaTime 계산)
            std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - prevTime;
            deltaTime = elapsed.count();
            prevTime = currentTime;

            Input();
            Update();
            Render();

            // CPU 과부하 방지 (약 60~100 FPS 유지 시도)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    GameLoop() {
        // # 어차피 gLoop.Initialize를 호출하는데, 생성자 호출 단계에서 또 호출할 필요 없다
        //Initialize();
    }
    ~GameLoop()
    {
        // [정리] 메모리 해제
        for (int i = 0; i < (int)gameWorld.size(); i++)
            delete gameWorld[i]; // GameObject 소멸자가 컴포넌트들도 지움

        // [정리]
        if (pInputLayout) pInputLayout->Release();
        if (vShader) vShader->Release();
        if (pShader) pShader->Release();
        if (g_pRenderTargetView) g_pRenderTargetView->Release();
        if (g_pSwapChain) g_pSwapChain->Release();
        if (g_pImmediateContext) g_pImmediateContext->Release();
        if (g_pd3dDevice) g_pd3dDevice->Release();
    }

};

// --- [4단계: 메인 엔진 루프] ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    //게임루프
    GameLoop gLoop;
    gLoop.Initialize(hInstance, hPrevInstance, nCmdShow);

    // 시스템 정보 객체 조립
    GameObject* sysInfo = new GameObject("SystemManager");
    InfoDisplay* pInfo = new InfoDisplay();
    sysInfo->AddComponent(pInfo);
    gLoop.gameWorld.push_back(sysInfo);

    // 플레이어 객체 조립
    GameObject* player1 = new GameObject("Player1", 0.8f, 0.8f);
    TriangleControl* pControl1 = new TriangleControl(0);
    player1->AddComponent(pControl1);
    gLoop.gameWorld.push_back(player1);
    GameObject* player2 = new GameObject("Player2", -0.8f, -0.8f);
    TriangleControl* pControl2 = new TriangleControl(1);
    player2->AddComponent(pControl2);
    gLoop.gameWorld.push_back(player2);

    //게임루프 실행
    gLoop.Run();

    return 0;
}