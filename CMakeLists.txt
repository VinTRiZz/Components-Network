if (COMPONENTS_IS_ENABLED_QT)
    COMPONENTS_CONFIGURE_COMPONENT(Network
        Qt5::Core
        Qt5::Network
        ssl
    )
else()
    COMPONENTS_CONFIGURE_COMPONENT(Network
        ssl
    )
endif()

COMPONENTS_LINK_COMPONENT(Network Logger)
COMPONENTS_LINK_COMPONENT(Network Encryption)

# Websocketpp
if (websocketpp_SOURCE_DIR)
    target_include_directories(Network PRIVATE ${websocketpp_SOURCE_DIR})
else()
    # Remove unsupported files
    get_target_property(CURRENT_SOURCES Network SOURCES)
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/client.cpp")
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/client.hpp")
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/server.cpp")
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/server.hpp")
    set_target_properties(Network PROPERTIES SOURCES "${CURRENT_SOURCES}")
endif()
