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
    
    if (!ImGui::Begin("Editor")) {
        ImGui::End();
    } else {
        ImGui::End();
    }
}

} // namespace editor
} // namespace game
