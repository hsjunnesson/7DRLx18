#pragma once

#include <collection_types.h>

namespace engine {
struct Engine;
} // namespace engine

namespace game {
struct Game;

namespace editor {
using namespace foundation;

struct EditorState {
    Allocator &allocator;

    EditorState(Allocator &allocator);
};

void render_imgui(engine::Engine &engine, game::Game &game, EditorState &state);

} // namespace editor
} // namespace game
