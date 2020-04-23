#include "BasicShaderHeader.hlsli"

// 定数バッファ
cbuffer cubuff0 : register(b0)
{
	matrix world;
	matrix view;
	matrix proj;
	float3 eye;
}

Output BasicVS(
float4 pos : POSITION,
float4 normal : NORMAL,
float2 uv : TEXCOORD,
min16uint2 boneno : BONE_NO,
min16uint2 weight : WEIGHT
)
{
	Output output;
	output.pos = mul(mul(proj, mul(view, world)), pos);
	normal.w = 0;
	output.normal = mul(world, normal);
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(pos.xyz - eye);
	return output;
}