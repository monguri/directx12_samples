// モデル描画用
struct BasicType
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
};

// 板ポリ描画用
struct PeraType
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};
