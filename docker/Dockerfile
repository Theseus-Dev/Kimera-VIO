FROM ghcr.io/theseus-dev/amd-compile-amd64-builder

RUN apt-get update && \
    apt-get install -y \
      build-essential \
      cmake \
      gfortran \
      git \
      libatlas-base-dev \
      libboost-all-dev \
      libeigen3-dev \
      libgflags-dev \
      libgoogle-glog-dev \
      libmetis-dev \
      libopencv-dev \
      libopencv-contrib-dev \
      libtbb-dev \
      pkg-config \
      xvfb \
      python3 \
      python3-dev \
      python3-pip \
      python3-tk

RUN cd /tmp && \
    git clone https://github.com/borglab/gtsam.git && \
    cd gtsam && \
    git checkout 4.2 && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DGTSAM_USE_SYSTEM_EIGEN=ON -DGTSAM_POSE3_EXPMAP=ON -DGTSAM_ROT3_EXPMAP=ON -DGTSAM_TANGENT_PREINTEGRATION=OFF .. && \
    make -j$(nproc) install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/gtsam

RUN cd /tmp && \
    git clone https://github.com/laurentkneip/opengv.git && \
    cd opengv && \
    mkdir build && \
    cd build && \
    cmake .. -DEIGEN_INCLUDE_DIR=/usr/include/eigen3 -DEIGEN_INCLUDE_DIRS=/usr/include/eigen3 && \
    make -j$(nproc) install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/opengv

COPY .patches/fix_vocab.patch dbow2.patch
RUN cd /tmp && \
    git clone https://github.com/dorian3d/DBoW2.git && \
    cd DBoW2 && \
    git apply /workspace/dbow2.patch && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) install && \
    cd / && \
    rm -rf /tmp/DBoW2

# Install RobustPGO
ADD https://api.github.com/repos/MIT-SPARK/Kimera-RPGO/git/refs/heads/master version.json
RUN cd /tmp && \
    git clone https://github.com/MIT-SPARK/Kimera-RPGO.git && \
    cd Kimera-RPGO && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) install && \
    cd / && \
    rm -rf /tmp/Kimera-RPGO


RUN apt-get update && apt install -y \
    libgl1-mesa-dev libwayland-dev libxkbcommon-dev wayland-protocols libegl1-mesa-dev \
    libc++-dev libepoxy-dev libglew-dev libeigen3-dev cmake g++ ninja-build \
    libjpeg-dev libpng-dev catch2 \
    libavcodec-dev libavutil-dev libavformat-dev libswscale-dev libavdevice-dev \
    mesa-utils \
    mesa-common-dev \
    libgl1-mesa-glx \
    libglu1-mesa-dev \
    libegl1-mesa \
    libgles2-mesa \
    libgtk-3-dev \
    libcanberra-gtk3-module

# Install pangolin (visualizer)
RUN cd /tmp && \
    git clone --recursive https://github.com/stevenlovegrove/Pangolin.git && \
    cd Pangolin && \
    mkdir build && \
    cd build && \
    cmake .. && \
    cmake --build . --target install && \
    ldconfig && \
    cd / && \
    rm -rf /tmp/Pangolin

RUN apt-get install -y sudo libunwind-dev libgoogle-glog-dev

# Accept build arguments for user creation
ARG USER_ID=1000
ARG GROUP_ID=1000
ARG USERNAME=user

# Create user with specified UID/GID
RUN groupadd -g ${GROUP_ID} ${USERNAME} && \
    useradd -u ${USER_ID} -g ${GROUP_ID} -m -s /bin/bash ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers    

# Set working directory and ownership
WORKDIR /workspace
RUN chown -R ${USERNAME}:${USERNAME} /workspace