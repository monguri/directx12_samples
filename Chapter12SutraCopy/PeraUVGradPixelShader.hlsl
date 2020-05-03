#include "Type.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 PeraPS(PeraType input) : SV_TARGET
{
	return float4(input.uv, 1.0f, 1.0f);
}