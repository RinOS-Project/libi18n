project('RinI18n', 'c', version: '1.0.0',
        default_options: ['c_std=c11', 'warning_level=3'])

i18n_inc = include_directories('.', '../libunicode', '../rinresource/include')
i18n_sources = files('rin_i18n.c', '../libunicode/core.c')
rini18n = library('rin_i18n', i18n_sources,
                  include_directories: i18n_inc,
                  install: false)
rini18n_dep = declare_dependency(link_with: rini18n,
                                 include_directories: i18n_inc)

test('rini18n-catalog-resource',
     executable('rini18n-catalog-resource-test',
                'tests/catalog_resource_test.c',
                dependencies: rini18n_dep,
                c_args: ['-Wall', '-Wextra', '-Wpedantic']))
test('rini18n-plural-decimal',
     executable('rini18n-plural-decimal-test',
                'tests/plural_decimal_test.c',
                dependencies: rini18n_dep,
                c_args: ['-Wall', '-Wextra', '-Wpedantic']))
