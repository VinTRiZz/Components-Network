if (COMPONENTS_IS_ENABLED_QT)
    COMPONENTS_CONFIGURE_COMPONENT(Network "1.0.0"
        Qt5::Core
        Qt5::Network
        ssl
        crypto
    )
else()
    COMPONENTS_CONFIGURE_COMPONENT(Network "1.0.0"
        ssl
        crypto
    )
endif()

COMPONENTS_LINK_COMPONENT(Components-Network Logger)
COMPONENTS_LINK_COMPONENT(Components-Network Encryption)

# Websocketpp
if (websocketpp_SOURCE_DIR)
    target_include_directories(Components-Network PRIVATE ${websocketpp_SOURCE_DIR})
else()
    # Remove unsupported files
    get_target_property(CURRENT_SOURCES Components-Network SOURCES)
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/client.cpp")
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/client.hpp")
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/server.cpp")
    list(REMOVE_ITEM CURRENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/WebSockets/server.hpp")
    set_target_properties(Components-Network PROPERTIES SOURCES "${CURRENT_SOURCES}")
endif()
