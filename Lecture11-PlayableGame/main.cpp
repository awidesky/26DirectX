//텍스쳐 매핑 관련 예시

//framework.hpp -> texture
//그외 수정내용 : material, meshrenderer



#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include "GameLoop.hpp"
#include "MeshRenderer.hpp"
#include "PlayerController.hpp"



void SetPlayerCharacter()
{

}

void SetBackgroundImage()
{

}

void SetEnemyMissle()
{
}

void SetPlayerKillEffect()
{
}

void SetScoreText()
{

}

void SetPlayButton()
{

}

void SetGameText()
{

}


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
    // 엔진 매니저 초기화
    GameLoop gEngine;
    gEngine.Initialize(hI, GlobalWndProc);

    // 단색 정점에 맞춘 Input Layout 선언 (POSITION + COLOR)
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // 셰이더 로드 (단색 셰이더 파일명이 "color.hlsl" 또는 "color.cso"라고 가정)
    ShaderSet colorShaders;
    gEngine.gfx.LoadVertexShader(&colorShaders, L"color", ied, ARRAYSIZE(ied));
    gEngine.gfx.LoadPixelShader(&colorShaders, L"color");

    // 공용 단색 머티리얼 생성
    Material* colorMat = new Material();
    colorMat->SetShaderSet(&colorShaders);


    // -------------------------------------------------------------------------
    // 2. 사각형(네모) 오브젝트 생성 및 버텍스 정의
    // -------------------------------------------------------------------------
    // 가로세로 0.4 크기의 초록색 사각형 (삼각형 2개, 6개 정점)
    std::vector<ColorVertex> boxVertices = {
        // 첫 번째 삼각형 (좌상, 우상, 좌하)
        { {-0.2f,  0.2f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        { { 0.2f,  0.2f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        { {-0.2f, -0.2f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        // 두 번째 삼각형 (우하, 좌하, 우상)
        { { 0.2f, -0.2f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        { {-0.2f, -0.2f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} },
        { { 0.2f,  0.2f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f} }
    };

    Mesh* boxMesh = new Mesh();
    // 프레임워크 내부 템플릿 변환 방식에 맞춰 캐스팅 혹은 원형 유지 호출
    boxMesh->Create(&gEngine.gfx, *reinterpret_cast<std::vector<Vertex>*>(&boxVertices));

    GameObject* boxObj = new GameObject(-0.4f, 0.0f, 0.0f); // 왼쪽에 배치
    boxObj->AddComponent(new MeshRenderer(boxMesh, colorMat));

    // 키보드 제어를 원하므로 PlayerControl 컴포넌트 장착
    boxObj->AddComponent(new PlayerControl());
    gEngine.world.push_back(boxObj);


    // -------------------------------------------------------------------------
    // 3. 원(정다각형) 오브젝트 생성 및 버텍스 정의
    // -------------------------------------------------------------------------
    // 정32각형을 부채꼴 꼴로 쪼개어 원을 그림
    std::vector<ColorVertex> circleVertices;
    float radius = 0.2f;
    int sliceCount = 32;
    DirectX::XMFLOAT4 circleColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // 빨간색 원

    for (int i = 0; i < sliceCount; ++i)
    {
        float angle1 = ((float)i / sliceCount) * 2.0f * 3.141592f;
        float angle2 = ((float)(i + 1) / sliceCount) * 2.0f * 3.141592f;

        // 중심점
        circleVertices.push_back({ {0.0f, 0.0f, 0.0f}, circleColor });
        // 원주 위의 점 1
        circleVertices.push_back({ {radius * cosf(angle1), radius * sinf(angle1), 0.0f}, circleColor });
        // 원주 위의 점 2
        circleVertices.push_back({ {radius * cosf(angle2), radius * sinf(angle2), 0.0f}, circleColor });
    }

    Mesh* circleMesh = new Mesh();
    circleMesh->Create(&gEngine.gfx, *reinterpret_cast<std::vector<Vertex>*>(&circleVertices));

    GameObject* circleObj = new GameObject(0.4f, 0.0f, 0.0f); // 오른쪽에 배치
    circleObj->AddComponent(new MeshRenderer(circleMesh, colorMat));

    // 원 오브젝트에도 제어가 필요하다면 추가
    circleObj->AddComponent(new PlayerController());
    gEngine.world.push_back(circleObj);


    // -------------------------------------------------------------------------
    // 4. 엔진 실행 및 자원 해제
    // -------------------------------------------------------------------------
    gEngine.Run();

    // 자원 해제
    delete colorMat;
    colorShaders.Release();
    delete boxMesh;
    delete circleMesh;

    return 0;

    return 0;
}


