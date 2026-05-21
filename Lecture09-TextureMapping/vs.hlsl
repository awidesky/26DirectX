#include "struct.hlsli"


PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    //output.pos = mul(float4(input.pos, 1.0f), matWorld);
    output.pos = float4(input.pos, 1.0f);
    output.uv = input.uv;
    return output;
}