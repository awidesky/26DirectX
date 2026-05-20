#include "struct.hlsli"

cbuffer cbWorld : register(b0)
{
    matrix matWorld;
};

struct VS_IN
{
    float3 pos : POSITION;
    float4 col : COLOR;
};

PS_IN main(VS_IN input)
{
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), matWorld);
    output.col = input.col;
    return output;
}