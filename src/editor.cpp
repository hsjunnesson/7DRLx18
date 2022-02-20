#include "editor.h"
#include "game.h"

#include <engine/engine.h>
#include <imgui.h>

namespace game {
namespace editor {

EditorState::EditorState(Allocator &allocator)
: allocator(allocator) {}

void render_imgui(engine::Engine &engine, game::Game &game, EditorState &state) {
    (void)engine;
    (void)game;
    (void)state;

    // Room Templates
    {
        if (!ImGui::Begin("Room Templates")) {
            ImGui::End();
        } else {
            if (ImGui::BeginMenu("File")) {
                ImGui::EndMenu();
            }
            ImGui::End();
        }
    }
}

} // namespace editor
} // namespace game
