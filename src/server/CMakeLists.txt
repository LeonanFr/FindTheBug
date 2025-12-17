find_package(Crow REQUIRED)

add_executable(findthebug-server main.cpp)

target_sources(findthebug-server
    PRIVATE
        SessionManager.cpp
        HttpServer.cpp
)

target_link_libraries(findthebug-server 
    PRIVATE 
        findthebug-engine
        Crow::Crow
)