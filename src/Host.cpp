#include "Host.hpp"
#include "Assert.hpp"
#include <thread>


namespace HexGPU
{

namespace
{

float CalculateAspect(const vk::Extent2D& viewport_size)
{
	return float(viewport_size.width) / float(viewport_size.height);
}

} // namespace

Host::Host()
	: system_window_()
	, window_vulkan_(system_window_)
	, world_processor_(window_vulkan_)
	, world_renderer_(window_vulkan_, world_processor_)
	, build_prism_renderer_(window_vulkan_, world_processor_)
	, camera_controller_(CalculateAspect(window_vulkan_.GetViewportSize()))
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
{
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	const float dt_s= float(dt.count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);

	bool build_triggered= false, destroy_triggered= false;
	for( const SDL_Event& event : system_window_.ProcessEvents() )
	{
		if(event.type == SDL_QUIT)
			quit_requested_= true;
		if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
			quit_requested_= true;
		if(event.type == SDL_MOUSEBUTTONDOWN)
		{
			if(event.button.button == SDL_BUTTON_LEFT)
				destroy_triggered= true;
			if(event.button.button == SDL_BUTTON_RIGHT)
				build_triggered= true;
		}
	}

	camera_controller_.Update(dt_s, system_window_.GetKeyboardState());

	const vk::CommandBuffer command_buffer= window_vulkan_.BeginFrame();

	// TODO - pass directly events and keyboard state.
	world_processor_.Update(command_buffer, camera_controller_.GetCameraPosition(), build_triggered, destroy_triggered);

	world_renderer_.PrepareFrame(command_buffer);

	window_vulkan_.EndFrame(
		[&](const vk::CommandBuffer command_buffer)
		{
			const m_Mat4 view_matrix= camera_controller_.CalculateFullViewMatrix();
			world_renderer_.Draw(command_buffer, view_matrix);
			build_prism_renderer_.Draw(command_buffer, view_matrix);
		});

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_end_time - tick_start_time;

	const float max_fps= 20.0f;

	const std::chrono::milliseconds min_frame_duration(uint32_t(1000.0f / max_fps));
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return quit_requested_;
}

} // namespace HexGPU
