#include "rin_i18n.h"
#include "rin_unicode.h"

#include <string.h>

#define RMSG_HEADER_SIZE 64u
#define RMSG_ENTRY_SIZE 20u

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static int text_length_bounded(const char* text, size_t limit,
                               size_t* length_out) {
    size_t index;
    if (!text || !length_out) return 0;
    for (index = 0u; index < limit; ++index) {
        if (text[index] == '\0') {
            *length_out = index;
            return 1;
        }
    }
    return 0;
}

static int text_equal(const char* lhs, const char* rhs) {
    size_t lhs_length;
    size_t rhs_length;
    size_t index;
    if (!text_length_bounded(lhs, RIN_I18N_MAX_FILE_SIZE, &lhs_length) ||
        !text_length_bounded(rhs, RIN_I18N_MAX_FILE_SIZE, &rhs_length) ||
        lhs_length != rhs_length)
        return 0;
    for (index = 0u; index < lhs_length; ++index) {
        if (lhs[index] != rhs[index]) return 0;
    }
    return 1;
}

static int text_compare(const char* lhs, const char* rhs) {
    size_t lhs_length;
    size_t rhs_length;
    size_t index;
    if (!text_length_bounded(lhs, RIN_I18N_MAX_FILE_SIZE, &lhs_length) ||
        !text_length_bounded(rhs, RIN_I18N_MAX_FILE_SIZE, &rhs_length))
        return 0;
    for (index = 0u; index < lhs_length && index < rhs_length; ++index) {
        if ((uint8_t)lhs[index] < (uint8_t)rhs[index]) return -1;
        if ((uint8_t)lhs[index] > (uint8_t)rhs[index]) return 1;
    }
    if (lhs_length < rhs_length) return -1;
    if (lhs_length > rhs_length) return 1;
    return 0;
}

static int range_valid(size_t size, uint32_t offset, uint32_t length) {
    return (size_t)offset <= size && (size_t)length <= size - (size_t)offset;
}

static int plural_rule_valid(uint32_t rule) {
    return rule <= RIN_I18N_PLURAL_RULE_LATVIAN;
}

typedef struct RinI18nPluralOperands {
    uint64_t integer;
    uint32_t visible_fraction_digits;
} RinI18nPluralOperands;

static int plural_number_parse(const char* number,
                               RinI18nPluralOperands* operands) {
    size_t length;
    size_t index;
    size_t dot = (size_t)-1;
    uint64_t integer = 0u;
    if (!operands ||
        !text_length_bounded(number, RIN_I18N_MAX_PLURAL_NUMBER_BYTES,
                             &length) ||
        length == 0u)
        return 0;
    for (index = 0u; index < length; ++index) {
        if (number[index] == '.') {
            if (dot != (size_t)-1 || index == 0u || index + 1u >= length)
                return 0;
            dot = index;
        }
    }
    if (dot == (size_t)-1) dot = length;
    for (index = 0u; index < dot; ++index) {
        uint64_t digit;
        if (number[index] < '0' || number[index] > '9') return 0;
        digit = (uint64_t)(number[index] - '0');
        if (integer > (UINT64_MAX - digit) / 10u) return 0;
        integer = integer * 10u + digit;
    }
    if (dot != length) {
        for (index = dot + 1u; index < length; ++index) {
            if (number[index] < '0' || number[index] > '9') return 0;
        }
    }
    operands->integer = integer;
    operands->visible_fraction_digits =
        dot == length ? 0u : (uint32_t)(length - dot - 1u);
    return 1;
}

