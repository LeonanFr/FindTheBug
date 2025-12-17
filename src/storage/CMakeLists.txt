find_package(mongocxx REQUIRED)

add_library(findthebug-mongostore STATIC)

target_sources(findthebug-mongostore
    PRIVATE
        MongoStore.cpp
)

target_link_libraries(findthebug-mongostore 
    PUBLIC 
        findthebug-shared
        mongo::mongocxx_shared
)