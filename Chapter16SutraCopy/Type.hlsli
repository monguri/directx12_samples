// ƒ‚ƒfƒ‹•`‰æ—p
struct BasicType
{
	float4 svpos : SV_POSITION;
	float4 tpos : TPOS;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
	uint instNo : SV_InstanceID;
};

struct PixelOutput
{
	float4 col : SV_Target0;
	float4 normal : SV_Target1;
	float4 highLum : SV_Target2;
};

// ”Âƒ|ƒŠ•`‰æ—p
struct PeraType
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct BlurOutput
{
	float4 highLum : SV_Target0;
	float4 col : SV_Target1;
};
