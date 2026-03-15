cmake_minimum_required(VERSION 3.22)

# ─── Testes de integração / homologação ───────────────────────────────────────

function(add_ppcomp_integration_test test_name source_file)
    add_executable(${test_name} ${source_file} ${ARGN})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/tests/mocks
    )
    target_link_libraries(${test_name} PRIVATE
        ppcomp
        GTest::gtest_main
        GTest::gmock
    )
    gtest_discover_tests(${test_name})
endfunction()

add_ppcomp_integration_test(test_homologacao_se
    test_homologacao_se.cpp
    ../mocks/sdi_mock.cpp
)
