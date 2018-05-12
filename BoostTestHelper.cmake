function(add_boost_test SOURCE_FILE_NAME)

    SET (DEPENDENCY_LIB ${ARGN})
    # MESSAGE("Process ${SOURCE_FILE_NAME}, with dependency ${DEPENDENCY_LIB}.")

    # 从文件名中提取出无后缀的执行文件名
    get_filename_component(TEST_EXECUTABLE_NAME ${SOURCE_FILE_NAME} NAME_WE)

    add_executable(${TEST_EXECUTABLE_NAME} ${SOURCE_FILE_NAME})
    target_link_libraries(${TEST_EXECUTABLE_NAME}
                          ${DEPENDENCY_LIB} boost_unit_test_framework)
    set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/test_bin)

    file(READ "${SOURCE_FILE_NAME}" SOURCE_FILE_CONTENTS)
    string(REGEX MATCHALL "BOOST_AUTO_TEST_CASE\\( *([A-Za-z_0-9]+) *\\)"
            FOUND_TESTS ${SOURCE_FILE_CONTENTS})
    list(LENGTH FOUND_TESTS TEST_COUNT)

    string(REGEX MATCHALL "BOOST_AUTO_TEST_SUITE\\( *([A-Za-z_0-9]+) *\\)"
            FOUND_TEST_SUITES ${SOURCE_FILE_CONTENTS})
    list(LENGTH FOUND_TEST_SUITES TEST_SUITE_COUNT)


    MESSAGE("Found total ${TEST_SUITE_COUNT} test suite(s), ${TEST_COUNT} test case(s).")

    if(${TEST_SUITE_COUNT} GREATER 1)
        add_test(NAME ${TEST_EXECUTABLE_NAME}
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_bin/${TEST_EXECUTABLE_NAME}
        )

    elseif(${TEST_SUITE_COUNT} EQUAL 1)
        list(GET FOUND_TEST_SUITES 0 SUITE_NAME)
        string(REGEX REPLACE ".*\\( *([A-Za-z_0-9]+) *\\).*" "\\1" SUITE_NAME_R ${SUITE_NAME})

        # 分别显示每个执行文件中的单独case结果，附带TEST_SUITE前缀
        foreach(HIT ${FOUND_TESTS})
            string(REGEX REPLACE ".*\\( *([A-Za-z_0-9]+) *\\).*" "\\1" TEST_NAME ${HIT})

            add_test(NAME "${TEST_EXECUTABLE_NAME}.${TEST_NAME}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_bin
                     COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_bin/${TEST_EXECUTABLE_NAME}
                     --run_test=${SUITE_NAME_R}/${TEST_NAME} --catch_system_error=yes
             )
        endforeach()

    else (${TEST_SUITE_COUNT} GREATER 1)  # no suite found

        # 分别显示每个执行文件中的单独case结果
        foreach(HIT ${FOUND_TESTS})
            string(REGEX REPLACE ".*\\( *([A-Za-z_0-9]+) *\\).*" "\\1" TEST_NAME ${HIT})

            add_test(NAME "${TEST_EXECUTABLE_NAME}.${TEST_NAME}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_bin
                     COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_bin/${TEST_EXECUTABLE_NAME}
                              --run_test=${TEST_NAME} --catch_system_error=yes
             )
        endforeach()

    endif(${TEST_SUITE_COUNT} GREATER 1)

endfunction()
