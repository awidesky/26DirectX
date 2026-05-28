/*
 * =================================================================================
 * [강의 노트: 컴포넌트 기반 AABB 물리 엔진 시스템의 설계와 구현]
 * =================================================================================
 *
 * 본 실습 예제는 최신 상용 엔진(Unity, Unreal)의 기반이 되는 '컴포넌트 기반 아키텍처'를
 * 모방하여, 개별 오브젝트가 스스로의 충돌 영역을 인지하고(BoxCollider), 이를 전역 공간의
 * 싱글톤 관제탑(Physics)에서 통합 처리하여 이벤트를 역으로 라우팅하는 구조를 학습함.
 *
 * ---------------------------------------------------------------------------------
 * 1. 프로젝트 주요 변경 및 수정 파일 명세
 * ---------------------------------------------------------------------------------
 * [수정] Framework.hpp       : Vertex 구조체를 {float3 pos, float2 uv}에서
 * 텍스처 없이 순수 정점 색상 변환 처리를 위한 {float3 pos, float4 color}로 확장.
 * [수정] ObjectBase.hpp      : Component 인터페이스에 OnCollisionEnter/Stay/Exit 가상 함수 추가.
 * GameObject에 충돌 이벤트를 하위 컴포넌트 전체에 뿌려주는 전파 함수 구현.
 * [수정] GameLoop.hpp        : 컴포넌트의 Update 루프가 완료되어 모든 Transform 변환이 완료된 직후,
 * Update()에서 물리 매니저의 UpdatePhysics()를 트리거하도록 파이프라인 시퀀스 조정.
 * [신규] Collider.hpp        : 물리 계층과 엔진 구조 계층의 결합을 끊기 위해 전방 선언만을 활용한
 * BoxCollider 컴포넌트 설계 및 월드 스케일 기반 AABB 연산 구현.
 * [신규] Physics.hpp         : 런타임 중에 수집된 콜라이더들을 60fps로 교차 검사하고,
 * '이전 프레임 상태'와의 대조를 통해 3단계 이벤트를 추출하는 싱글톤 관제탑.
 * [신규] CollisionTestScript.hpp : 유니티의 MonoBehaviour 스크립트처럼 오버라이딩하여 컨텍스트 기반
 * 게임 로직(콘솔 출력 등)을 수행하는 사용자 커스텀 스크립트. (새로 시도해보는 C 기초)
 *
 * ---------------------------------------------------------------------------------
 * 2. 핵심 디자인 패턴: 싱글톤(Singleton)을 통한 결합도(Coupling) 제어
 * ---------------------------------------------------------------------------------
 * 물리 시스템(Physics)을 GameLoop의 자식으로 선언하게 되면, Collider가 자신의 물리 시스템을
 * 참조하기 위해 GameLoop의 주소를 알아야 하고, 이로 인해 상호 참조 `#include` 지옥이 발생함.
 *
 * 이를 완전히 격리하기 위해 Physics를 '전역 단일 인스턴스' 형태인 싱글톤으로 설계함:
 * - static Physics& GetInstance()를 통해 프로그램 전체에서 단 하나의 인스턴스만 보장.
 * - 생성자/복사 생성자/대입 연산자를 명시적 딜리트(delete)하여 객체의 다중 생성을 철저히 방지.
 * - Collider는 GameLoop가 존재하는지도 모른 채, Physics::GetInstance() 통로만으로 통신함.
 *
 * ---------------------------------------------------------------------------------
 * 3. 기하학 이론: AABB (Axis-Aligned Bounding Box) 충돌 감지 알고리즘
 * ---------------------------------------------------------------------------------
 * AABB는 회전하지 않는, 오직 월드 축(X, Y)에 평행한 사각형 박스를 의미함.
 * 박스의 회전을 고려하지 않기 때문에 계산 비용이 매우 저렴하여 대규모 충돌 일차 필터링에 다수 활용됨.
 *
 * [AABB 경계 구하기]
 * - BoxCollider({0.5f, 0.5f})에서 인자로 넘겨받은 수치는 오브젝트 중심 기준의 로컬 크기(Size)임.
 * - 매 프레임 Update() 단계에서 오브젝트의 변환 값(pos)과 크기 값(scale)을 가압함:
 * halfExtentX = (Size.X * Scale.X) * 0.5f;
 * minBound.x = WorldCenter.X - halfExtentX; // 좌측 경계선
 * maxBound.x = WorldCenter.X + halfExtentX; // 우측 경계선
 *
 * [충돌 판정 식 (CheckAABB)]
 * 두 박스 A와 B가 서로 완전히 비껴가는 '예외 상황' 4가지를 검사하여 하나라도 해당하면 충돌이 아님:
 * ① A의 오른쪽 끝이 B의 왼쪽 끝보다 작을 때 (A가 B의 왼쪽에 완벽히 떨어져 있음)
 * ② A의 왼쪽 끝이 B의 오른쪽 끝보다 클 때 (A가 B의 오른쪽에 완벽히 떨어져 있음)
 * ③ A의 위쪽 끝이 B의 아래쪽 끝보다 작을 때 (A가 B의 아래에 완벽히 떨어져 있음)
 * ④ A의 아래쪽 끝이 B의 위쪽 끝보다 클 때 (A가 B의 위에 완벽히 떨어져 있음)
 * 이 조건들에 걸리지 않는다면, 두 사각형은 최소 한 점 이상 공간 상에서 겹쳐져(Overlap) 있는 것임.
 *
 * ---------------------------------------------------------------------------------
 * 4. 고급 상태 추적 기법: 3단계 충돌 이벤트(Enter, Stay, Exit) 분기 메커니즘
 * ---------------------------------------------------------------------------------
 * 단순 교차 여부만 검사하는 OBB/AABB 함수는 '지금 겹쳤는가?'의 bool 값만 줄 뿐임.
 * 게임 엔진처럼 단 한 번만 실행되는 Enter와 Exit, 누적 실행되는 Stay를 분리하려면
 * '이전 프레임의 충돌 상태 기록'을 유지해야 함.
 *
 * - currentPairs: 지난 프레임에 충돌 중이었던 [오브젝트A, 오브젝트B] 주소 쌍의 셋(std::set).
 * - newPairs: 이번 프레임에 AABB 알고리즘으로 검출된 따끈따끈한 충돌 주소 쌍의 셋.
 *
 * [트리거 분기 공식]
 * ① Enter 조건: (pair ∈ newPairs) 이고 (pair ∉ currentPairs)
 * -> 지난 번엔 안 부딪혔는데 이번에 검출됨 = '충돌 시작(Enter)' -> FireCollisionEnter()
 * ② Stay 조건: (pair ∈ newPairs) 이고 (pair ∈ currentPairs)
 * -> 지난 번에도 부딪혔고 이번에도 여전히 검출됨 = '충돌 중(Stay)' -> FireCollisionStay()
 * ③ Exit 조건: (pair ∉ newPairs) 이고 (pair ∈ currentPairs)
 * -> 지난 번엔 부딪혔는데 이번 판정에서는 빠짐 = '충돌 종료(Exit)' -> FireCollisionExit()
 *
 * 이 연산이 완료되면 currentPairs = newPairs; 로 대치하여 다음 프레임의 거울 데이터로 사용함.
 *
 * ---------------------------------------------------------------------------------
 * 5. C++ 컴파일러 지식: 전방 선언(Forward Declaration)과 파일 분리의 당위성
 * ---------------------------------------------------------------------------------
 * C++는 컴파일러가 위에서 아래로 한 줄씩 해석하는 단방향 언어임.
 * - Collider.hpp가 Physics의 멤버 함수를 호출하려 하고, Physics.hpp가 Collider 포인터 벡터를 들고 있으면
 * 서로가 서로를 인클루드하는 순환 참조 구조가 형성되어 "알 수 없는 식별자" 에러가 터짐.
 *
 * [해결책]
 * - 헤더(.hpp) 계층에서는 `class Physics;`, `class BoxCollider;` 처럼 포인터 크기(8바이트)만
 * 컴파일러가 인지할 수 있도록 명칭 선언(전방 선언)만 떼어내어 고립시킴.
 * - 두 포인터가 실질적으로 상대방의 내부 멤버 변수(.minBound나 GetInstance())에 직접 접근하는
 * 실제 기계어 번역 코드 본문은, 두 헤더를 모두 알고 있는 소스 파일(.cpp) 영역으로 분리 컴파일함.
 * - 이를 통해 헤더 의존성이 거미줄처럼 엉키는 현상을 방지하고 빌드 속도를 극한으로 끌어올림.
 * =================================================================================
 */


 // 텍스처 매핑 기반 프레임워크 확장 실습 예제
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#include "GameLoop.hpp"
#include "MeshRenderer.hpp"
#include "PlayerController.hpp"
#include <iostream>

 // -----------------------------------------------------------------------------
 // [사용자 정의 컴포넌트: 충돌 테스트용 커스텀 로직 스크립트]
 // -----------------------------------------------------------------------------
 // 유니티의 MonoBehaviour를 상속받아 커스텀 스크립트를 짜는 방식과 완벽히 일치함.
