#include "mob.h"
#include "color.inl"

#include <engine/file.h>
#include <engine/log.h>

#pragma warning(push, 0)
#include <array.h>
#include <memory.h>
#include <temp_allocator.h>
#include <string_stream.h>
#include <assert.h>
#pragma warning(pop)

namespace {
const char *mob_templates_header = "MOBTEMPLATES";
const size_t mob_templates_header_len = strlen(mob_templates_header);
} // namespace

namespace game {
using namespace foundation;
using namespace foundation::string_stream;

MobTemplate::MobTemplate(foundation::Allocator &allocator)
: allocator(allocator)
, name(nullptr)
, sprite_name(nullptr)
, sprite_color(color::white)
, rarity(1)
, tags(0)
{
    name = MAKE_NEW(allocator, string_stream::Buffer, allocator);
    sprite_name = MAKE_NEW(allocator, string_stream::Buffer, allocator);
}

MobTemplate::MobTemplate(const MobTemplate &other)
: allocator(allocator)
, name(nullptr)
, sprite_name(nullptr)
, sprite_color(other.sprite_color)
, rarity(other.rarity)
, tags(other.tags)
{
    name = MAKE_NEW(allocator, string_stream::Buffer, *other.name);
    sprite_name = MAKE_NEW(allocator, string_stream::Buffer, *other.sprite_name);
}

MobTemplate::~MobTemplate() {
    MAKE_DELETE(allocator, Array, name);
    MAKE_DELETE(allocator, Array, sprite_name);
}

MobTemplates::MobTemplates(Allocator &allocator)
: allocator(allocator)
, mob_templates(allocator) {
}

MobTemplates::~MobTemplates() {
    for (MobTemplate **iter = array::begin(mob_templates); iter != array::end(mob_templates); ++iter) {
        MAKE_DELETE(allocator, MobTemplate, *iter);
    }
}

void MobTemplates::read(const char *filename) {
    assert(filename != nullptr);

    // Clear old templates
    {
        for (MobTemplate **iter = array::begin(mob_templates); iter != array::end(mob_templates); ++iter) {
            MAKE_DELETE(allocator, MobTemplate, *iter);
        }

        array::clear(mob_templates);
    }

    TempAllocator2048 ta;
    Buffer file_buffer(ta);
    if (!engine::file::read(file_buffer, filename)) {
        log_fatal("Could not parse: %s", filename);
    }

    if (array::size(file_buffer) < mob_templates_header_len + 1) { // Account for additional version byte
        log_info("Empty file: %s", filename);
        return;
    }

    char *p = array::begin(file_buffer);
    const char *pe = array::end(file_buffer);

    // Header
    {
        char header_buf[256];
        memcpy(header_buf, p, mob_templates_header_len);
        if (strncmp(mob_templates_header, header_buf, mob_templates_header_len) != 0) {
            log_fatal("Could not parse: %s invalid header", filename);
        }

        p = p + mob_templates_header_len;
    }

    const uint8_t version = *p;
    ++p;

    // Version 1 original
    // Version 2 sprite_color added
    const uint8_t max_version = 2;
    const uint8_t min_version = 1;

    if (version > max_version) {
        log_fatal("Could not parse: %s version %u not supported, max version is %u", filename, version, max_version);
    }

    if (version < min_version) {
        log_fatal("Could not parse: %s version %u not supported, min version is %u", filename, version, min_version);
    }

    while (p < pe) {
        const uint8_t name_length = *p;
        ++p;

        if (p >= pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        string_stream::Buffer *name_buffer = MAKE_NEW(allocator, Array<char>, allocator);
        string_stream::push(*name_buffer, p, name_length);

        p = p + name_length;

        if (p >= pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        uint8_t rarity = *p;
        ++p;

        if (p >= pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        uint8_t tags = *p;
        ++p;

        if (p >= pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        color::Color4f sprite_color;
        if (version >= 2) {
            // TODO: rewrite this, yikes.
            memcpy(&sprite_color.r, p, sizeof(float));
            p += sizeof(float);
            if (p >= pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }

            memcpy(&sprite_color.g, p, sizeof(float));
            p += sizeof(float);
            if (p >= pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }

            memcpy(&sprite_color.b, p, sizeof(float));
            p += sizeof(float);
            if (p >= pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }

            memcpy(&sprite_color.a, p, sizeof(float));
            p += sizeof(float);
            if (p >= pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }
        }

        const uint8_t sprite_name_length = *p;
        ++p;

        if (p > pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        string_stream::Buffer *sprite_name_buffer = MAKE_NEW(allocator, Array<char>, allocator);
        if (sprite_name_length > 0) {
            string_stream::push(*sprite_name_buffer, p, sprite_name_length);

            p += sprite_name_length;
            
            if (p > pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }
        }

        MobTemplate *mob_template = MAKE_NEW(allocator, MobTemplate, allocator);
        MAKE_DELETE(allocator, Array, mob_template->name);
        mob_template->name = name_buffer;
        mob_template->rarity = rarity;
        mob_template->tags = tags;
        MAKE_DELETE(allocator, Array, mob_template->sprite_name);
        mob_template->sprite_name = sprite_name_buffer;
        mob_template->sprite_color = sprite_color;

        array::push_back(mob_templates, mob_template);
    }
}

void MobTemplates::write(const char *filename) {
    assert(filename != nullptr);

    FILE *file = nullptr;
    if (fopen_s(&file, filename, "wb") != 0) {
        log_fatal("Could not write: %s", filename);
    }

    // Header
    fwrite(mob_templates_header, mob_templates_header_len, 1, file);

    // Version
    const uint8_t version = 2;
    fwrite(&version, sizeof(uint8_t), 1, file);

    // Write mobs
    for (MobTemplate **iter = array::begin(mob_templates); iter != array::end(mob_templates); ++iter) {
        MobTemplate *mob_template = *iter;

        const uint8_t name_length = (uint8_t)array::size(*mob_template->name);
        fwrite(&name_length, sizeof(uint8_t), 1, file);
        fwrite(array::begin(*mob_template->name), sizeof(char), name_length, file);

        fwrite(&mob_template->rarity, sizeof(uint8_t), 1, file);

        fwrite(&mob_template->tags, sizeof(uint8_t), 1, file);

        fwrite(&mob_template->sprite_color.r, sizeof(float), 1, file);
        fwrite(&mob_template->sprite_color.g, sizeof(float), 1, file);
        fwrite(&mob_template->sprite_color.b, sizeof(float), 1, file);
        fwrite(&mob_template->sprite_color.a, sizeof(float), 1, file);

        const uint8_t sprite_name_length = (uint8_t)array::size(*mob_template->sprite_name);
        fwrite(&sprite_name_length, sizeof(uint8_t), 1, file);
        fwrite(array::begin(*mob_template->sprite_name), sizeof(char), sprite_name_length, file);
    }

    fclose(file);
}

} // namespace game
