# imgui-starter
Some starter code to get a window with [ImGui](https://github.com/ocornut/imgui) running.

## Usage
```cmake
target_link_libraries(${TARGET} imgui-window)
```
```c++
if (im_window::window_init() != 0)
	// panic

im_window::window_run([/* any representation of state */] {
	ImGui::Text("Hello, world");
});

im_window::window_dispose();
```