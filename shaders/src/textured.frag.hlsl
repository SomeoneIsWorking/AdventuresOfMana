Texture2D<float4> color_texture : register(t0, space2);
SamplerState color_sampler : register(s0, space2);

float4 main(float4 color : TEXCOORD0, float2 texcoord : TEXCOORD1)
    : SV_Target0 {
    return color_texture.Sample(color_sampler, texcoord) * color;
}
