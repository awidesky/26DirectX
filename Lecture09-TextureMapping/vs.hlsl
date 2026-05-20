#include "struct.hlsli"

cbuffer cbWorld : register(b0)
{
    matrix matWorld;
};

PS_IN main(VS_IN input)
{
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), matWorld);
    output.col = input.col;
    return output;
}