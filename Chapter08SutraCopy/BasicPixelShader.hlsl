#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
SamplerState smp : register(s0);

cbuffer Material : register(b1)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
};

float4 BasicPS(Output input) : SV_Target
{
	float3 light = normalize(float3(1.0f, -1.0f, 1.0f));
	float brightness = dot(-light, input.normal.xyz);
	float2 sphereMapUV = input.vnormal.xy;

	float4 texColor = tex.Sample(smp, input.uv);
	return float4(brightness, brightness, brightness, 1) * diffuse * texColor * sph.Sample(smp, sphereMapUV) + spa.Sample(smp, sphereMapUV) + float4(ambient * texColor.rgb, 1);
}