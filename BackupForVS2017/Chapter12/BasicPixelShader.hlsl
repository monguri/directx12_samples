#include"Type.hlsli"
SamplerState smp : register(s0);
SamplerState clutSmp : register(s1);

//�V�[���Ǘ��p�X���b�g
cbuffer SceneBuffer : register(b1) {
	matrix view;//�r���[
	matrix proj;//�v���W�F�N�V����
	float3 eye;//���_
};

//�}�e���A���p�X���b�g
cbuffer MaterialBuffer : register(b0) {
	float4 diffuse;
	float power;
	float3 specular;
	float3 ambient;
};
Texture2D<float4> tex : register(t0);//�ʏ�e�N�X�`��
Texture2D<float4> sph : register(t1);//�X�t�B�A�}�b�v(��Z)
Texture2D<float4> spa : register(t2);//�X�t�B�A�}�b�v(���Z)
Texture2D<float4> toon : register(t3);//�g�D�[���e�N�X�`��



//�s�N�Z���V�F�[�_
float4 BasicPS(BasicType input):SV_TARGET {
	float3 eyeray = normalize(input.pos-eye);
	float3 light = normalize(float3(1,-1,1));
	float3 rlight = reflect(light, input.normal);
		
	//�X�y�L�����P�x
	float p = saturate(dot(rlight, -eyeray));

	//MSDN��pow�̃h�L�������g�ɂ���
	//p=0��������p==0&&power==0�̂Ƃ�NAN�̉\����
	//���邽�߁A�O�̂��߈ȉ��̂悤�ȃR�[�h�ɂ��Ă���
	//https://docs.microsoft.com/ja-jp/windows/win32/direct3dhlsl/dx-graphics-hlsl-pow
	float specB = 0;
	if (p > 0 && power > 0) {
		specB=pow(p, power);
	}

	//�f�B�t���[�Y���邳		
	float diffB = dot(-light, input.normal);
	float4 toonCol = toon.Sample(clutSmp, float2(0, 1 - diffB));
	

	float4 texCol =  tex.Sample(smp, input.uv);
	
	//col.rgb= pow(col.rgb, 1.0 / 2.2);
	float2 spUV= (input.normal.xy
		*float2(1, -1) //�܂��㉺�����Ђ����肩����
		+ float2(1, 1)//(1,1)�𑫂���-1�`1��0�`2�ɂ���
		) / 2;
	float4 sphCol = sph.Sample(smp, spUV);
	float4 spaCol = spa.Sample(smp, spUV);
	
	//float4 ret= spaCol + sphCol * texCol*toonCol*diffuse + float4(ambient*0.6, 1)
	float4 ret = float4((spaCol + sphCol * texCol * toonCol*diffuse).rgb,diffuse.a)
		+ float4(specular*specB, 1);
	
	//float rim = pow(1 - dot(input.normal, -eyeray),2);
	//return float4(ret.rgb+float3(rim,rim*0.2,rim*0.2),ret.a);
	//return float4(pow(ret.rgb,2.2), ret.a);
	return ret;
}