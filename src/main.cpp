#include "game.h"

#include <engine/engine.h>
#include <engine/input.h>

#include <backward.hpp>
#include <memory.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int status = 0;

    backward::SignalHandling sh;

    foundation::memory_globals::init();

    foundation::Allocator &allocator = foundation::memory_globals::default_allocator();

    {
        engine::Engine engine(allocator, "assets/engine_params.json");

        game::Game game(allocator);
        engine::EngineCallbacks engine_callbacks;
        engine_callbacks.on_input = game::on_input;
        engine_callbacks.update = game::update;
        engine_callbacks.render = game::render;
        engine_callbacks.render_imgui = game::render_imgui;
        engine_callbacks.on_shutdown = game::on_shutdown;

        engine.engine_callbacks = &engine_callbacks;
        engine.game_object = &game;

        status = engine::run(engine);
    }

    foundation::memory_globals::shutdown();

    return status;
}
