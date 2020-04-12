#include "BasicShaderHeader.hlsli"

Output BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	Output output;
	output.pos = pos;
	output.uv = uv;
	return output;
}