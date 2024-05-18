#include"BasicType.hlsli"


//�萔�o�b�t�@0
cbuffer SceneData : register(b0) {
	matrix view;
	matrix proj;//�r���[�v���W�F�N�V�����s��
	float3 eye;
};
cbuffer Transform : register(b1) {
	matrix world;//���[���h�ϊ��s��
	matrix bones[256];//�{�[���s��
}

//�萔�o�b�t�@1
//�}�e���A���p
cbuffer Material : register(b2) {
	float4 diffuse;//�f�B�t���[�Y�F
	float4 specular;//�X�y�L����
	float3 ambient;//�A���r�G���g
};


BasicType BasicVS(float4 pos : POSITION , float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONENO, min16uint weight:WEIGHT) {
	BasicType output;//�s�N�Z���V�F�[�_�֓n���l
	float w = (float)weight / 100.0f;
	matrix bm = bones[boneno[0]] * w + bones[boneno[1]] * (1.0f - w);
	pos = mul(bm, pos);
	pos = mul(world, pos);
	output.svpos = mul(mul(proj,view),pos);//�V�F�[�_�ł͗�D��Ȃ̂Œ���
	output.pos = mul(view, pos);
	normal.w = 0;//�����d�v(���s�ړ������𖳌��ɂ���)
	output.normal = mul(world,mul(bm,normal));//�@���ɂ��{�[���ƃ��[���h�ϊ����s��
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(output.pos.xyz - mul(view,eye));//�����x�N�g��

	return output;
}
