#include"Type.hlsli"


//�V�[���Ǘ��p�X���b�g
cbuffer SceneBuffer : register(b1) {
	matrix view;//�r���[
	matrix proj;//�v���W�F�N�V����
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
BasicType BasicVS(float4 pos:POSITION,float4 normal:NORMAL,float2 uv:TEXCOORD,min16uint2 boneno:BONENO,min16uint weight:WEIGHT) {
	//1280,720�𒼂Ŏg���č\��Ȃ��B
	BasicType output;
	float fWeight = float(weight) / 100.0f;
	matrix conBone = bones[boneno.x]*fWeight + 
						bones[boneno.y]*(1.0f - fWeight);

	output.pos = mul(world, 
						mul(conBone,pos)
					);
	output.svpos = mul(proj,mul(view, output.pos));
	output.uv = uv;
	normal.w = 0;
	output.normal = mul(world,normal);
	output.weight = (float)weight/100.0f;
	output.boneno = boneno[0]/122.0;
	//output.uv = uv;
	return output;
}
