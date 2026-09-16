/* SPDX-License-Identifier: MIT */

#include "../rin_i18n.h"

#include <assert.h>
#include <string.h>

static void put16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
}

static void put32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8u);
    p[2] = (uint8_t)(value >> 16u);
    p[3] = (uint8_t)(value >> 24u);
}

static size_t build_rmsg(uint8_t* bytes, size_t capacity) {
    static const char pool[] = "en-US\0common\0hello\0Hello\0";
    const uint32_t entries_offset = 64u;
    const uint32_t strings_offset = entries_offset + 20u;
    const size_t size = strings_offset + sizeof(pool);
    assert(capacity >= size);
    memset(bytes, 0, size);
    memcpy(bytes, "RMSG", 4u);
    put16(bytes + 4u, RIN_I18N_RMSG_VERSION);
    put16(bytes + 6u, 64u);
    put32(bytes + 8u, (uint32_t)size);
    put32(bytes + 16u, 0u);
    put32(bytes + 20u, 5u);
    put32(bytes + 24u, 1u);
    put32(bytes + 28u, entries_offset);
    put32(bytes + 32u, strings_offset);
    put32(bytes + 36u, (uint32_t)sizeof(pool));
    put32(bytes + 40u, 1u);
    put32(bytes + entries_offset, rin_i18n_hash("common") ^
        (rin_i18n_hash("hello") + 0x9E3779B9u +
         (rin_i18n_hash("common") << 6u) +
         (rin_i18n_hash("common") >> 2u)));
    put32(bytes + entries_offset + 4u, 6u);
    put32(bytes + entries_offset + 8u, 13u);
    put32(bytes + entries_offset + 12u, 19u);
    put32(bytes + entries_offset + 16u, 5u);
    memcpy(bytes + strings_offset, pool, sizeof(pool));
    put32(bytes + 12u,
          rin_i18n_crc32(bytes + 64u, size - 64u));
    return size;
}

static RinResourceCatalogV1 make_catalog(
    RinResourceCatalogEntryV1* entry, const uint8_t* bytes, size_t size)
{
    RinResourceCatalogV1 catalog;
    memset(entry, 0, sizeof(*entry));
    entry->struct_size = sizeof(*entry);
    entry->version = RIN_RESOURCE_CATALOG_VERSION_1;
    entry->type = RIN_RESOURCE_CATALOG_TYPE_LOCALIZATION;
    entry->resource_id = 17u;
    entry->flags = RIN_RESOURCE_CATALOG_FLAG_IMMUTABLE |
                   RIN_RESOURCE_CATALOG_SOURCE_BLOB;
    entry->data = bytes;
    entry->data_size = size;
    memset(&catalog, 0, sizeof(catalog));
    catalog.struct_size = sizeof(catalog);
    catalog.version = RIN_RESOURCE_CATALOG_VERSION_1;
    catalog.entries = entry;
    catalog.entry_count = 1u;
    catalog.generation = 1u;
    return catalog;
}

int main(void) {
    uint8_t source[256];
    uint8_t storage[256];
    RinResourceCatalogEntryV1 entry;
    RinResourceCatalogV1 resources;
    RinI18nCatalog catalog;
    uint64_t storage_size = UINT64_MAX;
    const size_t source_size = build_rmsg(source, sizeof(source));

    resources = make_catalog(&entry, source, source_size);
    assert(rin_i18n_catalog_open_resource(
               &catalog, &resources, 17u, NULL, NULL, storage,
               sizeof(storage), &storage_size) == RIN_I18N_OK);
    assert(storage_size == source_size &&
           strcmp(rin_i18n_get(&catalog, "common", "hello", "fallback"),
                  "Hello") == 0);

    storage_size = UINT64_MAX;
    assert(rin_i18n_catalog_open_resource(
               &catalog, &resources, 17u, NULL, NULL, storage,
               source_size - 1u, &storage_size) == RIN_I18N_NO_SPACE);
    assert(storage_size == 0u && catalog.data == NULL);

    source[source_size - 1u] ^= 1u;
    storage_size = UINT64_MAX;
    assert(rin_i18n_catalog_open_resource(
               &catalog, &resources, 17u, NULL, NULL, storage,
               sizeof(storage), &storage_size) == RIN_I18N_CORRUPT);
    assert(storage_size == 0u && catalog.data == NULL);

    /* The direct public catalog entry point must have the same
     * failure-atomic behavior as the resource adapter. */
    catalog.data = source;
    catalog.size = source_size;
    assert(rin_i18n_catalog_open(&catalog, source, source_size) ==
           RIN_I18N_CORRUPT);
    assert(catalog.data == NULL && catalog.size == 0u);
    return 0;
}
