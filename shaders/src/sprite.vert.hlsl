struct Output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

Output main(float2 position : TEXCOORD0, float2 uv : TEXCOORD1) {
  Output output;
  output.position = float4(position, 0.0, 1.0);
  output.uv = uv;
  return output;
}
