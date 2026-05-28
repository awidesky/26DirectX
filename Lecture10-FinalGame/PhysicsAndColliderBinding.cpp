#include "Physics.hpp"
#include "Collider.hpp"
#include "ObjectBase.hpp" // GameObject 멤버 접근용

/*
 * =================================================================================
 * [강의 노트: 왜 코드를 여기에 몰아서 구현하고 분리한 것인가? (초보자용 해설)]
 * =================================================================================
 *
 * 1. 닭이 먼저냐, 달걀이 먼저냐? (순환 참조를 박살 낸 비결)
 * - `Collider.hpp`는 "나 충돌했어! 관리자(Physics)야, 나 등록해줘!" 하고 외쳐야 함.
 * - `Physics.hpp`는 "내가 모든 콜라이더(BoxCollider)들을 모아서 검사해 줄게!" 라고 함.
 * - 즉, 서로가 서로를 알아야만 코드를 짤 수 있는 '외통수(순환 참조)'에 걸림.
 *
 * - 해결책: 헤더 파일(.hpp)에서는 포인터(`*`) 선언만 두고 세부 내용은 모른 척함.
 * 포인터는 주소값(8바이트 숫자)일 뿐이라 내부 구조를 몰라도 변수를 만들 수 있음.
 * - 그리고 바로 여기! 두 헤더의 설계도를 모두 펼쳐볼 수 있는 .cpp 파일에 와서야
 * 비로소 `a->maxBound`나 `pair.first->FireCollisionEnter` 같은 실제 알맹이 변수와
 * 함수들을 찌르며 연산하는 것임. 이로써 컴파일러가 꼬이지 않고 완벽하게 빌드됨.
 *
 * 2. FireCollisionXXX (이벤트 전파)의 원리: "확성기 패턴"
 * - 물리 매니저(`Physics`)는 어떤 컴포넌트들이 붙어 있는지 구체적으로 모름. 오직 `GameObject` 주소만 앎.
 * - 충돌이 나면 `Physics`는 `GameObject`에게 "너 충돌했어!"라고 신호(`Fire`)를 보냄.
 * - 신호를 받은 `GameObject`는 자기 몸에 붙어 있는 모든 자식 컴포넌트(커스텀 스크립트, 사운드 등)에게
 * "충돌! 다들 `OnCollisionEnter` 켜!" 하고 확성기로 방송을 때려주는 구조임.
 * =================================================================================
 */

 //-----------------------------------------------------------------------------
 // [1단계] BoxCollider 라이프사이클 구현부
 //-----------------------------------------------------------------------------
 // 컴포넌트가 태어나고 죽을 때, 전역 싱글톤 관제탑에 출근(?) 체크 및 퇴근(?) 보고를 하는 영역
void BoxCollider::Start(GraphicsContext* gfx)
{
    // "매니저님, 저 이번에 새로 태어난 박스 콜라이더입니다. 명단에 올려주세요."
    Physics::GetInstance().RegisterCollider(this);
}

BoxCollider::~BoxCollider()
{
    // "매니저님, 저 파괴되어서 나갑니다. 명단에서 빼주세요." (안 빼면 나중에 빈 주소 찔러서 메모리침범 크래시 터짐)
    Physics::GetInstance().UnregisterCollider(this);
}

//-----------------------------------------------------------------------------
// [2단계] Physics 싱글톤 매니저 명단 관리 구현부
//-----------------------------------------------------------------------------
void Physics::RegisterCollider(BoxCollider* collider)
{
    colliders.push_back(collider); // 출석부(vector)에 추가
}

void Physics::UnregisterCollider(BoxCollider* collider)
{
    // 출석부에서 해당 콜라이더 주소를 찾아서 삭제
    auto it = std::find(colliders.begin(), colliders.end(), collider);
    if (it != colliders.end())
    {
        colliders.erase(it);
    }
}

//-----------------------------------------------------------------------------
// [3단계] 순수 기하학 AABB 교차 검사
//-----------------------------------------------------------------------------
bool Physics::CheckAABB(BoxCollider* a, BoxCollider* b)
{
    // 두 사각형이 서로 완전히 비껴나가는 예외 케이스 4가지를 검사함 (하나라도 맞으면 안 부딪힌 것)
    if (a->maxBound.x < b->minBound.x || a->minBound.x > b->maxBound.x) return false; // 좌우로 완전히 떨어짐
    if (a->maxBound.y < b->minBound.y || a->minBound.y > b->maxBound.y) return false; // 상하로 완전히 떨어짐

    return true; // 위 예외에 안 걸렸다면 무조건 겹쳐 있는 상태임
}

//-----------------------------------------------------------------------------
// [4단계] 핵심: 유니티 스타일 3단계 충돌 이벤트 분기 파이프라인
//-----------------------------------------------------------------------------
void Physics::UpdatePhysics()
{
    // 이번 프레임에 부딪힌 녀석들을 담을 임시 주머니
    std::set<std::pair<GameObject*, GameObject*>> newPairs;

    // 1. 전수 조사 단계: 등록된 모든 콜라이더들을 2개씩 짝지어 검사함 (O(N^2) 브루트포스)
    for (size_t i = 0; i < colliders.size(); ++i)
    {
        for (size_t j = i + 1; j < colliders.size(); ++j)
        {
            BoxCollider* colA = colliders[i];
            BoxCollider* colB = colliders[j];

            if (CheckAABB(colA, colB))
            {
                GameObject* objA = colA->pOwner;
                GameObject* objB = colB->pOwner;

                // [정렬 팁] (A, B)와 (B, A)는 같은 충돌인데 컴퓨터는 다르게 인식할 수 있음.
                // 메모리 주소 숫자가 작은 녀석을 항상 왼쪽에 둠으로써 "중복 쌍"을 원천 차단함.
                if (objA > objB) std::swap(objA, objB);

                newPairs.insert({ objA, objB }); // 이번 프레임 충돌 주머니에 저장
            }
        }
    }

    // 2. 판정 및 이벤트 전파 단계: "과거 기록(currentPairs)"과 "현재 기록(newPairs)"을 대조함.

    // [Enter 및 Stay 판정]
    for (const auto& pair : newPairs)
    {
        // 과거 기록 주머니에 없다면? -> "이번에 처음 부딪힌 거네!" -> Enter!
        if (currentPairs.find(pair) == currentPairs.end())
        {
            pair.first->FireCollisionEnter(pair.second);
            pair.second->FireCollisionEnter(pair.first);
        }
        // 과거 기록 주머니에도 이미 있었다면? -> "지난 프레임부터 계속 부딪히고 있네!" -> Stay!
        else
        {
            pair.first->FireCollisionStay(pair.second);
            pair.second->FireCollisionStay(pair.first);
        }
    }

    // [Exit 판정]
    for (const auto& pair : currentPairs)
    {
        // 과거 기록에는 있었는데, 이번 새 주머니에는 없다면? -> "방금 떨어졌네!" -> Exit!
        if (newPairs.find(pair) == newPairs.end())
        {
            pair.first->FireCollisionExit(pair.second);
            pair.second->FireCollisionExit(pair.first);
        }
    }

    // 3. 기록 대치: 다음 프레임을 위해 현재 기록을 과거 기록으로 업데이트함
    currentPairs = newPairs;
}