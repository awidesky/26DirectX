#include "struct.hlsli"


PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    // ★ 중요: 입력받은 3D 위치를 월드 변환 행렬과 곱해서 위치를 갱신해야 함
    output.pos = mul(float4(input.pos, 1.0f), matWorld);
    output.color = input.color;
    output.color.a = 0.5f;
    return output;
}