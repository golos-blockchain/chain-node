set(CURRENT_TARGET exchange)

list(APPEND CURRENT_TARGET_HEADERS
    include/golos/plugins/exchange/exchange.hpp
    include/golos/plugins/exchange/exchange_queries.hpp
    include/golos/plugins/exchange/exchange_types.hpp
)

list(APPEND CURRENT_TARGET_SOURCES
    exchange.cpp
)

if(BUILD_SHARED_LIBRARIES)
    add_library(golos_${CURRENT_TARGET} SHARED
        ${CURRENT_TARGET_HEADERS}
        ${CURRENT_TARGET_SOURCES}
    )
else()
    add_library(golos_${CURRENT_TARGET} STATIC
        ${CURRENT_TARGET_HEADERS}
        ${CURRENT_TARGET_SOURCES}
    )
endif()

add_library(golos::${CURRENT_TARGET} ALIAS golos_${CURRENT_TARGET})
set_property(TARGET golos_${CURRENT_TARGET} PROPERTY EXPORT_NAME ${CURRENT_TARGET})

target_link_libraries(
    golos_${CURRENT_TARGET}
    golos::chain_plugin
    golos::json_rpc
    appbase
)

target_include_directories(golos_${CURRENT_TARGET}
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

install(TARGETS
    golos_${CURRENT_TARGET}

    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
