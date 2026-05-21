/**
 * [DirectX 11 Shader Management & Compilation Guide]
 *
 * - 변경부분 : GraphicsContext.hpp, Shader파일, main.cpp 참조
 * 
 * 1. CSO (Compiled Shader Object) 란?
 *    - HLSL 소스 코드(.hlsl)를 마이크로소프트의 fxc(Legacy)나 dxc 컴파일러를 통해
 *      GPU가 이해할 수 있는 바이너리 형태(Bytecode)로 구워낸 결과물 파일임.
 *
 * 2. CSO 사용의 장단점
 *    - 장점 (Efficiency & Security):
 *      ① 성능: 실행 시점에 소스 코드를 해석(Compile)하는 오버헤드가 없어 로딩 속도가 매우 빠름.
 *      ② 보안: 셰이더 로직(자산)이 텍스트로 노출되지 않아 코드 유출을 방지함.
 *      ③ 안정성: 빌드 타임에 문법 오류를 미리 발견할 수 있어 런타임 크래시를 방지함.
 *    - 단점 (Flexibility):
 *      ① 수정의 번거로움: 셰이더 코드를 고칠 때마다 다시 빌드해야 하므로 실시간 수정 확인이 어려움.
 *      ② 설정 복잡도: Visual Studio 프로젝트 설정을 정확히 맞춰야 경로 문제가 발생하지 않음.
 *
 * 3. .hlsli (HLSL Include) 파일 활용법
 *    - 목적: C++의 .h 헤더 파일처럼 공통 구조체, 상수 버퍼, 함수를 모듈화하여 공유함.
 *    - 장점: 구조체 변경 시 여러 .hlsl 파일을 일일이 수정할 필요 없이 헤더만 고치면 됨.
 *    - 주의: .hlsli 파일은 단독 컴파일 대상이 아니므로 VS 속성에서 '빌드에서 제외'해야 함.
 *
 * 4. Visual Studio에서의 셰이더 컴파일 및 디버깅 방법
 *    - 컴파일 설정:
 *      ① .hlsl 우클릭 -> [속성] -> [항목 유형]을 'HLSL 컴파일러'로 설정.
 *      ② [HLSL 컴파일러] -> [일반] -> '셰이더 유형'(/vs, /ps 등)과 '셰이더 모델'(/5_0) 지정.
 *    - 디버깅 방법:
 *      ① 그래픽 디버깅: 메뉴의 [디버그] -> [그래픽] -> [그래픽 디버깅 시작]을 통해 픽셀 단위 분석 가능.
 *      ② 경로 해결: 셰이더 개체 파일 이름을 '$(OutDir)%(Filename).cso'로 설정하여 실행 파일 옆에 배치.
 *      ③ 런타임 메시지: D3DCompileFromFile 사용 시 'errorBlob'을 출력하여 실시간 문법 에러 확인.
 *
 * 5. 게임 적용 워크플로우 (Hybrid Strategy)
 *    - [개발 단계]: .hlsl 소스 파일을 실시간으로 읽어(D3DCompileFromFile) 즉각적인 수치 조정 및 테스트.
 *    - [배포 단계]: 모든 셰이더를 .cso로 구워서 동봉하고 코드에서는 바이너리 로드(D3DReadFileToBlob)로 전환.
 */

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include "GameLoop.hpp"
#include "MeshRenderer.hpp"
#include "PlayerControl.hpp"

// -----------------------------------------------------------------------------
// [윈도우 메시지 처리기]
// -----------------------------------------------------------------------------
LRESULT CALLBACK GlobalWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(h, m, w, l);
}

// -----------------------------------------------------------------------------
// [메인 엔트리 포인트]
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS) 
{
    // 1. 엔진 매니저 초기화
    GameLoop gEngine;
    gEngine.Initialize(hI, GlobalWndProc);

    D3D11_INPUT_ELEMENT_DESC ied[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // 셰이더 컴파일 및 생성
    ShaderSet starShaders;
    // 버텍스 셰이더와 레이아웃 로드
    gEngine.gfx.LoadVertexShader(&starShaders, L"vs", ied, ARRAYSIZE(ied));
    // 픽셀 셰이더 로드
    gEngine.gfx.LoadPixelShader(&starShaders, L"ps");

    // 3. 메쉬 데이터 생성 (별 모양)
    float outerR = 0.5f;
    float innerR = 0.2f;
    XMFLOAT3 p[10];
    for (int i = 0; i < 10; ++i) 
    {
        float r = (i % 2 == 0) ? outerR : innerR;
        float angle = XM_PIDIV2 - (i * XM_2PI / 10.0f);
        p[i] = { cosf(angle) * r, sinf(angle) * r, 0.0f };
    }

    std::vector<Vertex> vStar;
    for (int i = 0; i < 10; i++) {
        vStar.push_back({ {0,0,0}, {1,1,1,1} }); // 중심점
        vStar.push_back({ p[i], {1,1,1,1} });
        vStar.push_back({ p[(i + 1) % 10], {1,1,1,1} });
    }

    Mesh* gMesh = new Mesh();
    gMesh->Create(&gEngine.gfx, vStar);

    // 4. 머티리얼 생성
    ColorMaterial* goldMat = new ColorMaterial(&starShaders, { 1, 0.8f, 0, 1 }, gEngine.gfx.Device);
    ColorMaterial* redMat = new ColorMaterial(&starShaders, { 1, 0.1f, 0.1f, 1 }, gEngine.gfx.Device);

    // 5. 난수를 이용한 다수의 별 생성
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> disPos(-1.0f, 1.0f);
    std::uniform_real_distribution<float> disScale(0.1f, 0.3f);

    for (int i = 0; i < 20; i++) {
        GameObject* star = new GameObject(disPos(gen), disPos(gen), 0);
        star->scale = { disScale(gen), disScale(gen), 1.0f };

        // 렌더러와 컨트롤러 장착
        Material* selectedMat = (i % 2 == 0) ? (Material*)goldMat : (Material*)redMat;
        star->AddComponent(new MeshRenderer(gMesh, selectedMat));
        star->AddComponent(new PlayerController());

        gEngine.world.push_back(star);
    }

    // 6. 엔진 실행 (메인 루프)
    gEngine.Run();

    // 7. 자원 해제
    // 머티리얼 해제
    delete goldMat;
    delete redMat;

    // 셰이더 세트 해제
    starShaders.Release();

    // 메쉬 해제
    delete gMesh;

    // gEngine은 소멸자에서 world 내의 모든 GameObject를 delete함
    return 0;
}