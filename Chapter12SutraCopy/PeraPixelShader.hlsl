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
	ret += tex.Sample(smp, input.uv + float2(-dx, -dy)); // ç∂è„
	ret += tex.Sample(smp, input.uv + float2(0.0f, -dy)); // è„
	ret += tex.Sample(smp, input.uv + float2(dx, -dy)); // âEè„
	ret += tex.Sample(smp, input.uv + float2(-dx, 0.0f)); // ç∂
	ret += tex.Sample(smp, input.uv + float2(0.0f, 0.0f)); // é©ï™
	ret += tex.Sample(smp, input.uv + float2(dx, 0.0f)); // âE
	ret += tex.Sample(smp, input.uv + float2(-dx, dy)); // ç∂â∫
	ret += tex.Sample(smp, input.uv + float2(0.0f, dy)); // â∫
	ret += tex.Sample(smp, input.uv + float2(dx, dy)); // âEâ∫
	return ret / 9.0f;
}

