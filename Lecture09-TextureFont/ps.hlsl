#include "struct.hlsli"

/*
 * [강의 노트: 레지스터와 샘플러의 비밀]
 *
 * 1. 왜 register(t0)라고 적는가?
 *    - GPU는 한 번에 여러 개의 텍스처를 읽을 수 있음(t0, t1, t2...). 
 *    - C++에서 context->PSSetShaderResources(0, ...); 라고 호출하는 것은 
 *      "0번 구멍(t0)에 이 텍스처를 꽂겠다"는 뜻임. 
 *    - 따라서 셰이더 코드에서도 반드시 register(t0)를 선언해서, 
 *      C++이 꽂아둔 그 데이터를 정확히 받아와야 함.
 *
 * 2. 샘플러(SamplerState)는 무엇인가?
 *    - 텍스처는 단순히 '픽셀 색상 배열'일 뿐임. 
 *    - 샘플러는 "이미지를 확대할 때 픽셀을 뭉갤지(Linear), 도트처럼 튀게 할지(Point)"를 
 *      결정하는 필터링 규칙임.
 *    - 텍스처와 샘플러는 서로 다른 슬롯(t vs s)을 사용하므로, 
 *      셰이더에서 register(s0)를 통해 샘플러를 따로 연결해줘야 함.
 */

float4 main(PS_INPUT input) : SV_TARGET
{
    return tex.Sample(sam, input.uv);
    
    //return float4(1, 1, 1, 1);
    //return colorTint;
}