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
// シャドウマップ
Texture2D<float> lightDepthTex : register(t6);
SamplerState smp : register(s0);

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy)
{
	float4 ret = 0;
	// 最上段
	ret += tex.Sample(smp, uv + float2(-2 * dx, -2 * dy)) * 1;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -2 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(0 * dx, -2 * dy)) * 6;
	ret += tex.Sample(smp, uv + float2(1 * dx, -2 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(2 * dx, -2 * dy)) * 1;

	// 一つ上段
	ret += tex.Sample(smp, uv + float2(-2 * dx, -1 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(0 * dx, -1 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(1 * dx, -1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(2 * dx, -1 * dy)) * 4;

	// 中断
	ret += tex.Sample(smp, uv + float2(-2 * dx, 0 * dy)) * 6;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 0 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(0 * dx, 0 * dy)) * 36;
	ret += tex.Sample(smp, uv + float2(1 * dx, 0 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(2 * dx, 0 * dy)) * 6;

	// 一つ上段
	ret += tex.Sample(smp, uv + float2(-2 * dx, 1 * dy)) * 4;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(0 * dx, 1 * dy)) * 24;
	ret += tex.Sample(smp, uv + float2(1 * dx, 1 * dy)) * 16;
	ret += tex.Sample(smp, uv + float2(2 * dx, 1 * dy)) * 4;

	// 最下段
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
	if (input.uv.x < 0.2f && input.uv.y < 0.2f) // 深度表示
	{
		float depth = depthtex.Sample(smp, input.uv * 5.0f);
		depth = 1.0f - pow(depth, 30); // 奥を0、手前を1に
		return float4(depth, depth, depth, 1.0f);
	}
	else if (input.uv.x < 0.2f && input.uv.y < 0.4f) // シャドウマップ表示
	{
		float depth = lightDepthTex.Sample(smp, (input.uv - float2(0.0f, 0.2f)) * 5.0f);
		depth = 1.0f - depth; // 奥を0、手前を1に
		// 平行投影なのでpowしなくても十分みやすい
		return float4(depth, depth, depth, 1.0f);
	}
	else if (input.uv.x < 0.2f && input.uv.y < 0.6f) // 法線表示
	{
		return texNormal.Sample(smp, (input.uv - float2(0.0f, 0.4f)) * 5.0f);
	}
	else if (input.uv.x < 0.2f && input.uv.y < 0.8f)
	{
#if 0 // ディファード実験
		return tex.Sample(smp, (input.uv - float2(0.0f, 0.6f)) * 5.0f); // カラー表示
#else // フォワード
		return texHighLum.Sample(smp, (input.uv - float2(0.0f, 0.6f)) * 5.0f); // 高輝度表示
#endif
	}
	else if (input.uv.x < 0.2f && input.uv.y < 1.0f)
	{
#if 0 // ディファード実験
		return tex.Sample(smp, (input.uv - float2(0.0f, 0.6f)) * 5.0f); // カラー表示
#else // フォワード
		return texShrinkHighLum.Sample(smp, (input.uv - float2(0.0f, 0.8f)) * 5.0f); // 高輝度表示
#endif
	}

#if 0 // ディファード実験
	float4 normal = texNormal.Sample(smp, input.uv);
	normal = normal * 2.0f - 1.0f;

	// とりあえずambientは適当に。ノーマルを使ったランバート反射けで、スペキュラはなし
	const float ambient = 0.25f;
	float3 light = normalize(float3(1.0f, -1.0f, 1.0f));
	float diffB = max(saturate(dot(-light, normal.xyz)), ambient);

	return tex.Sample(smp, input.uv) * float4(diffB, diffB, diffB, 1.0f);
#else // フォワード
	float w, h;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 bloomAccum = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float2 uvSize = float2(1.0f, 0.5f); // TODO:なぜかビューポートの設定にも関わらず、一枚目の幅が半分になってないのでそれに合わせる
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
	// 2 * は、効果をよりはっきり見せるためにやっている
	// ただし色が急激に変わる境界部分ではディザーっぽくなる
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)); // 左上
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)); // 上
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)); // 右上
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)); // 左
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)); // 自分
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)); // 右
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)); // 左下
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)); // 下
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)); // 右下
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
	// 2 * は、効果をよりはっきり見せるためにやっている
	// ただし色が急激に変わる境界部分ではディザーっぽくなる
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 2; // 左上
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)); // 上
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // 右上
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)); // 左
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)); // 自分
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)) * -1; // 右
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // 左下
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)) * -1; // 下
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * -2; // 右下

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
	// 2 * は、効果をよりはっきり見せるためにやっている
	// ただし色が急激に変わる境界部分ではディザーっぽくなる
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // 左上
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)) * -1; // 上
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // 右上
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)) * -1; // 左
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)) * 5; // 自分
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)) * -1; // 右
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // 左下
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)) * -1; // 下
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // 右下

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
	// 2 * は、効果をよりはっきり見せるためにやっている
	// ただし色が急激に変わる境界部分ではディザーっぽくなる
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // 左上
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2 * dy)) * -1; // 上
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // 右上
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0.0f)) * -1; // 左
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)) * 4; // 自分
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0.0f)) * -1; // 右
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // 左下
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2 * dy)) * -1; // 下
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // 右下

	// 輝度を取得
	// PAL : Y = 0.299 R + 0.587 G + 0.114 B
	float Y = dot(ret.rgb, float3(0.299f, 0.587f, 0.114f));
	// エッジをさらに強調
	Y = pow(1.0f - Y, 10);
	// 薄いエッジを出さない
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

	// ノーマルマップを利用したUVディストーション
	float2 nmTex = distex.Sample(smp, input.uv).xy;
	nmTex = nmTex * 2.0f - 1.0f;
	
	float4 col = tex.Sample(smp, input.uv);
	ret += bkweights[0] * col;
	for (int i = 1; i < 8; ++i)
	{
		// 0.1fは、そのままだとノーマルによるUVディストーションが大きすぎるので調整項
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
	// 平行投影なのでpowしなくても十分みやすい
	return float4(depth, depth, depth, 1.0f);
}

float4 PeraBlurPS(PeraType input) : SV_TARGET
{
	float w, h;
	tex.GetDimensions(w, h);
	return Get5x5GaussianBlur(tex, smp, input.uv, 1.0f / w, 1.0f / h);
}

