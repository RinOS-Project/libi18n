# RinI18n public library

RinI18n provides bounded RMSG catalog lookup, decimal plural selection, and
caller-owned resource loading for ordinary userspace applications.  It does
not own locale files, filesystem paths, service transport, or catalog
publication authority; those remain with the caller or private owner.

The CMake contract tests are enabled with
`-DRIN_I18N_BUILD_TESTS=ON`; Meson exposes the same
`rini18n-catalog-resource` and `rini18n-plural-decimal` tests.  The resource
test covers blob/path loading and failure-atomic catalog state, while the
plural test covers the bounded decimal selector rules, including the CLDR
Tachelhit `one`/`few`, Serbian `one`/`few`, Russian `one`/`few`/`many`, and
Albanian/Bulgarian-like numeric-one boundary subsets.  The selector remains a bounded
catalog-format contract rather than a claim of complete CLDR locale coverage.
