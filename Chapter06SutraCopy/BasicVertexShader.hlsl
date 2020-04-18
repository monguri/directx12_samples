#include "BasicShaderHeader.hlsli"

// �萔�o�b�t�@
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