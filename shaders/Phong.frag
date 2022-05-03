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
  
  // (Placeholder code. You will want to replace it.)
  //out_color = (vec4(1, 1, 1, 0) + v_normal) / 2;
  //out_color.a = 1;

  vec3 k_d = u_color.xyz;
  vec3 l = u_light_pos - v_position.xyz;
  vec3 L_d = k_d * (u_light_intensity/dot(l, l)) * max(0.0, dot(v_normal.xyz, normalize(l)));

  vec3 k_s = vec3(0.5);
  vec3 v = u_cam_pos - v_position.xyz;
  vec3 h = normalize(v + l);
  float p = 100.0;
  vec3 L_s = k_s * (u_light_intensity/dot(l, l)) * pow(max(0.0, dot(v_normal.xyz, h)), p);

  vec3 k_a = vec3(0.1);
  vec3 I_a = vec3(1.0);

  vec3 L = k_a * I_a + L_d + L_s;

  out_color = vec4(L, 1.0);
  out_color.a = 1;
}

