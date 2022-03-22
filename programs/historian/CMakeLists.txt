add_executable(historian main.cpp)
if(UNIX AND NOT APPLE)
    set(rt_library rt)
endif()

target_link_libraries(historian
        PRIVATE golos_chain
        golos_protocol
        fc
        indicators
        ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS})

install(TARGETS
        historian

        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        )
