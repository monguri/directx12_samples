#include "Type.hlsli"

cbuffer Weight : register(b0)
{
	float4 bkweights[2];
	//float bkweights[8];
};
Texture2D<float4> tex : register(t0);
Texture2D<float4> distex : register(t1);
SamplerState smp : register(s0);

float4 PeraUVGradPS(PeraType input) : SV_TARGET
{
	return float4(input.uv, 1.0f, 1.0f);
}

float4 PeraPS(PeraType input) : SV_TARGET
{
	return tex.Sample(smp, input.uv);
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

	float4 ret = 0;
	// �ŏ�i
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 1;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, -2 * dy)) * 4;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, -2 * dy)) * 6;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, -2 * dy)) * 4;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 1;

	// ���i
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -1 * dy)) * 4;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, -1 * dy)) * 16;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, -1 * dy)) * 24;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, -1 * dy)) * 16;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -1 * dy)) * 4;

	// ���f
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0 * dy)) * 6;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 0 * dy)) * 24;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, 0 * dy)) * 36;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, 0 * dy)) * 24;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0 * dy)) * 6;

	// ���i
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 1 * dy)) * 4;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 1 * dy)) * 16;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, 1 * dy)) * 24;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, 1 * dy)) * 16;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 1 * dy)) * 4;

	// �ŉ��i
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 1;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 2 * dy)) * 4;
	ret += tex.Sample(smp, input.uv + float2(0 * dx, 2 * dy)) * 6;
	ret += tex.Sample(smp, input.uv + float2(1 * dx, 2 * dy)) * 4;
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 1;

	ret = ret / 256;
	float4 col = tex.Sample(smp, input.uv);
	return float4(ret.rgb, col.a);
}

float4 PeraHorizontalBokehPS(PeraType input) : SV_TARGET
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;

	for (int i = 0; i < 8; ++i)
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(i * dx, 0.0f));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(-i * dx, 0.0f));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(i * dx, 0.0f));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(-i * dx, 0.0f));
	}

	float4 col = tex.Sample(smp, input.uv);
	return float4(ret.rgb, col.a);
}

float4 PeraVerticalBokehPS(PeraType input) : SV_TARGET
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float w, h;
	tex.GetDimensions(w, h);

	float dy = 1.0f / h;

	for (int i = 0; i < 8; ++i)
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, i * dy));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, -i * dy));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(0.0f, i * dy));
		//ret += bkweights[i] * tex.Sample(smp, input.uv + float2(0.0f, -i * dy));
	}

	float4 col = tex.Sample(smp, input.uv);
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
	
	for (int i = 0; i < 8; ++i)
	{
		// 0.1f�́A���̂܂܂��ƃm�[�}���ɂ��UV�f�B�X�g�[�V�������傫������̂Œ�����
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, i * dy) + nmTex * 0.1f);
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0.0f, -i * dy) + nmTex * 0.1f);
	}

	float4 col = tex.Sample(smp, input.uv);
	return float4(ret.rgb, col.a);
}
