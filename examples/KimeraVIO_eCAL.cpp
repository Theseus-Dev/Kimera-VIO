/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   KimeraVIO_eCAL.cpp
 * @brief  Example of VIO pipeline using eCAL data provider
 * @author Generated for Kimera-VIO eCAL integration
 */

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <chrono>
#include <future>
#include <memory>
#include <utility>

#include "kimera-vio/dataprovider/EcalDataProvider.h"

#include "kimera-vio/frontend/StereoImuSyncPacket.h"
#include "kimera-vio/logging/Logger.h"
#include "kimera-vio/pipeline/Pipeline.h"
#include "kimera-vio/pipeline/MonoImuPipeline.h"
#include "kimera-vio/pipeline/StereoImuPipeline.h"
#include "kimera-vio/utils/Statistics.h"
#include "kimera-vio/utils/Timer.h"

#include <capnp/serialize.h>
#include <ecal/ecal.h>
#include <kj/array.h>
#include "odometry3d.capnp.h"

DEFINE_string(
    params_folder_path,
    "../params/ecal",
    "Path to the folder containing the yaml files with the VIO parameters.");

// eCAL-specific flags
DEFINE_string(imu_topic, "S1/imu", "eCAL topic for IMU data");
DEFINE_string(left_camera_topic, "S1/stereo1_l", "eCAL topic for left camera");
DEFINE_string(right_camera_topic, "S1/stereo2_r", "eCAL topic for right camera");
DEFINE_bool(ecal_logging_enabled, true, "Enable eCAL message logging");
DEFINE_int32(ecal_timeout_ms, 5000, "eCAL initialization timeout in milliseconds");




