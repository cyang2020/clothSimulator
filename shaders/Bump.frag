#version 330

uniform vec3 u_cam_pos;
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

uniform vec4 u_color;

uniform sampler2D u_texture_3;
uniform vec2 u_texture_3_size;

uniform float u_normal_scaling;
uniform float u_height_scaling;

in vec4 v_position;
in vec4 v_normal;
in vec4 v_tangent;
in vec2 v_uv;

out vec4 out_color;

float h(vec2 uv) {
  // You may want to use this helper function...
  return texture(u_texture_3, uv).x;
}

void main() {
  // YOUR CODE HERE
  
  // (Placeholder code. You will want to replace it.)
    
    vec3 b = cross(v_normal.xyz, v_tangent.xyz);
    mat3 TBN = mat3(v_tangent.xyz, b, v_normal.xyz);
    
    float kh = u_height_scaling;
    float kn = u_normal_scaling;
    float dU = (h(v_uv + vec2(1/u_texture_3_size.x, 0)) - h(v_uv)) * pow (kh, 2);
    float dV = (h(v_uv + vec2(0, 1/u_texture_3_size.y)) - h(v_uv)) * pow (kh, 2);
    vec3 n0 = vec3(-dU, -dV, 1.0);
    vec3 nd = TBN * n0;
    
    float ka = 0.0;
    float kd = 0.8;
    float ks = 0.5;
    float p = 25;
    
    vec4 i = vec4(u_light_intensity, 1.0);
    vec4 l = vec4(u_light_pos, 1.0) - v_position;
    vec4 Ia = vec4(0, 1, 1, 0);
    vec4 h = (vec4(u_cam_pos, 1.0) - v_position + l)/length(vec4(u_cam_pos, 1.0) - v_position + l);
    
    vec4 ambient = ka*Ia;
    vec4 diffuse = kd * i/pow(length(l), 2) * max(0, dot(vec4(nd, 1.0), normalize(l)));
    vec4 specular = ks * i/pow(length(l), 2) * pow(max(0, dot(vec4(nd, 1.0), normalize(h))), p);
    out_color = ambient + diffuse + specular;
    // (Placeholder code. You will want to replace it.)
    out_color.a = 1;
}

