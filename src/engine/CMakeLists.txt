add_library(findthebug-engine STATIC)

target_sources(findthebug-engine
    PRIVATE
        ActionSystem.cpp
        ValidationSystem.cpp
        GameEngine.cpp
)

target_link_libraries(findthebug-engine 
    PUBLIC 
        findthebug-shared
        findthebug-mongostore
)