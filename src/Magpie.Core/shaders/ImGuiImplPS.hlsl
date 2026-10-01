SamplerState sam : register(s0);
Texture2D tex : register(t0);

float4 main(noperspective float2 coord : TEXCOORD, noperspective float4 color : COLOR) : SV_Target {
	return color * float4(1, 1, 1, tex.Sample(sam, coord).r);
}
