cmake_minimum_required(VERSION 3.22)

# ─── Google Test via FetchContent ────────────────────────────────────────────

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ─── Helper para adicionar executáveis de teste ──────────────────────────────

function(add_ppcomp_test test_name source_file)
    add_executable(${test_name} ${source_file} ${ARGN})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/mocks
    )
    target_link_libraries(${test_name} PRIVATE
        ppcomp
        GTest::gtest_main
        GTest::gmock
    )
    gtest_discover_tests(${test_name})
endfunction()

# ─── Testes unitários ─────────────────────────────────────────────────────────

# Adicionar conforme cada módulo for implementado:
# add_ppcomp_test(test_lifecycle   test_lifecycle.cpp)
# add_ppcomp_test(test_callbacks   test_callbacks.cpp)
# add_ppcomp_test(test_mapping     test_mapping.cpp)
# add_ppcomp_test(test_tables      test_tables.cpp)
# add_ppcomp_test(test_detection   test_detection.cpp)
# add_ppcomp_test(test_emv_ct      test_emv_ct.cpp)
# add_ppcomp_test(test_emv_ctls    test_emv_ctls.cpp)
# add_ppcomp_test(test_pin         test_pin.cpp)

# ─── Testes de integração ────────────────────────────────────────────────────

# add_ppcomp_test(test_homologacao_se
#     integration/test_homologacao_se.cpp
#     mocks/sdi_mock.cpp
# )
# add_ppcomp_test(test_homologacao_adquirentes
#     integration/test_homologacao_adquirentes.cpp
#     mocks/sdi_mock.cpp
# )
