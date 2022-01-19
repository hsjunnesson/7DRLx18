#include <backward.hpp>
#include <memory.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int status = 0;

    backward::SignalHandling sh;

    foundation::memory_globals::init();

    foundation::Allocator &allocator = foundation::memory_globals::default_allocator();

    foundation::memory_globals::shutdown();

    return status;
}
