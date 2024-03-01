#version 450

layout(binding= 0) uniform uniforms_block_variable
{
	mat4 view_matrix;
	ivec4 build_pos; // w - direction
};

layout(location=0) in vec4 pos;

layout(location = 0) out float f_alpha;

void main()
{
	vec3 pos_corrected= pos.xyz + vec3(build_pos.xyz) * vec3(3.0, 2.0, 1.0);
	pos_corrected.y+= float((build_pos.x ^ 1) & 1);
	gl_Position= view_matrix * vec4(pos_corrected, 1.0);

	// Highlight active build prism side.
	f_alpha= int(pos.w) == build_pos.w ? 0.7 : 0.3;
}
