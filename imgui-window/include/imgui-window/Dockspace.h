#pragma once
#include <imgui.h>
#include <initializer_list>

namespace im_window
{
	enum SPLIT
	{
		SPLIT_VERTICAL = 0,
		SPLIT_HORIZONTAL = 1,
	};

	struct Dockspace_Node
	{
		float ratio;
		Dockspace_Node() = delete;

		const char *name;
		Dockspace_Node(const char *name, float ratio = 1.f);

		inline bool
		is_window() { return name != nullptr; }

		SPLIT split;
		std::initializer_list<Dockspace_Node> children;
		Dockspace_Node(std::initializer_list<Dockspace_Node> children);
		Dockspace_Node(SPLIT split, std::initializer_list<Dockspace_Node> children);
		Dockspace_Node(SPLIT split, float ratio, std::initializer_list<Dockspace_Node> children);

		inline bool
		is_container() { return children.size() > 0; }
	};

	void
	dockspace(Dockspace_Node layout, bool *setup_layout = nullptr);
}