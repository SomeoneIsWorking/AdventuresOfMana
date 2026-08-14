cbuffer Transform : register(b0, space1) {
    float4x4 model_view_projection : packoffset(c0);
    float4 light_direction_ambient : packoffset(c4);
    float4 light_color_diffuse : packoffset(c5);
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float3 normal : TEXCOORD5;
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(model_view_projection, float4(input.position, 1.0));
    float normal_length = length(input.normal);
    float diffuse = normal_length > 0.0
        ? max(dot(input.normal / normal_length,
                  normalize(light_direction_ambient.xyz)), 0.0)
        : 0.0;
    float3 light = light_direction_ambient.www +
                   light_color_diffuse.xyz * light_color_diffuse.w * diffuse;
    output.color = float4(input.color.rgb * light, input.color.a);
    output.texcoord = input.texcoord;
    return output;
}
