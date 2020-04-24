#include "BasicShaderHeader.hlsli"
Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3); // CLUT
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

	float diffuseB = saturate(dot(-light, input.normal.xyz));
	float4 toonDif = toon.Sample(smp, float2(0.0f, 1.0f - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1.0f, -1.0f)) * float2(0.5f, -0.5f);

	float4 texColor = tex.Sample(smp, input.uv);

	return max(
		saturate(toonDif * diffuse * texColor * sph.Sample(smp, sphereMapUV))
		+ saturate(spa.Sample(smp, sphereMapUV) * texColor + float4(specularB * specular.rgb, 1)),
		float4(ambient * texColor.rgb, 1)
	);
}