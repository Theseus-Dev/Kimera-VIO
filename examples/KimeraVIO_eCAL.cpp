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
#include "kimera-vio/pipeline/StereoImuPipeline.h"
#include "kimera-vio/utils/Statistics.h"
#include "kimera-vio/utils/Timer.h"

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
  LOG(INFO) << "Starting...";
  // Initialize Google's flags library.
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);

  // Parse VIO parameters from gflags.
  VIO::VioParams vio_params(FLAGS_params_folder_path);

  // Log parameters
  vio_params.print();
  LOG(INFO) << "Visualization: " << (FLAGS_visualize ? "ON" : "OFF");


  // Build VIO
  auto vio_pipeline = std::make_shared<VIO::StereoImuPipeline>(vio_params);
  
  LOG(INFO) << "Creating eCAL data provider...";
  
  // Configure eCAL data provider
  VIO::EcalDataProvider::Config ecal_config;
  ecal_config.imu_topic = FLAGS_imu_topic;
  ecal_config.left_camera_topic = FLAGS_left_camera_topic;
  ecal_config.right_camera_topic = FLAGS_right_camera_topic;
  ecal_config.enable_stereo = true;
  ecal_config.enable_mono = false;
  
  // Use camera parameters from VIO params
  ecal_config.left_camera_params = vio_params.camera_params_[0];
  ecal_config.right_camera_params = vio_params.camera_params_[1];
  
  VIO::DataProviderInterface::Ptr data_provider = std::make_shared<VIO::EcalDataProvider>(ecal_config);

  CHECK(data_provider);
  CHECK(vio_pipeline);

  

  // Register VIO pipeline callbacks
  data_provider->registerImuSingleCallback(
      std::bind(&VIO::StereoImuPipeline::fillSingleImuQueue, vio_pipeline.get(), std::placeholders::_1));
  data_provider->registerLeftFrameCallback(
      std::bind(&VIO::StereoImuPipeline::fillLeftFrameQueue, vio_pipeline.get(), std::placeholders::_1));
  data_provider->registerRightFrameCallback(
      std::bind(&VIO::StereoImuPipeline::fillRightFrameQueue, vio_pipeline.get(), std::placeholders::_1));

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