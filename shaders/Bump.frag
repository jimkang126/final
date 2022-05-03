#version 330

uniform vec3 u_cam_pos;
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

uniform vec4 u_color;

uniform sampler2D u_texture_2;
uniform vec2 u_texture_2_size;

uniform float u_normal_scaling;
uniform float u_height_scaling;

in vec4 v_position;
in vec4 v_normal;
in vec4 v_tangent;
in vec2 v_uv;

out vec4 out_color;

float h(vec2 uv) {
  // You may want to use this helper function...
  return texture(u_texture_2, uv).r;
}

void main() {
  // YOUR CODE HERE
  
  // (Placeholder code. You will want to replace it.)
  //out_color = (vec4(1, 1, 1, 0) + v_normal) / 2;
  //out_color.a = 1;

  float u = v_uv.x;
  float v = v_uv.y;
  float wd = u_texture_2_size.x;
  float ht = u_texture_2_size.y;

  float dU = ( h( vec2(u + 1/wd, v) ) - h(v_uv) ) * u_height_scaling * u_normal_scaling;
  float dV = ( h( vec2(u, v + 1/ht) ) - h(v_uv) ) * u_height_scaling * u_normal_scaling;
  vec3 n_o = vec3(-dU, -dV, 1.0);

  vec3 t = v_tangent.xyz;
  vec3 n = v_normal.xyz;
  vec3 b = cross(n, t);
  mat3 TBN = mat3(t, b, n);

  vec3 n_d = TBN*n_o;

  vec3 k_d = u_color.xyz;
  vec3 l = u_light_pos - v_position.xyz;
  vec3 L_d = k_d * (u_light_intensity/dot(l, l)) * max(0.0, dot(n_d, normalize(l)));

  vec3 k_s = vec3(0.5);
  vec3 v2 = u_cam_pos - v_position.xyz;
  vec3 h = normalize(v2 + l);
  float p = 100.0;
  vec3 L_s = k_s * (u_light_intensity/dot(l, l)) * pow(max(0.0, dot(n_d, h)), p);

  vec3 k_a = vec3(0.1);
  vec3 I_a = vec3(1.0);

  vec3 L = k_a * I_a + L_d + L_s;

  out_color = vec4(L, 1.0);
}

