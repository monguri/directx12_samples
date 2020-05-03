#include "Type.hlsli"

float4 PeraPS(PeraType input) : SV_TARGET
{
	return float4(input.uv, 1.0f, 1.0f);
}