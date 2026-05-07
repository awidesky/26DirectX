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

/*
 * [강의 1: 셰이더 컴파일과 재활용 - "요리법은 한 번만 읽어라"]
 *
 * 1. 런타임 컴파일 (D3DCompile):
 *    - 프로그램 실행 중에 .hlsl 소스 코드를 읽어 GPU 기계어로 바꾸는 과정.
 *    - 매우 무거운 작업이므로, 매 프레임 호출하면 게임이 멈춤(Stuttering).
 *
 * 2. 리소스 캐싱 (Caching):
 *    - 한 번 컴파일된 결과물(Blob)이나 Shader 객체는 메모리에 저장해두고 재사용함.
 *    - "별이 100개라고 셰이더를 100번 굽지 않는다."
 *
 * 3. 공유 (Sharing):
 *    - 동일한 소스 코드를 사용하는 객체들은 컴파일된 셰이더의 '포인터'만 나눠 가짐.
 * 
 * CompileAndCreate() 참조
 * 
 * --------------------------------------------------------------------------------------
 * [강의 2: 컴파일된 파일(.CSO) - "미리 구워둔(pre-baked) 빵"]
 *
 * 1. HLSL (Source Code): 텍스트 파일. 읽기 쉽지만 실행 시 컴파일 비용 발생.
 * 2. CSO (Compiled Shader Object): 바이너리 파일. GPU가 바로 이해할 수 있는 형태.
 *
 * [실무의 흐름]
 * - 개발 단계: 코드 수정이 잦으므로 소스(.hlsl)를 직접 컴파일함.
 * - 배포 단계: 빌드 시점에 미리 컴파일해서 .cso 파일만 배포함 (보안 및 속도 우위).
 * - 코드: D3DReadFileToBlob()을 사용해 컴파일 과정 없이 즉시 리소스 생성 가능.
 *
 * --------------------------------------------------------------------------------------
 * [강의 3: Mesh Renderer의 역할 - "접시와 서빙"]
 *
 * 1. 역할: '무엇을(Mesh)' '어떻게(Material)' 그릴지 결정하고 실행함.
 * 2. 데이터 바인딩:
 *    - 정점 버퍼(VB)를 슬롯에 꽂고, 상수 버퍼(CB)를 통해 월드 행렬을 전송함.
 * 3. 렌더링 파이프라인 제어:
 *    - IASetInputLayout, VSSetShader, PSSetShader 등을 호출하여 GPU의 상태를 설정함.
 *    - "렌더러는 요리사가 아니라, 손님 앞에 요리를 내놓는 서빙 담당자다."
 *
 * --------------------------------------------------------------------------------------
 * [강의 4: Material - "요리법(Shader)과 재료(Data)의 결합"]
 *
 * 1. 정의: 셰이더(코드) + 파라미터(데이터, 색상, 텍스처 등)의 묶음.
 * 2. 왜 만드는가?:
 *    - 셰이더 코드는 같지만 색깔만 다른 객체들을 효율적으로 관리하기 위함.
 *    - 예: '별 셰이더'는 하나지만, '황금 머티리얼'과 '빨간 머티리얼'은 데이터만 다름.
 * 3. 독립성: 렌더러가 셰이더의 세부 사항을 몰라도 머티리얼만 갈아 끼우면 모습이 바뀜.
 *
 * --------------------------------------------------------------------------------------
 * [강의 5: 다형성(Polymorphism) - "표준화된 인터페이스"]
 *
 * 1. Base Material (추상 클래스): 모든 머티리얼이 지켜야 할 약속(Bind() 함수) 정의.
 * 2. 상속 (ColorMaterial, TextureMaterial): 각자 필요한 데이터를 GPU 슬롯에 꽂음.
 *
 * [이득]
 * - 확장성: 새로운 효과가 필요하면 기존 코드를 건드리지 않고 새로운 머티리얼 클래스만 추가.
 * - 단순화: MeshRenderer는 부모 타입인 Material*만 들고 있으면 됨.
 *   그게 색상용인지 텍스처용인지 몰라도 Bind()만 호출하면 알아서 그려짐.
 * 
 * --------------------------------------------------------------------------------------
 * [강의 6: 소유권과 실행의 분리 - "누가 무엇을 들고 있는가?"]
 * 
 * 1. Mesh (Resource Owner): 
 *    - 정점 버퍼(VB)를 소유함. 
 *    - 자기 데이터의 크기와 개수를 알고 있음.
 * 
 * 2. Material (Resource Owner): 
 *    - 셰이더(VS, PS)와 입력 레이아웃(IL)을 소유함. 
 *    - 데이터를 어떻게 해석할지(Layout) 알고 있음.
 * 
 * 3. MeshRenderer (Executor): 
 *    - Mesh와 Material을 인자로 받아 조립함.
 *    - 소유권은 없으며, 매 프레임 GPU에게 "이걸로(Material) 이걸(Mesh) 그려라"라고 명령함. 
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

/*
 * [Mesh - "자기 몸은 자기가 만든다"]
 * - Vertex Buffer는 메쉬의 물리적인 몸체다.
 * - 메쉬는 정점 데이터를 받아서 GPU 메모리에 넣는 법을 스스로 알고 있어야 한다.
 * - Create()는 로딩 시점에 단 한 번만 호출한다.
 */


