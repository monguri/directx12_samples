#include "Type.hlsli"

Texture2D<float4> normtex : register(t1);
Texture2D<float> depthtex : register(t6);

float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float SsaoPS(PeraType input) : SV_Target
{
	return 1.0f;
}

