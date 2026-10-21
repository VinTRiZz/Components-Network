COMPONENTS_CONFIGURE_COMPONENT(Network "1.0.0"
    ssl
)
COMPONENTS_LINK_COMPONENT(Components-Network Core)
COMPONENTS_LINK_COMPONENT(Components-Network Encryption)
COMPONENTS_LINK_COMPONENT(Components-Network Logger)
if (COMPONENTS_IS_ENABLED_QT)
    target_link_libraries(Components-Network PRIVATE Qt5::Core Qt5::Network)
endif()
