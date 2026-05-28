// color.hlsl (또는 별도의 vs, ps 파일)

cbuffer ConstantBuffer : register(b0) // C++에서 VSSetConstantBuffers(0, ...)로 넘긴 데이터
{
    matrix matWorld;
};


struct VS_INPUT
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};