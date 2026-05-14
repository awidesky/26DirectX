/*
 * [강의 7: Finite State Machine (FSM) - "세상을 상태로 나누는 법"]
 *
 * 1. 정의 (Definition):
 *    - 유한한 개수의 '상태(State)'를 가지고,
 *    - 특정 '조건(Transition Condition)'에 따라 현재 상태에서 다른 상태로 전이하는 수학적 모델.
 *    - 게임 엔진에서는 객체의 행동 패턴(Idle, Move, Attack, Dead)이나
 *      게임의 흐름(Logo, Menu, Play, Result)을 관리할 때 필수적으로 사용됨.
 *
 * 2. 왜 FSM인가? (The Problem & Solution):
 *    - 문제점: if-else나 switch-case만으로 복잡한 로직을 짜면 소위 '스파게티 코드'가 됨.
 *    - 해결책: 행동을 상태 단위로 모듈화하여, "내가 지금 무엇을 하는지"에만 집중하게 만듦.
 *
 * 3. FSM의 핵심 3요소:
 *    (1) State (상태): 현재 객체가 처한 상황 (예: 잠자는 중, 뛰는 중).
 *    (2) Transition (전이): A상태에서 B상태로 바뀌는 행위.
 *    (3) Condition (조건): 전이를 발생시키는 트리거 (예: 적 발견, 체력 0).
 *
 * 4. FSM 라이프사이클 (State Lifecycle):
 *    - OnEnter: 상태에 진입할 때 딱 한 번 실행 (초기화).
 *    - OnUpdate: 해당 상태일 때 매 프레임 실행 (로직).
 *    - OnExit: 상태를 빠져나갈 때 딱 한 번 실행 (정리).
 *
 * --------------------------------------------------------------------------------------
 * [Dodge Game State Flowchart]
 *
 *      +-----------+           +-----------+           +-----------+
 *      |   START   |           |   PLAY    |           | GAME OVER |
 *      |  (Ready)  |---------->| (Dodge!)  |---------->| (Result)  |
 *      +-----------+ (KeySpace) +-----------+ (Collision) +-----------+
 *            ^                                               |
 *            |                  (Key 'R' - Restart)          |
 *            +-----------------------------------------------+
 *
 * --------------------------------------------------------------------------------------
 * [State Logic Detail]
 *
 *  [READY] ----(Input: Space)---> [PLAYING] ----(HP <= 0)-----> [GAMEOVER]
 *     |                              |                             |
 *     +-- Update: "Press Space"      +-- Update: "Spawning Bullets" +-- Update: "Final Score"
 *     +-- Exit: Reset Timer          +-- Update: "Check Collision"  +-- Exit: Reset Score
 *
 * --------------------------------------------------------------------------------------
 */

#include <iostream>
#include <string>
#include <conio.h>   //_getch() 사용을 위함
#include <Windows.h> // Sleep() 사용을 위함

 // 1. 상태 열거형 정의
enum class eGameState
{
    READY,
    PLAYING,
    GAMEOVER
};

// 2. 간단한 게임 매니저 (FSM 제어기)
class GameStateManager
{
private:
    eGameState currentState;
    int score;
    float timer;

public:
    GameStateManager() : currentState(eGameState::READY), score(0), timer(0.0f) {}

    void Update()
    {
        // 핵심: 현재 상태가 무엇이냐에 따라 행동이 완전히 분리됨 (격리)
        switch (currentState)
        {
        case eGameState::READY:
            UpdateReady();
            break;
        case eGameState::PLAYING:
            UpdatePlaying();
            break;
        case eGameState::GAMEOVER:
            UpdateGameOver();
            break;
        }
    }

    // --- 상태별 로직 (Modular Logic) ---

    void UpdateReady()
    {
        std::cout << "[READY STATE] 대기 중... 시작하려면 'Space'를 누르세요.\r";

        // 전이 조건 (Condition)
        if (_kbhit()) {
            if (_getch() == ' ') {
                ChangeState(eGameState::PLAYING);
            }
        }
    }

    void UpdatePlaying()
    {
        timer += 0.016f; // 가상의 DeltaTime
        score += 10;
        std::cout << "[PLAYING STATE] 총알 피하는 중! 현재 생존 시간: " << timer << "s | 점수: " << score << std::endl;

        // 전이 조건 (임의의 충돌 발생 가정)
        if (score > 100) {
            std::cout << "!!! 아차! 총알에 맞았습니다 !!!";
            ChangeState(eGameState::GAMEOVER);
        }
    }

    void UpdateGameOver()
    {
        std::cout << "[GAME OVER] 최종 점수: " << score << " | 다시 시작하려면 'R'을 누르세요.\r";

        if (_kbhit()) {
            char key = _getch();
            if (key == 'r' || key == 'R') {
                score = 0;
                timer = 0.0f;
                ChangeState(eGameState::READY);
            }
        }
    }

    // --- 상태 전이 함수 (Transition Helper) ---

    void ChangeState(eGameState nextState)
    {
        // [OnExit] 현재 상태를 나갈 때 처리할 것들
        std::cout << "\n >> Exit State: " << GetStateName(currentState) << std::endl;

        currentState = nextState;

        // [OnEnter] 새로운 상태에 들어올 때 처리할 것들
        std::cout << " >> Enter State: " << GetStateName(currentState) << std::endl;
        std::cout << "---------------------------------------------------------" << std::endl;
    }

    std::string GetStateName(eGameState state)
    {
        switch (state)
        {
        case eGameState::READY: return "READY";
        case eGameState::PLAYING: return "PLAYING";
        case eGameState::GAMEOVER: return "GAMEOVER";
        default: return "UNKNOWN";
        }
    }
};

int main()
{
    GameStateManager game;

    std::cout << "=== 미니 엔진 FSM 시뮬레이터 ===" << std::endl;

    // 가상의 게임 루프
    while (true)
    {
        game.Update();

        // 출력이 너무 빠르니 조금 쉬어줌
        Sleep(100);
    }

    return 0;
}