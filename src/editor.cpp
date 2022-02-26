#include "editor.h"
#include "game.h"
#include "dungen.h"

#include <array.h>
#include <string_stream.h>
#include <proto/game.pb.h>
#include <engine/engine.h>
#include <engine/log.h>
#include <imgui.h>

namespace {
bool did_reset = false;
bool room_templates_dirty = false;
}

namespace game {
namespace editor {
using namespace foundation;

EditorState::EditorState(Allocator &allocator)
: allocator(allocator) {}

// Return true on "dirty" edit
void room_templates_editor(engine::Engine &engine, game::Game &game, EditorState &state) {
    const char *menu_label = nullptr;
    if (room_templates_dirty) {
        menu_label = "Room Templates*###RoomTemplatesWindow";
    } else {
        menu_label = "Room Templates###RoomTemplatesWindow";
    }

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(menu_label, NULL, ImGuiWindowFlags_MenuBar)) {
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

        ImGui::EndMenuBar();
    }

    static int32_t selected_index = -1;
    static char template_name[256] = {'\0'};
    static int rows = 0;
    static int columns = 0;
    const int max_side = 256;
    static uint8_t room_template_tiles[max_side * max_side] = {0};
    
    // Left
    if (did_reset || (selected_index >= 0 && selected_index > array::size(game.room_templates->templates))) {
        selected_index = -1;
        template_name[0] = '\0';
    }

    {
        ImGui::BeginChild("room_templates_left_pane", ImVec2(150, 0), true);
        
        int32_t i = 0;
        for (auto it = array::begin(game.room_templates->templates); it != array::end(game.room_templates->templates); ++it) {
            string_stream::Buffer *name_buffer = (*it)->name;
            const char *name = string_stream::c_str(*name_buffer);
            if (ImGui::Selectable(name, selected_index == i)) {
                selected_index = i;
                memcpy(template_name, name, strlen(name));
                rows = (*it)->rows;
                columns = (*it)->columns;
                memcpy(room_template_tiles, (*it)->data, rows * columns);
            }
            ++i;
        }

        ImGui::EndChild();
    }
    
    ImGui::SameLine();

    // Right
    {
        RoomTemplates::Template *room_template = nullptr;
        if (selected_index >= 0) {
            room_template = game.room_templates->templates[selected_index];
        }

        if (room_template) {
            ImGui::BeginGroup();

            ImGui::PushItemWidth(240);
            if (ImGui::InputText("Name", template_name, 256, ImGuiInputTextFlags_CharsNoBlank)) {
                string_stream::Buffer *name = room_template->name;
                array::clear(*name);
                string_stream::push(*name, template_name, strlen(template_name));
                room_templates_dirty = true;
            }

            ImGui::PushItemWidth(100);
            if (ImGui::InputInt("Rows", &rows)) {
                room_templates_dirty = true;

                if (rows <= 0) {
                    rows = 1;
                } else if (rows > max_side) {
                    rows = max_side;
                } else {
                    if (rows != room_template->rows) {
                        uint32_t old_size = array::size(*room_template->data);
                        int32_t adjust_by = rows > room_template->rows ? room_template->columns : -room_template->columns;
                        uint32_t new_size = old_size + adjust_by;
                        array::resize(*room_template->data, new_size);
                        room_template->rows = (uint8_t)rows;

                        if (adjust_by > 0) {
                            memset(array::begin(*room_template->data) + old_size, 0, adjust_by);
                        }

                        memcpy(room_template_tiles, room_template->data, rows * columns);
                    }
                }
            }

            ImGui::SameLine();

            if (ImGui::InputInt("Columns", &columns)) {
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

            const char tile_items[] = {'.', '#', '%'};
            const int tile_items_count = 3;

            if (room_template) {
                for (uint32_t i = 0; i < array::size(*room_template->data); ++i) {
                    if (i % room_template->columns != 0) {
                        ImGui::SameLine();
                    }

                    uint8_t tile = (*room_template->data)[i];
                    char label[8] = {0};
                    char tile_item = tile_items[tile];
                    snprintf(label, 8, "%c", tile_item);

                    ImGuiComboFlags flags = ImGuiComboFlags_NoArrowButton;

                    ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
                    ImGui::PushID(i);

                    if (ImGui::BeginCombo("", label, flags)) {
                        for (uint8_t n = 0; n < tile_items_count; ++n) {
                            char selectable_label[8] = {0};
                            snprintf(selectable_label, 8, "%c", tile_items[n]);

                            const bool is_selected = room_template_tiles[i] == n;

                            ImGui::PushID(n);
                            if (ImGui::Selectable(selectable_label, is_selected)) {
                                (*room_template->data)[i] = n;
                                room_template_tiles[i] = n;
                                room_templates_dirty = true;
                            }
                            ImGui::PopID();

                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndGroup();
        }
    }

    ImGui::End();

    did_reset = false;
}

void render_imgui(engine::Engine &engine, game::Game &game, EditorState &state) {
    room_templates_editor(engine, game, state);
}

} // namespace editor
} // namespace game
