#include "Type.hlsli"

cbuffer SceneData : register(b0)
{
	matrix view;
	matrix proj;
	matrix invviewproj;
	matrix lightCamera;
	matrix shadow;
	float4 lightVec;
	float3 eye;
	bool isSelfShadow;
}

cbuffer Transform : register(b1)
{
	matrix world;
	matrix bones[256];
}

BasicType BasicVS(
float4 pos : POSITION,
float4 normal : NORMAL,
float2 uv : TEXCOORD,
min16uint2 boneno : BONE_NO,
min16uint2 weight : WEIGHT,
uint instNo : SV_InstanceID
)
{
	BasicType output;
	float w = (float)weight / 100.0f;

	matrix bm = bones[boneno[0]] * w + bones[boneno[1]] * (1.0f - w);
	pos = mul(bm, pos);
	pos = mul(world, pos);
	if (instNo > 0)
	{
		pos = mul(shadow, pos);
	}
	output.svpos = mul(mul(proj, view), pos);
	output.tpos = mul(lightCamera, pos);
	normal.w = 0;
	output.normal = mul(world, mul(bm, normal));
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	// 本ではmul(view, eye)にしているがrayはワールド座標で扱うのでviewは乗算しないのが正しい。実際にスペキュラをデバッグ表示して確認した
	output.ray = normalize(pos.xyz - eye);
	output.instNo = instNo;
	return output;
}

float4 ShadowVS(
float4 pos : POSITION,
float4 normal : NORMAL,
float2 uv : TEXCOORD,
min16uint2 boneno : BONE_NO,
min16uint2 weight : WEIGHT
) : SV_POSITION
{
	float w = (float)weight / 100.0f;

	matrix bm = bones[boneno[0]] * w + bones[boneno[1]] * (1.0f - w);
	pos = mul(bm, pos);
	pos = mul(world, pos);
	return mul(lightCamera, pos);
}
