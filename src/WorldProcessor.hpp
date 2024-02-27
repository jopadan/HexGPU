#pragma once
#include "BlockType.hpp"
#include "Vec.hpp"
#include "WindowVulkan.hpp"

namespace HexGPU
{

using WorldSizeChunks= std::array<uint32_t, 2>;

class WorldProcessor
{
public:
	WorldProcessor(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool);
	~WorldProcessor();

	void Update(
		vk::CommandBuffer command_buffer,
		const m_Vec3& player_pos,
		const m_Vec3& player_dir,
		BlockType build_block_type,
		bool build_triggered,
		bool destroy_triggered);

	vk::Buffer GetChunkDataBuffer() const;
	uint32_t GetChunkDataBufferSize() const;
	vk::Buffer GetLightDataBuffer() const;
	uint32_t GetLightDataBufferSize() const;

	vk::Buffer GetPlayerStateBuffer() const;

	WorldSizeChunks GetWorldSize() const;

public:	
	// This struct must be identical to the same struct in GLSL code!
	struct PlayerState
	{
		int32_t build_pos[4]; // component 3 - direction
		int32_t destroy_pos[4];
	};

private:
	struct BufferWithMemory
	{
		vk::UniqueBuffer buffer;
		vk::UniqueDeviceMemory memory;
	};

private:
	void GenerateWorld(vk::CommandBuffer command_buffer);
	void UpdateWorldBlocks(vk::CommandBuffer command_buffer);
	void UpdateLight(vk::CommandBuffer command_buffer);
	void UpdatePlayer(
		vk::CommandBuffer command_buffer,
		const m_Vec3& player_pos,
		const m_Vec3& player_dir,
		BlockType build_block_type,
		bool build_triggered,
		bool destroy_triggered);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const WorldSizeChunks world_size_;

	// Use double buffering for world update.
	// On each step data is read from one of them and written into another.
	BufferWithMemory chunk_data_buffers_[2];
	uint32_t chunk_data_buffer_size_= 0;

	// Use double buffering for light update.
	// On each step data is read from one of them and written into another.
	BufferWithMemory light_buffers_[2];
	uint32_t light_buffer_size_= 0;

	vk::UniqueBuffer player_state_buffer_;
	vk::UniqueDeviceMemory player_state_buffer_memory_;

	vk::UniqueShaderModule world_gen_shader_;
	vk::UniqueDescriptorSetLayout world_gen_decriptor_set_layout_;
	vk::UniquePipelineLayout world_gen_pipeline_layout_;
	vk::UniquePipeline world_gen_pipeline_;
	vk::DescriptorSet world_gen_descriptor_set_;

	vk::UniqueShaderModule world_blocks_update_shader_;
	vk::UniqueDescriptorSetLayout world_blocks_update_decriptor_set_layout_;
	vk::UniquePipelineLayout world_blocks_update_pipeline_layout_;
	vk::UniquePipeline world_blocks_update_pipeline_;
	vk::DescriptorSet world_blocks_update_descriptor_sets_[2];

	vk::UniqueShaderModule light_update_shader_;
	vk::UniqueDescriptorSetLayout light_update_decriptor_set_layout_;
	vk::UniquePipelineLayout light_update_pipeline_layout_;
	vk::UniquePipeline light_update_pipeline_;
	vk::DescriptorSet light_update_descriptor_sets_[2];

	vk::UniqueShaderModule player_update_shader_;
	vk::UniqueDescriptorSetLayout player_update_decriptor_set_layout_;
	vk::UniquePipelineLayout player_update_pipeline_layout_;
	vk::UniquePipeline player_update_pipeline_;
	vk::DescriptorSet player_update_descriptor_set_;

	bool world_generated_= false;
};

} // namespace HexGPU
