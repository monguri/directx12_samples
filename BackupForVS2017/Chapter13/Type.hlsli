//���f���`��p
struct BasicType {
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	uint instNo:SV_InstanceID;
};

//�y���|���S���`��p
struct PeraType {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};