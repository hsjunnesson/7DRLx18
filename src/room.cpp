#include "room.h"

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
const char *room_templates_header = "ROOMTEMPLATES";
const size_t room_templates_header_len = strlen(room_templates_header);
} // namespace

namespace game {
using namespace foundation;
using namespace foundation::string_stream;

RoomTemplate::RoomTemplate(Allocator &allocator)
: allocator(allocator)
, name(nullptr)
, rarity(1)
, tags(0)
, rows(0)
, columns(0)
, tiles(nullptr) {
    name = MAKE_NEW(allocator, Buffer, allocator);
    tiles = MAKE_NEW(allocator, Array<uint8_t>, allocator);
}

RoomTemplate::RoomTemplate(const RoomTemplate &other)
: allocator(other.allocator)
, name(nullptr)
, rarity(other.rarity)
, tags(other.tags)
, rows(other.rows)
, columns(other.columns)
, tiles(nullptr) {
    const Array<char> &other_name = *other.name;
    const Array<uint8_t> &other_data = *other.tiles;

    name = MAKE_NEW(allocator, string_stream::Buffer, other_name);
    tiles = MAKE_NEW(allocator, Array<uint8_t>, other_data);
}

RoomTemplate::~RoomTemplate() {
    MAKE_DELETE(allocator, Array, name);
    MAKE_DELETE(allocator, Array, tiles);
}

RoomTemplates::RoomTemplates(Allocator &allocator)
: allocator(allocator)
, room_templates(allocator) {
}

RoomTemplates::~RoomTemplates() {
    for (RoomTemplate **iter = array::begin(room_templates); iter != array::end(room_templates); ++iter) {
        MAKE_DELETE(allocator, RoomTemplate, *iter);
    }
}

void RoomTemplates::read(const char *filename) {
    assert(filename != nullptr);

    // Clear old templates
    {
        for (RoomTemplate **iter = array::begin(room_templates); iter != array::end(room_templates); ++iter) {
            MAKE_DELETE(allocator, RoomTemplate, *iter);
        }

        array::clear(room_templates);
    }

    TempAllocator2048 ta;
    Buffer file_buffer(ta);
    if (!engine::file::read(file_buffer, filename)) {
        log_fatal("Could not parse: %s", filename);
    }

    if (array::size(file_buffer) < room_templates_header_len + 1) { // Account for additional version byte
        log_info("Empty file: %s", filename);
        return;
    }

    char *p = array::begin(file_buffer);
    const char *pe = array::end(file_buffer);

    // Header
    {
        char header_buf[256];
        memcpy(header_buf, p, room_templates_header_len);
        if (strncmp(room_templates_header, header_buf, room_templates_header_len) != 0) {
            log_fatal("Could not parse: %s invalid header", filename);
        }

        p = p + room_templates_header_len;
    }

    const uint8_t version = *p;
    ++p;

    // Version 1 original
    // Version 2 added rarity and tags
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

        uint8_t rarity = 1;
        if (version >= 2) {
            rarity = *p;
            ++p;

            if (p >= pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }
        }

        uint8_t tags = 0;
        if (version >= 2) {
            tags = *p;
            ++p;

            if (p >= pe) {
                log_fatal("Could not parse: %s invalid file format", filename);
            }
        }

        const uint8_t rows = *p;
        ++p;

        if (p >= pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        const uint8_t columns = *p;
        ++p;

        if (p >= pe) {
            log_fatal("Could not parse: %s invalid file format", filename);
        }

        const uint16_t data_length = rows * columns;

        Array<uint8_t> *data = MAKE_NEW(allocator, Array<uint8_t>, allocator);
        array::resize(*data, data_length);
        memcpy(array::begin(*data), p, data_length);

        p = p + data_length;

        RoomTemplate *room_template = MAKE_NEW(allocator, RoomTemplate, allocator);
        MAKE_DELETE(allocator, Array, room_template->name);
        room_template->name = name_buffer;
        room_template->rarity = rarity;
        room_template->tags = tags;
        room_template->rows = rows;
        room_template->columns = columns;
        MAKE_DELETE(allocator, Array, room_template->tiles);
        room_template->tiles = data;

        array::push_back(room_templates, room_template);
    }
}

void RoomTemplates::write(const char *filename) {
    assert(filename != nullptr);

    FILE *file = nullptr;
    if (fopen_s(&file, filename, "wb") != 0) {
        log_fatal("Could not write: %s", filename);
    }

    // Header
    fwrite(room_templates_header, room_templates_header_len, 1, file);

    // Version
    const uint8_t version = 2;
    fwrite(&version, sizeof(uint8_t), 1, file);

    // Write rooms
    for (RoomTemplate **iter = array::begin(room_templates); iter != array::end(room_templates); ++iter) {
        RoomTemplate *room_template = *iter;
        const uint8_t name_length = (uint8_t)array::size(*room_template->name);
        fwrite(&name_length, sizeof(uint8_t), 1, file);

        fwrite(array::begin(*room_template->name), sizeof(char), name_length, file);

        fwrite(&room_template->rarity, sizeof(uint8_t), 1, file);
        fwrite(&room_template->tags, sizeof(uint8_t), 1, file);
        fwrite(&room_template->rows, sizeof(uint8_t), 1, file);
        fwrite(&room_template->columns, sizeof(uint8_t), 1, file);

        fwrite(array::begin(*room_template->tiles), sizeof(uint8_t), array::size(*room_template->tiles), file);
    }

    fclose(file);
}

} // namespace game