static const char* plural_suffix(const RinI18nCatalog* catalog,
                                 RinI18nPluralOperands operands) {
    const char* suffix = ".other";
    uint64_t mod10 = operands.integer % 10u;
    uint64_t mod100 = operands.integer % 100u;
    if (!catalog) return suffix;
    switch (catalog->plural_rule) {
    case RIN_I18N_PLURAL_RULE_ONE:
        if (operands.visible_fraction_digits == 0u &&
            operands.integer == 1u) suffix = ".one";
        break;
    case RIN_I18N_PLURAL_RULE_ZERO_ONE:
        /* The French-like zero-one rule is based on the CLDR integer
         * operand, not on visible fraction count: 0.5 and 1.0 therefore
         * remain in the one category while 2.0 does not. */
        if (operands.integer <= 1u) suffix = ".one";
        break;
    case RIN_I18N_PLURAL_RULE_ONE_FEW:
        if (operands.visible_fraction_digits == 0u &&
            operands.integer == 1u) suffix = ".one";
        else if (operands.visible_fraction_digits == 0u &&
                 operands.integer >= 2u && operands.integer <= 4u)
            suffix = ".few";
        break;
    case RIN_I18N_PLURAL_RULE_ONE_FEW_MANY:
        /* Russian-like cardinal rules classify every visible fraction as
         * many; only integers participate in the one/few modulo tests. */
        if (operands.visible_fraction_digits != 0u)
            suffix = ".many";
        else if (mod10 == 1u &&
            mod100 != 11u)
            suffix = ".one";
        else if (mod10 >= 2u && mod10 <= 4u &&
                 (mod100 < 12u || mod100 > 14u))
            suffix = ".few";
        else if (mod10 == 0u || mod10 >= 5u ||
                 (mod100 >= 11u && mod100 <= 14u))
            suffix = ".many";
        break;
    case RIN_I18N_PLURAL_RULE_ARABIC:
        if (operands.visible_fraction_digits == 0u &&
            operands.integer == 0u)
            suffix = ".zero";
        else if (operands.visible_fraction_digits == 0u &&
                 operands.integer == 1u)
            suffix = ".one";
        else if (operands.visible_fraction_digits == 0u &&
                 operands.integer == 2u)
            suffix = ".two";
        else if (operands.visible_fraction_digits == 0u &&
                 mod100 >= 3u && mod100 <= 10u)
            suffix = ".few";
        else if (operands.visible_fraction_digits == 0u &&
                 mod100 >= 11u && mod100 <= 99u)
            suffix = ".many";
        break;
    case RIN_I18N_PLURAL_RULE_ONE_FEW_MANY_V:
        if (operands.visible_fraction_digits == 0u &&
            operands.integer == 1u)
            suffix = ".one";
        else if (operands.visible_fraction_digits == 0u &&
                 operands.integer >= 2u && operands.integer <= 4u)
            suffix = ".few";
        else if (operands.visible_fraction_digits != 0u)
            suffix = ".many";
        break;
    case RIN_I18N_PLURAL_RULE_POLISH:
        if (operands.visible_fraction_digits != 0u)
            break;
        if (operands.integer == 1u) {
            suffix = ".one";
        } else if (mod10 >= 2u && mod10 <= 4u &&
                   (mod100 < 12u || mod100 > 14u)) {
            suffix = ".few";
        } else {
            suffix = ".many";
        }
        break;
    case RIN_I18N_PLURAL_RULE_SLOVENIAN:
        if (operands.visible_fraction_digits != 0u) {
            suffix = ".few";
        } else if (mod100 == 1u) {
            suffix = ".one";
        } else if (mod100 == 2u) {
            suffix = ".two";
        } else if (mod100 == 3u || mod100 == 4u) {
            suffix = ".few";
        }
        break;
    case RIN_I18N_PLURAL_RULE_ROMANIAN:
        if (operands.visible_fraction_digits != 0u) {
            suffix = ".many";
        } else if (operands.integer == 1u) {
            suffix = ".one";
        } else if (operands.integer == 0u ||
                   (mod100 >= 1u && mod100 <= 19u)) {
            suffix = ".few";
        }
        break;
    case RIN_I18N_PLURAL_RULE_LITHUANIAN:
        if (operands.visible_fraction_digits != 0u) {
            suffix = ".many";
        } else if (mod10 == 1u &&
                   (mod100 < 11u || mod100 > 19u)) {
            suffix = ".one";
        } else if (mod10 >= 2u && mod10 <= 9u &&
                   (mod100 < 11u || mod100 > 19u)) {
            suffix = ".few";
        }
        break;
    case RIN_I18N_PLURAL_RULE_UKRAINIAN:
        if (operands.visible_fraction_digits != 0u) {
            break;
        }
        if (mod10 == 1u && mod100 != 11u) {
            suffix = ".one";
        } else if (mod10 >= 2u && mod10 <= 4u &&
                   (mod100 < 12u || mod100 > 14u)) {
            suffix = ".few";
        } else if (mod10 == 0u || mod10 >= 5u ||
                   (mod100 >= 11u && mod100 <= 14u)) {
            suffix = ".many";
        }
        break;
    case RIN_I18N_PLURAL_RULE_IRISH:
        if (operands.visible_fraction_digits != 0u) {
            break;
        }
        if (operands.integer == 1u) {
            suffix = ".one";
        } else if (operands.integer == 2u) {
            suffix = ".two";
        } else if (operands.integer >= 3u && operands.integer <= 6u) {
            suffix = ".few";
        } else if (operands.integer >= 7u && operands.integer <= 10u) {
            suffix = ".many";
        }
        break;
    case RIN_I18N_PLURAL_RULE_HEBREW:
        if (operands.visible_fraction_digits != 0u) {
            break;
        }
        if (operands.integer == 1u) {
            suffix = ".one";
        } else if (operands.integer == 2u) {
            suffix = ".two";
        } else if (operands.integer != 0u && mod10 == 0u) {
            suffix = ".many";
        }
        break;
    case RIN_I18N_PLURAL_RULE_MALTESE:
        if (operands.visible_fraction_digits != 0u) {
            break;
        }
        if (operands.integer == 1u) {
            suffix = ".one";
        } else if (operands.integer == 2u) {
            suffix = ".two";
        } else if (operands.integer == 0u ||
                   (mod100 >= 3u && mod100 <= 10u)) {
            suffix = ".few";
        } else if (mod100 >= 11u && mod100 <= 19u) {
            suffix = ".many";
        }
        break;
    case RIN_I18N_PLURAL_RULE_LATVIAN:
        if (operands.visible_fraction_digits != 0u) {
            break;
        }
        if (mod10 == 1u && (mod100 < 11u || mod100 > 19u)) {
            suffix = ".one";
        } else if (operands.integer == 0u || mod10 == 0u ||
                   (mod100 >= 11u && mod100 <= 19u)) {
            suffix = ".zero";
        }
        break;
    case RIN_I18N_PLURAL_RULE_OTHER:
        break;
    default:
        return (const char*)0;
    }
    return suffix;
}