class CollisionTestScript : public Component
{
public:
    void Start(GraphicsContext* gfx) override {}
    void Input() override {}
    void Update(float dt) override {}
    void Render(GraphicsContext* gfx) override {}

    // ★ Physics 매니저가 이벤트를 전파할 때 실행될 가상 함수 오버라이딩 슬롯

    // 두 물체가 공간 상에서 최초로 교차(Overlap)한 단 1프레임만 호출됨
    void OnCollisionEnter(GameObject* other) override
    {
        std::cout << "쿵! 충돌 시작됨. 상대 오브젝트 주소: " << other << std::endl;
    }

    // 두 물체가 떨어지지 않고 계속 겹쳐 있는 상태를 유지하는 매 프레임 호출됨
    void OnCollisionStay(GameObject* other) override
    {
        std::cout << "뿌가가가각 충돌 중..." << std::endl;
    }

    // 겹쳐 있던 두 물체가 서로 완전히 비껴나간 단 1프레임만 호출됨
    void OnCollisionExit(GameObject* other) override
    {
        std::cout << "스쳐지나감. 충돌 종료." << std::endl;
    }
};

// -----------------------------------------------------------------------------
// [윈도우 메시지 처리기 (WndProc)]
// -----------------------------------------------------------------------------
// Windows OS로부터 들어오는 시스템 메시지(마우스 클릭, 종료 등)를 처리하는 전역 콜백 함수
LRESULT CALLBACK GlobalWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) PostQuitMessage(0); // 창 닫기 버튼 누를 시 프로그램 종료 루틴 시동
    return DefWindowProc(h, m, w, l);
}

