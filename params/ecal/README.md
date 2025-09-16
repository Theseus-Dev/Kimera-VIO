# Kimera-VIO eCAL Integration

This directory contains configuration files for running Kimera-VIO with eCAL data streams.

## Prerequisites

1. eCAL must be installed and available on the system
2. Cap'n Proto must be installed
3. The vns-sdk `vns_capnp` library must be built and available
4. Kimera-VIO must be built with eCAL support enabled

## Configuration Files

- `EcalDataProviderParams.yaml` - eCAL-specific configuration (topics, validation settings)
- `*CameraParams.yaml` - Camera calibration parameters (adjust for your cameras)
- `ImuParams.yaml` - IMU parameters (adjust for your IMU)
- `PipelineParams.yaml` - VIO pipeline configuration
- `flags/stereoVIOEcal.flags` - Command-line flags for eCAL mode

## Running Kimera-VIO with eCAL

### Method 1: Using the dedicated eCAL executable

```bash
# Build Kimera-VIO with eCAL support
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8

# Run with eCAL data provider
./stereoVIOEcal \
  --params_folder_path=../params/ecal \
  --dataset_type=2 \
  --flagfile=../params/ecal/flags/stereoVIOEcal.flags
```

### Method 2: Using the original executable with eCAL flags

```bash
./stereoVIOEuroc \
  --params_folder_path=../params/ecal \
  --dataset_type=2 \
  --imu_topic="/imu/data" \
  --left_camera_topic="/camera/left/image" \
  --right_camera_topic="/camera/right/image" \
  --flagfile=../params/ecal/flags/stereoVIOEcal.flags
```

## Customizing for Your Setup

### 1. Update Topic Names
Edit `EcalDataProviderParams.yaml` to match your eCAL topic names:

```yaml
imu_topic: "/your/imu/topic"
left_camera_topic: "/your/left/camera/topic" 
right_camera_topic: "/your/right/camera/topic"
```

### 2. Update Camera Calibration
Edit `LeftCameraParams.yaml` and `RightCameraParams.yaml` with your camera calibration:

```yaml
intrinsics: [fx, fy, cx, cy]  # Your camera intrinsics
distortion_coefficients: [k1, k2, p1, p2]  # Your distortion coefficients
T_BS: # Your camera-to-body transformation matrix
```

### 3. Update IMU Parameters
Edit `ImuParams.yaml` with your IMU specifications:

```yaml
gyroscope_noise_density: 0.187  # Your gyro noise
accelerometer_noise_density: 0.186  # Your accel noise
# ... other IMU parameters
```

## Expected eCAL Message Format

The eCAL data provider expects messages in Cap'n Proto format matching the vns-sdk schema:

- **IMU messages**: `vkc::Imu` format with `header`, `linearAcceleration`, `angularVelocity`
- **Image messages**: `vkc::Image` format with `header`, `encoding`, `width`, `height`, `data`

Supported image encodings:
- `mono8` (preferred for VIO)
- `bgr8` (will be converted to grayscale)
- `mono16` (will be converted to 8-bit)

## Troubleshooting

### Common Issues:

1. **"eCAL not found"** - Install eCAL and ensure it's in your PATH
2. **"vns_capnp not found"** - Build the vns-sdk project and ensure the library path is correct
3. **"Failed to subscribe to topic"** - Check that your eCAL topics are being published
4. **"No IMU data received"** - Verify IMU topic name and message format
5. **"Camera calibration errors"** - Update camera parameters to match your setup

### Debug Mode:
Run with additional logging:
```bash
export GLOG_v=1
./stereoVIOEcal --params_folder_path=../params/ecal --dataset_type=2
```

### Checking eCAL Status:
```bash
# List available eCAL topics
ecal_mon_cli

# Monitor specific topic
ecal_mon_gui
```

## Performance Tips

1. **Use shared memory**: eCAL automatically uses shared memory for local communication
2. **Adjust buffer sizes**: Modify buffer sizes in `EcalDataProviderParams.yaml`
3. **Monitor message rates**: Enable rate logging to identify bottlenecks
4. **Image format**: Use `mono8` encoding for best performance

## Integration with VNS-SDK

This implementation is designed to work with the VNS-SDK eCAL ecosystem:
- Uses the same Cap'n Proto message definitions
- Compatible with VNS-SDK publishers
- Follows VNS-SDK naming conventions for topics