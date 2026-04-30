struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Col : COLOR;
};

//-----------------------------------------------------

VS_OUTPUT VS(float3 pos : POSITION, float4 col : COLOR)
{
    VS_OUTPUT output;
    output.Pos = float4(pos, 1.0f);
    output.Col = col;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_Target
{
    return input.Col;
}