// -----------------------------------------------------------------------------
// [메인 엔트리 포인트 (WinMain)]
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS)
{
    // [1단계] 엔진 매니저 인스턴스화 및 그래픽스 콘텍스트 초기화
    GameLoop gEngine;
    gEngine.Initialize(hI, GlobalWndProc);

    // [2단계] DirectX11 정점 입력 레이아웃(Input Layout) 정의
    // - GPU에게 우리가 보낼 Vertex 구조체의 메모리 덩어리가 어떻게 생겨먹었는지 알려주는 명세서.
    // - POSITION(12바이트), COLOR(16바이트) 순서로 바이트 오프셋(0, 12)을 나누어 배치함.
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // 셰이더 컴파일 및 생성 (버텍스 셰이더와 픽셀 셰이더 리소스 로드)
    ShaderSet texShaders;
    gEngine.gfx.LoadVertexShader(&texShaders, L"vs", ied, ARRAYSIZE(ied));
    gEngine.gfx.LoadPixelShader(&texShaders, L"ps");

    // [3단계] 메쉬용 정점 데이터 정의 (삼각형 2개로 사각형(Quad)을 구성)

    // 3-1. 청록색 기본 사각형 정점 배열 (가로 1.0, 세로 1.0 크기)
    std::vector<Vertex> vQuad;
    vQuad.push_back({ {-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} }); // 좌상
    vQuad.push_back({ { 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} }); // 우상
    vQuad.push_back({ {-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} }); // 좌하
    vQuad.push_back({ { 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} }); // 우하
    vQuad.push_back({ {-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} }); // 좌하
    vQuad.push_back({ { 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f} }); // 우상

    // 3-2. 빨간색 작은 사각형 정점 배열 (가로 0.5, 세로 0.5 크기)
    std::vector<Vertex> vRedQuad;
    vRedQuad.push_back({ {-0.25f,  0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }); // 좌상
    vRedQuad.push_back({ { 0.25f,  0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }); // 우상
    vRedQuad.push_back({ {-0.25f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }); // 좌하
    vRedQuad.push_back({ { 0.25f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }); // 우하
    vRedQuad.push_back({ {-0.25f, -0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }); // 좌하
    vRedQuad.push_back({ { 0.25f,  0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} }); // 우상

    // [4단계] 머티리얼 및 메쉬 실체화
    Material* texMat = new Material();
    texMat->SetShaderSet(&texShaders);

    Mesh* quadMesh = new Mesh();
    quadMesh->Create(&gEngine.gfx, vQuad);

    Mesh* redQuadMesh = new Mesh();
    redQuadMesh->Create(&gEngine.gfx, vRedQuad);

    // [5단계] 게임 오브젝트 동적 생성 및 컴포넌트 주입 단계

    // 5-1. 청록색 메인 플레이어 오브젝트 조립 (월드 중심 (0,0)에 배치)
    GameObject* obj = new GameObject(0.0f, 0.0f, 0.0f);
    obj->AddComponent(new MeshRenderer(quadMesh, texMat)); // 그래픽 외형 컴포넌트
    obj->AddComponent(new PlayerController());             // 키보드(방향키) 제어 컴포넌트
    obj->AddComponent(new BoxCollider({ 1.0f, 1.0f }));    // 정점 크기(1.0)에 맞춘 AABB 충돌체 생성
    obj->AddComponent(new CollisionTestScript());          // 충돌 시 콘솔 로깅 처리 컴포넌트
    gEngine.world.push_back(obj);                          // 월드 관리 매트릭스에 등록

    // 5-2. 빨간색 서브 플레이어 오브젝트 조립 (오른쪽 (0.5, 0)에 배치)
    GameObject* objRed = new GameObject(0.5f, 0.0f, 0.0f);
    objRed->AddComponent(new MeshRenderer(redQuadMesh, texMat));
    objRed->AddComponent(new PlayerController2());          // 또 다른 키보드 제어 컴포넌트(W,A,S,D 매핑 등)
    objRed->AddComponent(new BoxCollider({ 0.5f, 0.5f })); // 정점 크기(0.5)에 맞춘 AABB 충돌체 생성
    objRed->AddComponent(new CollisionTestScript());
    gEngine.world.push_back(objRed);

    // [6단계] 엔진 실행 (무한 렌더 업데이트 게임루프 가동)
    gEngine.Run();

    // -------------------------------------------------------------------------
    // [7단계] 메모리 자원 해제 (Clean Up)
    // -------------------------------------------------------------------------
    // 주의: 생성의 역순으로 해제하며, 그래픽스 하드웨어 객체와 동적 할당 메모리를 비워줌.
    delete texMat;
    texShaders.Release();
    delete quadMesh;
    delete redQuadMesh;

    // ※ 팁: gEngine은 자신의 소멸자(~GameLoop)에서 world 벡터 풀 내부에 적재된 
    //        모든 GameObject 객체 및 종속 컴포넌트들을 내부적으로 순회하며 자동 delete 처리함.
    return 0;
}
