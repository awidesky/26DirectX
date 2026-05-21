//텍스쳐 매핑 관련 예시

//framework.hpp -> texture
//그외 수정내용 : material, meshrenderer



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
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // 셰이더 컴파일 및 생성
    ShaderSet texShaders;

    // 버텍스 셰이더와 레이아웃 로드
    gEngine.gfx.LoadVertexShader(&texShaders, L"vs", ied, ARRAYSIZE(ied));

    // 픽셀 셰이더 로드
    gEngine.gfx.LoadPixelShader(&texShaders, L"ps");



 // 1. 그릴 문자열 정의 (0~9로 구성)
    std::string text = "1234567890";
    float charWidth = 0.3f; // 사각형 하나의 가로 크기
    float charHeight = 0.3f;
    float uvWidth = 0.1f;  // 1/10 (0~9가 10개이므로)

    std::vector<Vertex> vText;

    for (size_t i = 0; i < text.length(); ++i)
    {
        int num = text[i] - '0'; // 문자를 숫자로 변환
        if (num < 0 || num > 9) continue;

        float uStart = (float)num * uvWidth;
        float uEnd = uStart + uvWidth;
        float xOffset = (float)i * charWidth; // 오른쪽으로 나열

        // 쿼드 6개 버텍스 (삼각형 2개)
        // 위치는 xOffset을 기준으로 설정
        vText.push_back({ {xOffset + 0.0f,      charHeight, 0.0f}, {uStart, 0.0f} }); // 좌상
        vText.push_back({ {xOffset + charWidth, charHeight, 0.0f}, {uEnd,   0.0f} }); // 우상
        vText.push_back({ {xOffset + 0.0f,      0.0f,       0.0f}, {uStart, 1.0f} }); // 좌하

        vText.push_back({ {xOffset + charWidth, 0.0f,       0.0f}, {uEnd,   1.0f} }); // 우하
        vText.push_back({ {xOffset + 0.0f,      0.0f,       0.0f}, {uStart, 1.0f} }); // 좌하
        vText.push_back({ {xOffset + charWidth, charHeight, 0.0f}, {uEnd,   0.0f} }); // 우상
    }

    // 이후 메쉬 생성 및 오브젝트 추가 과정
    Mesh* textMesh = new Mesh();
    textMesh->Create(&gEngine.gfx, vText);

    Material* texMat = new Material();
    texMat->SetShaderSet(&texShaders);

    Texture* tex = new Texture();
    tex->Load(gEngine.gfx.Device, L"digital-numbers.png");
    tex->CreateSampler(gEngine.gfx.Device);
    texMat->AddTexture(tex);

    GameObject* obj = new GameObject(0, 0, 0);
    obj->AddComponent(new MeshRenderer(textMesh, texMat));
    gEngine.world.push_back(obj);




   
    
    // 엔진 실행 (메인 루프)
    gEngine.Run();

    // ----자원 해제--------------
    // 머티리얼 해제
    delete texMat;

    // 셰이더 세트 해제
    texShaders.Release();

    // 메쉬 해제
    delete quadMesh;

    // gEngine은 소멸자에서 world 내의 모든 GameObject를 delete함
    return 0;
}


