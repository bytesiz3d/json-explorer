#pragma once

namespace im_window
{
	// Return 0 on success
	int
	window_init();

	void
	window_poll();

	bool
	window_should_exit();

	void
	window_frame_start();

	void
	window_frame_render();

	void
	window_dispose();

	// Utility function to run the event loop
	template <typename Fn>
	void
	window_run(Fn frame)
	{
		while (window_should_exit() == false)
		{
			window_poll();
			window_frame_start();
			frame();
			window_frame_render();
		}
	}
}