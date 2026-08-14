Texture2D<float4> image_texture : register(t0, space2);
SamplerState image_sampler : register(s0, space2);

float4 main(float2 uv : TEXCOORD0) : SV_Target0 {
  return image_texture.Sample(image_sampler, uv);
}
