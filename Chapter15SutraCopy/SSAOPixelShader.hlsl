#include "Type.hlsli"

Texture2D<float4> normtex : register(t1);
Texture2D<float> depthtex : register(t6);

SamplerState smp : register(s0);

cbuffer SceneData : register(b0)
{
	matrix view;
	matrix proj;
	matrix invviewproj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
}

float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float SsaoPS(PeraType input) : SV_Target
{
	// 元のワールド座標を復元する
	// TODO:本ではinvprojを使っていてビュー座標を復元してるがnormtexがワールド座標なのでおかしなことになる
	float dp = depthtex.Sample(smp, input.uv);
	float4 respos = mul(invviewproj, float4(input.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), dp, 1.0f));
	// TODO:割るんだっけ？
	respos.xyz = respos.xyz / respos.w;

	float w, h;
	depthtex.GetDimensions(w, h);
	float dx = 1.0f / w;
	float dy = 1.0f / w;

	// 最後に割り算するための遮蔽が全くない時の総和
	float div = 0.0f;

	// 遮蔽率
	float ao = 0.0f;

	// 法線ベクトル
	// 本ではnormalizeしてるが、BasicPixelShaderでもPMDから取得した法線をそのまま
	// normalizeせずランバート反射計算に使ってるのでそのままでいいと思う
	float3 norm = normtex.Sample(smp, input.uv).xyz * 2.0f - 1.0f;

	const int trycnt = 32;
	const float radius = 0.5f;

	if (dp < 1.0f) // Far面をつきぬけたピクセルについては計算しない
	{
		[unroll]
		for (int i = 0; i < trycnt; ++i)
		{
			float rnd1 = random(float2(i * dx, i * dy)) * 2.0f - 1.0f;
			float rnd2 = random(float2(rnd1, i * dy)) * 2.0f - 1.0f;
			float rnd3 = random(float2(rnd2, rnd1)) * 2.0f - 1.0f;
			// ランダムレイ
			float3 omega = normalize(float3(rnd1, rnd2, rnd3));

			// cosThetaにあたる
			float dt = dot(norm, omega);
			float sgn = sign(dt);

			// 裏面方向だった場合はランダムレイを逆方向にする
			omega *= sgn;
			dt *= sgn;

			div += dt;

			// ランダムレイで半径だけ移動したワールド位置からNDC位置を求める
			float4 rpos = mul(proj, mul(view, float4(respos.xyz + omega * radius, 1.0f)));
			rpos.xyz /= rpos.w;

			// ランダムレイの半球面上の点のデプスが、実際のそのピクセルのデプスより大きければ遮蔽されているのでcosThetaを加算
			ao += step(
				depthtex.Sample(smp, (rpos.xy + float2(1.0f, -1.0f)) * float2(0.5f, -0.5f)),
				rpos.z
			) * dt;
		}

		ao /= div;
	}

	return 1.0f - ao;
}

