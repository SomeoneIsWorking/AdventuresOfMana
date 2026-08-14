struct VertexInput {
    float2 position : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.texcoord = input.texcoord;
    return output;
}
