/*
 * [강의 노트: Material 시스템의 이해]
 *
 * 1. Material의 본질 (렌더링 설정집)
 *    - Mesh가 "형태(뼈대)"라면, Material은 "피부(질감)"임.
 *    - 렌더링에 필요한 모든 설정(셰이더, 텍스처, 상수 값)을 묶어 관리하는 데이터 컨테이너임.
 *
 * 2. 일반화된 데이터 관리 (uint8_t와 패딩)
 *    - 모든 데이터는 '바이트 단위(uint8_t)'로 관리됨. 이는 엔진이 데이터의 구체적인 타입(구조체)을
 *      모르더라도 동일하게 GPU 메모리로 복사할 수 있게 해줌.
 *    - ★중요★ DX11의 16바이트 정렬 규칙: GPU의 하드웨어 특성상 상수 버퍼는 16의 배수 크기여야 함.
 *      본 클래스는 자동 패딩(Padding)을 통해 학생들이 겪을 정렬 오류를 엔진 레벨에서 방지함.
 *
 * 3. 아키텍처: 왜 Bind()가 추상 메서드인가?
 *    - 단순한 래스터라이저부터 최신 Compute Shader까지, 렌더링 방식에 따라 GPU에 전달해야 할
 *      리소스의 종류와 위치(Slot)는 달라짐.
 *    - '어떤 데이터를 전달할 것인가(AddTexture/AddConstantData)'는 공통 로직이지만,
 *      '어떻게 바인딩할 것인가(Bind)'는 사용하는 셰이더의 특성에 따라 다르기 때문에
 *      자식 클래스에서 구체적으로 정의함(다형성 활용).
 *    - 소스코드에는 기본적으로 모든 쉐이더에 동일 레지스터로 업로드하는 기본로직 탑재로 귀찮은 사람들은 그대로 쓰도록 유도
 *
 * 4. Bind 시스템의 확장성
 *    - 향후 UAV(Unordered Access View)나 SamplerState가 추가되어도, Bind() 로직 내부의
 *      [설정 로드 -> GPU 세팅 -> 리소스 바인딩] 흐름만 유지하면 엔진 구조를 뒤엎지 않고 확장 가능함.
 *
 * 5. 사용법:
 *    class MyMat : public Material { ... Bind 구현 ... };
 *    MyMat* mat = new MyMat(shaderSet);
 *    mat->AddTexture(tex);      // t0 슬롯 바인딩 예약
 *    mat->AddConstantData(data);// b2 슬롯 바인딩 예약
 */

#pragma once
#include "Framework.hpp"


class Material
{
public:
    ShaderSet* shaders;
    
    // 텍스처 리스트: AddTexture를 호출한 순서대로 t0, t1, t2... 슬롯에 들어감
    std::vector<Texture*> textureList;

    // 상수 버퍼 데이터 리스트: 구조체 데이터를 보관
    // (데이터를 바이트 단위로 저장) - CPU 메모리에서 보관
    std::vector<std::vector<uint8_t>> constantDataList;
    // 순서대로 b1, b2, b3... 슬롯에 바인딩됨 (b0: 월드행렬용으로 예약) - GPU에 업로드된 상태
    std::vector<ID3D11Buffer*> constantBuffers;

    Material() {};
    Material(ShaderSet* s) : shaders(s) {}
    virtual ~Material() {
        for (auto cb : constantBuffers) if (cb) cb->Release();
    }

    //ShaderSet 설정
    void SetShaderSet(ShaderSet* shaderSet)
    {
        shaders = shaderSet;
    }

    //텍스처를 머티리얼에 추가함 (호출 순서가 곧 셰이더 레지스터 번호)
    void AddTexture(Texture* tex)
    {
        textureList.push_back(tex);
    }
    //커스텀 구조체 데이터를 머티리얼에 추가함
    //템플릿을 사용하여 어떤 형태의 구조체라도 '바이트 덩어리'로 변환하여 저장함.
    template<typename T>
    void AddConstantData(const T& data)
    {
        size_t originalSize = sizeof(T);

        // [자동 16바이트 정렬 로직]
        // DX11 요구사항에 맞춰 16의 배수로 크기를 올림함 (예: 12바이트 -> 16바이트)
        // 식은 ~15( 이진수로 11110000) 랑 &연산으로 16/32/48/64/.... 등 16의 배수로 사이즈를 잡아주는 식
        size_t alignedSize = (originalSize + 15) & ~15;

       
        // 바이트(uint8_t) 단위의 임시 버퍼 생성
        std::vector<uint8_t> byteBuffer(alignedSize, 0);

        // 실제 데이터를 바이트 버퍼의 맨 앞부분에 복사 (나머지 공간은 0으로 채워짐)
        memcpy(byteBuffer.data(), &data, originalSize);

        // 완성된 바이트 덩어리를 리스트에 보관
        constantDataList.push_back(byteBuffer);

        

    }

    // 기본적으로 자동 바인딩 로직을 수행함.
    virtual void Bind(ID3D11DeviceContext* context)
    {
        if (!shaders) return;

        // [중요] 필요한 시점에 GPU 버퍼 생성 (Lazy Initialization)
        for (int i = 0; i < (int)constantBuffers.size(); ++i) {
            if (constantBuffers[i] == nullptr) {
                // Device는 context를 통해 얻어옴 (ID3D11DeviceContext -> Device)
                ID3D11Device* device = nullptr;
                context->GetDevice(&device);

                D3D11_BUFFER_DESC desc = {};
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.ByteWidth = (UINT)constantDataList[i].size();
                desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

                D3D11_SUBRESOURCE_DATA initData = {};
                initData.pSysMem = constantDataList[i].data();

                device->CreateBuffer(&desc, &initData, &constantBuffers[i]);
                device->Release(); // GetDevice는 참조 카운트를 증가시키므로 해제
            }
        }

        // 1. 셰이더 적용
        context->IASetInputLayout(shaders->layout);
        context->VSSetShader(shaders->vs, nullptr, 0);
        context->PSSetShader(shaders->ps, nullptr, 0);
        
        // 2. 텍스처 오토 바인딩
        for (int i = 0; i < (int)textureList.size(); ++i)
        {
            ID3D11ShaderResourceView* srv = textureList[i]->pSRV;
            context->VSSetShaderResources(i, 1, &srv);
            context->PSSetShaderResources(i, 1, &srv);

            if (textureList[i]->pSampler)
            {
                context->VSSetSamplers(0, 1, &textureList[i]->pSampler);
                context->PSSetSamplers(0, 1, &textureList[i]->pSampler);
            }
        }


        // 3. 상수버퍼 오토 바인딩 (b1부터 바인딩함)
        for (int i = 0; i < (int)constantBuffers.size(); ++i)
        {
            context->VSSetConstantBuffers(i + 1, 1, &constantBuffers[i]);
            context->PSSetConstantBuffers(i + 1, 1, &constantBuffers[i]);
        }


    }

    template<typename T>
    void UpdateConstantData(ID3D11DeviceContext* context, int index, const T& data) {
        if (index >= constantBuffers.size()) return;

        // 이미 GPU에 올라가 있는 그릇(Buffer)에 새로운 데이터(data)를 덮어씀
        context->UpdateSubresource(constantBuffers[index], 0, nullptr, &data, 0, 0);
    }
};
