#include "struct.hlsli"

cbuffer cbMaterial : register(b1)
{
    float4 tintColor;
};

float4 main(PS_IN input) : SV_Target
{
    return tintColor;
}