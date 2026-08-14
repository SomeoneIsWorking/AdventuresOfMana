cbuffer Skinning : register(b0, space1) {
    float4x4 model_view_projection : packoffset(c0);
    float4 joints[240] : packoffset(c4);
    float4 light_direction_ambient : packoffset(c244);
    float4 light_color_diffuse : packoffset(c245);
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float4 weight : TEXCOORD3;
    uint4 incidence : TEXCOORD4;
    float3 normal : TEXCOORD5;
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

float3 skin_direction(float3 direction, uint bone) {
    uint row = bone * 3;
    float4 vector = float4(direction, 0.0);
    return float3(dot(vector, joints[row]),
                  dot(vector, joints[row + 1]),
                  dot(vector, joints[row + 2]));
}

VertexOutput main(VertexInput input) {
    float4 skinned = float4(0.0, 0.0, 0.0, 1.0);
    skinned.xyz += skin_position(float4(input.position, 1.0),
                                 input.incidence.x) * input.weight.x;
    skinned.xyz += skin_position(float4(input.position, 1.0),
                                 input.incidence.y) * (1.0 - input.weight.x);
    VertexOutput output;
    output.position = mul(model_view_projection, skinned);
    float3 normal = skin_direction(input.normal, input.incidence.x) *
                    input.weight.x;
    normal += skin_direction(input.normal, input.incidence.y) *
              (1.0 - input.weight.x);
    float normal_length = length(normal);
    float diffuse = normal_length > 0.0
        ? max(dot(normal / normal_length,
                  normalize(light_direction_ambient.xyz)), 0.0)
        : 0.0;
    float3 light = light_direction_ambient.www +
                   light_color_diffuse.xyz * light_color_diffuse.w * diffuse;
    output.color = float4(input.color.rgb * light, input.color.a);
    output.texcoord = input.texcoord;
    return output;
}
