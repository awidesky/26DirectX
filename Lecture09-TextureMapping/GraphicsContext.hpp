#pragma once
#include "Framework.hpp"

class GraphicsContext 
{
public:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* ImmediateContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
    bool IsFullscreen = false;
    int VSync = 1;

    ~GraphicsContext() 
    {
        if (RTV) RTV->Release();
        if (SwapChain) SwapChain->Release();
        if (ImmediateContext) ImmediateContext->Release();
        if (Device) Device->Release();
    }

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
        if (RTV) { RTV->Release(); RTV = nullptr; }
        SwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        CreateRTV(w, h);
    }

    void SetFullscreen(bool goFull) 
    {
        IsFullscreen = goFull;
        SwapChain->SetFullscreenState(goFull, NULL);
    }





    ShaderSet CompileAndCreate(const void* source, size_t length, bool isFile, D3D11_INPUT_ELEMENT_DESC* ied, UINT iedCount)
    {
        ShaderSet res;
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errBlob = nullptr;
        HRESULT hr;

        if (isFile)
        {
            // VS 컴파일
            hr = D3DCompileFromFile((LPCWSTR)source, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
            if (FAILED(hr))
            {
                HandleCompileError(hr, errBlob, L"VS");
                if (vsBlob) vsBlob->Release();
                return res;
            }

            // PS 컴파일
            hr = D3DCompileFromFile((LPCWSTR)source, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &errBlob);
            if (FAILED(hr))
            {
                HandleCompileError(hr, errBlob, L"PS");
                if (psBlob) psBlob->Release();
                return res;
            }
        }
        else
        {
            // 메모리 소스 VS 컴파일
            hr = D3DCompile(source, length, nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
            if (FAILED(hr))
            {
                HandleCompileError(hr, errBlob, L"VS");
                if (vsBlob) vsBlob->Release();
                return res;
            }

            // 메모리 소스 PS 컴파일
            hr = D3DCompile(source, length, nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &errBlob);
            if (FAILED(hr))
            {
                HandleCompileError(hr, errBlob, L"PS");
                if (psBlob) psBlob->Release();
                return res;
            }
        }

        // 리소스 생성
        Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &res.vs);
        Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &res.ps);

        if (vsBlob && ied)
        {
            Device->CreateInputLayout(ied, iedCount, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &res.layout);
        }

        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();

        return res;
    }

    /**
 * [셰이더 로딩 전략]
 * 1. .cso 파일 존재 여부 확인 -> 있으면 D3DReadFileToBlob으로 즉시 로드 (성능 최적화)
 * 2. .cso 없음 -> .hlsl 파일 확인 -> D3DCompileFromFile로 실시간 빌드 (유연성)
 * 3. 둘 다 없음 -> 에러 메시지 출력 후 빈 구조체 반환
 */
 /**
* [Vertex Shader 전용 로더]
* - CSO 우선 시도 후 HLSL 컴파일
* - Input Layout 생성 포함
*/
    void LoadVertexShader(ShaderSet* res, const std::wstring& shaderName, D3D11_INPUT_ELEMENT_DESC* ied, UINT iedCount)
    {
        if (!res) return;

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* errBlob = nullptr;
        std::wstring csoPath = shaderName + L".cso";
        std::wstring hlslPath = shaderName + L".hlsl";

        // 1. CSO 시도
        HRESULT hr = D3DReadFileToBlob(csoPath.c_str(), &vsBlob);

        // 2. 실패 시 HLSL 컴파일 시도
        if (FAILED(hr))
        {
            hr = D3DCompileFromFile(hlslPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "main", "vs_5_0", 0, 0, &vsBlob, &errBlob);
            if (FAILED(hr))
            {
                HandleCompileError(hr, errBlob, hlslPath.c_str());
                return;
            }
        }

        // 3. 리소스 및 레이아웃 생성
        if (vsBlob)
        {
            Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &res->vs);

            if (ied)
            {
                Device->CreateInputLayout(ied, iedCount, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &res->layout);
            }
            vsBlob->Release();
        }
    }

    /**
 * [Pixel Shader 전용 로더]
 * - CSO 우선 시도 후 HLSL 컴파일
 */
    void LoadPixelShader(ShaderSet* res, const std::wstring& shaderName)
    {
        if (!res) return;

        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errBlob = nullptr;
        std::wstring csoPath = shaderName + L".cso";
        std::wstring hlslPath = shaderName + L".hlsl";

        // 1. CSO 시도
        HRESULT hr = D3DReadFileToBlob(csoPath.c_str(), &psBlob);

        // 2. 실패 시 HLSL 컴파일 시도
        if (FAILED(hr))
        {
            hr = D3DCompileFromFile(hlslPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "main", "ps_5_0", 0, 0, &psBlob, &errBlob);
            if (FAILED(hr))
            {
                HandleCompileError(hr, errBlob, hlslPath.c_str());
                return;
            }
        }

        // 3. 리소스 생성
        if (psBlob)
        {
            Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &res->ps);
            psBlob->Release();
        }
    }

    // 에러 처리를 위한 헬퍼 함수
    void HandleCompileError(HRESULT hr, ID3DBlob* errBlob, const wchar_t* path) 
    {
        if (errBlob) 
        {
            OutputDebugStringA((char*)errBlob->GetBufferPointer());
            MessageBoxA(NULL, (char*)errBlob->GetBufferPointer(), "Shader Compile Error", MB_ICONERROR);
            errBlob->Release();
        }
        else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) 
        {
            std::wstring msg = L"파일을 찾을 수 없습니다: " + std::wstring(path);
            MessageBoxW(NULL, msg.c_str(), L"File Error", MB_ICONERROR);
        }
    }
};