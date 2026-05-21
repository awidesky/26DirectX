#pragma once

// [Framework.hpp] 시스템 헤더 및 전역 구조체
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <wincodec.h> // Windows 기본 이미지 디코딩 라이브러리

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// 1. 공통 데이터 구조체
struct Vertex 
{
    XMFLOAT3 pos;
    XMFLOAT2 uv;
};

struct ConstantBuffer 
{
    XMMATRIX matWorld;
};


// 2. 셰이더 리소스 묶음
struct ShaderSet 
{
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;

    ShaderSet() = default;
    ShaderSet(ID3D11VertexShader* v, ID3D11PixelShader* p, ID3D11InputLayout* l)
        : vs(v), ps(p), layout(l) 
    {    }

    void Release() 
    {
        if (vs) { vs->Release(); vs = nullptr; }
        if (ps) { ps->Release(); ps = nullptr; }
        if (layout) { layout->Release(); layout = nullptr; }
    }
};

/*
 * [강의 노트 1: 텍스처(Texture)란?]
 * 1. 데이터의 원재료: 텍스처는 단순히 이미지 파일이 아니라, GPU 메모리에 올라간 픽셀 배열이야.
 * 2. 통로(SRV): GPU는 텍스처를 직접 만지지 않아. 'Shader Resource View(SRV)'라는 통로를
 *    통해서만 데이터를 읽을 수 있지. 이 클래스는 그 통로를 관리하는 관리자야.
 *
 * [강의 노트 2: 레지스터(Register) 번호]
 * 1. t0, t1... : GPU의 수많은 슬롯 중 텍스처가 꽂힐 위치를 의미해.
 *    C++에서 PSSetShaderResources(0, ...) 라고 하면 t0 슬롯에 연결된다는 뜻이지.
 *
 * [강의 노트 3: 샘플러(Sampler)란?]
 * 1. 해석의 규칙: 사진을 크게 확대하거나 작게 축소할 때, 픽셀을 어떻게 섞을지 정하는 필터야.
 * 2. 왜 필요한가?: 똑같은 사진도 상황에 따라 부드럽게(Linear) 볼지, 도트처럼(Point) 볼지
 *    선택해야 하기 때문이지.
 * 3. 속성값:
 *    - Filter: 픽셀을 섞는 방식 (MIN_MAG_MIP_LINEAR는 가장 부드러운 방식)
 *    - AddressU/V: 이미지가 잘릴 때 반복할지(Wrap) 혹은 가장자리 픽셀로 채울지(Clamp) 결정.
 * 
 * 
 * 
 */

 /*
  * [강의 노트: 샘플러 스테이트(Sampler State) 완전 정복]
  *
  * 1. 샘플러의 본질:
  *    - 텍스처(이미지)는 연속적인 데이터가 아닌 '픽셀의 격자'임.
  *    - 샘플러는 셰이더가 "텍스처의 이 좌표(UV)는 무슨 색이야?"라고 물어볼 때,
  *      주변 픽셀들을 어떻게 해석해서 알려줄지 결정하는 '해석 규칙'임.
  *
  * 2. 필터(Filter) 옵션:
  *    - D3D11_FILTER_MIN_MAG_MIP_LINEAR:
  *        - 가장 많이 쓰임. 확대/축소 시 인접 픽셀들의 평균값을 계산(Linear)하여 부드럽게 표현함.
  *    - D3D11_FILTER_MIN_MAG_MIP_POINT:
  *        - '도트 그래픽' 느낌을 낼 때 필수. 보간 없이 가장 가까운 픽셀 값을 그대로 가져옴.
  *    - D3D11_FILTER_ANISOTROPIC:
  *        - 비등방성 필터링. 바닥처럼 비스듬하게 누워있는 텍스처를 멀리서 볼 때
  *          뭉개지는 현상을 방지하고 선명하게 보여줌(고사양 그래픽용).
  *
  * 3. 어드레스 모드(AddressU/V/W):
  *    - UV 좌표가 [0, 1] 범위를 벗어날 때(예: 1.5, -0.2) 어떻게 처리할지 결정함.
  *    - D3D11_TEXTURE_ADDRESS_WRAP:
  *        - 타일처럼 반복됨 (가장 기본).
  *    - D3D11_TEXTURE_ADDRESS_CLAMP:
  *        - 끝부분 픽셀 값을 쭉 늘려서 채움 (이미지가 찢어지는 듯한 효과 방지).
  *    - D3D11_TEXTURE_ADDRESS_BORDER:
  *        - 지정한 고정 색상(BorderColor)으로 채움.
  *
  * 4. 기타 설정:
  *    - ComparisonFunc:
  *        - 주로 섀도우 맵(Shadow Map) 등을 만들 때 깊이 값을 비교하는 용도로 사용.
  *        - 일반 텍스처는 D3D11_COMPARISON_NEVER.
  *    - MinLOD / MaxLOD:
  *        - MipMap 수준(Level of Detail)의 범위를 지정함.
  *        - 0 ~ FLT_MAX로 두면 모든 MipMap 단계를 다 쓰겠다는 뜻임.
  */
