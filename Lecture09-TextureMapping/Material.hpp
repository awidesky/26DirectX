/*
 * [강의 노트: Material 시스템의 이해]
 *
 * 1. Material이란 무엇인가?
 *    - Mesh가 "형태(뼈대)"라면, Material은 "피부(질감)"임.
 *    - 내부적으로는 [어떤 셰이더로 그릴지] + [어떤 데이터(색상, 텍스처)를 보낼지]를 결정함.
 *
 * 2. 일반화된 데이터 전송 (uint8_t의 비밀)
 *    - 각자 다른 구조체(색상만 있는 것, 빛 반사율도 있는 것 등)를 정의해서 사용함.
 *    - 엔진은 이를 일일이 알 수 없으므로, 모든 데이터를 '바이트 단위(uint8_t)'로 쪼개서 관리함.
 *    - 8비트(1바이트) 단위로 데이터를 저장하면, 어떤 크기의 데이터라도 안전하게 GPU로 복사할 수 있음.
 *
 * 3. ★중요★ 16바이트 정렬 규칙 (DirectX 11의 제약)
 *    - DX11의 Constant Buffer는 반드시 16바이트의 배수 크기여야 함.
 *    - 만약 12바이트(float 3개)만 보내면 GPU가 화를 내며 렌더링을 거부함.
 *    - 본 클래스는 학생들의 실수를 방지하기 위해 내부적으로 자동 패딩(Padding)을 수행함.
 *
 * 4. 사용 방법:
 *    Material* myMat = new Material(shaderSet);
 *    myMat->AddTexture(tex1); // t0 슬롯에 할당
 *    myMat->AddConstantData(myStruct); // b2 슬롯에 할당 (b0, b1은 엔진 전용)
 */

#pragma once
#include "Framework.hpp"

class Material {
public:
    // 학생들이 사용할 셰이더 세트 (VS, PS, InputLayout 포함)
    ShaderSet* pShader;

    // ---------------------------------------------------------
    // [데이터 저장소]
    // ---------------------------------------------------------

    // 텍스처 리스트: AddTexture를 호출한 순서대로 t0, t1, t2... 슬롯에 들어감
    std::vector<Texture*> textureList;

    // 상수 버퍼 데이터 리스트: 구조체 데이터를 바이트(uint8_t) 덩어리로 변환하여 보관
    // 순서대로 b2, b3... 슬롯에 바인딩됨 (b0: 전역변수, b1: 월드행렬용으로 예약)
    std::vector<std::vector<uint8_t>> constantDataList;

    Material(ShaderSet* shader) : pShader(shader) {}

    /**
     * @brief 텍스처를 머티리얼에 추가함 (호출 순서가 곧 셰이더 레지스터 번호)
     */
    void AddTexture(Texture* tex) {
        textureList.push_back(tex);
    }

    /**
     * @brief 커스텀 구조체 데이터를 머티리얼에 추가함
     * @param data: 학생들이 정의한 구조체 변수
     * 템플릿을 사용하여 어떤 형태의 구조체라도 '바이트 덩어리'로 변환하여 저장함.
     */
    template<typename T>
    void AddConstantData(const T& data) {
        size_t originalSize = sizeof(T);

        // [자동 16바이트 정렬 로직]
        // DX11 요구사항에 맞춰 16의 배수로 크기를 올림함 (예: 12바이트 -> 16바이트)
        size_t alignedSize = (originalSize + 15) & ~15;

        // 바이트(uint8_t) 단위의 임시 버퍼 생성
        std::vector<uint8_t> byteBuffer(alignedSize, 0);

        // 실제 데이터를 바이트 버퍼의 맨 앞부분에 복사 (나머지 공간은 0으로 채워짐)
        memcpy(byteBuffer.data(), &data, originalSize);

        // 완성된 바이트 덩어리를 리스트에 보관
        constantDataList.push_back(byteBuffer);
    }

    /**
     * @brief 실제 그리기 직전에 호출되어 GPU에 모든 데이터를 세팅함
     * MeshRenderer에서 이 함수를 대신 호출해주므로 학생들은 원리만 이해하면 됨.
     */
    void Bind(ID3D11DeviceContext* context) {
        if (!pShader) return;

        // 1. 셰이더 바인딩 (그리는 공식 세팅)
        pShader->Bind(context);

        // 2. 텍스처 바인딩 (그림 데이터 세팅)
        for (int i = 0; i < (int)textureList.size(); ++i) {
            if (textureList[i] && textureList[i]->pSRV) {
                context->PSSetShaderResources(i, 1, &textureList[i]->pSRV);
            }
            else {
                // 텍스처가 없는 경우를 대비해 엔진의 기본 흰색 텍스처를 꽂아줌 (에러 방지)
                ID3D11ShaderResourceView* whiteSRV = GetDefaultWhiteSRV();
                context->PSSetShaderResources(i, 1, &whiteSRV);
            }
        }

        // 3. 상수 버퍼 바인딩 (수치 데이터 세팅)
        for (int i = 0; i < (int)constantDataList.size(); ++i) {
            // 엔진이 미리 확보해둔 i번째 임시 상수버퍼를 가져옴
            ID3D11Buffer* cb = GetGlobalConstantBuffer(i);

            // 바이트 덩어리를 통째로 GPU 메모리로 전송
            context->UpdateSubresource(cb, 0, nullptr, constantDataList[i].data(), 0, 0);

            // PS 전용 슬롯에 바인딩 (b2부터 시작하기 위해 i+2 사용)
            context->PSSetConstantBuffers(i + 2, 1, &cb);
        }
    }
};








class ColorMaterial : public Material {
public:
    XMFLOAT4 color;
    ID3D11Buffer* pColorBuffer = nullptr;

    ColorMaterial(ShaderSet s, XMFLOAT4 col, ID3D11Device* device)
        : Material(s), color(col) {
        D3D11_BUFFER_DESC cbd = { 0 };
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ColorBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        device->CreateBuffer(&cbd, nullptr, &pColorBuffer);
    }

    virtual ~ColorMaterial() {
        if (pColorBuffer) pColorBuffer->Release();
    }

    void SetColor(XMFLOAT4 col) { color = col; }

    void Bind(ID3D11DeviceContext* context) override {
        context->IASetInputLayout(shaders.layout);
        context->VSSetShader(shaders.vs, nullptr, 0);
        context->PSSetShader(shaders.ps, nullptr, 0);

        ColorBuffer cb = { color };
        context->UpdateSubresource(pColorBuffer, 0, nullptr, &cb, 0, 0);
        context->PSSetConstantBuffers(1, 1, &pColorBuffer);
    }
};