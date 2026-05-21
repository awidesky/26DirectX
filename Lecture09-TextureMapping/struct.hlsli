Texture2D tex : register(t0);
SamplerState sam : register(s0);


// 월드 행렬을 b0로 이동
cbuffer WorldBuffer : register(b0)
{
    matrix matWorld;
};
// 머티리얼 데이터 (b1부터 시작)
cbuffer CustomData : register(b1)
{
    float4 colorTint;
};




struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};