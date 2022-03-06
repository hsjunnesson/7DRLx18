#include "editor.h"
#include "game.h"
#include "dungen.h"

#pragma warning(push, 0)
#include <array.h>
#include <string_stream.h>
#include <string_stream.h>
#include <proto/game.pb.h>
#include <engine/engine.h>
#include <engine/log.h>
#include <imgui.h>
#pragma warning(pop)

namespace {
bool did_reset = false;
bool room_templates_dirty = false;
}

namespace game {
namespace editor {
using namespace foundation;

EditorState::EditorState(Allocator &allocator)
: allocator(allocator) {}

char tile_tool_label(uint8_t tile_type) {
    switch (static_cast<RoomTemplates::Template::TileType>(tile_type)) {
    case RoomTemplates::Template::TileType::Empty:
        return ' ';
    case RoomTemplates::Template::TileType::Floor:
        return '.';
    case RoomTemplates::Template::TileType::Wall:
        return '#';
    case RoomTemplates::Template::TileType::Connection:
        return '%';
    case RoomTemplates::Template::TileType::Stair:
        return '>';
    default:
        log_fatal("Unsupported TileType %u", tile_type);
    }
}

const char *tile_tool_description(uint8_t tile_type) {
    switch (static_cast<RoomTemplates::Template::TileType>(tile_type)) {
    case RoomTemplates::Template::TileType::Empty:
        return "Empty";
    case RoomTemplates::Template::TileType::Floor:
        return "Floor";
    case RoomTemplates::Template::TileType::Wall:
        return "Wall";
    case RoomTemplates::Template::TileType::Connection:
        return "Connection";
    case RoomTemplates::Template::TileType::Stair:
        return "Stair";
    default:
        log_fatal("Unsupported TileType %u", tile_type);
    }
}

ImVec4 tile_tool_color(uint8_t tile_type) {
    // salmon  217, 105, 65
    // red  166, 43, 31

    switch (static_cast<RoomTemplates::Template::TileType>(tile_type)) {
    case RoomTemplates::Template::TileType::Empty:
        return ImColor(0, 0, 0);
    case RoomTemplates::Template::TileType::Floor:
        return ImColor(25, 60, 64);
    case RoomTemplates::Template::TileType::Wall:
        return ImColor(33, 64, 1);
    case RoomTemplates::Template::TileType::Connection:
        return ImColor(46, 89, 2);
    case RoomTemplates::Template::TileType::Stair:
        return ImColor(25, 60, 64);
    default:
        log_fatal("Unsupported TileType %u", tile_type);
    }
}

void room_templates_editor(game::Game &game, bool *show_window) {
    if (*show_window == false) {
        return;
    }

    static int32_t selected_template_index = -1;
    static char template_name[256] = {'\0'};
    static int rarity = 0;
    static uint8_t tags = 0;
    static bool tags_start_room = false;
    static bool tags_boss_room = false;
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

            if (ImGui::MenuItem("Save", nullptr, false, room_templates_dirty)) {
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

                        if (i == (uint32_t)selected_template_index) {
                            array::push_back(game.room_templates->templates, selected_template_copy);
                        }
                    }

                    room_templates_dirty = true;
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (did_reset || (selected_template_index >= 0 && selected_template_index > (int32_t)array::size(game.room_templates->templates))) {
        selected_template_index = -1;
        memset(template_name, 0, 256);
    }

    // Left
    {
        if (ImGui::Button("Add")) {
            RoomTemplates::Template *t = MAKE_NEW(game.room_templates->allocator, game::RoomTemplates::Template, game.room_templates->allocator);
            string_stream::push(*t->name, "Room", 4);
            t->rarity = 1;
            t->tags = 0;
            t->columns = 3;
            t->rows = 3;

            for (int i = 0; i < t->columns * t->rows; ++i) {
                array::push_back(*t->tiles, (uint8_t)0);
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
                    if (i != (uint32_t)selected_template_index) {
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
            ImGui::PushID(i);
            if (ImGui::Selectable(name, selected_template_index == i)) {
                selected_template_index = i;
                memset(template_name, 0, 256);
                memcpy(template_name, name, strlen(name));
                rarity = (*it)->rarity;
                tags = (*it)->tags;
                tags_start_room = (tags & RoomTemplates::Template::Tags::RoomTemplateTagsStartRoom) != 0;
                tags_boss_room = (tags & RoomTemplates::Template::Tags::RoomTemplateTagsBossRoom) != 0;
                rows = (*it)->rows;
                columns = (*it)->columns;
                memcpy(room_template_tiles, (*it)->tiles, rows * columns);
            }
            ImGui::PopID();

            if (ImGui::BeginPopupContextItem()) {
                ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
                if (i == 0) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move up", false, flags)) {
                    Array<RoomTemplates::Template *> room_templates_copy = game.room_templates->templates;
                    array::clear(game.room_templates->templates);

                    for (uint32_t n = 0; n < array::size(room_templates_copy); ++n) {
                        uint32_t index = n;

                        if (n == (uint32_t)i - 1) {
                            index = i;
                        } else if (n == (uint32_t)i) {
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

                    for (int32_t n = 0; (uint32_t)n < array::size(room_templates_copy); ++n) {
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
                    string_stream::push(*name, template_name, (uint32_t)strlen(template_name));
                    room_templates_dirty = true;
                }

                ImGui::SameLine();

                ImGui::PushItemWidth(80);
                if (ImGui::InputInt("Rarity", &rarity)) {
                    room_templates_dirty = true;
                    if (rarity <= 0) {
                        rarity = 1;
                    }
                    
                    if (rarity > 4) {
                        rarity = 4;
                    }

                    room_template->rarity = (uint8_t)rarity;
                }
            }

            // Rows & Columns
            {
                ImGui::PushItemWidth(100);
                if (ImGui::InputInt("Rows", &rows)) {
                    room_templates_dirty = true;

                    if (rows <= 0) {
                        rows = 1;
                    } else if (rows > max_side) {
                        rows = max_side;
                    } else {
                        if (rows != room_template->rows) {
                            uint8_t old_rows = room_template->rows;
                            uint32_t old_size = array::size(*room_template->tiles);
                            int32_t adjust_by = rows > room_template->rows ? room_template->columns : -room_template->columns;
                            uint32_t new_size = old_size + adjust_by;
                            array::resize(*room_template->tiles, new_size);
                            room_template->rows = (uint8_t)rows;

                            if (abs(old_rows - rows) != 1) {
                                log_fatal("Adjusting room template by more than 1 row");
                            }

                            if (adjust_by > 0) {
                                memset(array::begin(*room_template->tiles) + old_size, 0, adjust_by);
                            }

                            memcpy(room_template_tiles, room_template->tiles, rows * columns);
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
                            uint32_t old_size = array::size(*room_template->tiles);
                            int32_t adjust_by = columns > room_template->columns ? room_template->rows : -room_template->rows;
                            uint32_t new_size = old_size + adjust_by;
                            room_template->columns = (uint8_t)columns;

                            if (abs(old_columns - columns) != 1) {
                                log_fatal("Adjusting room template by more than 1 column");
                            }

                            if (adjust_by > 0) {
                                uint8_t copy_buf[max_side];

                                array::resize(*room_template->tiles, new_size);
                                memset(array::begin(*room_template->tiles) + old_size, 0, adjust_by);

                                for (uint8_t  i = room_template->rows - 1; i > 0; --i) {
                                    uint8_t *start = array::begin(*room_template->tiles) + i * old_columns;
                                    memcpy(copy_buf, start, old_columns);
                                    memset(start, 0, old_columns);
                                    memcpy(start + i, copy_buf, old_columns);
                                }
                            } else {
                                uint8_t copy_buf[max_side];

                                for (uint8_t  i = 1; i < room_template->rows; ++i) {
                                    uint8_t *start = array::begin(*room_template->tiles) + i * old_columns;
                                    memcpy(copy_buf, start, old_columns - 1);
                                    memcpy(start - i, copy_buf, old_columns - 1);
                                }

                                array::resize(*room_template->tiles, new_size);
                            }

                            memcpy(room_template_tiles, room_template->tiles, rows * columns);
                        }
                    }
                }
            }

            // Tags
            {
                if (ImGui::Checkbox("Start room", &tags_start_room)) {
                    if (tags_start_room) {
                        tags = tags | RoomTemplates::Template::Tags::RoomTemplateTagsStartRoom;
                    } else {
                        tags = tags ^ RoomTemplates::Template::Tags::RoomTemplateTagsStartRoom;
                    }

                    room_template->tags = tags;
                }

                ImGui::SameLine();

                if (ImGui::Checkbox("Boss room", &tags_boss_room)) {
                    if (tags_boss_room) {
                        tags = tags | RoomTemplates::Template::Tags::RoomTemplateTagsBossRoom;
                    } else {
                        tags = tags ^ RoomTemplates::Template::Tags::RoomTemplateTagsBossRoom;
                    }

                    room_template->tags = tags;
                }
            }

            // Tile tool palette
            {
                ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
                ImGui::BeginChild("ToolPalette", ImVec2(120, 0), false, window_flags);

                ImDrawList *draw_list = ImGui::GetWindowDrawList();

                for (uint8_t i = 0; i < static_cast<uint8_t>(RoomTemplates::Template::TileType::Count); ++i) {
                    char label[256] = {0};
                    snprintf(label, 256, "%c %s", tile_tool_label(i), tile_tool_description(i));

                    draw_list->ChannelsSplit(2);
                    draw_list->ChannelsSetCurrent(1);

                    if (ImGui::Selectable(label, i == selected_tile_tool_index)) {
                        selected_tile_tool_index = i;
                    }

                    draw_list->ChannelsSetCurrent(0);
                    ImVec2 p_min = ImGui::GetItemRectMin();
                    ImVec2 p_max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::GetColorU32(tile_tool_color(i)));

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

                for (uint32_t i = 0; i < array::size(*room_template->tiles); ++i) {
                    if (i % room_template->columns != 0) {
                        ImGui::SameLine();
                    }

                    uint8_t tile = (*room_template->tiles)[i];
                    char label[8] = {0};
                    char tile_item = tile_tool_label(tile);
                    snprintf(label, 8, "%c", tile_item);

                    draw_list->ChannelsSplit(2);
                    draw_list->ChannelsSetCurrent(1);

                    ImGui::PushID(i);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                    ImGui::Selectable(label, false, 0, ImVec2(20, 20));
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        (*room_template->tiles)[i] = selected_tile_tool_index;
                        room_template_tiles[i] = selected_tile_tool_index;
                        room_templates_dirty = true;
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();

                    draw_list->ChannelsSetCurrent(0);
                    ImVec2 p_min = ImGui::GetItemRectMin();
                    ImVec2 p_max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, ImGui::GetColorU32(tile_tool_color(tile)));

                    draw_list->ChannelsMerge();
                }

                ImGui::EndChild();
            }

            ImGui::EndGroup();
        }
    }

    ImGui::End();
}

void gamestate_controls_window(engine::Engine &engine, game::Game &game, bool *show_window) {
    if (*show_window == false) {
        return;
    }

    if (!ImGui::Begin("Gamestate Controls", show_window, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // Game state label
    {
        const char *game_state_label = nullptr;
        
        switch (game.game_state) {
        case GameState::None:
            game_state_label = "None";
            break;
        case GameState::Initializing:
            game_state_label = "Initializing";
            break;
        case GameState::Menus:
            game_state_label = "Menus";
            break;
        case GameState::Dungen:
            game_state_label = "Dungen";
            break;
        case GameState::Playing:
            game_state_label = "Playing";
            break;
        case GameState::Quitting:
            game_state_label = "Quitting";
            break;
        case GameState::Terminate:
            game_state_label = "Terminate";
            break;
        }

        static char state_buf[256];
        snprintf(state_buf, 256, "State (%s)", game_state_label);

        ImGui::Text(state_buf);
    }
    
    // State buttons
    {
        if (ImGui::Button("Initializing")) {
            transition(engine, &game, GameState::Initializing);
        }

        ImGui::SameLine();

        if (ImGui::Button("Menus")) {
            transition(engine, &game, GameState::Menus);
        }

        ImGui::SameLine();

        if (ImGui::Button("Dungen")) {
            transition(engine, &game, GameState::Dungen);
        }

        ImGui::SameLine();

        if (ImGui::Button("Playing")) {
            transition(engine, &game, GameState::Playing);
        }

        ImGui::SameLine();

        if (ImGui::Button("Quitting")) {
            transition(engine, &game, GameState::Quitting);
        }

        ImGui::SameLine();

        if (ImGui::Button("Terminate")) {
            transition(engine, &game, GameState::Terminate);
        }
    }

    ImGui::End();
}

void render_imgui(engine::Engine &engine, game::Game &game, EditorState &state) {
    (void)engine;
    (void)state;

    static bool show_room_templates_window = true;
    static bool show_gamestate_controls_window = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("Room templates", nullptr, show_room_templates_window)) {
                show_room_templates_window = !show_room_templates_window;
            }

            if (ImGui::MenuItem("Gamestate Controls", nullptr, show_gamestate_controls_window)) {
                show_gamestate_controls_window = !show_gamestate_controls_window;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    room_templates_editor(game, &show_room_templates_window);
    gamestate_controls_window(engine, game, &show_gamestate_controls_window);

    did_reset = false;
}

} // namespace editor
} // namespace game
