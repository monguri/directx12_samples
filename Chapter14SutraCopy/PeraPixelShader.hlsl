#include "Type.hlsli"

cbuffer Weight : register(b0)
{
	float4 bkweights[2];
	//float bkweights[8];
};
Texture2D<float4> tex : register(t0);
Texture2D<float4> texNormal : register(t1);
Texture2D<float4> texHighLum : register(t2);
Texture2D<float4> texShrinkHighLum : register(t3);
Texture2D<float4> distex : register(t4);
Texture2D<float> depthtex : register(t5);
// �V���h�E�}�b�v
Texture2D<float> lightDepthTex : register(t6);
SamplerState smp : register(s0);

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy)
{
	float4 ret = 0;
	// �ŏ�i
	ret += tex.Sample(smp, uv + float2(-2 * dx, -2 * dy)) * 1;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -2 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(0 * dx, -2 * dy)) * 6;
	ret += tex.Sample(smp, uv + float2(1 * dx, -2 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(2 * dx, -2 * dy)) * 1;

	// ���i
	ret += tex.Sample(smp, uv + float2(-2 * dx, -1 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(0 * dx, -1 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(1 * dx, -1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(2 * dx, -1 * dy)) * 4;

	// ���f
	ret += tex.Sample(smp, uv + float2(-2 * dx, 0 * dy)) * 6;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 0 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(0 * dx, 0 * dy)) * 36;
	ret += tex.Sample(smp, uv + float2(1 * dx, 0 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(2 * dx, 0 * dy)) * 6;

	// ���i
	ret += tex.Sample(smp, uv + float2(-2 * dx, 1 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(0 * dx, 1 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(1 * dx, 1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(2 * dx, 1 * dy)) * 4;

	// �ŉ��i
	ret += tex.Sample(smp, uv + float2(-2 * dx, 2 * dy)) * 1;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 2 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(0 * dx, 2 * dy)) * 6;
	ret += tex.Sample(smp, uv + float2(1 * dx, 2 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(2 * dx, 2 * dy)) * 1;

	ret = ret / 256;
	float4 col = tex.Sample(smp, uv);
	return float4(ret.rgb, col.a);
}

float4 PeraUVGradPS(PeraType input) : SV_TARGET
{
	return float4(input.uv, 1.0f, 1.0f);
}

float4 PeraPS(PeraType input) : SV_TARGET
{
	if (input.uv.x < 0.2f && input.uv.y < 0.2f) // �[�x�\��
	{
		float depth = depthtex.Sample(smp, input.uv * 5.0f);
		depth = 1.0f - pow(depth, 30); // ����0�A��O��1��
		return float4(depth, depth, depth, 1.0f);
	}
	else if (input.uv.x < 0.2f && input.uv.y < 0.4f) // �V���h�E�}�b�v�\��
	{
		float depth = lightDepthTex.Sample(smp, (input.uv - float2(0.0f, 0.2f)) * 5.0f);
		depth = 1.0f - depth; // ����0�A��O��1��
		// ���s���e�Ȃ̂�pow���Ȃ��Ă��\���݂₷��
		return float4(depth, depth, depth, 1.0f);
	}
	else if (input.uv.x < 0.2f && input.uv.y < 0.6f) // �@���\��
	{
		return texNormal.Sample(smp, (input.uv - float2(0.0f, 0.4f)) * 5.0f);
	}
	else if (input.uv.x < 0.2f && input.uv.y < 0.8f)
	{
#if 0 // �f�B�t�@�[�h����
		return tex.Sample(smp, (input.uv - float2(0.0f, 0.6f)) * 5.0f); // �J���[�\��
#else // �t�H���[�h
		return texHighLum.Sample(smp, (input.uv - float2(0.0f, 0.6f)) * 5.0f); // ���P�x�\��
#endif
	}
	else if (input.uv.x < 0.2f && input.uv.y < 1.0f)
	{
#if 0 // �f�B�t�@�[�h����
		return tex.Sample(smp, (input.uv - float2(0.0f, 0.6f)) * 5.0f); // �J���[�\��
#else // �t�H���[�h
		return texShrinkHighLum.Sample(smp, (input.uv - float2(0.0f, 0.8f)) * 5.0f); // ���P�x�\��
#endif
	}

#if 0 // �f�B�t�@�[�h����
	float4 normal = texNormal.Sample(smp, input.uv);
	normal = normal * 2.0f - 1.0f;

	// �Ƃ肠����ambient�͓K���ɁB�m�[�}�����g���������o�[�g���˂��ŁA�X�y�L�����͂Ȃ�
	const float ambient = 0.25f;
	float3 light = normalize(float3(1.0f, -1.0f, 1.0f));
	float diffB = max(saturate(dot(-light, normal.xyz)), ambient);

	return tex.Sample(smp, input.uv) * float4(diffB, diffB, diffB, 1.0f);
#else // �t�H���[�h
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 bloomAccum = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float2 uvSize = float2(1.0f, 0.5f); // TODO:�Ȃ����r���[�|�[�g�̐ݒ�ɂ��ւ�炸�A�ꖇ�ڂ̕��������ɂȂ��ĂȂ��̂ł���ɍ��킹��
	float2 uvOfst = float2(0.0f, 0.0f);
	[unroll]
	for (int i = 0; i < 8; ++i)
	{
		bloomAccum += Get5x5GaussianBlur(texShrinkHighLum, smp, input.uv * uvSize + uvOfst, dx, dy);
		uvOfst.y += uvSize.y;
		uvSize *= 0.5f;
	}

	return tex.Sample(smp, input.uv) + Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy) + saturate(bloomAccum);
#endif
}

float4 PeraGrayscalePS(PeraType input) : SV_TARGET
{
	float4 col = tex.Sample(smp, input.uv);
	// PAL : Y = 0.299 R + 0.587 G + 0.114 B
	float Y = dot(col.rgb, float3(0.299f, 0.587f, 0.114f));
	return float4(Y, Y, Y, col.a);
}

float4 PeraInverseColorPS(PeraType input) : SV_TARGET
{
	float4 col = tex.Sample(smp, input.uv);
	return float4(1.0f - col.rgb, col.a);
}

float4 PeraDownToneLevelPS(PeraType input) : SV_TARGET
{
	float4 col = tex.Sample(smp, input.uv);
	return float4(col.rgb - fmod(col.rgb, 0.25f), col.a);
}

float4 Pera9AveragePS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 ret = 0;
	// 2 * �́A���ʂ����͂����茩���邽�߂ɂ���Ă���
	// �������F���}���ɕς�鋫�E�����ł̓f�B�U�[���ۂ��Ȃ�
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)); // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)); // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)); // �E��
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)); // ��
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)); // ����
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)); // �E
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)); // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)); // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)); // �E��
	ret /= 9.0f;
	float4 col = tex.Sample(smp, input.uv);
	return float4(ret.rgb, col.a);
}