//기본 쉐이더 셋
struct ShaderSet {
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;

    ShaderSet() = default;

    // 생성자에서 초기화하기 편하게 추가
    ShaderSet(ID3D11VertexShader* v, ID3D11PixelShader* p, ID3D11InputLayout* l)
        : vs(v), ps(p), layout(l) 
    {
    }

    // * 소멸자에서 안전하게 해제
    void Release() 
    {
        if (vs)     { vs->Release(); vs = nullptr; }
        if (ps)     { ps->Release(); ps = nullptr; }
        if (layout) { layout->Release(); layout = nullptr; }
    }
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

    // 실제 컴파일 로직을 담당하는 내부 함수
    ShaderSet CompileAndCreate(const void* source, size_t length, bool isFile, D3D11_INPUT_ELEMENT_DESC* ied, UINT iedCount)
    {
        ShaderSet res;
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errBlob = nullptr;

        HRESULT hr;
        if (isFile) 
        {
            // 파일에서 읽기
            hr = D3DCompileFromFile((LPCWSTR)source, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
            hr = D3DCompileFromFile((LPCWSTR)source, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &errBlob);
        }
        else 
        {
            // 메모리(String)에서 읽기
            hr = D3DCompile(source, length, nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
            hr = D3DCompile(source, length, nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &errBlob);
        }

        if (FAILED(hr)) 
        {
            if (errBlob) 
            {
                OutputDebugStringA((char*)errBlob->GetBufferPointer());
                errBlob->Release();
            }
            return res;
        }

        // GPU 리소스 생성
        Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &res.vs);
        Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &res.ps);

        // 매개변수로 받은 ied와 iedCount를 사용해서 레이아웃 생성!
        if (vsBlob && ied)
        {
            Device->CreateInputLayout(ied, iedCount, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &res.layout);
        }        

        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();

        return res;
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
public:
    ID3D11Buffer* vBuffer;
    UINT vertexCount;

    Mesh()
    {
        vBuffer = nullptr;
        vertexCount = 0;
    }

    ~Mesh()
    {
        if (vBuffer)
        {
            vBuffer->Release();
            vBuffer = nullptr;
        }
    }

    // [핵심] 외부에서 정점 벡터를 던져주면 스스로 GPU 버퍼를 생성함
    void Create(GraphicsContext* gfx, const std::vector<Vertex>& vertices)
    {
        vertexCount = (UINT)vertices.size();

        D3D11_BUFFER_DESC bd = { 0 };
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(Vertex) * vertexCount;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd = { 0 };
        sd.pSysMem = vertices.data();

        gfx->Device->CreateBuffer(&bd, &sd, &vBuffer);
    }
};

class Material {
public:
    ShaderSet shaders; // 모든 머티리얼은 셰이더를 가짐


