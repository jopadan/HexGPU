#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/keyboard.glsl"
#include "inc/matrix.glsl"
#include "inc/mouse.glsl"
#include "inc/player_state.glsl"
#include "inc/player_world_window.glsl"
#include "inc/world_blocks_external_update_queue.glsl"

layout(push_constant) uniform uniforms_block
{
	float aspect;
	float time_delta_s;
	uint keyboard_state;
	uint mouse_state;
};

layout(binding= 1, std430) buffer player_state_buffer
{
	PlayerState player_state;
};

layout(binding= 2, std430) buffer world_blocks_external_update_queue_buffer
{
	WorldBlocksExternalUpdateQueue world_blocks_external_update_queue;
};

layout(binding= 3, std430) buffer player_world_window_buffer
{
	PlayerWorldWindow player_world_window;
};

void MovePlayer()
{
	const float c_walk_speed= 4.0;
	const float c_sprint_speed= 16.0;
	const float c_angle_speed= 1.0;

	const float c_jump_speed= 0.8 * c_walk_speed;
	const float c_sprint_jump_speed= 0.8 * c_sprint_speed;

	float speed= c_walk_speed;
	float jump_speed= c_jump_speed;
	if((keyboard_state & c_key_mask_sprint) != 0)
	{
		speed= c_sprint_speed;
		jump_speed= c_sprint_jump_speed;
	}

	vec3 forward_vector= vec3(-sin(player_state.angles.x), +cos(player_state.angles.x), 0.0);
	vec3 left_vector= vec3(cos(player_state.angles.x), sin(player_state.angles.x), 0.0);

	vec3 move_vector= vec3(0.0, 0.0, 0.0);

	if((keyboard_state & c_key_mask_forward) != 0)
		move_vector+= forward_vector;
	if((keyboard_state & c_key_mask_backward) != 0)
		move_vector-= forward_vector;
	if((keyboard_state & c_key_mask_step_left) != 0)
		move_vector+= left_vector;
	if((keyboard_state & c_key_mask_step_right) != 0)
		move_vector-= left_vector;

	const float move_vector_length= length(move_vector);
	if(move_vector_length > 0.0)
		player_state.pos.xyz+= move_vector * (time_delta_s * speed / move_vector_length);

	if((keyboard_state & c_key_mask_fly_up) != 0)
		player_state.pos.z+= time_delta_s * jump_speed;
	if((keyboard_state & c_key_mask_fly_down) != 0)
		player_state.pos.z-= time_delta_s * jump_speed;

	if((keyboard_state & c_key_mask_rotate_left) != 0)
		player_state.angles.x+= time_delta_s * c_angle_speed;
	if((keyboard_state & c_key_mask_rotate_right) != 0)
		player_state.angles.x-= time_delta_s * c_angle_speed;

	if((keyboard_state & c_key_mask_rotate_up) != 0)
		player_state.angles.y+= time_delta_s * c_angle_speed;
	if((keyboard_state & c_key_mask_rotate_down) != 0)
		player_state.angles.y-= time_delta_s * c_angle_speed;

	while(player_state.angles.x > +c_pi)
		player_state.angles.x-= 2.0 * c_pi;
	while(player_state.angles.x < -c_pi)
		player_state.angles.x+= 2.0 * c_pi;

	player_state.angles.y= max(-0.5 * c_pi, min(player_state.angles.y, +0.5 * c_pi));
}

void UpdateBuildBlockType()
{
	if((mouse_state & c_mouse_mask_wheel_up_clicked) != 0)
	{
		++player_state.build_block_type;
	}
	if((mouse_state & c_mouse_mask_wheel_down_clicked) != 0)
	{
		if(player_state.build_block_type - 1 == int(c_block_type_air))
			player_state.build_block_type= uint8_t(c_num_block_types - 1);
		else
			--player_state.build_block_type;
	}

	if(player_state.build_block_type <= 0 || player_state.build_block_type >= c_num_block_types)
		player_state.build_block_type= c_block_type_spherical_block;
}

uint8_t GetBuildDirection(ivec3 last_grid_pos, ivec3 grid_pos)
{
	if(last_grid_pos.z < grid_pos.z)
		return c_direction_up;
	else if(last_grid_pos.z > grid_pos.z)
		return c_direction_down;
	else if(last_grid_pos.x < grid_pos.x)
	{
		if((grid_pos.x & 1) == 0)
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_north_east;
			else
				return c_direction_south_east;
		}
		else
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_south_east;
			else
				return c_direction_north_east;
		}
	}
	else if(last_grid_pos.x > grid_pos.x)
	{
		if((grid_pos.x & 1) == 0)
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_north_west;
			else
				return c_direction_south_west;
		}
		else
		{
			if(last_grid_pos.y == grid_pos.y)
				return c_direction_south_west;
			else
				return c_direction_north_west;
		}
	}
	else
	{
		if(last_grid_pos.y < grid_pos.y)
			return c_direction_north;
		else
			return c_direction_south;
	}
}

