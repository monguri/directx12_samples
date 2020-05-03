#include "Type.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

PeraType PeraVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	PeraType output;
	output.pos = pos;
	output.uv = uv;
	return output;
}