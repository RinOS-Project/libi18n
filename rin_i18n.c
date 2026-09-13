#include "rin_i18n.h"
#include "rin_unicode.h"

#define RMSG_HEADER_SIZE 64u
#define RMSG_ENTRY_SIZE 20u

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static size_t text_length(const char* text) {
    size_t length = 0u;
    if (!text) return 0u;
    while (text[length] != '\0') length++;
    return length;
}

static int text_equal(const char* lhs, const char* rhs) {
    size_t index = 0u;
    if (!lhs || !rhs) return 0;
    while (lhs[index] && rhs[index] && lhs[index] == rhs[index]) index++;
    return lhs[index] == rhs[index];
}

static int range_valid(size_t size, uint32_t offset, uint32_t length) {
    return (size_t)offset <= size && (size_t)length <= size - (size_t)offset;
}

static int pool_string(const RinI18nCatalog* catalog, uint32_t offset,
                       const char** output, size_t* length) {
    size_t cursor;
    size_t end;
    if (!catalog || offset >= catalog->strings_size) return 0;
    cursor = (size_t)catalog->strings_offset + offset;
    end = (size_t)catalog->strings_offset + catalog->strings_size;
    if (cursor >= catalog->size || end > catalog->size) return 0;
    if (output) *output = (const char*)catalog->data + cursor;
    if (length) *length = 0u;
    while (cursor < end && catalog->data[cursor] != 0u) {
        cursor++;
        if (length) (*length)++;
    }
    return cursor < end;
}

