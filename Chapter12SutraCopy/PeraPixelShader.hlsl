#include "Type.hlsli"

Texture2D<float4> tex : register(t0);
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
	return float4(Y, Y, Y, 1.0f);
}

float4 PeraInverseColorPS(PeraType input) : SV_TARGET
{
	float4 col = tex.Sample(smp, input.uv);
	return float4(1.0f - col.rgb, 1.0f);
}

float4 PeraDownToneLevelPS(PeraType input) : SV_TARGET
{
	float4 col = tex.Sample(smp, input.uv);
	return float4(col.rgb - fmod(col.rgb, 0.25f), 1.0f);
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
	return ret / 9.0f;
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
	return ret;
}
