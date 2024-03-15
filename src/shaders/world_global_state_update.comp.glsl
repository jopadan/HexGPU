#version 450

#extension GL_GOOGLE_include_directive : require

#include "inc/constants.glsl"
#include "inc/world_global_state.glsl"

layout(push_constant) uniform uniforms_block
{
	float time_of_day; // In range 0 - 1
};

layout(binding= 0, std430) buffer world_global_state_buffer
{
	WorldGlobalState world_global_state;
};

const vec3 c_daytime_sky_color= vec3(1.0, 1.0, 1.5);
const vec3 c_nighttime_sky_color= vec3(0.0, 0.0, 0.0);

void main()
{
	float sun_phase= (c_pi * 2.0) * time_of_day - c_pi * 0.5;
	vec3 sun_direction= vec3(cos(sun_phase), 0.0, sin(sun_phase));

	const float c_twilight_below_horizon= -0.1;
	const float c_twilight_above_horizon= 0.05;

	float daynight_k= 0.0;
	if(sun_direction.z > c_twilight_above_horizon)
		daynight_k= 1.0;
	else if(sun_direction.z < c_twilight_below_horizon)
		daynight_k= 0.0;
	else
		daynight_k= (sun_direction.z - c_twilight_below_horizon) / (c_twilight_above_horizon - c_twilight_below_horizon);

	world_global_state.current_sky_color= vec4(mix(c_nighttime_sky_color, c_daytime_sky_color, daynight_k), 0.0);

	world_global_state.sun_direction= vec4(sun_direction, 0.0);
}
