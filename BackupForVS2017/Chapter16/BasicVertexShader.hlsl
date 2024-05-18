#include"Type.hlsli"
//�}�e���A���p�X���b�g
cbuffer MaterialBuffer : register(b0) {
	float4 diffuse;
	float power;
	float3 specular;
	float3 ambient;
};
//�V�[���Ǘ��p�X���b�g
cbuffer SceneBuffer : register(b1) {
	matrix view;//�r���[
	matrix proj;//�v���W�F�N�V����
	matrix invproj;//�v���W�F�N�V����
	matrix lightCamera;//���C�g�r���[�v���W�F
	matrix shadow;//�e�s��
	float4 lightVec;//���C�g�x�N�g��
	float3 eye;//���_
	bool isSelfShadow;//�V���h�E�}�b�v�t���O
};

//�A�N�^�[���W�ϊ��p�X���b�g
cbuffer TransBuffer : register(b2) {
	matrix world;
}

//�{�[���s��z��
cbuffer BonesBuffer : register(b3) {
	matrix bones[512];
}


PrimitiveType PrimitiveVS(float4 pos:POSITION, float4 normal : NORMAL) {
	PrimitiveType output;
	output.svpos = mul(proj, mul(view, pos));
	output.tpos = mul(lightCamera, pos);
	output.normal = normal;
	return output;
}
//���_�V�F�[�_(���_��񂩂�K�v�Ȃ��̂����̐l�֓n��)
//�p�C�v���C���ɓ����邽�߂ɂ�SV_POSITION���K�v
BasicType BasicVS(float4 pos:POSITION,float4 normal:NORMAL,float2 uv:TEXCOORD,min16uint2 boneno:BONENO,min16uint weight:WEIGHT,uint instNo:SV_InstanceID) {
	//1280,720�𒼂Ŏg���č\��Ȃ��B
	BasicType output;
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x]*fWeight + 
						bones[boneno.y]*(1.0f - fWeight);

	output.pos = mul(world, 
						mul(conBone,pos)
					);
	output.instNo = (float)instNo;
	output.svpos = mul(proj,mul(view, output.pos));
	output.tpos = mul(lightCamera, output.pos);
//	output.tpos.w = 1;
	output.uv = uv;
	normal.w = 0;
	output.normal = mul(world,mul(conBone,normal));
	return output;
}

//�e�p���_���W�ϊ�
float4 
ShadowVS(float4 pos:POSITION, float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONENO, min16uint weight : WEIGHT) :SV_POSITION{
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x] * fWeight +
						bones[boneno.y] * (1.0f - fWeight);

	pos = mul(world, mul(conBone, pos));
	return  mul(lightCamera, pos);
}


