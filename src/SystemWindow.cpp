#include "SystemWindow.hpp"
#include "Assert.hpp"
#include "Log.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstring>


namespace HexGPU
{

SystemWindow::SystemWindow()
{
	// TODO - check errors.
	SDL_Init(SDL_INIT_VIDEO);

	const Uint32 window_flags= SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN;

	const int width = 640;
	const int height= 480;

	window_=
		SDL_CreateWindow(
			"HexGPU",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height,
			window_flags);

	if(window_ == nullptr)
		Log::FatalError("Could not create window");

	Log::Info("Window created with size ", width, "x", height);
}

SystemWindow::~SystemWindow()
{
	Log::Info("Destroying system window");
	SDL_DestroyWindow(window_);
}

SDL_Window* SystemWindow::GetSDLWindow() const
{
	return window_;
}

} // namespace HexGPU
