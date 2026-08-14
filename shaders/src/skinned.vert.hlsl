cbuffer Skinning : register(b0, space1) {
    float4x4 model_view_projection : packoffset(c0);
    float4 joints[240] : packoffset(c4);
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float4 weight : TEXCOORD3;
    uint4 incidence : TEXCOORD4;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

float3 skin_position(float4 position, uint bone) {
    uint row = bone * 3;
    return float3(dot(position, joints[row]),
                  dot(position, joints[row + 1]),
                  dot(position, joints[row + 2]));
}

VertexOutput main(VertexInput input) {
    float4 skinned = float4(0.0, 0.0, 0.0, 1.0);
    skinned.xyz += skin_position(float4(input.position, 1.0),
                                 input.incidence.x) * input.weight.x;
    skinned.xyz += skin_position(float4(input.position, 1.0),
                                 input.incidence.y) * (1.0 - input.weight.x);
    VertexOutput output;
    output.position = mul(model_view_projection, skinned);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}