static const char* plural_lookup(const RinI18nCatalog* catalog,
                                 const char* domain, const char* key,
                                 const char* suffix,
                                 const char* fallback) {
    char composite[192];
    size_t length;
    size_t suffix_length;
    size_t index;
    if (!suffix ||
        !text_length_bounded(key, RIN_I18N_MAX_LOOKUP_TEXT_BYTES, &length))
        return fallback;
    suffix_length = strlen(suffix);
    if (length > sizeof(composite) - suffix_length - 1u) return fallback;
    for (index = 0u; index < length; ++index) composite[index] = key[index];
    for (index = 0u; index < suffix_length; ++index)
        composite[length + index] = suffix[index];
    composite[length + suffix_length] = '\0';
    return rin_i18n_get(catalog, domain, composite, fallback);
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
    size_t length;
    size_t index;
    if (!text_length_bounded(text, RIN_I18N_MAX_LOOKUP_TEXT_BYTES,
                             &length))
        return 0u;
    for (index = 0u; index < length; ++index) {
        hash ^= (uint8_t)text[index];
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
    uint32_t plural_rule;
    uint32_t previous_hash = 0u;
    const char* previous_domain = (const char*)0;
    const char* previous_key = (const char*)0;
    uint32_t index;
    if (!catalog) return RIN_I18N_INVALID;
    /* Direct callers must not retain a previously valid catalog when a new
     * source is rejected.  The resource-backed wrapper already provides this
     * guarantee; keep the lower-level entry point equally failure-atomic. */
    memset(catalog, 0, sizeof(*catalog));
    if (!data || size < RMSG_HEADER_SIZE || size > RIN_I18N_MAX_FILE_SIZE)
        return RIN_I18N_INVALID;
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
    plural_rule = read_u32(bytes + 40u);
    if (file_size != size || entries_offset < RMSG_HEADER_SIZE ||
        entry_count > (RIN_I18N_MAX_FILE_SIZE / RMSG_ENTRY_SIZE) ||
        !range_valid(size, entries_offset, entry_count * RMSG_ENTRY_SIZE) ||
        !range_valid(size, strings_offset, strings_size) ||
        locale_offset >= strings_size || locale_length >= strings_size - locale_offset ||
        !plural_rule_valid(plural_rule) ||
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
    catalog->plural_rule = plural_rule;
    /* The generator orders records by hash, domain, and key.  Requiring the
     * same order here makes duplicate keys and ambiguous lookup impossible. */
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
            memset(catalog, 0, sizeof(*catalog));
            return RIN_I18N_CORRUPT;
        }
        if (index != 0u && hash == previous_hash &&
            (text_compare(domain, previous_domain) < 0 ||
             (text_compare(domain, previous_domain) == 0 &&
              text_compare(key, previous_key) <= 0))) {
            memset(catalog, 0, sizeof(*catalog));
            return RIN_I18N_CORRUPT;
        }
        previous_hash = hash;
        previous_domain = domain;
        previous_key = key;
    }
    return RIN_I18N_OK;
}

