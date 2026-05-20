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
#include <wrl/client.h> // Microsoft::WRL::ComPtr 사용을 위함
#include <wincodec.h> // Windows 기본 이미지 디코딩 라이브러리

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// 1. 공통 데이터 구조체
struct Vertex {
    XMFLOAT3 pos; XMFLOAT4 col;
};

// 2. 셰이더 리소스 묶음
struct ShaderSet {
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;

    ShaderSet() = default;
    ShaderSet(ID3D11VertexShader* v, ID3D11PixelShader* p, ID3D11InputLayout* l)
        : vs(v), ps(p), layout(l) {
    }

    void Release() {
        if (vs) { vs->Release(); vs = nullptr; }
        if (ps) { ps->Release(); ps = nullptr; }
        if (layout) { layout->Release(); layout = nullptr; }
    }
};

class Texture {
public:
    // [GPU 리소스 포인터]
    // ID3D11ShaderResourceView: 셰이더가 텍스처를 "쳐다보는 통로" 역할임.
    // ComPtr을 사용하면 Release()를 자동으로 호출해주어 메모리 누수를 방지함.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRV;

    // 텍스처의 가로/세로 크기 (UI 배치나 비율 계산 시 학생들에게 유용함)
    uint32_t width = 0;
    uint32_t height = 0;

    Texture() = default;

    // 복사 방지 (GPU 리소스는 함부로 복사하면 안 됨. 주소값만 전달해야 함)
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /**
     * @brief 이미지 파일을 로드하여 GPU 리소스를 생성함.
     * @param device: DirectX11 장치 (리스트 생성 주체)
     * @param path: 이미지 파일 경로 (유니코드 문자열 L"..." 권장)
     * @return 성공 여부 (bool)
     */
    bool Load(ID3D11Device* device, const std::wstring& path) {
        // 0. 초기화 및 기존 리소스 정리
        pSRV.Reset();

        // 1. WIC 팩토리 생성 (이미지 처리를 위한 공장 엔진 시작)
        Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        if (FAILED(hr)) return false;

        // 2. 이미지 디코더 생성 (파일 열기)
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        hr = wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) return false;

        // 3. 첫 번째 프레임 가져오기 (대부분의 이미지는 1개임)
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) return false;

        // 4. 이미지 크기 정보 획득
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr)) return false;

        // 5. DX11이 좋아하는 RGBA 8비트 형식으로 강제 변환
        // (이미지 파일이 JPG(RGB)든 PNG(RGBA)든 무조건 RGBA로 통일함)
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        hr = wicFactory->CreateFormatConverter(&converter);
        if (FAILED(hr)) return false;

        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) return false;

        // 6. CPU 메모리에 픽셀 데이터 복사 (버퍼 할당)
        std::vector<uint8_t> pixelData(width * height * 4); // RGBA는 픽셀당 4바이트
        hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixelData.size(), pixelData.data());
        if (FAILED(hr)) return false;

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

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
        hr = device->CreateTexture2D(&texDesc, &initData, &tex2D);
        if (FAILED(hr)) return false;

        // 8. 최종 결과물인 Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(tex2D.Get(), &srvDesc, &pSRV);

        return SUCCEEDED(hr);
    }

    /**
     * @brief 텍스처가 정상적으로 로드되었는지 확인함.
     */
    bool IsValid() const {
        return pSRV != nullptr;
    }

    /**
     * @brief GPU 슬롯에 이 텍스처를 직접 꽂아줌.
     * 보통은 Material::Bind() 내부에서 자동으로 호출됨.
     * @param slot: register(tX)의 X 번호
     */
    void Bind(ID3D11DeviceContext* context, uint32_t slot) {
        if (IsValid()) {
            context->PSSetShaderResources(slot, 1, pSRV.GetAddressOf());
        }
    }
};