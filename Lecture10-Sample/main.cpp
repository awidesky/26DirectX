#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include "GameLoop.hpp"
#include "MeshRenderer.hpp"
#include "PlayerController.hpp" 
#include "Collider.hpp"       
#include "Physics.hpp"        
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

/*
 * =================================================================================
 * [개선 과제: 메모리 파편화 방지를 위한 오브젝트 풀링(Object Pooling) 구조]
 * =================================================================================
 * 1. 기존 아키텍처의 문제점
 * - 매번 총알이 생길 때마다 `new`를 하고 밖으로 나가면 메모리를 방치하거나 지우는 방식은
 * CPU의 메모리 할당자(Allocator)에게 엄청난 오버헤드를 주며 실시간 프레임 드랍을 유발함.
 *
 * 2. 오브젝트 풀링(Pooling) 원리
 * - 게임 시동 직후(WinMain 초기화 단계) 사용할 최대 총알 개수(30개)를 미리 완벽히 생성함.
 * - 평소에는 비활성화(`isActive = false`) 상태로 숨겨두었다가, 매니저가 신호를 주면
 * 위치를 초기화하고 활성화(`isActive = true`)함.
 * - 화면을 벗어나거나 충돌 시 삭제하는 대신 다시 비활성화 상태로 되돌림(반납).
 * =================================================================================
 */

bool g_bIsGameStarted = false;

// -----------------------------------------------------------------------------
// [1. 활성/비활성 제어 기능이 탑재된 적 총알 스크립트]
// -----------------------------------------------------------------------------
class BulletScript : public Component
{
public:
    float speed = 1.5f;
    bool isActive = false; // 객체의 생명 상태 제어 플래그

    void Start(GraphicsContext* gfx) override {}
    void Input() override {}

    void Update(float dt) override
    {
        // 핵심: 비활성 상태인 총알은 물리 연산 및 업데이트를 건너뜀 (메모리 풀 대기 상태)
        if (!isActive || !g_bIsGameStarted) return;

        // 아래로 낙하 연산
        pOwner->pos.y -= speed * dt;

        // 화면 하단을 벗어나면 파괴(delete)하지 않고, 풀(Pool)에 반납 처리
        if (pOwner->pos.y < -1.2f)
        {
            Deactivate();
        }
    }

    void Render(GraphicsContext* gfx) override
    {
        // TODO: 프레임워크 자체 렌더 루프(`GameLoop::Render`) 내부에 컴포넌트 활성화 예외 처리가 없다면,
        // 이곳 Render 진입부에서 렌더링을 강제로 생략하도록 트릭을 쓰거나 알파를 0으로 밀어야 함.
        // 여기서는 그리기 명령을 무시하도록 조기 리턴 처리함.
        if (!isActive)
        {
            // 현재 프레임워크 구조상 MeshRenderer가 뒤이어 그려지는 것을 막기 위해 
            // 임시 포지션을 화면 밖 저 멀리(z값 원거리 등)로 밀어버리는 안전장치 적용 가능
            pOwner->pos.z = 9999.0f;
            return;
        }
        pOwner->pos.z = 0.0f; // 활성화 시 제자리
    }

    void Activate(float startX, float startY)
    {
        isActive = true;
        pOwner->pos.x = startX;
        pOwner->pos.y = startY;
        pOwner->pos.z = 0.0f;
    }

    void Deactivate()
    {
        isActive = false;
        pOwner->pos.y = 99.0f; // 충돌 연산 범위(AABB)에서 완전히 격리하기 위해 y를 저 멀리 보냄
        pOwner->pos.z = 9999.0f;
    }
};

// -----------------------------------------------------------------------------
// [2. 플레이어 충돌 감지 컴포넌트]
// -----------------------------------------------------------------------------
class PlayerCollisionScript : public Component
{
public:
    void Start(GraphicsContext* gfx) override {}
    void Input() override {}
    void Update(float dt) override {}
    void Render(GraphicsContext* gfx) override {}

    void OnCollisionEnter(GameObject* other) override
    {
        // 충돌한 상대 오브젝트의 BulletScript를 검사하여 실제 활성화된 총알인지 교차 검증
        // (화면 밖에 대기 중인 비활성 총알과의 허상 충돌을 방지)
        for (auto c : other->components)
        {
            BulletScript* bullet = dynamic_cast<BulletScript*>(c);
            if (bullet && !bullet->isActive) return; // 대기 상태 덤프면 무시
        }

        std::cout << "========================================" << std::endl;
        std::cout << " GAME OVER! 적 총알에 충돌했습니다. " << std::endl;
        std::cout << "========================================" << std::endl;
        PostQuitMessage(0);
    }
};

// -----------------------------------------------------------------------------
// [3. 오너십과 엔진 포인터가 제거된 순수 자원 관리형 게임 매니저 스크                   ]
// -----------------------------------------------------------------------------
class GameManagerScript : public Component
{
private:
    std::vector<BulletScript*> bulletPool; // 메인에서 넘겨받은 총알 풀 주머니 (엔진 접근 원천 차단)
    float spawnTimer = 0.0f;
    float spawnInterval = 0.2f; // 스폰 주기 가속화 (풀이 버티므로 더 촘촘히 발사 가능)

public:
    // 생성자에서 엔진 포인터(`GameLoop*`)를 받지 않고, 미리 할당된 풀의 컴포넌트 주소들만 이관받음
    GameManagerScript(const std::vector<BulletScript*>& pool) : bulletPool(pool) {}

