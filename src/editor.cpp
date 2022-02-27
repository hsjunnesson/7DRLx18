#include "editor.h"
#include "game.h"
#include "dungen.h"

#include <array.h>
#include <string_stream.h>
#include <string_stream.h>
#include <proto/game.pb.h>
#include <engine/engine.h>
#include <engine/log.h>
#include <imgui.h>

namespace {
bool did_reset = false;
bool room_templates_dirty = false;
const uint8_t tile_tool_labels_count = 3;
const char tile_tool_labels[] = {'.', '#', '%'};
const char *tile_tool_descriptions[] = {"Empty", "Wall", "Connection"};

// salmon  217, 105, 65
// red  166, 43, 31
const ImVec4 tile_tool_colors[] = {ImColor(25, 60, 64), ImColor(33, 64, 1), ImColor(46, 89, 2)};
}

namespace game {
namespace editor {
using namespace foundation;

EditorState::EditorState(Allocator &allocator)
: allocator(allocator) {}

void room_templates_editor(engine::Engine &engine, game::Game &game, EditorState &state, bool *show_window) {
    if (*show_window == false) {
        return;
    }

    static int32_t selected_template_index = -1;
    static char template_name[256] = {'\0'};
    static int rows = 0;
    static int columns = 0;
    const int max_side = 256;
    static uint8_t room_template_tiles[max_side * max_side] = {0};
    static uint8_t selected_tile_tool_index = 0;

    const char *menu_label = nullptr;
    if (room_templates_dirty) {
        menu_label = "Room Templates*###RoomTemplatesWindow";
    } else {
        menu_label = "Room Templates###RoomTemplatesWindow";
    }

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(menu_label, show_window, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open")) {
                const char *filename = game.params->room_templates_filename().c_str();
                game.room_templates->read(filename);
                did_reset = true;
                room_templates_dirty = false;
            }

            if (ImGui::MenuItem("Save", NULL, false, room_templates_dirty)) {
                const char *filename = game.params->room_templates_filename().c_str();
                game.room_templates->write(filename);
                room_templates_dirty = false;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Copy")) {
                if (selected_template_index >= 0) {
                    Array<RoomTemplates::Template *> room_templates_copy = game.room_templates->templates;
                    RoomTemplates::Template *selected_template = game.room_templates->templates[selected_template_index];
                    RoomTemplates::Template *selected_template_copy = MAKE_NEW(game.room_templates->allocator, RoomTemplates::Template, *selected_template);
                    string_stream::push(*selected_template_copy->name, "_copy", 5);

                    array::clear(game.room_templates->templates);

                    for (uint32_t i = 0; i < array::size(room_templates_copy); ++i) {
                        array::push_back(game.room_templates->templates, room_templates_copy[i]);

                        if (i == selected_template_index) {
                            array::push_back(game.room_templates->templates, selected_template_copy);
                        }
                    }

                    ++selected_template_index;
                    room_templates_dirty = true;
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Left
    if (did_reset || (selected_template_index >= 0 && selected_template_index > array::size(game.room_templates->templates))) {
        selected_template_index = -1;
        memset(template_name, 0, 256);
    }

    {
        if (ImGui::Button("Add")) {
            RoomTemplates::Template *t = MAKE_NEW(game.room_templates->allocator, game::RoomTemplates::Template, game.room_templates->allocator);
            string_stream::push(*t->name, "Room", 4);
            t->columns = 3;
            t->rows = 3;

            for (int i = 0; i < t->columns * t->rows; ++i) {
                array::push_back(*t->data, (uint8_t)0);
            }

            array::push_back(game.room_templates->templates, t);
            room_templates_dirty = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Delete")) {
            if (selected_template_index >= 0) {
                Array<RoomTemplates::Template *> room_templates_copy = game.room_templates->templates;
                RoomTemplates::Template *selected_template = game.room_templates->templates[selected_template_index];
                
                array::clear(game.room_templates->templates);

                for (uint32_t i = 0; i < array::size(room_templates_copy); ++i) {
                    if (i != selected_template_index) {
                        array::push_back(game.room_templates->templates, room_templates_copy[i]);
                    }
                }

                MAKE_DELETE(game.room_templates->allocator, Template, selected_template);
                selected_template = nullptr;

                --selected_template_index;

                room_templates_dirty = true;
            }
        }

        ImGui::BeginChild("room_templates_left_pane", ImVec2(150, 0), true);

        int32_t i = 0;
        for (auto it = array::begin(game.room_templates->templates); it != array::end(game.room_templates->templates); ++it) {
            string_stream::Buffer *name_buffer = (*it)->name;
            const char *name = string_stream::c_str(*name_buffer);
            if (ImGui::Selectable(name, selected_template_index == i)) {
                selected_template_index = i;
                memset(template_name, 0, 256);
                memcpy(template_name, name, strlen(name));
                rows = (*it)->rows;
                columns = (*it)->columns;
                memcpy(room_template_tiles, (*it)->data, rows * columns);
            }

            if (ImGui::BeginPopupContextItem()) {
                ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
                if (i == 0) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move up", false, flags)) {
                    Array<RoomTemplates::Template *> room_templates_copy = game.room_templates->templates;
                    array::clear(game.room_templates->templates);

                    for (int32_t n = 0; n < array::size(room_templates_copy); ++n) {
                        int32_t index = n;

                        if (n == i - 1) {
                            index = i;
                        } else if (n == i) {
                            index = i - 1;
                        }

                        array::push_back(game.room_templates->templates, room_templates_copy[index]);
                    }

                    --selected_template_index;
                    room_templates_dirty = true;
                }

                flags = ImGuiSelectableFlags_None;
                if (i == (int32_t)array::size(game.room_templates->templates) - 1) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move down", false, flags)) {
                    Array<RoomTemplates::Template *> room_templates_copy = game.room_templates->templates;
                    array::clear(game.room_templates->templates);

                    for (int32_t n = 0; n < array::size(room_templates_copy); ++n) {
                        int32_t index = n;

                        if (n == i + 1) {
                            index = i;
                        } else if (n == i) {
                            index = i + 1;
                        }

                        array::push_back(game.room_templates->templates, room_templates_copy[index]);
                    }

                    ++selected_template_index;
                    room_templates_dirty = true;
                }

                ImGui::EndPopup();
            }

            ++i;
        }

        ImGui::EndChild();
    }
    
    ImGui::SameLine();

    // Right
    {
        RoomTemplates::Template *room_template = nullptr;
        if (selected_template_index >= 0) {
            room_template = game.room_templates->templates[selected_template_index];
        }

        if (room_template) {
            ImGui::BeginGroup();

            // Name
            {
                ImGui::PushItemWidth(240);
                if (ImGui::InputText("Name", template_name, 256, ImGuiInputTextFlags_CharsNoBlank)) {
                    string_stream::Buffer *name = room_template->name;
                    array::clear(*name);
                    string_stream::push(*name, template_name, strlen(template_name));
                    room_templates_dirty = true;
                }
            }

            // Rows & Columns
            {
                ImGui::PushItemWidth(100);
                if (ImGui::InputInt("Rows", &rows, 1, 1, ImGuiInputTextFlags_None)) {
                    room_templates_dirty = true;

                    if (rows <= 0) {
                        rows = 1;
                    } else if (rows > max_side) {
                        rows = max_side;
                    } else {
                        if (rows != room_template->rows) {
                            uint8_t old_rows = room_template->rows;
                            uint32_t old_size = array::size(*room_template->data);
                            int32_t adjust_by = rows > room_template->rows ? room_template->columns : -room_template->columns;
                            uint32_t new_size = old_size + adjust_by;
                            array::resize(*room_template->data, new_size);
                            room_template->rows = (uint8_t)rows;

                            if (abs(old_rows - rows) != 1) {
                                log_fatal("Adjusting room template by more than 1 row");
                            }

                            if (adjust_by > 0) {
                                memset(array::begin(*room_template->data) + old_size, 0, adjust_by);
                            }

                            memcpy(room_template_tiles, room_template->data, rows * columns);
                        }
                    }
                }

                ImGui::SameLine();

                if (ImGui::InputInt("Columns", &columns, 1, 1, ImGuiInputTextFlags_None)) {
                    room_templates_dirty = true;

                    if (columns <= 0) {
                        columns = 1;
                    } else if (columns > max_side) {
                        columns = max_side;
                    } else {
                        if (columns != room_template->columns) {
                            uint8_t old_columns = room_template->columns;
                            uint32_t old_size = array::size(*room_template->data);
                            int32_t adjust_by = columns > room_template->columns ? room_template->rows : -room_template->rows;
                            uint32_t new_size = old_size + adjust_by;
                            room_template->columns = (uint8_t)columns;

                            if (abs(old_columns - columns) != 1) {
                                log_fatal("Adjusting room template by more than 1 column");
                            }

                            if (adjust_by > 0) {
                                uint8_t copy_buf[max_side];

                                array::resize(*room_template->data, new_size);
                                memset(array::begin(*room_template->data) + old_size, 0, adjust_by);

                                for (uint8_t  i = room_template->rows - 1; i > 0; --i) {
                                    uint8_t *start = array::begin(*room_template->data) + i * old_columns;
                                    memcpy(copy_buf, start, old_columns);
                                    memset(start, 0, old_columns);
                                    memcpy(start + i, copy_buf, old_columns);
                                }
                            } else {
                                uint8_t copy_buf[max_side];

                                for (uint8_t  i = 1; i < room_template->rows; ++i) {
                                    uint8_t *start = array::begin(*room_template->data) + i * old_columns;
                                    memcpy(copy_buf, start, old_columns - 1);
                                    memcpy(start - i, copy_buf, old_columns - 1);
                                }

                                array::resize(*room_template->data, new_size);
                            }

                            memcpy(room_template_tiles, room_template->data, rows * columns);
                        }
                    }
                }
            }

            // Tile tool palette
            {
                ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
                ImGui::BeginChild("ToolPalette", ImVec2(120, 0), false, window_flags);

                ImDrawList *draw_list = ImGui::GetWindowDrawList();

                for (uint8_t i = 0; i < tile_tool_labels_count; ++i) {
                    char label[256] = {0};
                    snprintf(label, 256, "%c %s", tile_tool_labels[i], tile_tool_descriptions[i]);

                    draw_list->ChannelsSplit(2);
                    draw_list->ChannelsSetCurrent(1);

                    if (ImGui::Selectable(label, i == selected_tile_tool_index)) {
                        selected_tile_tool_index = i;
                    }

                    draw_list->ChannelsSetCurrent(0);
                    ImVec2 p_min = ImGui::GetItemRectMin();
                    ImVec2 p_max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::GetColorU32(tile_tool_colors[i]));

                    draw_list->ChannelsMerge();
                }

                ImGui::EndChild();
            }

            ImGui::SameLine();

            // Tile buttons
            {
                ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
                ImGui::BeginChild("Tiles", ImVec2(0, 0), false, window_flags);

                ImDrawList *draw_list = ImGui::GetWindowDrawList();

                for (uint32_t i = 0; i < array::size(*room_template->data); ++i) {
                    if (i % room_template->columns != 0) {
                        ImGui::SameLine();
                    }

                    uint8_t tile = (*room_template->data)[i];
                    char label[8] = {0};
                    char tile_item = tile_tool_labels[tile];
                    snprintf(label, 8, "%c", tile_item);

                    draw_list->ChannelsSplit(2);
                    draw_list->ChannelsSetCurrent(1);

                    ImGui::PushID(i);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                    ImGui::Selectable(label, false, 0, ImVec2(20, 20));
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        (*room_template->data)[i] = selected_tile_tool_index;
                        room_template_tiles[i] = selected_tile_tool_index;
                        room_templates_dirty = true;
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();

                    draw_list->ChannelsSetCurrent(0);
                    ImVec2 p_min = ImGui::GetItemRectMin();
                    ImVec2 p_max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::GetColorU32(tile_tool_colors[tile]));

                    draw_list->ChannelsMerge();
                }

                ImGui::EndChild();
            }

            ImGui::EndGroup();
        }
    }

    ImGui::End();

    did_reset = false;
}

void render_imgui(engine::Engine &engine, game::Game &game, EditorState &state) {
    static bool show_room_templates_window = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("Room templates", nullptr, show_room_templates_window)) {
                show_room_templates_window = !show_room_templates_window;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    room_templates_editor(engine, game, state, &show_room_templates_window);
}

} // namespace editor
} // namespace game
