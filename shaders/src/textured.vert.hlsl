cbuffer Transform : register(b0, space1) {
    float4x4 model_view_projection : packoffset(c0);
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(model_view_projection, float4(input.position, 1.0));
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}
