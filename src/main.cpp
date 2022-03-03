#include "game.h"

#pragma warning(push, 0)
#define RND_IMPLEMENTATION
#include "rnd.h"

#include <engine/engine.h>
#include <engine/input.h>

#include <backward.hpp>
#include <memory.h>

#if defined(LIVE_PP)
#include "LPP_API.h"
#include <Windows.h>
#endif
#pragma warning(pop)

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#if defined(LIVE_PP)
    HMODULE livePP = lpp::lppLoadAndRegister(L"LivePP", "AGroupName");
    lpp::lppEnableAllCallingModulesSync(livePP);
#endif

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

#if defined(LIVE_PP)
    lpp::lppShutdown(livePP);
    ::FreeLibrary(livePP);
#endif

    return status;
}