int main(int argc, char* argv[]) {
  // Initialize Google's flags library.
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);

  FLAGS_logtostderr = 1;
  FLAGS_colorlogtostderr = 1;

  // Parse VIO parameters from gflags.
  VIO::VioParams vio_params(FLAGS_params_folder_path);

  // Log parameters
  // vio_params.print();
  LOG(INFO) << "Visualization: " << (FLAGS_visualize ? "ON" : "OFF");

  LOG(INFO) << "Creating eCAL data provider...";
  // Configure eCAL data provider
  VIO::EcalDataProvider::Config ecal_config;
  ecal_config.imu_topic = FLAGS_imu_topic;
  ecal_config.left_camera_topic = FLAGS_left_camera_topic;
  ecal_config.right_camera_topic = FLAGS_right_camera_topic;
  ecal_config.enable_stereo = vio_params.frontend_type_ == VIO::FrontendType::kStereoImu;
  
  // Use camera parameters from VIO params
  if (vio_params.camera_params_.size() < 1) {
    LOG(FATAL) << "No camera parameters loaded!";
  }
  
  ecal_config.left_camera_params = vio_params.camera_params_[0];
  
  // Only set right camera params if we're in stereo mode AND have a second camera
  if (ecal_config.enable_stereo) {
    if (vio_params.camera_params_.size() < 2) {
      LOG(FATAL) << "Stereo mode requires 2 camera parameter sets, but only " 
                 << vio_params.camera_params_.size() << " found!";
    }
    ecal_config.right_camera_params = vio_params.camera_params_[1];
  }

  VIO::DataProviderInterface::Ptr data_provider = std::make_shared<VIO::EcalDataProvider>(ecal_config);

  // Build VIO
  // auto vio_pipeline = std::make_shared<VIO::StereoImuPipeline>(vio_params);
  VIO::Pipeline::Ptr vio_pipeline;

  switch (vio_params.frontend_type_) {
    case VIO::FrontendType::kMonoImu: {
      vio_pipeline = std::make_unique<VIO::MonoImuPipeline>(vio_params);
    } break;
    case VIO::FrontendType::kStereoImu: {
      vio_pipeline = std::make_unique<VIO::StereoImuPipeline>(vio_params);
    } break;
    default: {
      LOG(FATAL) << "Unrecognized Frontend type: "
                 << VIO::to_underlying(vio_params.frontend_type_)
                 << ". 0: Mono, 1: Stereo.";
    } break;
  }
  

  CHECK(data_provider);
  CHECK(vio_pipeline);
  

  // Register VIO pipeline callbacks
  data_provider->registerImuSingleCallback(
      std::bind(&VIO::Pipeline::fillSingleImuQueue, vio_pipeline.get(), std::placeholders::_1));
  data_provider->registerLeftFrameCallback(
      std::bind(&VIO::Pipeline::fillLeftFrameQueue, vio_pipeline.get(), std::placeholders::_1));
  if (vio_params.frontend_type_ == VIO::FrontendType::kStereoImu)
  {
    auto stereo_pipeline =
        std::dynamic_pointer_cast<VIO::StereoImuPipeline>(vio_pipeline);
    data_provider->registerRightFrameCallback(
      std::bind(&VIO::StereoImuPipeline::fillRightFrameQueue, stereo_pipeline.get(), std::placeholders::_1));

  }

  auto output_pub = std::make_unique<eCAL::CPublisher>("kimera/vio", "capnp:Odometry3D", "Kimera VIO output");
  if (!output_pub->IsCreated()) {
      LOG(FATAL) << "Failed to create eCAL publisher";
  }
  uint64_t seq = 0;

  auto outputCallback = [&](const VIO::VioBackendModule::OutputSharedPtr& output) {
    // TODO: Update to a message type that publishes useful state and debug info like IMU bias, tracking stats, and publish num resets, covariance
    gtsam::Point3 pos = output->W_State_Blkf_.pose_.translation();
    gtsam::Vector3 velocity = output->W_State_Blkf_.velocity_;
    gtsam::Rot3 R = output->W_State_Blkf_.pose_.rotation();
    Eigen::Quaterniond quat = R.toQuaternion();

    capnp::MallocMessageBuilder message;
    vkc::Odometry3d::Builder odometry = message.initRoot<vkc::Odometry3d>();

    // Set header information
    auto header = odometry.initHeader();
    header.setSeq(seq++);
    header.setStampMonotonic(output->W_State_Blkf_.timestamp_);

    // Set body, reference, and velocity frames
    // odometry.setBodyFrame("imu");
    // odometry.setReferenceFrame("imu");

    // Set position and orientation (transformed)
    auto pose = odometry.initPose();
    auto position = pose.initPosition();
    position.setX(pos.x());
    position.setY(pos.y());
    position.setZ(pos.z());

    // Convert orientation back to quaternion format for the message
    auto orientation = pose.initOrientation();
    orientation.setW(quat.w());
    orientation.setX(quat.x());
    orientation.setY(quat.y());
    orientation.setZ(quat.z());

    // Set twist (transformed)
    auto twist = odometry.initTwist();
    auto linear = twist.initLinear();
    linear.setX(velocity.x());
    linear.setY(velocity.y());
    linear.setZ(velocity.z());

    // Keep the original angular velocity
    // auto angular = twist.initAngular();
    // angular.setX(latest_odom_msg->twist.angular[0]);
    // angular.setY(latest_odom_msg->twist.angular[1]);
    // angular.setZ(latest_odom_msg->twist.angular[2]);

    // odometry.setResetCounter(output->debug_info_->);


    kj::Array<capnp::word> words = capnp::messageToFlatArray(message);
    kj::ArrayPtr<const char> array(reinterpret_cast<const char*>(words.begin()),
                                    words.size() * sizeof(capnp::word));
    output_pub->Send(array.begin(), array.size());
  };

  vio_pipeline->registerBackendOutputCallback(outputCallback);
      
  // Spin dataset.
  auto tic = VIO::utils::Timer::tic();
  bool is_pipeline_successful = false;
  if (vio_params.parallel_run_) {
    auto handle = std::async(
        std::launch::async, &VIO::DataProviderInterface::spin, data_provider);
    auto handle_pipeline =
        std::async(std::launch::async, &VIO::Pipeline::spin, vio_pipeline);
    auto handle_shutdown = std::async(
        std::launch::async,
        &VIO::Pipeline::waitForShutdown,
        vio_pipeline,
        [&data_provider]() -> bool { return !data_provider->hasData(); },
        500,
        true);
    vio_pipeline->spinViz();
    is_pipeline_successful = !handle.get();
    handle_shutdown.get();
    handle_pipeline.get();
  } else {
    while (data_provider->spin() && vio_pipeline->spin()) {
      continue;
    };
    vio_pipeline->shutdown();
    is_pipeline_successful = true;
  }

  // Output stats.
  auto spin_duration = VIO::utils::Timer::toc(tic);
  LOG(WARNING) << "Spin took: " << spin_duration.count() << " ms.";
  LOG(INFO) << "Pipeline successful? "
            << (is_pipeline_successful ? "Yes!" : "No!");

  if (is_pipeline_successful) {
    // Log overall time of pipeline run.
    VIO::PipelineLogger logger;
    logger.logPipelineOverallTiming(spin_duration);
  }

  return is_pipeline_successful ? EXIT_SUCCESS : EXIT_FAILURE;
}