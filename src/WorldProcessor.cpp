#include "WorldProcessor.hpp"
#include "Constants.hpp"
#include "ShaderList.hpp"

namespace HexGPU
{

namespace
{

void FillChunkData(const uint32_t chunk_x, const uint32_t chunk_y, uint8_t* const data)
{
	for(uint32_t x= 0; x < c_chunk_width; ++x)
	for(uint32_t y= 0; y < c_chunk_width; ++y)
	{
		const uint32_t global_x= x + (chunk_x << c_chunk_width_log2);
		const uint32_t global_y= y + (chunk_y << c_chunk_width_log2);
		const uint32_t ground_z= uint32_t(6.0f + 1.5f * std::sin(float(global_x) * 0.5f) + 2.0f * std::sin(float(global_y) * 0.3f));
		for(uint32_t z= 0; z < c_chunk_height; ++z)
		{
			data[ChunkBlockAddress(x, y, z)]= z >= ground_z ? 0 : 1;
		}
	}
}

namespace WorldGenShaderBindings
{

uint32_t chunk_data_buffer= 0;

}

// Size limit of this struct is 128 bytes.
// 128 bytes is guaranted maximum size of push constants uniform block.
struct ChunkPositionUniforms
{
	int32_t chunk_position[2];
};

} // namespace

WorldProcessor::WorldProcessor(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, vk_queue_family_index_(window_vulkan.GetQueueFamilyIndex())
{
	// Create chunk data buffer.
	{
		chunk_data_buffer_size_= c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1];

		vk_chunk_data_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					chunk_data_buffer_size_,
					vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_chunk_data_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_chunk_data_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_chunk_data_buffer_, *vk_chunk_data_buffer_memory_, 0u);

		// Fil lthe buffer with initial values.
		void* data_gpu_side= nullptr;
		vk_device_.mapMemory(*vk_chunk_data_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_side);

		for(uint32_t x= 0; x < c_chunk_matrix_size[0]; ++x)
		for(uint32_t y= 0; y < c_chunk_matrix_size[1]; ++y)
			FillChunkData(
				x,
				y,
				reinterpret_cast<uint8_t*>(data_gpu_side) + (x + y * c_chunk_matrix_size[0]) * c_chunk_volume);

		vk_device_.unmapMemory(*vk_chunk_data_buffer_memory_);
	}

	// Create world generation shader.
	world_gen_shader_= CreateShader(vk_device_, ShaderNames::world_gen_comp);

	// Create world generation descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
		{
			{
				WorldGenShaderBindings::chunk_data_buffer,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		vk_world_gen_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));
	}

	// Create world generation pipeline layout.
	{
		const vk::PushConstantRange vk_push_constant_range(
			vk::ShaderStageFlagBits::eCompute,
			0u,
			sizeof(ChunkPositionUniforms));

		vk_world_gen_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*vk_world_gen_decriptor_set_layout_,
				1u, &vk_push_constant_range));
	}

	// Create world generation pipeline.
	vk_world_gen_pipeline_= vk_device_.createComputePipelineUnique(
		nullptr,
		vk::ComputePipelineCreateInfo(
			vk::PipelineCreateFlags(),
			vk::PipelineShaderStageCreateInfo(
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eCompute,
				*world_gen_shader_,
				"main"),
			*vk_world_gen_pipeline_layout_));

	// Create world generation descriptor set pool.
	{
		const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u /*num descriptors*/);
		vk_world_gen_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					4u, // max sets.
					1u, &vk_descriptor_pool_size));
	}

	// Create world generation descriptor set.
	vk_world_gen_descriptor_set_=
		std::move(
			vk_device_.allocateDescriptorSetsUnique(
				vk::DescriptorSetAllocateInfo(
					*vk_world_gen_descriptor_pool_,
					1u, &*vk_world_gen_decriptor_set_layout_)).front());

	// Update world generator descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_data_buffer_info(
			vk_chunk_data_buffer_.get(),
			0u,
			chunk_data_buffer_size_);

		vk_device_.updateDescriptorSets(
			{
				{
					*vk_world_gen_descriptor_set_,
					WorldGenShaderBindings::chunk_data_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_data_buffer_info,
					nullptr
				},
			},
			{});
	}
}

WorldProcessor::~WorldProcessor()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

vk::Buffer WorldProcessor::GetChunkDataBuffer() const
{
	return vk_chunk_data_buffer_.get();
}

uint32_t WorldProcessor::GetChunkDataBufferSize() const
{
	return chunk_data_buffer_size_;
}

} // namespace HexGPU
