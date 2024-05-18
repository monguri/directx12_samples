
struct BasicType {
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 tpos : TPOS;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float instNo : INSTNO;
};



struct PrimitiveType {
	float4 svpos:SV_POSITION;
	float4 tpos : TPOS;
	float4 normal:NORMAL;
};

struct PixelOutput {
	float4 col:SV_TARGET0;//�ʏ�̃����_�����O����
	float4 normal:SV_TARGET1;//�@��
	float4 highLum:SV_TARGET2;//���P�x(High Luminance)
};

//�y���|���S���`��p
struct PeraType {
	float4 pos: SV_POSITION;
	float2 uv:TEXCOORD;
};

struct BlurOutput {
	float4 highLum:SV_TARGET0;//���P�x(High Luminance)
	float4 col:SV_TARGET1;//�ʏ�̃����_�����O����
};
