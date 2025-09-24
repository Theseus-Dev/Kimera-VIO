
#pragma once

#include <ecal/ecal.h>
#include "imu.capnp.h"
#include "image.capnp.h"

#include <memory>
#include <mutex>
#include <string>
#include <atomic>

#include "kimera-vio/dataprovider/DataProviderInterface.h"
#include "kimera-vio/frontend/Frame.h"
#include "kimera-vio/frontend/VisionImuFrontend-definitions.h"
#include "kimera-vio/utils/Macros.h"

// Forward declarations to avoid including capnp headers in header file
namespace capnp {
  template<typename T> class Reader;
}

namespace VIO {

/**
 * @brief eCAL Data Provider that subscribes to eCAL topics and converts
 * Cap'n Proto messages to Kimera-VIO data structures.
 */
class EcalDataProvider : public DataProviderInterface {
 public:
  KIMERA_DELETE_COPY_CONSTRUCTORS(EcalDataProvider);
  KIMERA_POINTER_TYPEDEFS(EcalDataProvider);

  struct Config {
    std::string imu_topic = "/imu/data";
    std::string left_camera_topic = "/camera/left/image";
    std::string right_camera_topic = "/camera/right/image";
    bool enable_stereo = true;
    
    // Camera parameters - these should match your camera calibration
    CameraParams left_camera_params;
    CameraParams right_camera_params;
  };

  /**
   * @brief Constructor
   * @param config Configuration for topics and camera parameters
   */
  explicit EcalDataProvider(const Config& config);

  /**
   * @brief Destructor - ensures proper eCAL cleanup
   */
  virtual ~EcalDataProvider();

  /**
   * @brief Initialize eCAL and create subscribers
   * @return True if initialization successful
   */
  bool initialize();

  /**
   * @brief Spin the data provider - keeps eCAL running
   * @return True if still receiving data
   */
  bool spin() override;

  /**
   * @brief Check if data provider has more data
   * @return True if not shutdown
   */
  bool hasData() const override;

  /**
   * @brief Shutdown the data provider
   */
  void shutdown() override;

 private:
  /**
   * @brief eCAL callback for IMU data
   * @param topic_name Topic name
   * @param data Received data buffer
   */
  void onImuMessage(const char* topic_name, 
                    const struct eCAL::SReceiveCallbackData* data);

  /**
   * @brief eCAL callback for left camera data
   * @param topic_name Topic name
   * @param data Received data buffer
   */
  void onLeftImageMessage(const char* topic_name,
                          const struct eCAL::SReceiveCallbackData* data);

  /**
   * @brief eCAL callback for right camera data
   * @param topic_name Topic name
   * @param data Received data buffer
   */
  void onRightImageMessage(const char* topic_name,
                           const struct eCAL::SReceiveCallbackData* data);

  /**
   * @brief Convert Cap'n Proto IMU message to Kimera IMU measurement
   * @param data Raw message data
   * @param size Message size
   * @return Kimera IMU measurement
   */
  ImuMeasurement convertImuMessage(const vkc::Imu::Reader&);

  /**
   * @brief Convert Cap'n Proto Image message to Kimera Frame
   * @param data Raw message data
   * @param size Message size
   * @param camera_params Camera parameters for this frame
   * @param resizeDim Optional resize dimensions for center crop [width, height]
   * @return Kimera Frame unique pointer
   */
  Frame::UniquePtr convertImageMessage(const vkc::Image::Reader&,
                                       const CameraParams& camera_params,
                                       const std::pair<int, int>& resizeDim = {0, 0});


 private:
  Config config_;
  bool initialized_;
  std::mutex mutex_;
  
  // eCAL subscribers
  std::unique_ptr<eCAL::CSubscriber> imu_subscriber_;
  std::unique_ptr<eCAL::CSubscriber> left_camera_subscriber_;
  std::unique_ptr<eCAL::CSubscriber> right_camera_subscriber_;
  
  // Message counters for debugging
  std::atomic<size_t> imu_message_count_;
  std::atomic<size_t> left_image_count_;
  std::atomic<size_t> right_image_count_;
};

}  // namespace VIO