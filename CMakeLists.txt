if (COMPONENTS_IS_ENABLED_QT)
    COMPONENTS_CONFIGURE_COMPONENT(Network
        Qt5::Core
        Qt5::WebSockets
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
