#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 BasicPS(Output input) : SV_Target
{
	return float4(input.normal.xyz, 1);
}