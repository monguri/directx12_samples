#include"Type.hlsli"

Texture2D<float4> tex : register(t0);//�ʏ�J���[
Texture2D<float4> texNormal : register(t1);//�@��
Texture2D<float4> texHighLum : register(t2);//���P�x
Texture2D<float4> texShrinkHighLum : register(t3);//�k���o�b�t�@���P�x
Texture2D<float4> texShrink : register(t4);//�k���o�b�t�@�ʏ�

Texture2D<float4> distTex : register(t5);//�c�݃e�N�X�`��

//�[�x�l�p
Texture2D<float> depthTex : register(t6);//�f�v�X
Texture2D<float> lightDepthTex : register(t7);//���C�g�f�v�X

Texture2D<float> texSSAO : register(t8);//SSAO�p

SamplerState smp : register(s0);//��{�T���v��

cbuffer Weights : register(b0) {
	//CPU����float[8]�œn���ꂽ���̂�
	//�������󂯎�낤�Ƃ����float4[2]��
	//������𓾂Ȃ����߁��̂悤�ȏ������ɂȂ�
	float4 wgts[2];
};

cbuffer PostSetting : register(b1) {
	bool isDebugDisp;//�f�o�b�O�\���t���O
	bool isSSAO;//SSAO�L���t���O
	float3 bloomColor;//�u���[���̒��F
};

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp,float2 uv,float dx,float dy,float4 rect){
	float4 ret = tex.Sample(smp, uv);

	float l1 = -dx, l2 = -2 * dx;
	float r1 = dx, r2 = 2 * dx;
	float u1 = -dy, u2 = -2 * dy;
	float d1 = dy, d2 = 2 * dy;
	l1 = max(uv.x + l1,rect.x)-uv.x;
	l2 = max(uv.x + l2,rect.x)-uv.x;
	r1 = min(uv.x + r1, rect.z-dx) - uv.x;
	r2 = min(uv.x + r2, rect.z-dx) - uv.x;

	u1 = max(uv.y + u1, rect.y) - uv.y;
	u2 = max(uv.y + u2, rect.y) - uv.y;
	d1 = min(uv.y + d1, rect.w-dy) - uv.y;
	d2 = min(uv.y + d2, rect.w-dy) - uv.y;

	return float4((
		  tex.Sample(smp, uv + float2(l2, u2)).rgb
		+ tex.Sample(smp, uv + float2(l1, u2)).rgb*4
		+ tex.Sample(smp, uv + float2(0, u2)).rgb*6
		+ tex.Sample(smp, uv + float2(r1, u2)).rgb*4
		+ tex.Sample(smp, uv + float2(r2, u2)).rgb

		+ tex.Sample(smp, uv + float2(l2,u1)).rgb*4
		+ tex.Sample(smp, uv + float2(l1,u1)).rgb*16
		+ tex.Sample(smp, uv + float2(0,u1)).rgb*24
		+ tex.Sample(smp, uv + float2(r1,u1)).rgb*16
		+ tex.Sample(smp, uv + float2(r2,u1)).rgb*4

		+ tex.Sample(smp, uv + float2(l2, 0)).rgb*6
		+ tex.Sample(smp, uv + float2(l1, 0)).rgb*24
		+ ret.rgb*36
		+ tex.Sample(smp, uv + float2(r1, 0)).rgb*24
		+ tex.Sample(smp, uv + float2(r2, 0)).rgb*6

		+ tex.Sample(smp, uv + float2(l2, d1)).rgb*4
		+ tex.Sample(smp, uv + float2(l1, d1)).rgb*16
		+ tex.Sample(smp, uv + float2(0, d1)).rgb*24
		+ tex.Sample(smp, uv + float2(r1, d1)).rgb*16
		+ tex.Sample(smp, uv + float2(r2, d1)).rgb*4

		+ tex.Sample(smp, uv + float2(l2, d2)).rgb
		+ tex.Sample(smp, uv + float2(l1, d2)).rgb*4
		+ tex.Sample(smp, uv + float2(0, d2)).rgb*6
		+ tex.Sample(smp, uv + float2(r1, d2)).rgb*4
		+ tex.Sample(smp, uv + float2(r2, d2)).rgb
	) / 256.0f, ret.a);
}


float4 PeraPS(PeraType input) : SV_TARGET{
	if (isDebugDisp) {//�f�o�b�O�o��
		if (input.uv.x < 0.2&&input.uv.y < 0.2) {//�[�x�o��
			float depth = depthTex.Sample(smp, input.uv * 5);
			depth = 1.0f - pow(depth, 30);
			return float4(depth, depth, depth, 1);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 0.4) {//���C�g����̐[�x�o��
			float depth = lightDepthTex.Sample(smp, (input.uv - float2(0,0.2)) * 5);
			depth = 1 - depth;
			return float4(depth, depth, depth, 1);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 0.6) {//�@���o��
			return texNormal.Sample(smp, (input.uv - float2(0, 0.4)) * 5);
		}
		else if (input.uv.x < 0.2&&input.uv.y < 0.8) {//AO
			float s = texSSAO.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
			return float4(s,s,s,1);
		}
	}
	float w, h, miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;
	float4 bloomAccum = float4(0, 0, 0, 0);
	float2 uvSize = float2(1, 0.5);
	float2 uvOfst = float2(0, 0);
	for (int i = 0; i < 8; ++i) {
		bloomAccum += Get5x5GaussianBlur(texShrinkHighLum, smp, input.uv*uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
		uvOfst.y += uvSize.y;
		uvSize *= 0.5f;
	}

	float4 col=tex.Sample(smp, input.uv);
	return float4(col.rgb*texSSAO.Sample(smp, input.uv),col.a)+float4(bloomAccum.xyz*bloomColor,bloomAccum.a);
}

float4 VerticalBlurPS(PeraType input) : SV_TARGET{
	float w,h,miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;
	float4 col = tex.Sample(smp,input.uv);
	float3 ret = col.rgb * wgts[0];
	for (int i = 1; i < 8; ++i) {
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, dy*i));
		ret += wgts[i >> 2][i % 4] * tex.Sample(smp, input.uv - float2(0, dy*i));
	}
	return float4(ret,col.a);
}
//���C���e�N�X�`����5x5�u���[�łڂ����s�N�Z���V�F�[�_
BlurOutput BlurPS(PeraType input)
{
	float w,h,miplevels;
	tex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;
	BlurOutput ret;
	ret.col = tex.Sample(smp,input.uv);// Get5x5GaussianBlur(tex, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	ret.highLum= Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	return ret;
}