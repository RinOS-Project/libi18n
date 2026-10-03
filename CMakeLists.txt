cmake_minimum_required(VERSION 3.16)
project(RinI18n VERSION 1.0.0 LANGUAGES C)

option(RIN_I18N_BUILD_TESTS
       "Build RinI18n public contract tests"
       OFF)

add_library(RinI18n STATIC
            rin_i18n.c
            ../libunicode/core.c)
add_library(RinI18n::RinI18n ALIAS RinI18n)

target_compile_features(RinI18n PUBLIC c_std_11)
target_include_directories(RinI18n PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}/../libunicode
                           ${CMAKE_CURRENT_SOURCE_DIR}/../rinresource/include)
if(NOT MSVC)
    target_compile_options(RinI18n PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RIN_I18N_BUILD_TESTS)
    enable_testing()
    add_executable(rini18n-catalog-resource-test
                   tests/catalog_resource_test.c)
    target_link_libraries(rini18n-catalog-resource-test PRIVATE RinI18n::RinI18n)
    target_compile_features(rini18n-catalog-resource-test PRIVATE c_std_11)
    if(NOT MSVC)
        target_compile_options(rini18n-catalog-resource-test PRIVATE
                               -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME rini18n-catalog-resource
             COMMAND rini18n-catalog-resource-test)

    add_executable(rini18n-plural-decimal-test
                   tests/plural_decimal_test.c)
    target_link_libraries(rini18n-plural-decimal-test PRIVATE RinI18n::RinI18n)
    target_compile_features(rini18n-plural-decimal-test PRIVATE c_std_11)
    if(NOT MSVC)
        target_compile_options(rini18n-plural-decimal-test PRIVATE
                               -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME rini18n-plural-decimal
             COMMAND rini18n-plural-decimal-test)
endif()
