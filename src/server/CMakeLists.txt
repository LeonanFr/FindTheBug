add_library(Crow INTERFACE)
add_library(Crow::Crow ALIAS Crow)

target_include_directories(Crow
    INTERFACE
        /usr/local/include
)

add_executable(findthebug-server main.cpp)

target_sources(findthebug-server
    PRIVATE
        SessionManager.cpp
        HttpServer.cpp
)

target_link_libraries(findthebug-server
    PRIVATE
        findthebug-engine
        findthebug-infra
        Crow::Crow
)
