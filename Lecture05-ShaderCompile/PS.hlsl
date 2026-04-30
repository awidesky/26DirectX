struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Col : COLOR;
};

//-----------------------------------------------------



float4 main(VS_OUTPUT input) : SV_Target
{
    return input.Col;
}