    Material(ShaderSet s) : shaders(s) {}
    virtual ~Material() {}

    // 이 머티리얼이 가진 셰이더와 파라미터를 GPU 슬롯에 꽂는 함수
    virtual void Bind(ID3D11DeviceContext* context) = 0;
};

// 픽셀 셰이더에서 쓸 색상 상수 버퍼 구조체
struct ColorBuffer 
{
    XMFLOAT4 tintColor;
};

class ColorMaterial : public Material {

public:

    XMFLOAT4 color;
    ID3D11Buffer* pColorBuffer = nullptr; // 색상 전송용 상수 버퍼


    ColorMaterial(ShaderSet s, XMFLOAT4 col, ID3D11Device* device)
        : Material(s), color(col)
    {
        // 색상 정보를 담을 전용 상수 버퍼 생성 (b1 슬롯용)
        D3D11_BUFFER_DESC cbd = { 0 };
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ColorBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        device->CreateBuffer(&cbd, nullptr, &pColorBuffer);
    }

    virtual ~ColorMaterial()
    {
        if (pColorBuffer) pColorBuffer->Release();
    }

    // 색상을 실시간으로 바꿀 수 있게 제공 (애니메이션용)
    void SetColor(XMFLOAT4 col) { color = col; }

    void Bind(ID3D11DeviceContext* context) override 
    {
        // 1. 셰이더 및 레이아웃 바인딩 (공통)
        context->IASetInputLayout(shaders.layout);
        context->VSSetShader(shaders.vs, nullptr, 0);
        context->PSSetShader(shaders.ps, nullptr, 0);

        // 2. 머티리얼 고유의 색상 데이터 업데이트 (b1 슬롯에 꽂기)
        ColorBuffer cb = { color };
        context->UpdateSubresource(pColorBuffer, 0, nullptr, &cb, 0, 0);

        // Pixel Shader의 1번 슬롯(b1)에 색상 버퍼를 꽂음
        context->PSSetConstantBuffers(1, 1, &pColorBuffer);
    }
};



class MeshRenderer : public Component
{
public:
    Mesh* pMeshData = nullptr;
    ID3D11Buffer* cBuffer = nullptr;
    Material* pMaterial; // 다형성(Polymorphism) 활용!

    MeshRenderer(Mesh* mesh, Material* mat) : Component()
    {
        pMeshData = mesh;
        pMaterial = mat;
        cBuffer = nullptr;
    }

    MeshRenderer(Mesh* mesh)
    {
        
        
    }

    ~MeshRenderer()
    {
        if (cBuffer)
        {
            cBuffer->Release();
            cBuffer = nullptr;
        }
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
        if (!pMeshData || !pMaterial) return;

        // 1. 머티리얼 형님한테 "네가 알아서 셰이더랑 색상 다 꽂아라"라고 시킴
        pMaterial->Bind(gfx->ImmediateContext);

        // 2. World 변환 행렬 업데이트 (b0 슬롯 - 이건 객체마다 다르니 여기서 처리)
        float s = 1.0f / (pOwner->pos.z + 1.0f);
        XMMATRIX world = XMMatrixScaling(s * pOwner->scale.x, s * pOwner->scale.y, 1.0f) *
            XMMatrixRotationZ(pOwner->rot.z) *
            XMMatrixTranslation(pOwner->pos.x, pOwner->pos.y, 0.0f);

        ConstantBuffer cb;
        cb.matWorld = XMMatrixTranspose(world);
        gfx->ImmediateContext->UpdateSubresource(cBuffer, 0, nullptr, &cb, 0, 0);
        gfx->ImmediateContext->VSSetConstantBuffers(0, 1, &cBuffer);

        // 3. 그리기
        UINT stride = sizeof(Vertex), offset = 0;
        gfx->ImmediateContext->IASetVertexBuffers(0, 1, &pMeshData->vBuffer, &stride, &offset);
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
        win.Initialize(hInst, 800, 600, wndProc);
        gfx.InitDX(win.hWnd, 800, 600);
    }

