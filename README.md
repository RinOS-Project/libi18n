# RinI18n public library

RinI18n provides bounded RMSG catalog lookup, decimal plural selection, and
caller-owned resource loading for ordinary userspace applications.  It does
not own locale files, filesystem paths, service transport, or catalog
publication authority; those remain with the caller or private owner.

The CMake contract tests are enabled with
`-DRIN_I18N_BUILD_TESTS=ON`; Meson exposes the same
`rini18n-catalog-resource` and `rini18n-plural-decimal` tests.  The resource
test covers blob/path loading and failure-atomic catalog state, while the
plural test covers bounded decimal and scientific-notation selector inputs,
including the CLDR
Tachelhit `one`/`few`, Serbian `one`/`few`, Russian `one`/`few`/`many`,
Albanian/Bulgarian-like numeric-one, Filipino, Sinhala, and one-two cardinal
boundary subsets.  `e`/`E` exponents are accepted only within the fixed
128-byte input and exponent bounds; the value is normalized to the bounded
integer/fraction operands before selection, and malformed or overflowing
forms remain failure-closed.  The selector remains a bounded
catalog-format contract rather than a claim of complete CLDR locale coverage.

The common sanitizer matrix also builds `fuzz/rini18n_fuzzer.c` against the
public catalog parser and public Unicode tables.  It exercises catalog open,
locale lookup, string lookup, plural decimal selection, and bounded format
expansion with a deterministic `libi18n` corpus capped at 64 KiB per input.
This target has no filesystem, locale-service, kernel, or repository
authority; sanitizer execution remains a CI/host validation concern.
