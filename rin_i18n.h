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
#define RIN_I18N_MAX_LOOKUP_TEXT_BYTES (64u * 1024u)
#define RIN_I18N_MAX_PLURAL_NUMBER_BYTES 128u

/* RMSG plural rules are bounded integer/decimal selectors.  They are
 * catalog-format values, not a claim of complete CLDR coverage. */
#define RIN_I18N_PLURAL_RULE_OTHER 0u
#define RIN_I18N_PLURAL_RULE_ONE 1u
#define RIN_I18N_PLURAL_RULE_ZERO_ONE 2u
#define RIN_I18N_PLURAL_RULE_ONE_FEW 3u
#define RIN_I18N_PLURAL_RULE_ONE_FEW_MANY 4u
#define RIN_I18N_PLURAL_RULE_ARABIC 5u
/* Czech/Slovak-like rule: integer 1 is one, integers 2..4 are few,
 * visible-fraction values are many, and all remaining integers are other. */
#define RIN_I18N_PLURAL_RULE_ONE_FEW_MANY_V 6u
/* Polish-like rule: integer 1 is one, 2..4 are few outside the 12..14
 * modulo-100 exception, integer zero/five-or-more forms are many, and
 * visible fractions are other. */
#define RIN_I18N_PLURAL_RULE_POLISH 7u
/* Slovenian-like rule: integer modulo-100 1 is one, 2 is two, 3..4
 * are few, visible-fraction values are few, and all remaining integers
 * are other. */
#define RIN_I18N_PLURAL_RULE_SLOVENIAN 8u
/* Romanian-like rule: integer 1 is one, integer zero or modulo-100
 * 1..19 is few, visible-fraction values are many, and the remaining
 * integers are other. */
#define RIN_I18N_PLURAL_RULE_ROMANIAN 9u
/* Lithuanian-like rule: integer values ending in 1 outside the 11..19
 * modulo-100 range are one, other integers ending in 2..9 outside that
 * range are few, visible-fraction values are many, and the rest are other. */
#define RIN_I18N_PLURAL_RULE_LITHUANIAN 10u
/* Ukrainian-like rule: integer endings select one/few/many with the
 * 11..14 modulo-100 exceptions, while visible-fraction values are other. */
#define RIN_I18N_PLURAL_RULE_UKRAINIAN 11u
/* Irish-like rule: integers 1, 2, 3..6, and 7..10 select one, two, few,
 * and many respectively; visible fractions and all remaining integers are
 * other. */
#define RIN_I18N_PLURAL_RULE_IRISH 12u
/* Hebrew-like rule: integer 1 is one, integer 2 is two, non-zero multiples
 * of 10 are many, visible fractions are other, and all other integers are
 * other. */
#define RIN_I18N_PLURAL_RULE_HEBREW 13u
/* Maltese-like rule: integer 1 is one, integer 2 is two, zero and
 * integers modulo 100 in 3..10 are few, integers modulo 100 in 11..19
 * are many, and visible-fraction values are other. */
#define RIN_I18N_PLURAL_RULE_MALTESE 14u
/* Latvian-like rule: integer 1 outside the 11 modulo-100 range is one,
 * integer zero or a value ending in zero or in 11..19 is zero, and visible
 * fractions are other. */
#define RIN_I18N_PLURAL_RULE_LATVIAN 15u

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
/* Selects a plural entry from a non-negative ASCII decimal literal.  The
 * bounded parser preserves visible fraction digits, so 1 and 1.0 can select
 * different CLDR-style categories.  Signs, exponents, malformed decimals,
 * and integer overflow are rejected and return fallback. */
const char* rin_i18n_plural_decimal(const RinI18nCatalog* catalog,
                                    const char* domain, const char* key,
                                    const char* number,
                                    const char* fallback);
/* Expands bounded {name} substitutions; {{ and }} emit literal braces.
 * The output is failure-atomic: any error clears output when available. */
int rin_i18n_format(char* output, size_t capacity, const char* pattern,
                    const RinI18nArg* args, size_t arg_count);

#ifdef __cplusplus
}
#endif

#endif
