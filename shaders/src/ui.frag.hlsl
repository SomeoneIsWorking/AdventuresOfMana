Texture2D<float4> font_atlas : register(t0, space2);
SamplerState font_sampler : register(s0, space2);

cbuffer UiStyle : register(b0, space3) {
    float4 tint : packoffset(c0);
    float4 options : packoffset(c1);
};

float4 main(float2 texcoord : TEXCOORD0) : SV_Target0 {
    float coverage = lerp(1.0, font_atlas.Sample(font_sampler, texcoord).r,
                          options.x);
    return float4(tint.rgb, tint.a * coverage);
}