// Result vector is normalized.
vec3 CalculateCameraDirection(vec2 angles)
{
	float elevation_sin= sin(angles.y);
	float elevation_cos= cos(angles.y);

	return vec3(-sin(angles.x) * elevation_cos, +cos(angles.x) * elevation_cos, elevation_sin);
}

void UpdateBuildPos()
{
	// For now just trace hex grid with fixed step to find nearest intersection.
	// TODO - perform more precise and not so slow search.
	const float c_build_radius= 5.0;
	const int c_num_steps= 64;

	vec3 player_dir_normalized= CalculateCameraDirection(player_state.angles.xy);
	vec3 cur_pos= player_state.pos.xyz;
	vec3 step_vec= player_dir_normalized * (c_build_radius / float(c_num_steps));

	ivec3 last_grid_pos= ivec3(-1, -1, -1);
	for(int i= 0; i < c_num_steps; ++i, cur_pos+= step_vec)
	{
		ivec3 grid_pos= ivec3(GetHexogonCoord(cur_pos.xy), int(floor(cur_pos.z)));
		if(grid_pos == last_grid_pos)
			continue; // Located in the same grid cell.

		ivec3 pos_in_window= grid_pos - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window) &&
			player_world_window.window_data[GetAddressOfBlockInPlayerWorldWindow(pos_in_window)] != c_block_type_air)
		{
			// Reached non-air block.
			// Destroy position is in this block, build position is in previous block.
			player_state.destroy_pos.xyz= grid_pos;
			player_state.build_pos.xyz= last_grid_pos;
			player_state.build_pos.w= int(GetBuildDirection(last_grid_pos, grid_pos));
			return;
		}

		last_grid_pos= grid_pos;
	}

	// Reached the end of the search - make build and destroy positions infinite.
	player_state.build_pos= ivec4(-1, -1, -1, 0);
	player_state.destroy_pos= ivec4(-1, -1, -1, 0);
}

void PushUpdateIntoQueue(WorldBlockExternalUpdate update)
{
	// Use atomic in case someone else pushes updates into this queue.
	uint index= atomicAdd(world_blocks_external_update_queue.num_updates, 1);
	if(index < c_max_world_blocks_external_updates)
		world_blocks_external_update_queue.updates[index]= update;
}

void UpdateBlocksMatrix()
{
	const float z_near= 0.125;
	const float z_far= 1024.0;
	const float fov_deg= 75.0;

	const float fov= radians(fov_deg);

	float fov_y= fov;

	mat4 perspective= MakePerspectiveProjectionMatrix(aspect, fov, z_near, z_far);
	mat4 basis_change= MakePerspectiveChangeBasisMatrix();
	mat4 rotate_x= MakeRotationXMatrix(-player_state.angles.y);
	mat4 rotate_z= MakeRotationZMatrix(-player_state.angles.x);
	mat4 translate= MateTranslateMatrix(-player_state.pos.xyz);
	mat4 blocks_scale= MakeScaleMatrix(vec3(0.5 / sqrt(3.0), 0.5, 1.0));

	player_state.blocks_matrix= perspective * basis_change * rotate_x * rotate_z * translate * blocks_scale;
}

void UpdateNextPlayerWorldWindowOffset()
{
	player_state.next_player_world_window_offset=
		ivec4(
			(GetHexogonCoord(player_state.pos.xy) - c_player_world_window_size.xy / 2) & 0xFFFFFFFE,
			int(floor(player_state.pos.z)) - c_player_world_window_size.z / 2,
			0);
}

void main()
{
	MovePlayer();
	UpdateBuildBlockType();

	UpdateBuildPos();

	// Perform building/destroying.
	// Update build pos if building/destroying was triggered.
	if((mouse_state & c_mouse_mask_r_clicked) != 0)
	{
		ivec3 pos_in_window= player_state.build_pos.xyz - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			int address_in_window= GetAddressOfBlockInPlayerWorldWindow(pos_in_window);

			WorldBlockExternalUpdate update;
			update.position= ivec4(player_state.build_pos.xyz, 0);
			update.old_block_type= player_world_window.window_data[address_in_window];
			update.new_block_type= player_state.build_block_type;
			PushUpdateIntoQueue(update);

			player_world_window.window_data[address_in_window]= player_state.build_block_type;
			UpdateBuildPos();
		}
	}
	if((mouse_state & c_mouse_mask_l_clicked) != 0)
	{
		ivec3 pos_in_window= player_state.destroy_pos.xyz - player_world_window.offset.xyz;
		if(IsPosInsidePlayerWorldWindow(pos_in_window))
		{
			int address_in_window= GetAddressOfBlockInPlayerWorldWindow(pos_in_window);

			WorldBlockExternalUpdate update;
			update.position= ivec4(player_state.destroy_pos.xyz, 0);
			update.old_block_type= player_world_window.window_data[address_in_window];
			update.new_block_type= c_block_type_air;
			PushUpdateIntoQueue(update);

			player_world_window.window_data[address_in_window]= c_block_type_air;
			UpdateBuildPos();
		}
	}

	UpdateBlocksMatrix();
	UpdateNextPlayerWorldWindowOffset();
}