float4 PeraEmbossPS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 ret = 0;
	// 2 * �́A���ʂ����͂����茩���邽�߂ɂ���Ă���
	// �������F���}���ɕς�鋫�E�����ł̓f�B�U�[���ۂ��Ȃ�
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 2; // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)); // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // �E��
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)); // ��
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)); // ����
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)) * -1; // �E
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * -2; // �E��

	float4 col = tex.Sample(smp, input.uv);
	return float4(ret.rgb, col.a);
}

float4 PeraSharpnessPS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 ret = 0;
	// 2 * �́A���ʂ����͂����茩���邽�߂ɂ���Ă���
	// �������F���}���ɕς�鋫�E�����ł̓f�B�U�[���ۂ��Ȃ�
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // �E��
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)) * 5; // ����
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)) * -1; // �E
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // �E��

	float4 col = tex.Sample(smp, input.uv);
	return float4(ret.rgb, col.a);
}

float4 PeraEdgeDetectionPS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 ret = 0;
	// 2 * �́A���ʂ����͂����茩���邽�߂ɂ���Ă���
	// �������F���}���ɕς�鋫�E�����ł̓f�B�U�[���ۂ��Ȃ�
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // �E��
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)) * 4; // ����
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)) * -1; // �E
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // ����
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)) * -1; // ��
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // �E��

	// �P�x���擾
	// PAL : Y = 0.299 R + 0.587 G + 0.114 B
	float Y = dot(ret.rgb, float3(0.299f, 0.587f, 0.114f));
	// �G�b�W������ɋ���
	Y = pow(1.0f - Y, 10);
	// �����G�b�W���o���Ȃ�
	Y = step(0.2f, Y);

	float4 col = tex.Sample(smp, input.uv);
	return float4(Y, Y, Y, col.a);
}

float4 PeraGaussianBlurPS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	return Get5x5GaussianBlur(tex, smp, input.uv, dx, dy);
}

float4 PeraHorizontalBokehPS(PeraType input) : SV_TARGET
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;

	float4 col = tex.Sample(smp, input.uv);
	ret += bkweights[0] * col;
	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(i * dx, 0.0f));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(-i * dx, 0.0f));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(i * dx, 0.0f));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(-i * dx, 0.0f));
	}

	return float4(ret.rgb, col.a);
}

float4 PeraVerticalBokehPS(PeraType input) : SV_TARGET
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float w, h;
	tex.GetDimensions(w, h);

	float dy = 1.0f / h;

	float4 col = tex.Sample(smp, input.uv);
	ret += bkweights[0] * col;
	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, i * dy));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, -i * dy));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(0.0f, i * dy));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(0.0f, -i * dy));
	}

	return float4(ret.rgb, col.a);
}

float4 PeraVerticalBokehAndDistortionPS(PeraType input) : SV_TARGET
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float w, h;
	tex.GetDimensions(w, h);

	float dy = 1.0f / h;

	// �m�[�}���}�b�v�𗘗p����UV�f�B�X�g�[�V����
	float2 nmTex = distex.Sample(smp, input.uv).xy;
	nmTex = nmTex * 2.0f - 1.0f;
	
	float4 col = tex.Sample(smp, input.uv);
	ret += bkweights[0] * col;
	for (int i = 1; i < 8; ++i)
	{
		// 0.1f�́A���̂܂܂��ƃm�[�}���ɂ��UV�f�B�X�g�[�V�������傫������̂Œ�����
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, i * dy) + nmTex * 0.1f);
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, -i * dy) + nmTex * 0.1f);
	}

	return float4(ret.rgb, col.a);
}

float4 PeraDepthDebugPS(PeraType input) : SV_TARGET
{
	float depth = depthtex.Sample(smp, input.uv);
	depth = pow(depth, 20);
	return float4(depth, depth, depth, 1.0f);
}

float4 PeraDepthFromLightDebugPS(PeraType input) : SV_TARGET
{
	float depth = lightDepthTex.Sample(smp, input.uv);
	// ���s���e�Ȃ̂�pow���Ȃ��Ă��\���݂₷��
	return float4(depth, depth, depth, 1.0f);
}

float4 PeraBlurPS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);
	return Get5x5GaussianBlur(tex, smp, input.uv, 1.0f / w, 1.0f / h);
}

