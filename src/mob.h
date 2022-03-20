#pragma once

#pragma warning(push, 0)
#include <engine/math.inl>
#include <stdint.h>
#include <collection_types.h>
#pragma warning(pop)

namespace game {

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
    math::Color4f sprite_color;
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

struct Mob {
    uint64_t sprite_id = 0;
    MobTemplate *mob_template = nullptr;
    int32_t tile_index = 0;
    int8_t energy = 0;
};

} // namespace game
