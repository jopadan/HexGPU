#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/noise.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 16, local_size_z= 1) in;

layout(binding= 0, rgba8) uniform writeonly image2D out_image;

void main()
{
	ivec2 texel_coord= ivec2(gl_GlobalInvocationID.xy);

	int size_log2= 7; // TODO - provide an uniform?

	const int c_octaves= 3;

	int scaler= 0;
	for(int i= 0; i < c_octaves; ++i)
		scaler+= 65536 >> i;

	int noise_val=
		hex_TriangularInterpolatedOctaveNoiseWraped(texel_coord.x, texel_coord.y, 0, c_octaves, size_log2);

	float brightness= float(noise_val) / float(scaler);
	vec4 color= vec4(brightness, brightness, brightness, 0.5);

	imageStore(out_image, texel_coord, color);
}
