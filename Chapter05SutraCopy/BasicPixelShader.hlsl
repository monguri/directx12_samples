#include "BasicShaderHeader.hlsli"

float4 BasicPS(Output input) : SV_Target
{
	return float4(input.uv, 1.0f, 1.0f);
}