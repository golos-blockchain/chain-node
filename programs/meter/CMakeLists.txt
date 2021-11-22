add_executable(meter main.cpp)
if(UNIX AND NOT APPLE)
    set(rt_library rt)
endif()

target_link_libraries(meter
        PRIVATE golos_chain
        golos::account_by_key
        golos::account_history
        golos::account_notes
        golos::event_plugin
        golos::market_history
        golos::private_message
        golos::worker_api
        golos_protocol fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS})

install(TARGETS
        meter

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
