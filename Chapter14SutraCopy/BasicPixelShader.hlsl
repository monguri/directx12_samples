#include "Type.hlsli"
Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3); // CLUT
// �V���h�E�}�b�v
Texture2D<float> lightDepthTex : register(t4);
SamplerState smp : register(s0);
SamplerState smpToon : register(s1);
SamplerComparisonState shadowSmp : register(s2);

cbuffer Material : register(b2)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
};

struct PixelOutput
{
	float4 col : SV_Target0;
	float4 normal : SV_Target1;
};

PixelOutput BasicPS(BasicType input)
{
	if (input.instNo > 0)
	{
		// �e���f���͐^�����ɂ���
		PixelOutput output;
		output.col = float4(0.0f, 0.0f, 0.0f, 1.0f);
		output.normal.rgb = (input.normal.xyz + 1.0f) * 0.5f;
		output.normal.a = 1.0f;
		return output;
	}

	float4 texCol = tex.Sample(smp, input.uv);
	float2 spUV = input.vnormal.xy;
	spUV = (spUV + float2(1.0f, -1.0f)) * float2(0.5f, -0.5f);
	float4 sphCol = sph.Sample(smp, spUV);
	float4 spaCol = spa.Sample(smp, spUV);

#if 0 // �f�B�t�@�[�h����
	PixelOutput output;
	output.col = float4(diffuse * texCol * sphCol + spaCol);
	output.normal.rgb = (input.normal.xyz + 1.0f) * 0.5f;
	output.normal.a = 1.0f;
	return output;
#else // �t�H���[�h
	float3 light = normalize(float3(1.0f, -1.0f, 1.0f));

	float diffuseB = saturate(dot(-light, input.normal.xyz));
	float4 toonDif = toon.Sample(smpToon, float2(0.0f, 1.0f - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float4 ret = max(
		saturate(toonDif * diffuse * texCol * sphCol)
		+ saturate(spaCol * texCol + float4(specularB * specular.rgb, 1)),
		float4(ambient * texCol.rgb, 1)
	);

	// �V���h�E�}�b�v�ɂ��A�`��
	// NDC�ւ̕ϊ�
	float3 posFromLightVP = input.tpos.xyz / input.tpos.w;
	// UV�ւ̕ϊ�
	float2 shadowUV = (posFromLightVP.xy + float2(1.0f, -1.0f)) * float2(0.5f, -0.5f);

	float shadowWeight = 1.0f;
#if 0 // ���͂ł̔�r
	float depthFromLight = lightDepthTex.Sample(smp, shadowUV);
	if (depthFromLight < posFromLightVP.z - 0.005f)
	{
		// �A�ɓ������甼���̋P�x�ɂ���
		shadowWeight = 0.5f; 
	}
#else // SamplerComparisonState���g������r
	float isShadow = lightDepthTex.SampleCmp(shadowSmp, shadowUV, posFromLightVP.z - 0.005f);
	shadowWeight = lerp(0.5f, 1.0f, isShadow);
#endif

	PixelOutput output;
	output.col = float4(ret.rgb * shadowWeight, ret.a);
	output.normal.rgb = (input.normal.xyz + 1.0f) * 0.5f;
	output.normal.a = 1.0f;
	return output;
#endif
}

