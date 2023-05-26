#include <imgui.h>
#include <imgui_internal.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_log.h>
#include <util/sokol_imgui.h>

#include <json-parser/json-parser.h>

struct App
{
	J_JSON _json;

	App()
	{
		this->_json = j_parse(R"({
            "album_type": "compilation",
            "total_tracks": 9.000000,
            "available_markets": ["CA", "BR", "IT"],
            "external_urls": {
                "spotify": "string"
            },
            "href": "string",
            "id": "2up3OPMp9Tb4dAKM2erWXQ",
            "images": [{
                "url": "https://i.scdn.co/image/ab67616d00001e02ff9ca10b55ce82ae553c8228",
                "height": 300.000000,
                "width": 300.000000
            }]
        })").json;
	}

	void
	show_json(J_JSON json, const char* json_key = nullptr)
	{
		if (json_key && json.kind != J_JSON_ARRAY && json.kind != J_JSON_OBJECT)
		{
			ImGui::TextColored(ImColor{0xf7'40'40'ff}, "%s", json_key);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x / 4);
		}

		switch (json.kind)
		{
		case J_JSON_NULL: return ImGui::Text("null");
		case J_JSON_BOOL: return ImGui::Text(json.as_bool ? "true" : "false");
		case J_JSON_NUMBER: return ImGui::Text("%lf", json.as_number);
		case J_JSON_STRING: return ImGui::Text("\"%s\"", json.as_string);

		case J_JSON_ARRAY:
			if (json_key)
				ImGui::PushStyleColor(ImGuiCol_Text, 0xf7'40'40'ff);

			if (ImGui::TreeNode(json.as_array.ptr, "[%zu] %s", json.as_array.count, json_key ? json_key : ""))
			{
				if (json_key)
					ImGui::PopStyleColor();

				for (auto it = json.as_array.ptr; it != json.as_array.ptr + json.as_array.count; it++)
					show_json(*it);

				ImGui::TreePop();
			}
			else
			{
				if (json_key)
					ImGui::PopStyleColor();
			}
			return;

		case J_JSON_OBJECT:
			if (json_key)
				ImGui::PushStyleColor(ImGuiCol_Text, 0xf7'40'40'ff);

			if (ImGui::TreeNode(json.as_object.pairs, "{%zu} %s", json.as_object.count, json_key ? json_key : ""))
			{
				if (json_key)
					ImGui::PopStyleColor();

				for (auto it = json.as_object.pairs; it != json.as_object.pairs + json.as_object.count; it++)
				{
					auto [key, value] = *it;
					show_json(value, key);
				}

				ImGui::TreePop();
			}
			else
			{
				if (json_key)
					ImGui::PopStyleColor();
			}
			return;
		}
	}

	void
	frame()
	{
		if (ImGui::Begin("JSON Explorer"))
		{
			show_json(this->_json);
		}
		ImGui::End();
	}
};

static sg_pass_action pass_action;
static App app{};

void
init(void)
{
	// setup sokol-gfx, sokol-time and sokol-imgui
	sg_desc desc = {};
	desc.context = sapp_sgcontext();
	desc.logger.func = slog_func;
	sg_setup(&desc);

	// use sokol-imgui with all default-options (we're not doing
	// multi-sampled rendering or using non-default pixel formats)
	simgui_desc_t simgui_desc = {};
	simgui_setup(&simgui_desc);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// initial clear color
	pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
	pass_action.colors[0].clear_value = {0.3f, 0.7f, 0.5f, 1.0f};
}

void
frame(void)
{
	const int width = sapp_width();
	const int height = sapp_height();
	simgui_new_frame({width, height, sapp_frame_duration(), sapp_dpi_scale()});

	auto dockspace_root = ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
	if (static bool setup_dockspace = true)
	{
		setup_dockspace = false;
		ImGui::DockBuilderDockWindow("JSON Explorer", dockspace_root);
	}
	app.frame();

	// the sokol_gfx draw pass
	sg_begin_default_pass(&pass_action, width, height);
	simgui_render();
	sg_end_pass();
	sg_commit();
}

void
cleanup(void)
{
	simgui_shutdown();
	sg_shutdown();
}

void
input(const sapp_event* event)
{
	simgui_handle_event(event);
}

sapp_desc
sokol_main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	sapp_desc desc = {};
	desc.init_cb = init;
	desc.frame_cb = frame;
	desc.cleanup_cb = cleanup;
	desc.event_cb = input;
	desc.width = 1024;
	desc.height = 768;
	desc.window_title = "JSON Explorer";
	desc.ios_keyboard_resizes_canvas = false;
	desc.icon.sokol_default = true;
	desc.enable_clipboard = true;
	desc.logger.func = slog_func;
	return desc;
}