/* SPDX-License-Identifier: MIT */

#include "../rin_i18n.h"

#include <assert.h>
#include <string.h>

static void put32(uint8_t* bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    bytes[3] = (uint8_t)(value >> 24u);
}

static uint32_t pool_offset(const char* pool, size_t pool_size,
                            const char* value) {
    const size_t length = strlen(value);
    size_t offset;
    for (offset = 0u; offset + length < pool_size; ++offset) {
        if (memcmp(pool + offset, value, length) == 0 &&
            pool[offset + length] == '\0')
            return (uint32_t)offset;
    }
    assert(0 && "missing string pool value");
    return 0u;
}

static uint32_t entry_hash(const char* key) {
    const uint32_t domain = rin_i18n_hash("common");
    return domain ^ (rin_i18n_hash(key) + 0x9E3779B9u +
                     (domain << 6u) + (domain >> 2u));
}

static size_t build_catalog(uint8_t* bytes, size_t capacity,
                            uint32_t plural_rule) {
    static const char pool[] =
        "en-US\0common\0items.zero\0zero\0items.one\0one\0"
        "items.two\0two\0items.few\0few\0items.many\0many\0"
        "items.other\0other\0";
    static const char* keys[] = {
        "items.zero", "items.one", "items.two", "items.few",
        "items.many", "items.other"
    };
    static const char* values[] = {"zero", "one", "two", "few", "many",
                                   "other"};
    unsigned order[sizeof(keys) / sizeof(keys[0])];
    uint8_t entries[sizeof(keys) / sizeof(keys[0]) * 20u];
    const uint32_t entries_offset = 64u;
    const uint32_t strings_offset = entries_offset + (uint32_t)sizeof(entries);
    const size_t size = strings_offset + sizeof(pool);
    size_t index;
    size_t compare;
    assert(capacity >= size);
    memset(bytes, 0, size);
    memcpy(bytes, "RMSG", 4u);
    bytes[4] = 1u;
    bytes[6] = 64u;
    put32(bytes + 8u, (uint32_t)size);
    put32(bytes + 20u, 5u);
    put32(bytes + 24u, (uint32_t)(sizeof(keys) / sizeof(keys[0])));
    put32(bytes + 28u, entries_offset);
    put32(bytes + 32u, strings_offset);
    put32(bytes + 36u, (uint32_t)sizeof(pool));
    put32(bytes + 40u, plural_rule);
    memcpy(bytes + strings_offset, pool, sizeof(pool));
    for (index = 0u; index < sizeof(order) / sizeof(order[0]); ++index)
        order[index] = (unsigned)index;
    for (index = 0u; index < sizeof(order) / sizeof(order[0]); ++index) {
        for (compare = index + 1u;
             compare < sizeof(order) / sizeof(order[0]); ++compare) {
            uint32_t left = entry_hash(keys[order[index]]);
            uint32_t right = entry_hash(keys[order[compare]]);
            if (left > right ||
                (left == right && strcmp(keys[order[index]],
                                         keys[order[compare]]) > 0)) {
                unsigned swap = order[index];
                order[index] = order[compare];
                order[compare] = swap;
            }
        }
    }
    for (index = 0u; index < sizeof(order) / sizeof(order[0]); ++index) {
        const char* key = keys[order[index]];
        const char* value = values[order[index]];
        uint8_t* entry = entries + index * 20u;
        put32(entry, entry_hash(key));
        put32(entry + 4u, pool_offset(pool, sizeof(pool), "common"));
        put32(entry + 8u, pool_offset(pool, sizeof(pool), key));
        put32(entry + 12u, pool_offset(pool, sizeof(pool), value));
        put32(entry + 16u, (uint32_t)strlen(value));
    }
    memcpy(bytes + entries_offset, entries, sizeof(entries));
    put32(bytes + 12u, rin_i18n_crc32(bytes + 64u, size - 64u));
    return size;
}

static void expect(const RinI18nCatalog* catalog, const char* number,
                   const char* expected) {
    assert(strcmp(rin_i18n_plural_decimal(catalog, "common", "items",
                                           number, "fallback"),
                  expected) == 0);
}

