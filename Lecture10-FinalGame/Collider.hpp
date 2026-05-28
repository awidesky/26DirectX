#pragma once
#include "ObjectBase.hpp"

/*
 * =================================================================================
 * [강의 노트: 컴포넌트 기반 BoxCollider2D 및 AABB 바운딩 박스 연산]
 * =================================================================================
 * * 1. 본 컴포넌트의 역할
 * - 본 클래스는 GameObject에 부착되어 해당 오브젝트의 '물리적 충돌 범위'를 정의함.
 * - 충돌 계산의 원형이 되는 AABB(Axis-Aligned Bounding Box)의 실시간 영역(Min/Max)을
 * 매 프레임 갱신하고, 중앙 물리 관제탑(Physics)과 상호작용함.
 * * 2. 아키텍처 핵심: 순환 참조(Circular Dependency)의 연결고리 제거
 * - BoxCollider는 Physics 매니저에 자신을 등록해야 하고, Physics는 BoxCollider들을 관리함.
 * - 헤더에서 상호 `#include`를 수행하면 컴파일러가 파일 크기와 식별자를 확정하지 못해 에러가 터짐.
 * - 본 설계에서는 하단의 `class Physics;` 전방 선언을 통해 헤더 단에서의 결합도를 완벽히 격리함.
 * - 실제 주소를 참조하는 `Start()`와 소멸자 `~BoxCollider()`의 실행 본문은 cpp 파일(혹은 binding 영역)로
 * 밀어내어 가상 함수 테이블(VTable) 오염 및 링크 에러를 원천 차단함.
 * =================================================================================
 */

 // 전방 선언(Forward Declaration): 
 // "Physics라는 클래스가 존재하니까 포인터 변수(8바이트) 자리는 일단 컴파일러 너가 인정해줘"
class Physics;

class BoxCollider : public Component
{
public:
    // -------------------------------------------------------------------------
    // [멤버 변수 정의: 로컬 설정값 및 월드 결과값]
    // -------------------------------------------------------------------------

    // [인스펙터 설정값] 오브젝트 중심점(Center)으로부터 충돌 박스가 얼마나 떨어져 있는가 (오프셋)
    XMFLOAT2 centerOffset = { 0.0f, 0.0f };

    // [인스펙터 설정값] 오브젝트의 회전을 배제한 순수 충돌 박스의 '로컬 가로/세로 전체 크기'
    XMFLOAT2 size = { 1.0f, 1.0f };

    // [물리 연산용 결과값] 실시간으로 계산된 월드 좌표계 상의 좌하단(Minimum) 평면 단점 좌표
    XMFLOAT2 minBound = { 0.0f, 0.0f };

    // [물리 연산용 결과값] 실시간으로 계산된 월드 좌표계 상의 우상단(Maximum) 평면 단점 좌표
    XMFLOAT2 maxBound = { 0.0f, 0.0f };


    // -------------------------------------------------------------------------
    // [생성자: 기본 생성 및 인자 초기화]
    // -------------------------------------------------------------------------
    // 유니티 인스펙터 창에서 Size와 Offset 값을 수동 타이핑하여 세팅하는 행위와 일치함.
    BoxCollider(XMFLOAT2 size = { 1.0f, 1.0f }, XMFLOAT2 offset = { 0.0f, 0.0f })
        : size(size), centerOffset(offset)
    {
    }

    // -------------------------------------------------------------------------
    // [라이프사이클 가상 함수 선언]
    // -------------------------------------------------------------------------
    // 가상 함수 본문을 헤더에서 치워 싱글톤 및 외부 파일과의 상호 참조 결합도를 완전히 분리함.
    // 본문의 실제 구현은 cpp 파일 혹은 main.cpp 하단 바인딩 영역에서 수행되어야 함.

    // 엔진에 의해 오브젝트가 활성화되는 첫 프레임에 Physics 싱글톤 매니저에 나 자신을 자동 등록(Register)함.
    void Start(GraphicsContext* gfx) override;

    // 오브젝트가 파괴되거나 메모리에서 해제될 때 Physics 매니저의 벡터 풀에서 나 자신을 자동 제외(Unregister)함.
    ~BoxCollider() override;

    // 물리 컴포넌트는 키보드/마우스의 다이렉트 입력을 받지 않으므로 공백 유지
    void Input() override {}


    // -------------------------------------------------------------------------
    // [AABB 충돌 평면 좌표 갱신 알고리즘]
    // -------------------------------------------------------------------------
    // 주의: GameLoop.hpp 파이프라인 상 반드시 모든 GameObject의 Transform(위치/크기) 변환 연산이 
    // 완료된 직후, 그리고 Physics::UpdatePhysics()가 실행되기 바로 직전에 호출되어야 신뢰성이 보장됨.
    void Update(float dt) override
    {
        // 1. 부모 오브젝트(GameObject)의 전역 위치 정보에 콜라이더 고유 오프셋을 더해 전역 중심점 도출
        float worldCenterX = pOwner->pos.x + centerOffset.x;
        float worldCenterY = pOwner->pos.y + centerOffset.y;

        // 2. 중요: 로컬 사각형 크기(size)에 오브젝트의 스케일 배율(pOwner->scale)을 곱하여 최종 크기 산출.
        //    그 후, 중심점으로부터 좌/우, 상/하 경계선까지의 거리인 '반폭(Half Extent)' 값을 연산함.
        float halfExtentX = (size.x * pOwner->scale.x) * 0.5f;
        float halfExtentY = (size.y * pOwner->scale.y) * 0.5f;

        // 3. 중심점 좌표에서 반폭을 빼고 더하여 사각형의 최종 좌하단(Min)선과 우상단(Max) 경계 평면을 확정.
        //    ※ 기존 코드의 타이포 오차(halfExtentY를 곱해야 하는 곳에 X가 들어갔던 부분) 정정 반영함.
        minBound = { worldCenterX - halfExtentX, worldCenterY - halfExtentY };
        maxBound = { worldCenterX + halfExtentX, worldCenterY + halfExtentY };
    }

    // 디버그용 충돌 기즈모(초록색 선 사각형 등)를 그리는 용도로 확장 가능한 슬롯, 현재는 렌더링 제외
    void Render(GraphicsContext* gfx) override {}
};