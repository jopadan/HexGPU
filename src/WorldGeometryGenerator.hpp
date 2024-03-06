#pragma once
#include "Buffer.hpp"
#include "GPUAllocator.hpp"
#include "Pipeline.hpp"
#include "WorldProcessor.hpp"

namespace HexGPU
{

struct WorldVertex
{
	// Use 16 bit for position in order to make this struct more compact (relative to floats).
	int16_t pos[4];
	int16_t tex_coord[4]; // component 3 - light
};

using QuadVertices= std::array<WorldVertex, 4>;

class WorldGeometryGenerator
{
public:
	// If this changed, the same struct in GLSL code must be changed too!
	struct ChunkDrawInfo
	{
		uint32_t num_quads= 0;
		uint32_t new_num_quads= 0;
		uint32_t first_quad= 0;
		uint32_t first_memory_unit= 0;
		uint32_t num_memory_units= 0;
	};

public:
	WorldGeometryGenerator(
		WindowVulkan& window_vulkan,
		const WorldProcessor& world_processor,
		vk::DescriptorPool global_descriptor_pool);
	~WorldGeometryGenerator();

	void Update(TaskOrganiser& task_organiser);

	vk::Buffer GetVertexBuffer() const;

	vk::Buffer GetChunkDrawInfoBuffer() const;
	vk::DeviceSize GetChunkDrawInfoBufferSize() const;

private:
	void InitialFillBuffers(TaskOrganiser& task_organiser);
	void ShiftChunkDrawInfo(TaskOrganiser& task_organiser, std::array<int32_t, 2> shift);
	void BuildChunksToUpdateList();
	void PrepareGeometrySizeCalculation(TaskOrganiser& task_organiser);
	void CalculateGeometrySize(TaskOrganiser& task_organiser);
	void AllocateMemoryForGeometry(TaskOrganiser& task_organiser);
	void GenGeometry(TaskOrganiser& task_organiser);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;
	const WorldProcessor& world_processor_;
	const WorldSizeChunks world_size_;

	bool buffers_initially_filled_= false;

	const Buffer chunk_draw_info_buffer_;
	const Buffer chunk_draw_info_buffer_temp_;

	const uint32_t vertex_buffer_num_quads_;
	const Buffer vertex_buffer_;

	GPUAllocator vertex_memory_allocator_;

	const ComputePipeline chunk_draw_info_shift_pipeline_;
	const vk::DescriptorSet chunk_draw_info_shift_descriptor_set_;

	const ComputePipeline geometry_size_calculate_prepare_pipeline_;
	const vk::DescriptorSet geometry_size_calculate_prepare_descriptor_set_;

	const ComputePipeline geometry_size_calculate_pipeline_;
	const std::array<vk::DescriptorSet, 2> geometry_size_calculate_descriptor_sets_;

	const ComputePipeline geometry_allocate_pipeline_;
	const vk::DescriptorSet geometry_allocate_descriptor_set_;

	const ComputePipeline geometry_gen_pipeline_;
	const std::array<vk::DescriptorSet, 2> geometry_gen_descriptor_sets_;

	WorldOffsetChunks world_offset_;

	uint32_t frame_counter_= 0;
	std::vector<std::array<uint32_t, 2>> chunks_to_update_;
};

} // namespace HexGPU
