#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_log.h>
#include <util/sokol_imgui.h>

#include <json-parser/json-parser.h>

#include <fstream>
#include <format>

#include "Roboto-Medium-ttf.h"

struct App
{
	J_Parse_Result _parse;
	std::string _json_buf;

	App() : _parse{}, _json_buf{}
	{
		update_json_buf("{}");
	}

	void
	show_json(J_JSON json, const char* json_key = "")
	{
		switch (json.kind)
		{
		case J_JSON_NULL: {
			return ImGui::Text("null");
		}

		case J_JSON_BOOL: {
			return ImGui::Text("%s", json.as_bool ? "true" : "false");
		}

		case J_JSON_NUMBER: {
			auto formatted_text = std::format("{}", json.as_number);
			return ImGui::Text("%s", formatted_text.c_str());
		}

		case J_JSON_STRING: {
			return ImGui::Text("\"%s\"", json.as_string);
		}

		case J_JSON_ARRAY: {
			if (ImGui::TreeNodeEx(json.as_array.ptr, ImGuiTreeNodeFlags_DefaultOpen, "%s [%zu]", json_key, json.as_array.count))
			{
				for (int i = 0; i < json.as_array.count; i++)
				{
					auto value = json.as_array.ptr[i];

					if (value.kind != J_JSON_ARRAY && value.kind != J_JSON_OBJECT)
						ImGui::Bullet();

					show_json(value);
				}

				ImGui::TreePop();
			}
			return;
		}

		case J_JSON_OBJECT: {
			if (ImGui::TreeNodeEx(json.as_object.pairs, ImGuiTreeNodeFlags_DefaultOpen, "%s {%zu}", json_key, json.as_object.count))
			{
				for (int i = 0; i < json.as_object.count; i++)
				{
					auto [key, value] = json.as_object.pairs[i];
					if (value.kind != J_JSON_ARRAY && value.kind != J_JSON_OBJECT)
					{
						ImGui::BulletText("%s", key);

						ImGui::SameLine();
						ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

						ImGui::SameLine();
						show_json(value);
					}
					else
					{
						show_json(value, key);
					}
				}
				ImGui::TreePop();
			}
			return;
		}
		}
	}

	void
	update_json_buf(const char* buf = nullptr)
	{
		if (buf)
		{
			_json_buf.clear();
			_json_buf.append(buf);
		}

		j_free(_parse.json);
		_parse = j_parse(_json_buf.c_str());
	}

	void
	frame()
	{
		if (ImGui::Begin("JSON Explorer"))
		{
			if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
			{
				ImGui::TableNextColumn();
				if (ImGui::InputTextMultiline("##JSON", &_json_buf, {-10.f, -30.f}, ImGuiInputTextFlags_AllowTabInput))
					update_json_buf();

				ImGui::TextColored(ImColor{144, 144, 144}, "Type into the textbox or drag'n'drop a JSON file");

				ImGui::TableNextColumn();
				if (ImGui::BeginChild("##JSON View", ImGui::GetContentRegionAvail(), false, ImGuiWindowFlags_HorizontalScrollbar))
				{
					if (_parse.err)
						ImGui::TextColored(ImColor{255, 0, 0}, "Invalid JSON");
					else
						show_json(_parse.json);
				}
				ImGui::EndChild();

				ImGui::EndTable();
			}
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
	simgui_desc_t simgui_desc = {.no_default_font = true};
	simgui_setup(&simgui_desc);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	{
		auto io = &ImGui::GetIO();
		ImFontConfig config{};
		config.FontDataOwnedByAtlas = false;
		io->Fonts->AddFontFromMemoryTTF(Roboto_Medium_ttf, Roboto_Medium_ttf_len, 18, &config);
		unsigned char* font_pixels;
		int font_width, font_height;
		io->Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
		sg_image_desc img_desc;
		_simgui_clear(&img_desc, sizeof(img_desc));
		img_desc.width = font_width;
		img_desc.height = font_height;
		img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
		img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
		img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
		img_desc.min_filter = SG_FILTER_LINEAR;
		img_desc.mag_filter = SG_FILTER_LINEAR;
		img_desc.data.subimage[0][0].ptr = font_pixels;
		img_desc.data.subimage[0][0].size = (size_t)(font_width * font_height) * sizeof(uint32_t);
		img_desc.label = "sokol-imgui-font";
		_simgui.img = sg_make_image(&img_desc);
		io->Fonts->TexID = (ImTextureID)(uintptr_t)_simgui.img.id;
	}

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
	static bool setup_dockspace = true;
	if (setup_dockspace)
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
event(const sapp_event* event)
{
	simgui_handle_event(event);

	if (event->type == SAPP_EVENTTYPE_FILES_DROPPED)
	{
		const int num_dropped_files = sapp_get_num_dropped_files();
		for (int i = 0; i < num_dropped_files; i++)
		{
			const char* path = sapp_get_dropped_file_path(num_dropped_files - 1);
			std::ifstream ifs{path};
			std::string file_content{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
			app.update_json_buf(file_content.c_str());
		}
	}
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
	desc.event_cb = event;
	desc.width = 1024;
	desc.height = 768;
	desc.window_title = "JSON Explorer";
	desc.ios_keyboard_resizes_canvas = false;
	desc.icon.sokol_default = true;
	desc.enable_dragndrop = true;
	desc.enable_clipboard = true;
	desc.logger.func = slog_func;
	return desc;
}