    void Input()
    {
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
        cbuffer cbWorld    : register(b0) { matrix matWorld; };
        cbuffer cbMaterial : register(b1) { float4 tintColor; }; // 머티리얼 색상 추가!

        struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
        
        PS_IN VS(VS_IN input) 
        {
            PS_IN output;
            output.pos = mul(float4(input.pos, 1.0f), matWorld);
            output.col = input.col; 
            return output;
        }

        // input.col(정점 색상) 무시하고 tintColor(머티리얼 색상)를 출력
        float4 PS(PS_IN input) : SV_Target 
        { 
            return tintColor; 
        }
    )";



    //별그리기
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
        vGold.push_back({ {0,0,0}, { 0, 0, 0, 0 } });
        vGold.push_back({ p[i], { 0, 0, 0, 0 } });
        vGold.push_back({ p[(i + 1) % 10], { 0, 0, 0, 0 } });
    }

    D3D11_INPUT_ELEMENT_DESC ied[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    Mesh* gMesh = new Mesh();
    gMesh->vertexCount = 30;

    gMesh->Create(&gEngine.gfx, vGold);

    // 1. 셰이더 딱 한 번만 구워두기 (재사용!)
    //ShaderSet starShaders = gEngine.gfx.CreateShaderFromMemory(triShader);
    ShaderSet starShaders = gEngine.gfx.CompileAndCreate(triShader.c_str(), triShader.length(), false, ied, 2);

    // 2. 머티리얼 딱 두 종류만 만들기 (붕어빵 틀)
    ColorMaterial* goldMat = new ColorMaterial(starShaders, { 1, 0.8f, 0, 1 }, gEngine.gfx.Device);
    ColorMaterial* redMat = new ColorMaterial(starShaders, { 1, 0, 0, 1 }, gEngine.gfx.Device);


    // 시드값 준비 (진짜 무작위성을 위해 하드웨어에서 값을 가져옴)
    std::random_device rd;

    // 난수 엔진 생성 (gen) - "숫자를 마구 뿜어내는 기계"
    std::mt19937 gen(rd());

    // 분포기 설정 (dis) - "뿜어져 나온 숫자를 0.0 ~ 1.0 사이로 골고루 펴주는 기계"
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    // 실행 (dis(gen))
    float randomValue = dis(gen); // "기계(gen)를 가동해서 결과물을 분포기(dis)에 통과시켜라"

    // 3. 별들 생성 (데이터는 공유, 상태는 개별)
    for (int i = 0; i < 20; i++) 
    {
        GameObject* star = new GameObject(dis(gen), dis(gen), 0);

        // 같은 셰이더를 쓰는 머티리얼을 컴포넌트에 장착
        star->AddComponent(new MeshRenderer(gMesh, (i % 2 == 0) ? goldMat : redMat));
        star->AddComponent(new PlayerController());

        gEngine.world.push_back(star);
    }



    gEngine.Run();


    // [중요: 공유 리소스 수동 해제]
    // 렌더러들이 이미 world 소멸 시점에 사라졌으므로, 이제 안전하게 리소스를 지울 수 있음.
    if (goldMat) { delete goldMat; goldMat = nullptr; }
    if (redMat) { delete redMat;  redMat = nullptr; }

    // 셰이더 세트도 릴리즈 (이건 수동으로 Release 호출해줘야 함)
    starShaders.Release();

    // 메쉬 데이터 해제
    if (gMesh) { delete gMesh; gMesh = nullptr; }

    return 0;
}