uint32_t rin_i18n_hash(const char* text) {
    uint32_t hash = 2166136261u;
    size_t index = 0u;
    if (!text) return 0u;
    while (text[index] != '\0') {
        hash ^= (uint8_t)text[index++];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t rin_i18n_crc32(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    size_t index;
    unsigned bit;
    if (!data && size != 0u) return 0u;
    for (index = 0u; index < size; ++index) {
        crc ^= bytes[index];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc >> 1u) ^ (0xEDB88320u &
                  (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

int rin_i18n_catalog_open(RinI18nCatalog* catalog,
                          const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t file_size;
    uint32_t expected_crc;
    uint32_t entry_count;
    uint32_t entries_offset;
    uint32_t strings_offset;
    uint32_t strings_size;
    uint32_t locale_offset;
    uint32_t locale_length;
    uint32_t previous_hash = 0u;
    uint32_t index;
    if (!catalog || !data || size < RMSG_HEADER_SIZE ||
        size > RIN_I18N_MAX_FILE_SIZE) return RIN_I18N_INVALID;
    if (bytes[0] != 'R' || bytes[1] != 'M' ||
        bytes[2] != 'S' || bytes[3] != 'G' ||
        read_u16(bytes + 4u) != RIN_I18N_RMSG_VERSION ||
        read_u16(bytes + 6u) != RMSG_HEADER_SIZE) return RIN_I18N_CORRUPT;
    file_size = read_u32(bytes + 8u);
    expected_crc = read_u32(bytes + 12u);
    locale_offset = read_u32(bytes + 16u);
    locale_length = read_u32(bytes + 20u);
    entry_count = read_u32(bytes + 24u);
    entries_offset = read_u32(bytes + 28u);
    strings_offset = read_u32(bytes + 32u);
    strings_size = read_u32(bytes + 36u);
    if (file_size != size || entries_offset < RMSG_HEADER_SIZE ||
        entry_count > (RIN_I18N_MAX_FILE_SIZE / RMSG_ENTRY_SIZE) ||
        !range_valid(size, entries_offset, entry_count * RMSG_ENTRY_SIZE) ||
        !range_valid(size, strings_offset, strings_size) ||
        locale_offset >= strings_size || locale_length >= strings_size - locale_offset ||
        rin_i18n_crc32(bytes + RMSG_HEADER_SIZE,
                       size - RMSG_HEADER_SIZE) != expected_crc) {
        return RIN_I18N_CORRUPT;
    }
    catalog->data = bytes;
    catalog->size = size;
    catalog->entry_count = entry_count;
    catalog->entries_offset = entries_offset;
    catalog->strings_offset = strings_offset;
    catalog->strings_size = strings_size;
    catalog->locale_offset = locale_offset;
    catalog->locale_length = locale_length;
    catalog->plural_rule = read_u32(bytes + 40u);
    for (index = 0u; index < entry_count; ++index) {
        const uint8_t* entry = bytes + entries_offset + index * RMSG_ENTRY_SIZE;
        uint32_t hash = read_u32(entry);
        const char* domain;
        const char* key;
        const char* value;
        size_t domain_len;
        size_t key_len;
        size_t value_len;
        if ((index != 0u && hash < previous_hash) ||
            !pool_string(catalog, read_u32(entry + 4u), &domain, &domain_len) ||
            !pool_string(catalog, read_u32(entry + 8u), &key, &key_len) ||
            !pool_string(catalog, read_u32(entry + 12u), &value, &value_len) ||
            read_u32(entry + 16u) != value_len ||
            !rin_unicode_validate_utf8(domain, domain_len, (size_t*)0) ||
            !rin_unicode_validate_utf8(key, key_len, (size_t*)0) ||
            !rin_unicode_validate_utf8(value, value_len, (size_t*)0)) {
            return RIN_I18N_CORRUPT;
        }
        previous_hash = hash;
    }
    return RIN_I18N_OK;
}

const char* rin_i18n_catalog_locale(const RinI18nCatalog* catalog,
                                    size_t* length) {
    const char* locale = (const char*)0;
    size_t actual = 0u;
    if (!catalog || !pool_string(catalog, catalog->locale_offset,
                                 &locale, &actual) ||
        actual != catalog->locale_length) return (const char*)0;
    if (length) *length = actual;
    return locale;
}

const char* rin_i18n_get(const RinI18nCatalog* catalog,
                         const char* domain, const char* key,
                         const char* fallback) {
    uint32_t wanted;
    uint32_t low = 0u;
    uint32_t high;
    uint32_t index;
    if (!catalog || !domain || !key) return fallback;
    wanted = rin_i18n_hash(domain);
    wanted ^= rin_i18n_hash(key) + 0x9E3779B9u + (wanted << 6u) + (wanted >> 2u);
    high = catalog->entry_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2u;
        const uint8_t* entry = catalog->data + catalog->entries_offset +
                               middle * RMSG_ENTRY_SIZE;
        if (read_u32(entry) < wanted) low = middle + 1u;
        else high = middle;
    }
    for (index = low; index < catalog->entry_count; ++index) {
        const uint8_t* entry = catalog->data + catalog->entries_offset +
                               index * RMSG_ENTRY_SIZE;
        const char* entry_domain;
        const char* entry_key;
        const char* value;
        if (read_u32(entry) != wanted) break;
        if (!pool_string(catalog, read_u32(entry + 4u), &entry_domain, 0) ||
            !pool_string(catalog, read_u32(entry + 8u), &entry_key, 0) ||
            !pool_string(catalog, read_u32(entry + 12u), &value, 0)) break;
        if (text_equal(domain, entry_domain) && text_equal(key, entry_key)) {
            return value;
        }
    }
    return fallback;
}

const char* rin_i18n_plural(const RinI18nCatalog* catalog,
                            const char* domain, const char* key,
                            uint64_t count, const char* fallback) {
    char composite[192];
    const char* suffix = ".other";
    size_t length = text_length(key);
    size_t suffix_length;
    size_t index;
    if (catalog && ((catalog->plural_rule == 1u && count == 1u) ||
                    (catalog->plural_rule == 2u && count <= 1u))) {
        suffix = ".one";
    }
    suffix_length = text_length(suffix);
    if (!key || length + suffix_length + 1u > sizeof(composite)) return fallback;
    for (index = 0u; index < length; ++index) composite[index] = key[index];
    for (index = 0u; index < suffix_length; ++index)
        composite[length + index] = suffix[index];
    composite[length + suffix_length] = '\0';
    return rin_i18n_get(catalog, domain, composite, fallback);
}

int rin_i18n_format(char* output, size_t capacity, const char* pattern,
                    const RinI18nArg* args, size_t arg_count) {
    size_t input = 0u;
    size_t written = 0u;
    if (!output || capacity == 0u || !pattern) return RIN_I18N_INVALID;
    while (pattern[input] != '\0') {
        if (pattern[input] == '{' && pattern[input + 1u] != '{') {
            size_t name_start = ++input;
            size_t name_length;
            size_t arg_index;
            const char* replacement = (const char*)0;
            while (pattern[input] && pattern[input] != '}') input++;
            if (pattern[input] != '}') return RIN_I18N_INVALID;
            name_length = input - name_start;
            for (arg_index = 0u; arg_index < arg_count; ++arg_index) {
                size_t candidate_length = text_length(args[arg_index].name);
                size_t compare;
                if (candidate_length != name_length) continue;
                for (compare = 0u; compare < name_length; ++compare) {
                    if (args[arg_index].name[compare] !=
                        pattern[name_start + compare]) break;
                }
                if (compare == name_length) {
                    replacement = args[arg_index].value;
                    break;
                }
            }
            if (!replacement) return RIN_I18N_NOT_FOUND;
            for (arg_index = 0u; replacement[arg_index] != '\0'; ++arg_index) {
                if (written + 1u >= capacity) return RIN_I18N_NO_SPACE;
                output[written++] = replacement[arg_index];
            }
            input++;
            continue;
        }
        if (written + 1u >= capacity) return RIN_I18N_NO_SPACE;
        output[written++] = pattern[input++];
    }
    output[written] = '\0';
    return (int)written;
}
