/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   EcalDataProvider.cpp
 * @brief  eCAL data provider implementation for the VIO pipeline.
 * @author Generated for Kimera-VIO eCAL integration
 */

#include "kimera-vio/dataprovider/EcalDataProvider.h"

// #ifdef ECAL_FOUND

#include <glog/logging.h>
#include <opencv2/opencv.hpp>

#include <chrono>
#include <thread>

// Include Cap'n Proto serialization
#include <capnp/serialize.h>
#include <kj/array.h>

// Include Cap'n Proto generated headers from vns-sdk
#include "imu.capnp.h"
#include "image.capnp.h"
#include "header.capnp.h"

namespace VIO {

EcalDataProvider::EcalDataProvider(const Config& config) 
    : DataProviderInterface(),
      config_(config),
      initialized_(false),
      imu_message_count_(0),
      left_image_count_(0),
      right_image_count_(0) {
}

EcalDataProvider::~EcalDataProvider() {
  shutdown();
}

bool EcalDataProvider::initialize() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (initialized_) {
    LOG(WARNING) << "EcalDataProvider already initialized";
    return true;
  }

  LOG(INFO) << "Initializing eCAL data provider...";
  
  // Initialize eCAL
  if (!eCAL::Initialize(0, nullptr, "KimeraVIO")) {
    LOG(ERROR) << "Failed to initialize eCAL";
    return false;
  }

  try {
    // Create IMU subscriber
    imu_subscriber_ = std::make_unique<eCAL::CSubscriber>(config_.imu_topic);
    if (!imu_subscriber_->IsCreated()) {
      LOG(ERROR) << "Failed to create IMU subscriber for topic: " << config_.imu_topic;
      return false;
    }
    
    // Register IMU callback using lambda to bind member function
    auto imu_callback = [this](const char* topic_name, 
                               const struct eCAL::SReceiveCallbackData* data, 
                               const void* par) {
      this->onImuMessage(topic_name, data, par);
    };
    imu_subscriber_->AddReceiveCallback(imu_callback);
    
    // Create left camera subscriber
    left_camera_subscriber_ = std::make_unique<eCAL::CSubscriber>(config_.left_camera_topic);
    if (!left_camera_subscriber_->IsCreated()) {
      LOG(ERROR) << "Failed to create left camera subscriber for topic: " << config_.left_camera_topic;
      return false;
    }
    
    auto left_callback = [this](const char* topic_name,
                                const struct eCAL::SReceiveCallbackData* data,
                                const void* par) {
      this->onLeftImageMessage(topic_name, data, par);
    };
    left_camera_subscriber_->AddReceiveCallback(left_callback);

    // Create right camera subscriber if stereo is enabled
    if (config_.enable_stereo) {
      right_camera_subscriber_ = std::make_unique<eCAL::CSubscriber>(config_.right_camera_topic);
      if (!right_camera_subscriber_->IsCreated()) {
        LOG(ERROR) << "Failed to create right camera subscriber for topic: " << config_.right_camera_topic;
        return false;
      }
      
      auto right_callback = [this](const char* topic_name,
                                   const struct eCAL::SReceiveCallbackData* data,
                                   const void* par) {
        this->onRightImageMessage(topic_name, data, par);
      };
      right_camera_subscriber_->AddReceiveCallback(right_callback);
    }

    initialized_ = true;
    LOG(INFO) << "eCAL data provider initialized successfully";
    LOG(INFO) << "  IMU topic: " << config_.imu_topic;
    LOG(INFO) << "  Left camera topic: " << config_.left_camera_topic;
    if (config_.enable_stereo) {
      LOG(INFO) << "  Right camera topic: " << config_.right_camera_topic;
    }
    
    return true;
    
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception during eCAL initialization: " << e.what();
    return false;
  }
}

