#ifndef RIN_I18N_H
#define RIN_I18N_H

#include <stddef.h>
#include <stdint.h>

#include "../rinresource/include/rinresource/loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_I18N_RMSG_VERSION 1u
#define RIN_I18N_MAX_FILE_SIZE (4u * 1024u * 1024u)
#define RIN_I18N_MAX_FORMAT_PATTERN_BYTES (64u * 1024u)
#define RIN_I18N_MAX_FORMAT_ARGS 64u
#define RIN_I18N_MAX_FORMAT_ARG_NAME_BYTES 128u
#define RIN_I18N_MAX_FORMAT_ARG_VALUE_BYTES (64u * 1024u)

enum {
    RIN_I18N_OK = 0,
    RIN_I18N_INVALID = -1,
    RIN_I18N_CORRUPT = -2,
    RIN_I18N_NOT_FOUND = -3,
    RIN_I18N_NO_SPACE = -4,
    RIN_I18N_IO_ERROR = -5
};

typedef struct RinI18nCatalog {
    const uint8_t* data;
    size_t size;
    uint32_t entry_count;
    uint32_t entries_offset;
    uint32_t strings_offset;
    uint32_t strings_size;
    uint32_t locale_offset;
    uint32_t locale_length;
    uint32_t plural_rule;
} RinI18nCatalog;

typedef struct RinI18nArg {
    const char* name;
    const char* value;
} RinI18nArg;

uint32_t rin_i18n_hash(const char* text);
uint32_t rin_i18n_crc32(const void* data, size_t size);
int rin_i18n_catalog_open(RinI18nCatalog* catalog,
                          const void* data, size_t size);
/* Load a localization entry through the public resource catalog into a
 * caller-owned buffer, then open it as an RMSG catalog.  No filesystem or
 * allocation is performed here; path authority remains in read_path.  Both
 * catalog and storage_size are cleared before any failure is reported. */
int rin_i18n_catalog_open_resource(
    RinI18nCatalog* catalog,
    const RinResourceCatalogV1* resources,
    uint32_t resource_id,
    RinResourceCatalogReadPathFunction read_path,
    void* context,
    uint8_t* storage,
    uint64_t storage_capacity,
    uint64_t* storage_size);
const char* rin_i18n_catalog_locale(const RinI18nCatalog* catalog,
                                    size_t* length);
const char* rin_i18n_get(const RinI18nCatalog* catalog,
                         const char* domain, const char* key,
                         const char* fallback);
const char* rin_i18n_plural(const RinI18nCatalog* catalog,
                            const char* domain, const char* key,
                            uint64_t count, const char* fallback);
/* Expands bounded {name} substitutions; {{ and }} emit literal braces.
 * The output is failure-atomic: any error clears output when available. */
int rin_i18n_format(char* output, size_t capacity, const char* pattern,
                    const RinI18nArg* args, size_t arg_count);

#ifdef __cplusplus
}
#endif

#endif
