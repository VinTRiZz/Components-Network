COMPONENTS_CONFIGURE_COMPONENT(Network "1.0.0"
    ssl
)
COMPONENTS_LINK_COMPONENT(Components-Network Encryption)
COMPONENTS_LINK_COMPONENT(Components-Network Logger)
if (COMPONENTS_IS_ENABLED_QT)
    target_link_libraries(Components-Network PRIVATE Qt5::Qt5::Network)
endif()
if (COMPONENTS_SW_websocketpp)
    target_link_libraries(Components-Network PRIVATE Qt5::Qt5::Network)
    target_include_directories(Components-Network PRIVATE "${websocketpp_SOURCE_DIR}")
else()
    COMPONENTS_REMOVE_SOURCES(Components-Network
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/client.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/client.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/server.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/websocketsession.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/websocketsession.h"
    )
endif()
