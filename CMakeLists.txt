cmake_minimum_required(VERSION 3.15)
#sea_battle
project(sea_battle)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "C:/boost")
set(BOOST_INCLUDEDIR "C:/boost")
set(BOOST_LIBRARYDIR "C:/boost/stage/lib")

include_directories(${BOOST_INCLUDEDIR})
link_directories(${BOOST_LIBRARYDIR})

add_executable(sea_battle
    main.cpp
    src/Game.cpp
    src/CommandProcessor.cpp
)

target_include_directories(sea_battle PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# web_server
add_executable(web_server
    src/WebServer.cpp
    src/Game.cpp
    src/CommandProcessor.cpp
)

target_include_directories(web_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(web_server PRIVATE 
        ws2_32 
        mswsock
        "libboost_json-mgw14-mt-s-x32-1_87.a"
    )
endif()
