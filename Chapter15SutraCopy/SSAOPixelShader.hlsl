#include "Type.hlsli"

float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float SsaoPS(PeraType input) : SV_Target
{
	return 0.0f;
}

