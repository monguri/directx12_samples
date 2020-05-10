#include "Type.hlsli"

Texture2D<float4> normtex : register(t1);
Texture2D<float> depthtex : register(t6);

SamplerState smp : register(s0);

cbuffer SceneData : register(b0)
{
	matrix view;
	matrix proj;
	matrix invviewproj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
}

float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float SsaoPS(PeraType input) : SV_Target
{
	// ���̃��[���h���W�𕜌�����
	// TODO:�{�ł�invproj���g���Ă��ăr���[���W�𕜌����Ă邪normtex�����[���h���W�Ȃ̂ł������Ȃ��ƂɂȂ�
	float dp = depthtex.Sample(smp, input.uv);
	float4 respos = mul(invviewproj, float4(input.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), dp, 1.0f));
	// TODO:����񂾂����H
	respos.xyz = respos.xyz / respos.w;

	float w, h;
	depthtex.GetDimensions(w, h);
	float dx = 1.0f / w;
	float dy = 1.0f / w;

	// �Ō�Ɋ���Z���邽�߂̎Օ����S���Ȃ����̑��a
	float div = 0.0f;

	// �Օ���
	float ao = 0.0f;

	// �@���x�N�g��
	// �{�ł�normalize���Ă邪�ABasicPixelShader�ł�PMD����擾�����@�������̂܂�
	// normalize���������o�[�g���ˌv�Z�Ɏg���Ă�̂ł��̂܂܂ł����Ǝv��
	float3 norm = normtex.Sample(smp, input.uv).xyz * 2.0f - 1.0f;

	const int trycnt = 32;
	const float radius = 0.5f;

	if (dp < 1.0f) // Far�ʂ����ʂ����s�N�Z���ɂ��Ă͌v�Z���Ȃ�
	{
		[unroll]
		for (int i = 0; i < trycnt; ++i)
		{
			float rnd1 = random(float2(i * dx, i * dy)) * 2.0f - 1.0f;
			float rnd2 = random(float2(rnd1, i * dy)) * 2.0f - 1.0f;
			float rnd3 = random(float2(rnd2, rnd1)) * 2.0f - 1.0f;
			// �����_�����C
			float3 omega = normalize(float3(rnd1, rnd2, rnd3));

			// cosTheta�ɂ�����
			float dt = dot(norm, omega);
			float sgn = sign(dt);

			// ���ʕ����������ꍇ�̓����_�����C���t�����ɂ���
			omega *= sgn;
			dt *= sgn;

			div += dt;

			// �����_�����C�Ŕ��a�����ړ��������[���h�ʒu����NDC�ʒu�����߂�
			float4 rpos = mul(proj, mul(view, float4(respos.xyz + omega * radius, 1.0f)));
			rpos.xyz /= rpos.w;

			// �����_�����C�̔����ʏ�̓_�̃f�v�X���A���ۂ̂��̃s�N�Z���̃f�v�X���傫����ΎՕ�����Ă���̂�cosTheta�����Z
			ao += step(
				depthtex.Sample(smp, (rpos.xy + float2(1.0f, -1.0f)) * float2(0.5f, -0.5f)),
				rpos.z
			) * dt;
		}

		ao /= div;
	}

	return 1.0f - ao;
}

