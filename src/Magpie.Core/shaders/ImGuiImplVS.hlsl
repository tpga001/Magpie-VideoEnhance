cbuffer vertexBuffer : register(b0) {
	float4x4 projectionMatrix;
};

void main(
	float2 pos : POSITION,
	float2 coord : TEXCOORD,
	float4 color : COLOR,
	out noperspective float2 outCoord : TEXCOORD,
	out noperspective float4 outColor : COLOR,
	out noperspective float4 outPos : SV_POSITION
) {
	outPos = mul(projectionMatrix, float4(pos, 0, 1));
	outCoord = coord;
	outColor = color;
}
