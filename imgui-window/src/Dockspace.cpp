#include "imgui-window/Dockspace.h"
#include <imgui_internal.h>

namespace im_window
{
	Dockspace_Node::Dockspace_Node(const char *name, float ratio)
		: name(name), ratio(ratio), split{}
	{
	}

	Dockspace_Node::Dockspace_Node(std::initializer_list<Dockspace_Node> children)
		: split(SPLIT_VERTICAL), ratio(1.f), children(children), name(nullptr)
	{
	}

	Dockspace_Node::Dockspace_Node(SPLIT split, std::initializer_list<Dockspace_Node> children)
		: split(split), ratio(1.f), children(children), name(nullptr)
	{
	}

	Dockspace_Node::Dockspace_Node(SPLIT split, float ratio, std::initializer_list<Dockspace_Node> children)
		: split(split), ratio(ratio), children(children), name(nullptr)
	{
	}

	constexpr inline static ImGuiDir
	_dir(SPLIT split)
	{
		if (split == SPLIT_HORIZONTAL) return ImGuiDir_Up;
		return ImGuiDir_Left;
	}

	void
	_layout(Dockspace_Node layout, ImGuiID root_id);

	inline static void
	_layout_window(Dockspace_Node window, ImGuiID root_id)
	{
		ImGui::DockBuilderDockWindow(window.name, root_id);
	}

	inline static void
	_layout_container(Dockspace_Node container, ImGuiID root_id)
	{
		ImGuiID remaining_id = root_id;
		for (auto n : container.children)
		{
			if (n.ratio == 1.f)
			{
				ImGui::DockBuilderRemoveNodeChildNodes(remaining_id);
				return _layout(n, remaining_id);
			}

			ImGuiID child_id = ImGui::DockBuilderSplitNode(remaining_id, _dir(container.split), n.ratio, nullptr, &remaining_id);
			_layout(n, child_id);
		}
	}

	void
	_layout(Dockspace_Node layout, ImGuiID root_id)
	{
		ImGui::DockBuilderRemoveNodeChildNodes(root_id);
		if (layout.is_container()) return _layout_container(layout, root_id);
		if (layout.is_window()) return _layout_window(layout, root_id);
	}

	void
	dockspace(Dockspace_Node layout, bool *setup_layout)
	{
		auto root_id = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

		if (setup_layout != nullptr)
		{
			if (*setup_layout == false) return;
			*setup_layout = false;
		}
		else
		{
			static bool setup_once = false;
			if (setup_once) return;
			setup_once = true;
		}

		_layout(layout, root_id);
	}
}  // namespace im_window