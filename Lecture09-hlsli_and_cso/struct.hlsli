/**
 * [HLSL Header (.hlsli) 완벽 이해하기]
 * 
 * 1. .hlsli 파일이란?
 *    - HLSL Include의 약자로, 여러 셰이더 파일(.hlsl)에서 공통으로 사용할 
 *      구조체, 상수 버퍼, 함수 등을 모아놓는 '공유 헤더 파일'임.
 *
 * 2. 왜 사용하는가? (The "Why")
 *    - 중복 제거: Vertex Shader와 Pixel Shader에서 같은 구조체를 쓸 때 두 번 쓸 필요 없음.
 *    - 유지보수: 구조체에 변수 하나만 추가해도 모든 셰이더에 자동 반영됨.
 *    - 가독성: 셰이더 본문 파일에는 실제 로직(Main 함수)만 남겨 깔끔하게 관리함.
 *
 * 3. 주요 포함 내용
 *    - 구조체 정의 (Input/Output Structs)
 *    - 상수 버퍼 정의 (Constant Buffers / cbuffer)
 *    - 공통 함수 (Math Helpers, Lighting Functions)
 *    - 전역 변수 및 매크로 (#define)
 *
 * 4. 사용법
 *    - #include "파일명.hlsli" 구문을 최상단에 작성함.
 *    - Visual Studio 프로젝트 설정에서 .hlsli 파일은 '빌드에서 제외'하거나 
 *      '헤더'로 분류해야 함 (컴파일 대상이 아니라 포함 대상이기 때문).
 */
struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};