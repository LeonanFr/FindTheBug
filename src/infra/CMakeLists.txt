find_package(Threads REQUIRED)

add_library(findthebug-infra STATIC)

target_sources(findthebug-infra
    PRIVATE
        TaskQueue.cpp
)

target_include_directories(findthebug-infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(findthebug-infra
    PUBLIC
        Threads::Threads
)