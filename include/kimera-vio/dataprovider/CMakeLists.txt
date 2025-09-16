### Add source code for IDEs
target_sources(kimera_vio PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/DataProviderInterface-definitions.h"
  "${CMAKE_CURRENT_LIST_DIR}/DataProviderModule.h"
  "${CMAKE_CURRENT_LIST_DIR}/StereoDataProviderModule.h"
  "${CMAKE_CURRENT_LIST_DIR}/RgbdDataProviderModule.h"
  "${CMAKE_CURRENT_LIST_DIR}/DataProviderInterface.h"
  "${CMAKE_CURRENT_LIST_DIR}/EurocDataProvider.h"
  # "${CMAKE_CURRENT_LIST_DIR}/KittiDataProvider.h"
)

# Conditionally add EcalDataProvider header if eCAL is found
if(eCAL_FOUND AND CapnProto_FOUND AND VNS_CAPNP_LIB)
  target_sources(kimera_vio PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/EcalDataProvider.h"
  )
endif()