bool EcalDataProvider::spin() {
  if (!initialized_) {
    LOG(ERROR) << "EcalDataProvider not initialized. Call initialize() first.";
    return false;
  }

  if (shutdown_.load()) {
    return false;
  }

  // eCAL handles message reception in background threads via callbacks
  // We just need to keep the process alive
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  
  // Check if eCAL is still OK
  return eCAL::Ok() && !shutdown_.load();
}

bool EcalDataProvider::hasData() const {
  return !shutdown_.load() && eCAL::Ok();
}

void EcalDataProvider::shutdown() {
  LOG(INFO) << "Shutting down eCAL data provider...";
  
  DataProviderInterface::shutdown();  // Set shutdown_ flag
  
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Reset subscribers (this will automatically unsubscribe)
  imu_subscriber_.reset();
  left_camera_subscriber_.reset();
  right_camera_subscriber_.reset();
  
  // Finalize eCAL
  if (initialized_) {
    try {
      eCAL::Finalize();
      LOG(INFO) << "eCAL finalized successfully";
    } catch (const std::exception& e) {
      LOG(ERROR) << "Exception during eCAL finalization: " << e.what();
    }
  }
  
  initialized_ = false;
  
  LOG(INFO) << "eCAL data provider shutdown complete";
  LOG(INFO) << "Message counts - IMU: " << imu_message_count_.load() 
            << ", Left: " << left_image_count_.load()
            << ", Right: " << right_image_count_.load();
}

void EcalDataProvider::onImuMessage(const char* topic_name,
                                    const struct eCAL::SReceiveCallbackData* data,
                                    const void* par) {
  if (shutdown_.load() || !data || !data->buf || data->size == 0) {
    return;
  }

  try {
    // Convert raw data to Cap'n Proto words
    kj::ArrayPtr<const capnp::word> words(
        reinterpret_cast<const capnp::word*>(data->buf),
        data->size / sizeof(capnp::word));
    
    // Create message reader
    capnp::FlatArrayMessageReader reader(words);
    auto imu_msg = reader.getRoot<vkc::Imu>();
    
    // Convert to Kimera IMU measurement
    ImuMeasurement imu_measurement = convertImuMessage(imu_msg);
    
    // Send to pipeline via callback
    if (imu_single_callback_) {
      imu_single_callback_(imu_measurement);
    }
    
    imu_message_count_++;
    
    if (imu_message_count_ % 100 == 0) {
      LOG(INFO) << "Received " << imu_message_count_ << " IMU messages";
    }
    
  } catch (const std::exception& e) {
    LOG(ERROR) << "Error processing IMU message: " << e.what();
  }
}

void EcalDataProvider::onLeftImageMessage(const char* topic_name,
                                          const struct eCAL::SReceiveCallbackData* data,
                                          const void* par) {
  if (shutdown_.load() || !data || !data->buf || data->size == 0) {
    return;
  }

  try {
    // Convert raw data to Cap'n Proto words
    kj::ArrayPtr<const capnp::word> words(
        reinterpret_cast<const capnp::word*>(data->buf),
        data->size / sizeof(capnp::word));
    
    // Create message reader
    capnp::FlatArrayMessageReader reader(words);
    auto image_msg = reader.getRoot<vkc::Image>();
    
    // Convert to Kimera Frame
    Frame::UniquePtr frame = convertImageMessage(image_msg, config_.left_camera_params);
    
    if (frame && left_frame_callback_) {
      left_frame_callback_(std::move(frame));
    }
    
    left_image_count_++;
    
    if (left_image_count_ % 30 == 0) {
      LOG(INFO) << "Received " << left_image_count_ << " left camera frames";
    }
    
  } catch (const std::exception& e) {
    LOG(ERROR) << "Error processing left image message: " << e.what();
  }
}

