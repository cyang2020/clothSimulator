#version 330

uniform vec4 u_color;
uniform vec3 u_cam_pos;
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

in vec4 v_position;
in vec4 v_normal;
in vec2 v_uv;

out vec4 out_color;

void main() {
  // YOUR CODE HERE
    float ka = 0.4;
    float kd = 0.9;
    float ks = 0.5;
    float p = 25;
    
    vec4 i = vec4(u_light_intensity, 1.0);
    vec4 l = vec4(u_light_pos, 1.0) - v_position;
    vec4 Ia = vec4(1, 0, 0, 1);
    vec4 h = (vec4(u_cam_pos, 1.0) - v_position + l)/length(vec4(u_cam_pos, 1.0) - v_position + l);

    vec4 ambient = ka*Ia;
    vec4 diffuse = kd * i/pow(length(l), 2) * max(0, dot(v_normal, normalize(l)));
    vec4 specular = ks * i/pow(length(l), 2) * pow(max(0, dot(v_normal, normalize(h))), p);
    out_color = ambient + diffuse + specular;
  // (Placeholder code. You will want to replace it.)
  out_color.a = 1;
}

