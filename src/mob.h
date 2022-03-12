#pragma once

#pragma warning(push, 0)
#include <stdint.h>
#include <collection_types.h>
#pragma warning(pop)

namespace game {

struct Mob {
    uint64_t sprite_id;
    int32_t template_index;
    int32_t tile_index;
    float energy;
};

struct MobTemplate {
    MobTemplate(foundation::Allocator &allocator);
    MobTemplate(const MobTemplate &other);
    MobTemplate &operator=(const MobTemplate &) = delete;
    ~MobTemplate();

    foundation::Allocator &allocator;
    foundation::Array<char> *name;
};

foundation::Array<MobTemplate> *init_mob_templates(foundation::Allocator &allocator, const char *filename);
void write_mob_templates(foundation::Array<MobTemplate> *mob_templates, const char *filename);

} // namespace game
