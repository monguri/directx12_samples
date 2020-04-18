#include "BasicShaderHeader.hlsli"

// 定数バッファ
cbuffer cubuff0 : register(b0)
{
	matrix mat;
}

Output BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	Output output;
	output.pos = mul(mat, pos);
	output.uv = uv;
	return output;
}