    void Start(GraphicsContext* gfx) override
    {
        std::cout << "========================================" << std::endl;
        std::cout << " [2D 탄막 피하기 - 오브젝트 풀링 버전] " << std::endl;
        std::cout << " >> PRESS SPACEBAR TO START GAME << " << std::endl;
        std::cout << "========================================" << std::endl;
    }

    void Input() override
    {
        if (!g_bIsGameStarted)
        {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000)
            {
                g_bIsGameStarted = true;
                std::cout << ">> 풀 가동 시작! 런타임 할당(new) 없음 <<" << std::endl;
            }
        }
    }

    void Update(float dt) override
    {
        if (!g_bIsGameStarted) return;

        spawnTimer += dt;
        if (spawnTimer >= spawnInterval)
        {
            spawnTimer = 0.0f;

            // 풀 내부에서 현재 잠들어 있는(`isActive == false`) 총알 하나를 색출함
            for (auto bullet : bulletPool)
            {
                if (!bullet->isActive)
                {
                    float randomX = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;

                    // 찾은 총알을 깨우고 위치만 상단 스폰 구역으로 초기화 (new 연산 유발 0%)
                    bullet->Activate(randomX, 1.2f);
                    break; // 한 발 깨웠으니 루프 탈출
                }
            }
        }
    }

    void Render(GraphicsContext* gfx) override {}
};

LRESULT CALLBACK GlobalWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(h, m, w, l);
}

// -----------------------------------------------------------------------------
// [메인 엔트리 포인트: 풀 미리 구성하기]
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS)
{
    srand((unsigned int)time(nullptr));

    GameLoop gEngine;
    gEngine.Initialize(hI, GlobalWndProc);

    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    ShaderSet texShaders;
    gEngine.gfx.LoadVertexShader(&texShaders, L"vs", ied, ARRAYSIZE(ied));
    gEngine.gfx.LoadPixelShader(&texShaders, L"ps");

    // 정점 명세 컴파일
    std::vector<Vertex> vTriangle;
    vTriangle.push_back({ { 0.0f,   0.2f,  0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} });
    vTriangle.push_back({ { 0.2f,  -0.2f,  0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} });
    vTriangle.push_back({ {-0.2f,  -0.2f,  0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} });

    std::vector<Vertex> vBulletQuad;
    vBulletQuad.push_back({ {-0.05f,  0.05f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    vBulletQuad.push_back({ { 0.05f,  0.05f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    vBulletQuad.push_back({ {-0.05f, -0.05f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    vBulletQuad.push_back({ { 0.05f, -0.05f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    vBulletQuad.push_back({ {-0.05f, -0.05f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });
    vBulletQuad.push_back({ { 0.05f,  0.05f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });

    Material* pMat = new Material();
    pMat->SetShaderSet(&texShaders);

    Mesh* playerMesh = new Mesh();
    playerMesh->Create(&gEngine.gfx, vTriangle);

    Mesh* bulletMesh = new Mesh();
    bulletMesh->Create(&gEngine.gfx, vBulletQuad);

    // 1. 플레이어 생성 및 배치
    GameObject* pPlayerObj = new GameObject(0.0f, -0.7f, 0.0f);
    pPlayerObj->AddComponent(new MeshRenderer(playerMesh, pMat));
    pPlayerObj->AddComponent(new PlayerController());
    pPlayerObj->AddComponent(new BoxCollider({ 0.3f, 0.3f }));
    pPlayerObj->AddComponent(new PlayerCollisionScript());
    gEngine.world.push_back(pPlayerObj);

    // 2. [핵심] 총알 30발 메모리 사전 할당 (오브젝트 풀 빌드)
    std::vector<BulletScript*> temporaryPoolCollector;
    const int POOL_SIZE = 30;

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        // 초기화 시 화면 저 멀리 보이지 않는 곳에 정렬 배정
        GameObject* bullet = new GameObject(0.0f, 99.0f, 9999.0f);
        bullet->AddComponent(new MeshRenderer(bulletMesh, pMat));
        bullet->AddComponent(new BoxCollider({ 0.1f, 0.1f }));

        BulletScript* bScript = new BulletScript();
        bullet->AddComponent(bScript);

        // 매니저에게 전달하기 위해 스크립트 포인터를 주머니에 수집
        temporaryPoolCollector.push_back(bScript);

        // 미리 엔진 월드에 바인딩해 두어 소멸 책임을 가짐과 동시에 런타임 제어권 확보
        gEngine.world.push_back(bullet);
    }

    // 3. 글로벌 관제 매니저 생성 및 풀 전달
    GameObject* pManagerObj = new GameObject(0.0f, 0.0f, 0.0f);
    pManagerObj->AddComponent(new GameManagerScript(temporaryPoolCollector));
    gEngine.world.push_back(pManagerObj);

    // 엔진 시동
    gEngine.Run();

    // 청소 및 복귀
    delete pMat;
    texShaders.Release();
    delete playerMesh;
    delete bulletMesh;

    return 0;
}