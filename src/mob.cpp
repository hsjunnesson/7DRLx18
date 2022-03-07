#include "mob.h"

#pragma warning(push, 0)
#include <memory.h>
#include <new>
#pragma warning(pop)

namespace game {
using namespace foundation;

Array<MobTemplate> *init_mob_templates(Allocator &allocator, const char *filename) {
    Array<MobTemplate> *mob_templates = MAKE_NEW(allocator, Array<MobTemplate>, allocator);

    return mob_templates;
}

void write_mob_templates(Array<MobTemplate> *mob_templates, const char *filename) {

}

} // namespace game