class Texture 
{
public:
    // [GPU 리소스 포인터]
    // ID3D11ShaderResourceView: 셰이더가 텍스처를 "쳐다보는 통로" 역할임.
    // ComPtr을 사용하면 Release()를 자동으로 호출해주어 메모리 누수를 방지함.
    ID3D11ShaderResourceView* pSRV = nullptr;
    ID3D11SamplerState* pSampler = nullptr; // 샘플러

    // 텍스처의 가로/세로 크기 (UI 배치나 비율 계산 시 학생들에게 유용함)
    uint32_t width = 0;
    uint32_t height = 0;

    Texture() = default;

    // 복사 방지 (GPU 리소스는 함부로 복사하면 안 됨. 주소값만 전달해야 함)
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    //소멸자
    ~Texture() {
        Release();
    }

    void Release() {
        if (pSRV) { pSRV->Release();     pSRV = nullptr; }
        if (pSampler) { pSampler->Release(); pSampler = nullptr; }
    }

    /**
     * * 이미지 파일을 로드하여 GPU 리소스를 생성함.
     * @param device: DirectX11 장치 (리스트 생성 주체)
     * @param path: 이미지 파일 경로 (유니코드 문자열 L"..." 권장)
     * @return 성공 여부 (bool)
     */
    bool Load(ID3D11Device* device, const std::wstring& path) 
    {
        // 0. 초기화 및 기존 리소스 정리
        Release();
                
        // 1. WIC 팩토리 생성 (이미지 처리를 위한 공장 엔진 시작)
        IWICImagingFactory* wicFactory = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        if (FAILED(hr))
             return false;

        // 2. 이미지 디코더 생성 (파일 열기)
        IWICBitmapDecoder* decoder = nullptr;
        hr = wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr))
        {
            if (wicFactory) 
                wicFactory->Release();
            return false;
        }

        // 3. 첫 번째 프레임 가져오기 (대부분의 이미지는 1개임)
        IWICBitmapFrameDecode* frame = nullptr;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            if (decoder)    decoder->Release();
            if (wicFactory) wicFactory->Release();
            return false;
        }

        // 4. 이미지 크기 정보 획득
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr))
        {
            if (decoder)    decoder->Release();
            if (wicFactory) wicFactory->Release();
            if (frame) frame->Release();
            return false;
        }

        // 5. DX11이 좋아하는 RGBA 8비트 형식으로 강제 변환
        // (이미지 파일이 JPG(RGB)든 PNG(RGBA)든 무조건 RGBA로 통일함)
        IWICFormatConverter* converter = nullptr;
        hr = wicFactory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (wicFactory) wicFactory->Release();
            return false;
        }

        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (wicFactory) wicFactory->Release();
            return false;
        }

        // 6. CPU 메모리에 픽셀 데이터 복사 (버퍼 할당)
        std::vector<uint8_t> pixelData(width * height * 4); // RGBA는 픽셀당 4바이트
        hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixelData.size(), pixelData.data());
        if (FAILED(hr))
        {
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (wicFactory) wicFactory->Release();
            return false;
        }

        // 7. GPU용 텍스처(ID3D11Texture2D) 생성
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 표준 RGBA 포맷
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = pixelData.data(); // 방금 복사한 픽셀 데이터 주소
        initData.SysMemPitch = width * 4;    // 한 줄의 바이트 크기

        ID3D11Texture2D* tex2D = nullptr;
        hr = device->CreateTexture2D(&texDesc, &initData, &tex2D);
        if (FAILED(hr))
        {
            if (tex2D) tex2D->Release();
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (wicFactory) wicFactory->Release();
            return false;
        }

        // 8. 최종 결과물인 Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(tex2D, &srvDesc, &pSRV);

        if (tex2D) tex2D->Release();
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (wicFactory) wicFactory->Release();

        return SUCCEEDED(hr);
    }

    //기본 샘플러 상태 생성 (기본값은 부드러운 Linear 방식)
    void CreateSampler(ID3D11Device* device) {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // [속성] 부드러운 확대/축소
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;    // [속성] U축 반복
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;    // [속성] V축 반복
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;    // [속성] W축 반복
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        device->CreateSamplerState(&desc, &pSampler);
    }

};