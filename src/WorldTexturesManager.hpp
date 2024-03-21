#pragma once
#include "TaskOrganizer.hpp"
#include "WindowVulkan.hpp"
#include "Image.hpp"

namespace HexGPU
{

class WorldTexturesManager
{
public:
	WorldTexturesManager(WindowVulkan& window_vulkan, vk::DescriptorPool global_descriptor_pool);
	~WorldTexturesManager();

	void PrepareFrame(TaskOrganizer& task_organizer);

	vk::ImageView GetImageView() const;
	vk::ImageView GetWaterImageView() const;
	TaskOrganizer::ImageInfo GetImageInfo() const;

private:
	struct TextureGenPipelineData
	{
		vk::UniqueShaderModule shader;
		vk::UniquePipeline pipeline;
		vk::UniqueImageView image_layer_view;
		vk::DescriptorSet descriptor_set;
	};

	struct TextureGenPipelines
	{
		vk::UniqueDescriptorSetLayout descriptor_set_layout;
		vk::UniquePipelineLayout pipeline_layout;
		std::array<TextureGenPipelineData, 1> pipelines;
	};

private:
	static TextureGenPipelines CreatePipelines(
		vk::Device vk_device,
		vk::DescriptorPool global_descriptor_pool,
		vk::Image image);

private:
	const vk::Device vk_device_;
	const uint32_t queue_family_index_;

	const vk::UniqueImage image_;
	vk::UniqueImageView image_view_;
	vk::UniqueImageView water_image_view_;
	vk::UniqueDeviceMemory image_memory_;

	const TextureGenPipelines texture_gen_pipelines_;

	vk::UniqueBuffer staging_buffer_;
	vk::UniqueDeviceMemory staging_buffer_memory_;

	bool textures_loaded_= false;
};

} // namespace HexGPU
