//���f���`��p
struct BasicType {
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 tpos : TPOS;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float instNo : INSTNO;
};

//��{�`��p
struct PrimitiveType {
	float4 svpos:SV_POSITION;
	float4 tpos : TPOS;
	float4 normal:NORMAL;
};


//�y���|���S���`��p
struct PeraType {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};