static int resource_status_to_i18n(RinResourceCatalogStatus status) {
    switch (status) {
        case RIN_RESOURCE_CATALOG_NOT_FOUND:
            return RIN_I18N_NOT_FOUND;
        case RIN_RESOURCE_CATALOG_BUFFER_TOO_SMALL:
            return RIN_I18N_NO_SPACE;
        case RIN_RESOURCE_CATALOG_IO_ERROR:
            return RIN_I18N_IO_ERROR;
        case RIN_RESOURCE_CATALOG_INVALID_ARGUMENT:
        case RIN_RESOURCE_CATALOG_WRONG_SOURCE:
            return RIN_I18N_INVALID;
        default:
            return RIN_I18N_CORRUPT;
    }
}

int rin_i18n_catalog_open_resource(
    RinI18nCatalog* catalog,
    const RinResourceCatalogV1* resources,
    uint32_t resource_id,
    RinResourceCatalogReadPathFunction read_path,
    void* context,
    uint8_t* storage,
    uint64_t storage_capacity,
    uint64_t* storage_size)
{
    RinResourceCatalogStatus resource_status;
    uint64_t loaded_size = 0u;
    int status;
    if (catalog != NULL) memset(catalog, 0, sizeof(*catalog));
    if (storage_size != NULL) *storage_size = 0u;
    if (catalog == NULL || resources == NULL || storage_size == NULL ||
        resource_id == 0u) return RIN_I18N_INVALID;
    resource_status = rin_resource_catalog_load(
        resources, RIN_RESOURCE_CATALOG_TYPE_LOCALIZATION, resource_id,
        read_path, context, storage, storage_capacity, &loaded_size);
    if (resource_status != RIN_RESOURCE_CATALOG_OK)
        return resource_status_to_i18n(resource_status);
    if (loaded_size > (uint64_t)SIZE_MAX)
        return RIN_I18N_NO_SPACE;
    status = rin_i18n_catalog_open(catalog, storage, (size_t)loaded_size);
    if (status != RIN_I18N_OK) {
        memset(catalog, 0, sizeof(*catalog));
        return status;
    }
    *storage_size = loaded_size;
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
    size_t ignored_length;
    if (!catalog ||
        !text_length_bounded(domain, RIN_I18N_MAX_LOOKUP_TEXT_BYTES,
                             &ignored_length) ||
        !text_length_bounded(key, RIN_I18N_MAX_LOOKUP_TEXT_BYTES,
                             &ignored_length))
        return fallback;
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
    RinI18nPluralOperands operands = {count, 0u};
    return plural_lookup(catalog, domain, key, plural_suffix(catalog, operands),
                         fallback);
}

const char* rin_i18n_plural_decimal(const RinI18nCatalog* catalog,
                                    const char* domain, const char* key,
                                    const char* number,
                                    const char* fallback) {
    RinI18nPluralOperands operands;
    if (!plural_number_parse(number, &operands)) return fallback;
    return plural_lookup(catalog, domain, key,
                         plural_suffix(catalog, operands), fallback);
}

static int rin_i18n_format_failure(char* output, int status) {
    if (output) output[0] = '\0';
    return status;
}

