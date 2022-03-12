#include "mob.h"

#pragma warning(push, 0)
#include <memory.h>
#include <array.h>
#include <string_stream.h>
#pragma warning(pop)

namespace game {
using namespace foundation;

MobTemplate::MobTemplate(foundation::Allocator &allocator)
: allocator(allocator)
, name(nullptr)
{
    name = MAKE_NEW(allocator, string_stream::Buffer, allocator);
}

MobTemplate::MobTemplate(const MobTemplate &other)
: allocator(allocator)
, name(nullptr)
{
    const Array<char> &other_name = *other.name;
    name = MAKE_NEW(allocator, string_stream::Buffer, other_name);
}

MobTemplate::~MobTemplate() {
    MAKE_DELETE(allocator, Array, name);
}

Array<MobTemplate> *init_mob_templates(Allocator &allocator, const char *filename) {
    Array<MobTemplate> *mob_templates = MAKE_NEW(allocator, Array<MobTemplate>, allocator);

    return mob_templates;
}

void write_mob_templates(Array<MobTemplate> *mob_templates, const char *filename) {

}

} // namespace game
