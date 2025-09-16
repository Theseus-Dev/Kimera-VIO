/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   EcalKimeraVIO.cpp
 * @brief  Example usage of Kimera-VIO with eCAL data provider
 * @author Generated for Kimera-VIO eCAL integration
 */

#include <gflags/gflags.h>
#include <glog/logging.h>

#ifdef ECAL_FOUND
#include "kimera-vio/dataprovider/EcalDataProvider.h"
#endif

#include "kimera-vio/frontend/Camera.h"
#include "kimera-vio/pipeline/Pipeline-definitions.h"
#include "kimera-vio/pipeline/StereoImuPipeline.h"
#include "kimera-vio/utils/Timer.h"

DEFINE_string(params_folder_path, "", "Path to folder containing VIO parameters.");
DEFINE_string(imu_topic, "/imu/data", "eCAL topic for IMU data");
DEFINE_string(left_camera_topic, "/camera/left/image", "eCAL topic for left camera");
DEFINE_string(right_camera_topic, "/camera/right/image", "eCAL topic for right camera");
DEFINE_bool(log_output, false, "Log output to files");

int main(int argc, char* argv[]) {
  // Initialize Google Logging and Flags
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_alsologtostderr = 1;
  FLAGS_colorlogtostderr = 1;

#ifndef ECAL_FOUND
  LOG(ERROR) << "eCAL support not compiled. Please install eCAL and CapnProto and recompile.";
  return -1;
#else

  LOG(INFO) << "Starting Kimera-VIO with eCAL data provider...";

  // Check required parameters
  if (FLAGS_params_folder_path.empty()) {
    LOG(ERROR) << "Please provide path to VIO parameters folder with --params_folder_path";
    return -1;
  }

  try {
    // Create VIO parameters (you'll need to load these from your config files)
    VIO::VioParams vio_params;
    // TODO: Load vio_params from FLAGS_params_folder_path
    // vio_params.parseYAML(FLAGS_params_folder_path + "/vio_params.yaml");

    // For now, set some basic parameters - you'll need to customize these
    LOG(WARNING) << "Using default VIO parameters. Please load from config files for production use.";
    
    // Create eCAL data provider configuration
    VIO::EcalDataProvider::Config ecal_config;
    ecal_config.imu_topic = FLAGS_imu_topic;
    ecal_config.left_camera_topic = FLAGS_left_camera_topic;
    ecal_config.right_camera_topic = FLAGS_right_camera_topic;
    ecal_config.enable_stereo = true;
    ecal_config.enable_mono = false;
    
    // TODO: Set camera parameters from your calibration
    // ecal_config.left_camera_params = load_camera_params(FLAGS_params_folder_path + "/left_camera.yaml");
    // ecal_config.right_camera_params = load_camera_params(FLAGS_params_folder_path + "/right_camera.yaml");
    
    LOG(WARNING) << "Using default camera parameters. Please set from calibration files for production use.";

    // Create data provider
    auto data_provider = std::make_unique<VIO::EcalDataProvider>(ecal_config);
    
    // Initialize eCAL data provider
    if (!data_provider->initialize()) {
      LOG(ERROR) << "Failed to initialize eCAL data provider";
      return -1;
    }

    LOG(INFO) << "eCAL data provider initialized successfully";
    LOG(INFO) << "Subscribing to topics:";
    LOG(INFO) << "  IMU: " << FLAGS_imu_topic;
    LOG(INFO) << "  Left camera: " << FLAGS_left_camera_topic;
    LOG(INFO) << "  Right camera: " << FLAGS_right_camera_topic;

    // Create VIO pipeline
    // TODO: You'll need to create and configure the actual VIO pipeline
    // VIO::StereoImuPipeline vio_pipeline(vio_params);
    
    // Register callbacks to connect data provider to VIO pipeline
    // data_provider->registerImuSingleCallback(
    //     [&vio_pipeline](const VIO::ImuMeasurement& imu) {
    //       // Forward IMU data to pipeline
    //       vio_pipeline.fillSingleImuQueue(imu);
    //     });
    
    // data_provider->registerLeftFrameCallback(
    //     [&vio_pipeline](VIO::Frame::UniquePtr frame) {
    //       // Forward left frame to pipeline
    //       vio_pipeline.fillLeftFrameQueue(std::move(frame));
    //     });
    
    // data_provider->registerRightFrameCallback(
    //     [&vio_pipeline](VIO::Frame::UniquePtr frame) {
    //       // Forward right frame to pipeline
    //       vio_pipeline.fillRightFrameQueue(std::move(frame));
    //     });

    LOG(INFO) << "Starting data processing loop...";
    LOG(INFO) << "Press Ctrl+C to stop";

    // Main processing loop
    VIO::utils::Timer timer;
    timer.tic();
    
    size_t loop_count = 0;
    while (data_provider->hasData()) {
      // Spin the data provider
      if (!data_provider->spin()) {
        LOG(INFO) << "Data provider finished or was shutdown";
        break;
      }
      
      // TODO: Spin the VIO pipeline
      // vio_pipeline.spin();
      
      loop_count++;
      if (loop_count % 1000 == 0) {
        auto elapsed = timer.toc();
        LOG(INFO) << "Processed " << loop_count << " iterations in " 
                  << elapsed << " seconds (" << (loop_count / elapsed) << " Hz)";
      }
    }

    auto total_elapsed = timer.toc();
    LOG(INFO) << "Processing completed in " << total_elapsed << " seconds";
    LOG(INFO) << "Average rate: " << (loop_count / total_elapsed) << " Hz";

  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception: " << e.what();
    return -1;
  }

  LOG(INFO) << "Kimera-VIO eCAL example finished successfully";
  return 0;

#endif  // ECAL_FOUND
}