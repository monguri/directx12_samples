#include"Type.hlsli"

//SSAO�����̂��߂����̃V�F�[�_
Texture2D<float4> normtex:register(t1);//1�p�X�ڂ̖@���`��
Texture2D<float> depthtex:register(t6);//1�p�X�ڂ̐[�x�e�N�X�`��

SamplerState smp:register(s0);


//�����W�����ɕK�v
cbuffer SceneBuffer : register(b1) {
	matrix view;//�r���[
	matrix proj;//�v���W�F�N�V����
	matrix invproj;//�t�v���W�F�N�V����
	matrix lightCamera;//���C�g�r���[�v���W�F
	matrix shadow;//�e�s��
	float3 eye;//���_
};

//���݂�UV�l�����ɗ�����Ԃ�
float random(float2 uv) {
	return frac(sin(dot(uv, float2(12.9898f, 78.233f)))*43758.5453f);
}
//SSAO(��Z�p�̖��x�̂ݏ���Ԃ���΂悢)
float SsaoPS(PeraType input) : SV_Target
{
	float dp = depthtex.Sample(smp, input.uv);//���݂�UV�̐[�x

	float w, h, miplevels;
	depthtex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0f / w;
	float dy = 1.0f / h;

	//SSAO
	//���̍��W�𕜌�����
	float4 respos = mul(invproj, float4(input.uv*float2(2, -2) + float2(-1, 1), dp, 1));
	respos.xyz = respos.xyz / respos.w;
	float div = 0.0f;
	float ao = 0.0f;
	float3 norm = normalize((normtex.Sample(smp, input.uv).xyz * 2) - 1);
	const int trycnt = 256;//�V�F�[�_�R���p�C�����x���ꍇ�͂��̐��l�����������Ă��������B
	const float radius = 0.5f;
	if (dp < 1.0f) {
		for (int i = 0; i < trycnt; ++i) {
			float rnd1 = random(float2(i*dx, i*dy)) * 2 - 1;
			float rnd2 = random(float2(rnd1, i*dy)) * 2 - 1;
			float rnd3 = random(float2(rnd2, rnd1)) * 2 - 1;
			float3 omega = normalize(float3(rnd1,rnd2,rnd3));
			omega = normalize(omega);
			//�����̌��ʖ@���̔��Α��Ɍ����Ă��甽�]����
			float dt = dot(norm, omega);
			float sgn = sign(dt);
			omega *= sign(dt);
			//���ʂ̍��W���Ăюˉe�ϊ�����
			float4 rpos = mul(proj, float4(respos.xyz + omega * radius, 1));
			rpos.xyz /= rpos.w;
			dt *= sgn;
			div += dt;
			//�v�Z���ʂ����݂̏ꏊ�̐[�x��艜�ɓ����Ă�Ȃ�Օ�����Ă���Ƃ������Ȃ̂ŉ��Z
			ao += step(depthtex.Sample(smp, (rpos.xy + float2(1, -1))*float2(0.5f, -0.5f)), rpos.z)*dt;
		}
		ao /= div;
	}
	return 1.0f - ao;
}
