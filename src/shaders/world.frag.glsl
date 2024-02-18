#version 450

layout(binding= 1) uniform sampler2DArray texture_image;

layout(location= 0) in float f_light;
layout(location= 1) in vec2 f_tex_coord;
layout(location= 2) in flat float f_tex_index;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color= f_light * texture(texture_image, vec3(f_tex_coord, f_tex_index));
}
