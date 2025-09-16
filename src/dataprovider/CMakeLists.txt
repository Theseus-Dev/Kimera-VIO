### Add source code for stereoVIO
target_sources(kimera_vio
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/DataProviderInterface-definitions.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DataProviderInterface.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DataProviderModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MonoDataProviderModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/RgbdDataProviderModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/StereoDataProviderModule.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/EurocDataProvider.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/KittiDataProvider.cpp"
)

# Conditionally add EcalDataProvider if eCAL is found
if(eCAL_FOUND AND CapnProto_FOUND AND VNS_CAPNP_LIB)
    target_sources(kimera_vio
        PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/EcalDataProvider.cpp"
    )
endif()
