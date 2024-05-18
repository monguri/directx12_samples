#include"Type.hlsli"

//�V�[���Ǘ��p�X���b�g
cbuffer sceneBuffer : register(b1) {
	matrix view;//�r���[
	matrix proj;//�v���W�F�N�V����
	matrix shadow;//�e
	float3 eye;//���_
};

//�A�N�^�[���W�ϊ��p�X���b�g
cbuffer TransBuffer : register(b2) {
	matrix world;
}

//�{�[���s��z��
cbuffer BonesBuffer : register(b3) {
	matrix bones[512];
}


//���_�V�F�[�_(���_��񂩂�K�v�Ȃ��̂����̐l�֓n��)
//�p�C�v���C���ɓ����邽�߂ɂ�SV_POSITION���K�v
BasicType BasicVS(float4 pos:POSITION,float4 normal:NORMAL,float2 uv:TEXCOORD,min16uint2 boneno:BONENO,min16uint weight:WEIGHT,uint instNo: SV_InstanceID) {
	//1280,720�𒼂Ŏg���č\��Ȃ��B
	BasicType output;
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x]*fWeight + 
						bones[boneno.y]*(1.0f - fWeight);
	output.pos = mul(world,	mul(conBone,pos));
	if (instNo > 0) {
		output.pos = mul(shadow, output.pos);
	}
	output.svpos = mul(proj,mul(view, output.pos));
	output.uv = uv;
	normal.w = 0;
	output.normal = mul(world,normal);
	
	output.instNo = instNo;
	//output.uv = uv;
	return output;
}
