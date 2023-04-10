#include "imgui-window/Window.h"

#include <SDL.h>
#include <assert.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer.h>
#include <stdio.h>

#define ASSERT(expr)        \
	do                      \
	{                       \
		auto result = expr; \
		assert(result);     \
	} while (0);

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

namespace im_window
{
	struct Window
	{
		SDL_Window *window;
		SDL_Renderer *renderer;
		bool should_exit;
	};
	inline static Window WINDOW{};

	int
	window_init()
	{
		// Setup SDL
		ASSERT(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);

		#ifdef SDL_HINT_IME_SHOW_UI
			SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
		#endif

		// Create window with SDL_Renderer graphics context
		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		SDL_Window *window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
		SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
		ASSERT(renderer);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
		ImGui_ImplSDLRenderer_Init(renderer);

		WINDOW.window = window;
		WINDOW.renderer = renderer;

		return 0;
	}

	void
	window_poll()
	{
		SDL_Event event{};
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				WINDOW.should_exit = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(WINDOW.window))
				WINDOW.should_exit = true;
		}
	}

	bool
	window_should_exit()
	{
		return WINDOW.should_exit;
	}

	void
	window_frame_start()
	{
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}

	void
	window_frame_render()
	{
		constexpr float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};
		ImGuiIO &io = ImGui::GetIO();

		ImGui::Render();
		SDL_RenderSetScale(WINDOW.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
		SDL_SetRenderDrawColor(WINDOW.renderer, (Uint8)(clear_color[0] * 255), (Uint8)(clear_color[1] * 255), (Uint8)(clear_color[2] * 255), (Uint8)(clear_color[3] * 255));
		SDL_RenderClear(WINDOW.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(WINDOW.renderer);
	}

	void
	window_dispose()
	{
		ImGui_ImplSDLRenderer_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyRenderer(WINDOW.renderer);
		SDL_DestroyWindow(WINDOW.window);
		SDL_Quit();
	}
}  // namespace app