static int rin_i18n_format_append(char* output, size_t capacity,
                                  size_t* written, const char* source,
                                  size_t source_length) {
    size_t index;
    if (!output || !written || source == NULL ||
        *written >= capacity || source_length >= capacity - *written)
        return 0;
    for (index = 0u; index < source_length; ++index)
        output[(*written)++] = source[index];
    return 1;
}

int rin_i18n_format(char* output, size_t capacity, const char* pattern,
                    const RinI18nArg* args, size_t arg_count) {
    size_t input = 0u;
    size_t written = 0u;
    size_t pattern_length = 0u;
    size_t arg_index;
    if (!output || capacity == 0u || !pattern ||
        arg_count > RIN_I18N_MAX_FORMAT_ARGS ||
        (arg_count != 0u && !args))
        return rin_i18n_format_failure(output, RIN_I18N_INVALID);
    output[0] = '\0';
    if (!text_length_bounded(pattern, RIN_I18N_MAX_FORMAT_PATTERN_BYTES,
                             &pattern_length))
        return rin_i18n_format_failure(output, RIN_I18N_INVALID);
    for (arg_index = 0u; arg_index < arg_count; ++arg_index) {
        size_t ignored_length;
        if (!args[arg_index].name || !args[arg_index].value ||
            !text_length_bounded(args[arg_index].name,
                                 RIN_I18N_MAX_FORMAT_ARG_NAME_BYTES,
                                 &ignored_length) ||
            !text_length_bounded(args[arg_index].value,
                                 RIN_I18N_MAX_FORMAT_ARG_VALUE_BYTES,
                                 &ignored_length))
            return rin_i18n_format_failure(output, RIN_I18N_INVALID);
    }
    while (input < pattern_length) {
        if (pattern[input] == '{') {
            size_t name_start;
            size_t name_length;
            const char* replacement = (const char*)0;
            size_t replacement_length = 0u;
            if (input + 1u < pattern_length && pattern[input + 1u] == '{') {
                if (!rin_i18n_format_append(output, capacity, &written,
                                            "{", 1u))
                    return rin_i18n_format_failure(output, RIN_I18N_NO_SPACE);
                input += 2u;
                continue;
            }
            name_start = ++input;
            while (input < pattern_length && pattern[input] != '}') {
                if (pattern[input] == '{')
                    return rin_i18n_format_failure(output, RIN_I18N_INVALID);
                ++input;
            }
            if (input >= pattern_length || input == name_start)
                return rin_i18n_format_failure(output, RIN_I18N_INVALID);
            name_length = input - name_start;
            for (arg_index = 0u; arg_index < arg_count; ++arg_index) {
                size_t candidate_length = 0u;
                size_t compare;
                if (!text_length_bounded(args[arg_index].name,
                                         RIN_I18N_MAX_FORMAT_ARG_NAME_BYTES,
                                         &candidate_length) ||
                    candidate_length != name_length)
                    continue;
                for (compare = 0u; compare < name_length; ++compare) {
                    if (args[arg_index].name[compare] !=
                        pattern[name_start + compare]) break;
                }
                if (compare == name_length) {
                    replacement = args[arg_index].value;
                    (void)text_length_bounded(
                        replacement, RIN_I18N_MAX_FORMAT_ARG_VALUE_BYTES,
                        &replacement_length);
                    break;
                }
            }
            if (!replacement)
                return rin_i18n_format_failure(output, RIN_I18N_NOT_FOUND);
            if (!rin_i18n_format_append(output, capacity, &written,
                                        replacement, replacement_length))
                return rin_i18n_format_failure(output, RIN_I18N_NO_SPACE);
            ++input;
            continue;
        }
        if (pattern[input] == '}') {
            if (input + 1u >= pattern_length || pattern[input + 1u] != '}')
                return rin_i18n_format_failure(output, RIN_I18N_INVALID);
            if (!rin_i18n_format_append(output, capacity, &written, "}", 1u))
                return rin_i18n_format_failure(output, RIN_I18N_NO_SPACE);
            input += 2u;
            continue;
        }
        if (!rin_i18n_format_append(output, capacity, &written,
                                    pattern + input, 1u))
            return rin_i18n_format_failure(output, RIN_I18N_NO_SPACE);
        ++input;
    }
    output[written] = '\0';
    return (int)written;
}