void EcalDataProvider::onRightImageMessage(const char* topic_name,
                                           const struct eCAL::SReceiveCallbackData* data,
                                           const void* par) {
  if (shutdown_.load() || !data || !data->buf || data->size == 0) {
    return;
  }

  try {
    // Convert raw data to Cap'n Proto words
    kj::ArrayPtr<const capnp::word> words(
        reinterpret_cast<const capnp::word*>(data->buf),
        data->size / sizeof(capnp::word));
    
    // Create message reader
    capnp::FlatArrayMessageReader reader(words);
    auto image_msg = reader.getRoot<vkc::Image>();
    
    // Convert to Kimera Frame
    Frame::UniquePtr frame = convertImageMessage(image_msg, config_.right_camera_params);
    
    if (frame && right_frame_callback_) {
      right_frame_callback_(std::move(frame));
    }
    
    right_image_count_++;
    
    if (right_image_count_ % 30 == 0) {
      LOG(INFO) << "Received " << right_image_count_ << " right camera frames";
    }
    
  } catch (const std::exception& e) {
    LOG(ERROR) << "Error processing right image message: " << e.what();
  }
}

ImuMeasurement EcalDataProvider::convertImuMessage(const capnp::Reader<vkc::Imu>& imu_msg) {
  ImuMeasurement measurement;
  
  // Extract timestamp from header
  auto header = imu_msg.getHeader();
  measurement.timestamp_ = header.getStampMonotonic();
  
  // Convert linear acceleration (Cap'n Proto Vector3d to gtsam::Vector3)
  auto linear_acc = imu_msg.getLinearAcceleration();
  measurement.acc_gyr_(0) = linear_acc.getX();
  measurement.acc_gyr_(1) = linear_acc.getY();
  measurement.acc_gyr_(2) = linear_acc.getZ();
  
  // Convert angular velocity (Cap'n Proto Vector3d to gtsam::Vector3)
  auto angular_vel = imu_msg.getAngularVelocity();
  measurement.acc_gyr_(3) = angular_vel.getX();
  measurement.acc_gyr_(4) = angular_vel.getY();
  measurement.acc_gyr_(5) = angular_vel.getZ();
  
  return measurement;
}

Frame::UniquePtr EcalDataProvider::convertImageMessage(const capnp::Reader<vkc::Image>& image_msg,
                                                       const CameraParams& camera_params) {
  // Extract timestamp
  auto header = image_msg.getHeader();
  Timestamp timestamp = header.getStampMonotonic();
  
  // Get image properties
  uint32_t width = image_msg.getWidth();
  uint32_t height = image_msg.getHeight();
  auto encoding = image_msg.getEncoding();
  
  // Get image data
  auto data = image_msg.getData();
  
  // Convert image data to OpenCV Mat based on encoding
  cv::Mat cv_image;
  
  switch (encoding) {
    case vkc::Image::Encoding::MONO8: {
      cv_image = cv::Mat(height, width, CV_8UC1, 
                         const_cast<void*>(static_cast<const void*>(data.begin())));
      break;
    }
    case vkc::Image::Encoding::BGR8: {
      cv_image = cv::Mat(height, width, CV_8UC3,
                         const_cast<void*>(static_cast<const void*>(data.begin())));
      // Convert BGR to grayscale for VIO
      cv::cvtColor(cv_image, cv_image, cv::COLOR_BGR2GRAY);
      break;
    }
    case vkc::Image::Encoding::MONO16: {
      cv_image = cv::Mat(height, width, CV_16UC1,
                         const_cast<void*>(static_cast<const void*>(data.begin())));
      // Convert to 8-bit if needed
      cv_image.convertTo(cv_image, CV_8UC1, 1.0/256.0);
      break;
    }
    default:
      LOG(ERROR) << "Unsupported image encoding: " << static_cast<int>(encoding);
      return nullptr;
  }
  
  // Clone the image to ensure we own the data
  cv_image = cv_image.clone();
  
  // Create Kimera Frame
  Frame::UniquePtr frame = std::make_unique<Frame>(
      timestamp,
      cv_image,
      camera_params);
  
  return frame;
}


}  // namespace VIO

// #endif // ECAL_FOUND