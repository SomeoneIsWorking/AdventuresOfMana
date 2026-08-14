cbuffer Overlay : register(b0, space1) {
    float4 overlay_color : packoffset(c0);
};

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID) {
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };
    VertexOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.color = overlay_color;
    return output;
}
