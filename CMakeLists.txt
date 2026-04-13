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
endif()
