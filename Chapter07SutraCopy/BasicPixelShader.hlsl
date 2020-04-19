#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 BasicPS(Output input) : SV_Target
{
	float3 light = normalize(float3(1.0f, -1.0f, 1.0f));
	float brightness = dot(-light, input.normal);
	return float4(brightness, brightness, brightness, 1);
}