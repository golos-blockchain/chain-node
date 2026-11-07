add_executable(golos-rpc-directory-client main.cpp)
if(WIN32)
    # Client-only process UTF-8 ACP; requires Windows 10 1903+ / Windows 11.
    # RC is enabled here so non-Windows builds retain their existing languages.
    enable_language(RC)
    target_sources(golos-rpc-directory-client PRIVATE client.rc)
    target_include_directories(golos-rpc-directory-client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        # The RC file owns resource #1; prevent a second linker-generated manifest.
        set_property(TARGET golos-rpc-directory-client APPEND_STRING PROPERTY LINK_FLAGS " /MANIFEST:NO")
    endif()
endif()
# Network/protocol serialization and FC only: no appbase, chain plugin, DB or RPC server.
target_link_libraries(golos-rpc-directory-client PRIVATE golos::network fc)
if(BUILD_RPC_DIRECTORY_LOOPBACK_TEST)
    add_executable(rpc_directory_loopback_test "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/rpc_directory/loopback_test.cpp")
    target_link_libraries(rpc_directory_loopback_test PRIVATE golos::network fc)
    add_test(NAME rpc_directory_loopback COMMAND rpc_directory_loopback_test)
    set_tests_properties(rpc_directory_loopback PROPERTIES TIMEOUT 40)
endif()
install(TARGETS golos-rpc-directory-client RUNTIME DESTINATION bin)
