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
    enum Tags {
        MobTemplateTagsNone = 0,
        MobTemplateTagsBoss = 1 << 0,
    };

    explicit MobTemplate(foundation::Allocator &allocator);
    explicit MobTemplate(const MobTemplate &other);
    MobTemplate(MobTemplate &&) noexcept = delete;
    MobTemplate &operator=(const MobTemplate &) = delete;
    MobTemplate &operator=(MobTemplate &&) noexcept = delete;
    ~MobTemplate();

    foundation::Allocator &allocator;
    foundation::Array<char> *name;
    foundation::Array<char> *sprite_name;
    uint8_t rarity;
    uint8_t tags;
};

struct MobTemplates {
    MobTemplates(foundation::Allocator &allocator);
    ~MobTemplates();
    MobTemplates(const MobTemplates &) = delete;
    MobTemplates &operator=(const MobTemplates &) = delete;

    foundation::Allocator &allocator;
    foundation::Array<MobTemplate *> mob_templates;

    void read(const char *filename);
    void write(const char *filename);
};

} // namespace game
