#include "editor.h"
#include "dungen.h"
#include "game.h"
#include "room.h"
#include "mob.h"
#include "color.inl"

#pragma warning(push, 0)
#include <array.h>
#include <hash.h>
#include <murmur_hash.h>
#include <engine/engine.h>
#include <engine/sprites.h>
#include <engine/atlas.h>
#include <engine/log.h>
#include <engine/texture.h>
#include <imgui.h>
#include <proto/game.pb.h>
#include <string_stream.h>
#pragma warning(pop)

namespace {
bool room_templates_did_reset = false;
bool room_templates_dirty = false;
bool mob_templates_did_reset = false;
bool mob_templates_dirty = false;
} // namespace

namespace game {
namespace editor {
using namespace foundation;

EditorState::EditorState(Allocator &allocator)
: allocator(allocator) {}

char tile_tool_label(uint8_t tile_type) {
    switch (static_cast<RoomTemplate::TileType>(tile_type)) {
    case RoomTemplate::TileType::Empty:
        return ' ';
    case RoomTemplate::TileType::Floor:
        return '.';
    case RoomTemplate::TileType::Wall:
        return '#';
    case RoomTemplate::TileType::Connection:
        return '%';
    case RoomTemplate::TileType::Stair:
        return '>';
    default:
        log_fatal("Unsupported TileType %u", tile_type);
    }

    return 0;
}

const char *tile_tool_description(uint8_t tile_type) {
    switch (static_cast<RoomTemplate::TileType>(tile_type)) {
    case RoomTemplate::TileType::Empty:
        return "Empty";
    case RoomTemplate::TileType::Floor:
        return "Floor";
    case RoomTemplate::TileType::Wall:
        return "Wall";
    case RoomTemplate::TileType::Connection:
        return "Connection";
    case RoomTemplate::TileType::Stair:
        return "Stair";
    default:
        log_fatal("Unsupported TileType %u", tile_type);
    }

    return nullptr;
}

ImVec4 tile_tool_color(uint8_t tile_type) {
    // salmon  217, 105, 65
    // red  166, 43, 31

    switch (static_cast<RoomTemplate::TileType>(tile_type)) {
    case RoomTemplate::TileType::Empty:
        return ImColor(0, 0, 0);
    case RoomTemplate::TileType::Floor:
        return ImColor(25, 60, 64);
    case RoomTemplate::TileType::Wall:
        return ImColor(33, 64, 1);
    case RoomTemplate::TileType::Connection:
        return ImColor(46, 89, 2);
    case RoomTemplate::TileType::Stair:
        return ImColor(25, 60, 64);
    default:
        log_fatal("Unsupported TileType %u", tile_type);
    }

    return ImVec4();
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
                room_templates_did_reset = true;
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
                    Array<RoomTemplate *> room_templates_copy = game.room_templates->room_templates;
                    RoomTemplate *selected_template = game.room_templates->room_templates[selected_template_index];
                    RoomTemplate *selected_template_copy = MAKE_NEW(game.room_templates->allocator, RoomTemplate, *selected_template);
                    string_stream::push(*selected_template_copy->name, "_copy", 5);

                    array::clear(game.room_templates->room_templates);

                    for (uint32_t i = 0; i < array::size(room_templates_copy); ++i) {
                        array::push_back(game.room_templates->room_templates, room_templates_copy[i]);

                        if (i == (uint32_t)selected_template_index) {
                            array::push_back(game.room_templates->room_templates, selected_template_copy);
                        }
                    }

                    room_templates_dirty = true;
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (room_templates_did_reset || (selected_template_index >= 0 && selected_template_index > (int32_t)array::size(game.room_templates->room_templates))) {
        selected_template_index = -1;
        memset(template_name, 0, 256);
    }

    // Left
    {
        if (ImGui::Button("Add")) {
            RoomTemplate *t = MAKE_NEW(game.room_templates->allocator, RoomTemplate, game.room_templates->allocator);
            string_stream::push(*t->name, "Room", 4);
            t->rarity = 1;
            t->tags = 0;
            t->columns = 3;
            t->rows = 3;

            for (int i = 0; i < t->columns * t->rows; ++i) {
                array::push_back(*t->tiles, (uint8_t)0);
            }

            array::push_back(game.room_templates->room_templates, t);
            room_templates_dirty = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Delete")) {
            if (selected_template_index >= 0) {
                Array<RoomTemplate *> room_templates_copy = game.room_templates->room_templates;
                RoomTemplate *selected_template = game.room_templates->room_templates[selected_template_index];

                array::clear(game.room_templates->room_templates);

                for (uint32_t i = 0; i < array::size(room_templates_copy); ++i) {
                    if (i != (uint32_t)selected_template_index) {
                        array::push_back(game.room_templates->room_templates, room_templates_copy[i]);
                    }
                }

                MAKE_DELETE(game.room_templates->allocator, RoomTemplate, selected_template);
                selected_template = nullptr;

                --selected_template_index;

                room_templates_dirty = true;
            }
        }

        ImGui::BeginChild("room_templates_left_pane", ImVec2(150, 0), true);

        int32_t i = 0;
        for (RoomTemplate **it = array::begin(game.room_templates->room_templates); it != array::end(game.room_templates->room_templates); ++it) {
            const RoomTemplate *room_template = *it;
            string_stream::Buffer *name_buffer = room_template->name;
            const char *name = string_stream::c_str(*name_buffer);
            ImGui::PushID(i);
            if (ImGui::Selectable(name, selected_template_index == i)) {
                selected_template_index = i;
                memset(template_name, 0, 256);
                memcpy(template_name, name, strlen(name));
                rarity = room_template->rarity;
                tags = room_template->tags;
                tags_start_room = (tags & RoomTemplate::Tags::RoomTemplateTagsStartRoom) != 0;
                tags_boss_room = (tags & RoomTemplate::Tags::RoomTemplateTagsBossRoom) != 0;
                rows = room_template->rows;
                columns = room_template->columns;
                memcpy(room_template_tiles, room_template->tiles, rows * columns);
            }
            ImGui::PopID();

            if (ImGui::BeginPopupContextItem()) {
                ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
                if (i == 0) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move up", false, flags)) {
                    Array<RoomTemplate *> room_templates_copy = game.room_templates->room_templates;
                    array::clear(game.room_templates->room_templates);

                    for (uint32_t n = 0; n < array::size(room_templates_copy); ++n) {
                        uint32_t index = n;

                        if (n == (uint32_t)i - 1) {
                            index = i;
                        } else if (n == (uint32_t)i) {
                            index = i - 1;
                        }

                        array::push_back(game.room_templates->room_templates, room_templates_copy[index]);
                    }

                    --selected_template_index;
                    room_templates_dirty = true;
                }

                flags = ImGuiSelectableFlags_None;
                if (i == (int32_t)array::size(game.room_templates->room_templates) - 1) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move down", false, flags)) {
                    Array<RoomTemplate *> room_templates_copy = game.room_templates->room_templates;
                    array::clear(game.room_templates->room_templates);

                    for (int32_t n = 0; (uint32_t)n < array::size(room_templates_copy); ++n) {
                        int32_t index = n;

                        if (n == i + 1) {
                            index = i;
                        } else if (n == i) {
                            index = i + 1;
                        }

                        array::push_back(game.room_templates->room_templates, room_templates_copy[index]);
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
        RoomTemplate *room_template = nullptr;
        if (selected_template_index >= 0) {
            room_template = game.room_templates->room_templates[selected_template_index];
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
            }

            // Rarity
            {
                ImGui::PushItemWidth(100);
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

                                for (uint8_t i = room_template->rows - 1; i > 0; --i) {
                                    uint8_t *start = array::begin(*room_template->tiles) + i * old_columns;
                                    memcpy(copy_buf, start, old_columns);
                                    memset(start, 0, old_columns);
                                    memcpy(start + i, copy_buf, old_columns);
                                }
                            } else {
                                uint8_t copy_buf[max_side];

                                for (uint8_t i = 1; i < room_template->rows; ++i) {
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
                        tags = tags | RoomTemplate::Tags::RoomTemplateTagsStartRoom;
                    } else {
                        tags = tags ^ RoomTemplate::Tags::RoomTemplateTagsStartRoom;
                    }

                    room_template->tags = tags;
                    room_templates_dirty = true;
                }

                ImGui::SameLine();

                if (ImGui::Checkbox("Boss room", &tags_boss_room)) {
                    if (tags_boss_room) {
                        tags = tags | RoomTemplate::Tags::RoomTemplateTagsBossRoom;
                    } else {
                        tags = tags ^ RoomTemplate::Tags::RoomTemplateTagsBossRoom;
                    }

                    room_template->tags = tags;
                    room_templates_dirty = true;
                }
            }

            // Tile tool palette
            {
                ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
                ImGui::BeginChild("ToolPalette", ImVec2(120, 0), false, window_flags);

                ImDrawList *draw_list = ImGui::GetWindowDrawList();

                for (uint8_t i = 0; i < static_cast<uint8_t>(RoomTemplate::TileType::Count); ++i) {
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

void mob_templates_editor(engine::Engine &engine, game::Game &game, bool *show_window) {
    if (*show_window == false) {
        return;
    }

    static int32_t selected_template_index = -1;
    static char template_name[256] = {'\0'};
    static int rarity = 0;
    static uint8_t tags = 0;
    static bool tags_boss = false;
    static char sprite_name[256] = {'\0'};
    static ImVec4 sprite_color;
    bool did_select_this_frame = false;

    const char *menu_label = nullptr;
    if (mob_templates_dirty) {
        menu_label = "Mob Templates*###MobTemplatesWindow";
    } else {
        menu_label = "Mob Templates###MobTemplatesWindow";
    }

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(menu_label, show_window, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open")) {
                const char *filename = game.params->mob_templates_filename().c_str();
                game.mob_templates->read(filename);
                mob_templates_did_reset = true;
                mob_templates_dirty = false;
            }

            if (ImGui::MenuItem("Save", nullptr, false, mob_templates_dirty)) {
                const char *filename = game.params->mob_templates_filename().c_str();
                game.mob_templates->write(filename);
                mob_templates_dirty = false;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Copy")) {
                if (selected_template_index >= 0) {
                    Array<MobTemplate *> mob_templates_copy = game.mob_templates->mob_templates;
                    MobTemplate *selected_template = game.mob_templates->mob_templates[selected_template_index];
                    MobTemplate *selected_template_copy = MAKE_NEW(game.mob_templates->allocator, MobTemplate, *selected_template);
                    string_stream::push(*selected_template_copy->name, "_copy", 5);

                    array::clear(game.mob_templates->mob_templates);

                    for (uint32_t i = 0; i < array::size(mob_templates_copy); ++i) {
                        array::push_back(game.mob_templates->mob_templates, mob_templates_copy[i]);

                        if (i == (uint32_t)selected_template_index) {
                            array::push_back(game.mob_templates->mob_templates, selected_template_copy);
                        }
                    }

                    mob_templates_dirty = true;
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (mob_templates_did_reset || (selected_template_index >= 0 && selected_template_index > (int32_t)array::size(game.mob_templates->mob_templates))) {
        selected_template_index = -1;
        memset(template_name, 0, 256);
        memset(sprite_name, 0, 256);
        sprite_color.x = color::white.r;
        sprite_color.y = color::white.g;
        sprite_color.z = color::white.b;
        sprite_color.w = color::white.a;
    }

    // Left
    {
        if (ImGui::Button("Add")) {
            MobTemplate *t = MAKE_NEW(game.mob_templates->allocator, MobTemplate, game.mob_templates->allocator);
            string_stream::push(*t->name, "Mob", 4);
            t->rarity = 1;
            t->tags = 0;

            array::push_back(game.mob_templates->mob_templates, t);
            mob_templates_dirty = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Delete")) {
            if (selected_template_index >= 0) {
                Array<MobTemplate *> mob_templates_copy = game.mob_templates->mob_templates;
                MobTemplate *selected_template = game.mob_templates->mob_templates[selected_template_index];

                array::clear(game.mob_templates->mob_templates);

                for (uint32_t i = 0; i < array::size(mob_templates_copy); ++i) {
                    if (i != (uint32_t)selected_template_index) {
                        array::push_back(game.mob_templates->mob_templates, mob_templates_copy[i]);
                    }
                }

                MAKE_DELETE(game.mob_templates->allocator, MobTemplate, selected_template);
                selected_template = nullptr;

                --selected_template_index;

                mob_templates_dirty = true;
            }
        }

        ImGui::BeginChild("mob_templates_left_pane", ImVec2(150, 0), true);

        int32_t i = 0;
        for (MobTemplate **it = array::begin(game.mob_templates->mob_templates); it != array::end(game.mob_templates->mob_templates); ++it) {
            const MobTemplate *mob_template = *it;
            const char *name = string_stream::c_str(*mob_template->name);
            ImGui::PushID(i);
            if (ImGui::Selectable(name, selected_template_index == i)) {
                selected_template_index = i;
                strncpy_s(template_name, name, 256);
                rarity = mob_template->rarity;
                tags = mob_template->tags;
                tags_boss = (tags & MobTemplate::Tags::MobTemplateTagsBoss) != 0;
                strncpy_s(sprite_name, string_stream::c_str(*mob_template->sprite_name), 256);

                sprite_color.x = mob_template->sprite_color.r;
                sprite_color.y = mob_template->sprite_color.g;
                sprite_color.z = mob_template->sprite_color.b;
                sprite_color.w = mob_template->sprite_color.a;

                did_select_this_frame = true;
            }
            ImGui::PopID();

            if (ImGui::BeginPopupContextItem()) {
                ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
                if (i == 0) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move up", false, flags)) {
                    Array<MobTemplate *> mob_templates_copy = game.mob_templates->mob_templates;
                    array::clear(game.mob_templates->mob_templates);

                    for (uint32_t n = 0; n < array::size(mob_templates_copy); ++n) {
                        uint32_t index = n;

                        if (n == (uint32_t)i - 1) {
                            index = i;
                        } else if (n == (uint32_t)i) {
                            index = i - 1;
                        }

                        array::push_back(game.mob_templates->mob_templates, mob_templates_copy[index]);
                    }

                    --selected_template_index;
                    mob_templates_dirty = true;
                    did_select_this_frame = true;
                }

                flags = ImGuiSelectableFlags_None;
                if (i == (int32_t)array::size(game.mob_templates->mob_templates) - 1) {
                    flags = ImGuiSelectableFlags_Disabled;
                }

                if (ImGui::Selectable("Move down", false, flags)) {
                    Array<MobTemplate *> mob_templates_copy = game.mob_templates->mob_templates;
                    array::clear(game.mob_templates->mob_templates);

                    for (int32_t n = 0; (uint32_t)n < array::size(mob_templates_copy); ++n) {
                        int32_t index = n;

                        if (n == i + 1) {
                            index = i;
                        } else if (n == i) {
                            index = i + 1;
                        }

                        array::push_back(game.mob_templates->mob_templates, mob_templates_copy[index]);
                    }

                    ++selected_template_index;
                    mob_templates_dirty = true;
                    did_select_this_frame = true;
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
        MobTemplate *mob_template = nullptr;
        if (selected_template_index >= 0) {
            mob_template = game.mob_templates->mob_templates[selected_template_index];
        }

        if (mob_template) {
            ImGui::BeginGroup();

            // Name
            {
                ImGui::PushItemWidth(240);
                if (ImGui::InputText("Name", template_name, 256, ImGuiInputTextFlags_CharsNoBlank)) {
                    string_stream::Buffer *name = mob_template->name;
                    array::clear(*name);
                    string_stream::push(*name, template_name, (uint32_t)strlen(template_name));
                    mob_templates_dirty = true;
                }
            }

            // Rarity
            {
                ImGui::PushItemWidth(100);
                if (ImGui::InputInt("Rarity", &rarity)) {
                    mob_templates_dirty = true;
                    if (rarity <= 0) {
                        rarity = 1;
                    }

                    if (rarity > 4) {
                        rarity = 4;
                    }

                    mob_template->rarity = (uint8_t)rarity;
                }
            }

            // Tags
            {
                if (ImGui::Checkbox("Boss", &tags_boss)) {
                    if (tags_boss) {
                        tags = tags | MobTemplate::Tags::MobTemplateTagsBoss;
                    } else {
                        tags = tags ^ MobTemplate::Tags::MobTemplateTagsBoss;
                    }

                    mob_template->tags = tags;
                    mob_templates_dirty = true;
                }
            }

            // Color
            {
                static bool saved_palette_init = true;
                const int palette_count = 16;
                static ImVec4 saved_palette[palette_count] = {};
                if (saved_palette_init) {
                    auto add_color = [](int i, math::Color4f color) {
                        saved_palette[i] = {color.r, color.g, color.b, color.a};
                    };
                    add_color(0, color::black);
                    add_color(1, color::storm);
                    add_color(2, color::wine);
                    add_color(3, color::moss);
                    add_color(4, color::tan);
                    add_color(5, color::slate);
                    add_color(6, color::silver);
                    add_color(7, color::white);
                    add_color(8, color::ember);
                    add_color(9, color::orange);
                    add_color(10, color::lemon);
                    add_color(11, color::lime);
                    add_color(12, color::sky);
                    add_color(13, color::dusk);
                    add_color(14, color::pink);
                    add_color(15, color::peach);

                    saved_palette_init = false;
                }
                
                ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
                bool open_popup = ImGui::ColorButton("Color#b", sprite_color, flags);
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                open_popup |= ImGui::Button("Color");

                if (open_popup) {
                    ImGui::OpenPopup("sprite_color_picker");
                }

                if (ImGui::BeginPopup("sprite_color_picker")) {
                    if (ImGui::ColorPicker4("##picker", (float*)&sprite_color, flags)) {
                            mob_template->sprite_color.r = sprite_color.x;
                            mob_template->sprite_color.g = sprite_color.y;
                            mob_template->sprite_color.b = sprite_color.z;
                            mob_template->sprite_color.a = sprite_color.w;

                            mob_templates_dirty = true;
                    }

                    ImGui::SameLine();
                    ImGui::BeginGroup();

                    for (int i = 0; i < palette_count; ++i) {
                        ImGui::PushID(i);
                        if ((i % 8) != 0) {
                            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
                        }

                        ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip;
                        if (ImGui::ColorButton("##palette", saved_palette[i], palette_button_flags, ImVec2(20, 20))) {
                            sprite_color = ImVec4(saved_palette[i].x, saved_palette[i].y, saved_palette[i].z, sprite_color.w);

                            mob_template->sprite_color.r = sprite_color.x;
                            mob_template->sprite_color.g = sprite_color.y;
                            mob_template->sprite_color.b = sprite_color.z;
                            mob_template->sprite_color.a = sprite_color.w;

                            mob_templates_dirty = true;
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndGroup();
                    ImGui::EndPopup();
                }
            }

            // Sprite
            {
                static ImGuiComboFlags flags = 0;
                const Array<Buffer *> &sprite_names = *engine.sprites->atlas->sprite_names;

                if (ImGui::BeginListBox("", ImVec2(200, 465))) {
                    int i = 0;

                    for (auto it = array::begin(sprite_names); it != array::end(sprite_names); ++it) {
                        ImGui::PushID(i);
                        bool did_select = false;

                        const char *selectable_sprite_name = string_stream::c_str(**it);
                        const bool is_selected = strncmp(sprite_name, selectable_sprite_name, 256) == 0;

                        static math::Rect default_sprite_rect;
                        uint64_t key = murmur_hash_64(selectable_sprite_name, strnlen_s(selectable_sprite_name, 256), 0);
                        if (hash::has(*engine.sprites->atlas->frames, key)) {
                            const math::Rect &rect = hash::get(*engine.sprites->atlas->frames, key, default_sprite_rect);

                            float texture_width = (float)engine.sprites->atlas->texture->width;
                            float texture_height = (float)engine.sprites->atlas->texture->height;

                            ImTextureID tex_id = (void *)(intptr_t)engine.sprites->atlas->texture->texture;

                            ImVec2 texture_size = ImVec2(32.0f, 32.0f);
                            ImVec2 uv_min = ImVec2(rect.origin.x / texture_width, rect.origin.y / texture_height);
                            ImVec2 uv_max = ImVec2((rect.origin.x + rect.size.x) / texture_width, (rect.origin.y + rect.size.y) / texture_height);
                            ImVec4 bg_col = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
                            if (ImGui::ImageButton(tex_id, texture_size, uv_min, uv_max, -1, bg_col, sprite_color)) {
                                did_select = true;
                            }
                        }

                        ImGui::SameLine();

                        if (ImGui::Selectable(selectable_sprite_name, is_selected)) {
                            did_select = true;
                        }

                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();

                            if (did_select_this_frame) {
                                ImGui::SetScrollHereY();
                            }
                        }

                        if (did_select) {
                            strncpy_s(sprite_name, selectable_sprite_name, 256);
                            array::clear(*mob_template->sprite_name);
                            string_stream::push(*mob_template->sprite_name, selectable_sprite_name, strnlen(selectable_sprite_name, 256));
                            mob_templates_dirty = true;
                        }

                        ++i;

                        ImGui::PopID();
                    }

                    ImGui::EndListBox();
                }
            }

            ImGui::EndGroup();
        }
    }

    ImGui::End();
}

void render_imgui(engine::Engine &engine, game::Game &game, EditorState &state) {
    (void)engine;
    (void)state;

    static bool show_gamestate_controls_window = true;
    static bool show_room_templates_window = true;
    static bool show_mob_templates_window = true;
    static bool show_metrics_window = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("Gamestate Controls", nullptr, show_gamestate_controls_window)) {
                show_gamestate_controls_window = !show_gamestate_controls_window;
            }

            if (ImGui::MenuItem("Room templates", nullptr, show_room_templates_window)) {
                show_room_templates_window = !show_room_templates_window;
            }

            if (ImGui::MenuItem("Mob templates", nullptr, show_mob_templates_window)) {
                show_mob_templates_window = !show_mob_templates_window;
            }

            if (ImGui::MenuItem("Show Metrics", nullptr, show_metrics_window)) {
                show_metrics_window = !show_metrics_window;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    gamestate_controls_window(engine, game, &show_gamestate_controls_window);
    room_templates_editor(game, &show_room_templates_window);
    mob_templates_editor(engine, game, &show_mob_templates_window);

    if (show_metrics_window) {
        ImGui::ShowMetricsWindow();
    }

    room_templates_did_reset = false;
    mob_templates_did_reset = false;
}

} // namespace editor
} // namespace game