int main(void) {
    uint8_t bytes[1024];
    RinI18nCatalog catalog;
    size_t size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_ONE);

    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "1.0", "other");
    expect(&catalog, "0.5", "other");
    expect(&catalog, "01", "one");
    expect(&catalog, "1e0", "fallback");
    expect(&catalog, ".5", "fallback");
    expect(&catalog, "1.", "fallback");
    expect(&catalog, "+1", "fallback");
    expect(&catalog, "18446744073709551616", "fallback");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_ZERO_ONE);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0.5", "one");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "2.0", "other");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_ONE_FEW_MANY);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "0", "many");
    expect(&catalog, "1.0", "many");
    expect(&catalog, "2.00", "many");
    expect(&catalog, "0.5", "many");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_ONE_FEW_MANY_V);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "4", "few");
    expect(&catalog, "5", "other");
    expect(&catalog, "1.0", "many");
    expect(&catalog, "2.00", "many");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_ARABIC);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "zero");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "few");
    expect(&catalog, "11", "many");
    expect(&catalog, "3.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_POLISH);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "4", "few");
    expect(&catalog, "12", "many");
    expect(&catalog, "14", "many");
    expect(&catalog, "15", "many");
    expect(&catalog, "0", "many");
    expect(&catalog, "1.0", "other");
    expect(&catalog, "2.50", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_SLOVENIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "few");
    expect(&catalog, "4", "few");
    expect(&catalog, "101", "one");
    expect(&catalog, "102", "two");
    expect(&catalog, "105", "other");
    expect(&catalog, "1.0", "few");
    expect(&catalog, "2.50", "few");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_ROMANIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "few");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "19", "few");
    expect(&catalog, "20", "other");
    expect(&catalog, "101", "few");
    expect(&catalog, "1.0", "many");
    expect(&catalog, "2.50", "many");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_LITHUANIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "11", "other");
    expect(&catalog, "21", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "10", "other");
    expect(&catalog, "12", "other");
    expect(&catalog, "0", "other");
    expect(&catalog, "1.0", "many");
    expect(&catalog, "2.50", "many");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_UKRAINIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "11", "many");
    expect(&catalog, "21", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "12", "many");
    expect(&catalog, "22", "few");
    expect(&catalog, "0", "many");
    expect(&catalog, "5", "many");
    expect(&catalog, "1.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_IRISH);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "few");
    expect(&catalog, "6", "few");
    expect(&catalog, "7", "many");
    expect(&catalog, "10", "many");
    expect(&catalog, "11", "other");
    expect(&catalog, "1.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_HEBREW);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "10", "many");
    expect(&catalog, "20", "many");
    expect(&catalog, "11", "other");
    expect(&catalog, "1.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_MALTESE);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "few");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "few");
    expect(&catalog, "10", "few");
    expect(&catalog, "11", "many");
    expect(&catalog, "19", "many");
    expect(&catalog, "20", "other");
    expect(&catalog, "103", "few");
    expect(&catalog, "1.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_LATVIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "zero");
    expect(&catalog, "1", "one");
    expect(&catalog, "10", "zero");
    expect(&catalog, "11", "zero");
    expect(&catalog, "19", "zero");
    expect(&catalog, "20", "zero");
    expect(&catalog, "21", "one");
    expect(&catalog, "1.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_BALKAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "11", "other");
    expect(&catalog, "21", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "4", "few");
    expect(&catalog, "12", "other");
    expect(&catalog, "14", "other");
    expect(&catalog, "22", "few");
    expect(&catalog, "1.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_WELSH);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "zero");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "few");
    expect(&catalog, "6", "many");
    expect(&catalog, "4", "other");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "6.00", "many");
    expect(&catalog, "1.5", "other");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_SCOTTISH_GAELIC);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "few");
    expect(&catalog, "10", "few");
    expect(&catalog, "11", "one");
    expect(&catalog, "12", "two");
    expect(&catalog, "13", "few");
    expect(&catalog, "19", "few");
    expect(&catalog, "20", "other");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "12.00", "two");
    expect(&catalog, "3.5", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_MANX);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "few");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "two");
    expect(&catalog, "3", "other");
    expect(&catalog, "11", "one");
    expect(&catalog, "12", "two");
    expect(&catalog, "20", "few");
    expect(&catalog, "21", "one");
    expect(&catalog, "40", "few");
    expect(&catalog, "80", "few");
    expect(&catalog, "100", "few");
    expect(&catalog, "1.0", "many");
    expect(&catalog, "0.5", "many");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_DANISH);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "other");
    expect(&catalog, "0.0", "other");
    expect(&catalog, "0.5", "one");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "1.5", "one");
    expect(&catalog, "2.0", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_FINNISH);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "1.0", "other");
    expect(&catalog, "0.5", "other");
    expect(&catalog, "2", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_ICELANDIC);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "11", "other");
    expect(&catalog, "21", "one");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "0.1", "one");
    expect(&catalog, "1.01", "one");
    expect(&catalog, "2.1", "one");
    expect(&catalog, "1.5", "other");
    expect(&catalog, "2.5", "other");
    expect(&catalog, "2.50", "other");
    expect(&catalog, "11.5", "other");
    expect(&catalog, "1.11", "other");
    expect(&catalog, "11.0", "other");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_PORTUGUESE);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "one");
    expect(&catalog, "0.5", "one");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "2", "other");
    expect(&catalog, "1000000", "many");
    expect(&catalog, "1000000.0", "other");
    expect(&catalog, "2000000", "many");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_MACEDONIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "1", "one");
    expect(&catalog, "11", "other");
    expect(&catalog, "21", "one");
    expect(&catalog, "1.0", "other");
    expect(&catalog, "0.1", "one");
    expect(&catalog, "1.1", "one");
    expect(&catalog, "2.1", "one");
    expect(&catalog, "1.11", "other");
    expect(&catalog, "1.10", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_BRETON);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "other");
    expect(&catalog, "1", "one");
    expect(&catalog, "21", "one");
    expect(&catalog, "11", "other");
    expect(&catalog, "71", "other");
    expect(&catalog, "2", "two");
    expect(&catalog, "12", "other");
    expect(&catalog, "3", "few");
    expect(&catalog, "9", "few");
    expect(&catalog, "10", "other");
    expect(&catalog, "19", "other");
    expect(&catalog, "1000000", "many");
    expect(&catalog, "1000000.0", "many");
    expect(&catalog, "1.1", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_FRENCH);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "one");
    expect(&catalog, "0.5", "one");
    expect(&catalog, "1", "one");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "1.5", "one");
    expect(&catalog, "2", "other");
    expect(&catalog, "1000000", "many");
    expect(&catalog, "1000000.0", "other");
    expect(&catalog, "2000000", "many");

    size = build_catalog(bytes, sizeof(bytes),
                         RIN_I18N_PLURAL_RULE_TACHELHIT);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "one");
    expect(&catalog, "0.5", "one");
    expect(&catalog, "1", "one");
    expect(&catalog, "1.0", "one");
    expect(&catalog, "1.1", "other");
    expect(&catalog, "2", "few");
    expect(&catalog, "10.00", "few");
    expect(&catalog, "10.1", "other");
    expect(&catalog, "11", "other");

    size = build_catalog(bytes, sizeof(bytes), RIN_I18N_PLURAL_RULE_RUSSIAN);
    assert(rin_i18n_catalog_open(&catalog, bytes, size) == RIN_I18N_OK);
    expect(&catalog, "0", "many");
    expect(&catalog, "1", "one");
    expect(&catalog, "2", "few");
    expect(&catalog, "4", "few");
    expect(&catalog, "5", "many");
    expect(&catalog, "10", "many");
    expect(&catalog, "11", "many");
    expect(&catalog, "12", "many");
    expect(&catalog, "14", "many");
    expect(&catalog, "21", "one");
    expect(&catalog, "22", "few");
    expect(&catalog, "24", "few");
    expect(&catalog, "25", "many");
    expect(&catalog, "101", "one");
    expect(&catalog, "111", "many");
    expect(&catalog, "1.0", "other");
    expect(&catalog, "2.50", "other");

    return 0;
}
