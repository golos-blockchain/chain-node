cmake_minimum_required(VERSION 3.10)
project(golos_rpc_directory_tests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 14)
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
add_executable(rpc_directory_policy_test policy_test.cpp)
target_include_directories(rpc_directory_policy_test PRIVATE "${ROOT}/libraries/network/include")
target_compile_options(rpc_directory_policy_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME rpc_directory_policy COMMAND rpc_directory_policy_test)
add_executable(rpc_directory_state_test state_test.cpp)
target_include_directories(rpc_directory_state_test PRIVATE "${ROOT}/libraries/network/include")
target_compile_options(rpc_directory_state_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME rpc_directory_state COMMAND rpc_directory_state_test)
add_executable(rpc_directory_search_test search_test.cpp)
target_include_directories(rpc_directory_search_test PRIVATE "${ROOT}/libraries/network/include")
target_compile_options(rpc_directory_search_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME rpc_directory_search COMMAND rpc_directory_search_test)
foreach(case duplicate_referral overlap_policy l3_policy)
    add_executable(rpc_directory_${case}_test adversarial/${case}_test.cpp)
    target_include_directories(rpc_directory_${case}_test PRIVATE "${ROOT}/libraries/network/include")
    target_compile_options(rpc_directory_${case}_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME rpc_directory_${case} COMMAND rpc_directory_${case}_test)
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/admission_definitions.hpp"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_admission.py"
            "${ROOT}/libraries/network/node.cpp" "${CMAKE_CURRENT_BINARY_DIR}/admission_definitions.hpp"
    DEPENDS "${ROOT}/libraries/network/node.cpp" "${CMAKE_CURRENT_LIST_DIR}/extract_admission.py"
    VERBATIM)
add_executable(rpc_directory_admission_test admission_test.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/admission_definitions.hpp" "${ROOT}/thirdparty/fc/src/shared_ptr.cpp")
target_include_directories(rpc_directory_admission_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
# FC's legacy include tree contains endian.h; do not shadow the system header.
target_compile_options(rpc_directory_admission_test PRIVATE "-idirafter${ROOT}/thirdparty/fc/include" -UNDEBUG)
add_test(NAME rpc_directory_admission COMMAND rpc_directory_admission_test)
option(RPC_DIRECTORY_SYNTAX_CHECKS "Compile actual modified sources against pinned repository headers (no linking)" ON)
if(RPC_DIRECTORY_SYNTAX_CHECKS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    # Use the repository's real build generator, never a hardfork stub.
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/programs/build_helpers/cat_parts.py"
                    "${ROOT}/libraries/chain/hardfork.d" "${CMAKE_CURRENT_BINARY_DIR}/generated/golos/chain/hardfork.hpp"
                    RESULT_VARIABLE generation_status)
    if(NOT generation_status EQUAL 0)
        message(FATAL_ERROR "Repository hardfork header generator failed")
    endif()
    set(network_includes "-idirafter${ROOT}/libraries/network/include" "-idirafter${ROOT}/libraries/protocol/include" "-idirafter${ROOT}/thirdparty/fc/include")
    set(plugin_includes ${network_includes} "-idirafter${CMAKE_CURRENT_BINARY_DIR}/generated" "-idirafter${ROOT}/thirdparty/appbase/include" "-idirafter${ROOT}/thirdparty/chainbase/include"
                        "-idirafter${ROOT}/libraries/chain/include" "-idirafter${ROOT}/libraries/api/include"
                        "-idirafter${ROOT}/plugins/p2p/include" "-idirafter${ROOT}/plugins/chain/include" "-idirafter${ROOT}/plugins/json_rpc/include" "-idirafter${ROOT}/plugins/rpc_directory/include")
    foreach(src libraries/network/node.cpp libraries/network/peer_connection.cpp programs/rpc_directory_client/main.cpp
                plugins/p2p/p2p_plugin.cpp plugins/rpc_directory/rpc_directory_plugin.cpp tests/rpc_directory/loopback_test.cpp)
        set(includes ${network_includes})
        if(src MATCHES "^plugins/")
            set(includes ${plugin_includes})
        endif()
        string(REPLACE "/" "_" label "${src}")
        add_test(NAME "syntax_${label}" COMMAND "${CMAKE_CXX_COMPILER}" -std=c++14 -Wno-deprecated-declarations
                 -fsyntax-only ${includes} "${ROOT}/${src}")
    endforeach()
endif()
