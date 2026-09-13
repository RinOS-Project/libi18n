#ifndef RIN_I18N_H
#define RIN_I18N_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_I18N_RMSG_VERSION 1u
#define RIN_I18N_MAX_FILE_SIZE (4u * 1024u * 1024u)

enum {
    RIN_I18N_OK = 0,
    RIN_I18N_INVALID = -1,
    RIN_I18N_CORRUPT = -2,
    RIN_I18N_NOT_FOUND = -3,
    RIN_I18N_NO_SPACE = -4
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
const char* rin_i18n_catalog_locale(const RinI18nCatalog* catalog,
                                    size_t* length);
const char* rin_i18n_get(const RinI18nCatalog* catalog,
                         const char* domain, const char* key,
                         const char* fallback);
const char* rin_i18n_plural(const RinI18nCatalog* catalog,
                            const char* domain, const char* key,
                            uint64_t count, const char* fallback);
int rin_i18n_format(char* output, size_t capacity, const char* pattern,
                    const RinI18nArg* args, size_t arg_count);

#ifdef __cplusplus
}
#endif

#endif
