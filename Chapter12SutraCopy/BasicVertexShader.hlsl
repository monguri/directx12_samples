#include "Type.hlsli"

// 定数バッファ
cbuffer SceneData : register(b0)
{
	matrix view;
	matrix proj;
	float3 eye;
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
min16uint2 weight : WEIGHT
)
{
	BasicType output;
	float w = (float)weight / 100.0f;

	matrix bm = bones[boneno[0]] * w + bones[boneno[1]] * (1.0f - w);
	pos = mul(bm, pos);
	pos = mul(world, pos);
	output.svpos = mul(mul(proj, view), pos);
	normal.w = 0;
	output.normal = mul(world, normal);
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	// 本ではmul(view, eye)にしているがrayはワールド座標で扱うのでviewは乗算しないのが正しい。実際にスペキュラをデバッグ表示して確認した
	output.ray = normalize(pos.xyz - eye);
	return output;
}