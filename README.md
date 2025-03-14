# Video Service
This repository contains the source code of the Video service, which is part of the [Underwater Camera Project](https://underwater-camera-project.github.io).

## Features

The service provides the following features:

* H.264 video stream (1920 x 1080 pixel)
* MPJPEG video stream (800 x 600 pixel)
* Remote Control Interface for changing the settings of the camera module.

## Installation

The installation of the video service consists of two parts: firstly, all the necessary dependencies must be made available, and then the service can be compiled.

### Installing the Dependencies

The following commands can be used to install the required dependencies.

```bash
apt update

apt install git meson pkg-config cmake g++ clang ninja-build \
    libyuv libyaml-dev python3-yaml python3-ply python3-jinja2 \
    git meson pkg-config cmake g++ clang ninja-build libyaml-dev \
    python3-yaml python3-ply python3-jinja2 wget libjpeg-dev

# libcamera
cd
git clone https://git.libcamera.org/libcamera/libcamera.git
cd libcamera
meson setup build
ninja -C build install

# boost
cd
wget https://archives.boost.io/release/1.82.0/source/boost_1_82_0.tar.gz
tar -tf boost_1_82_0.tar.gz
cd boost_1_82_0
``` 

### Compiling the Video Service

The following commands can be used to compile the video service:

```bash
cd
git clone https://github.com/tederer/octowatch-videoservice.git
cd octowatch-videoservice
./compile.sh
```

## Starting the service

Execute `start.sh`, which is located in the root folder of this project.

video service of the OctoWatch underwater camera

## Environment variables

|variable              |range                               |default        |description                                  |
|----------------------|------------------------------------|---------------|---------------------------------------------|
|OCTOWATCH_LOG_LEVEL   | [DEBUG, INFO, WARNING, ERROR, OFF] | INFO          | log level                                   |
|OCTOWATCH_JPEG_QUALITY| integer in the range [0, 100]      | 95            | JPEG image quality                          |
|OCTOWATCH_JPEG_ENCODER| [CPU, emptyString]                 | emptyString   | whether to use CPU or hardware JPEG encoder   |

## References

* [libcamera (open source camera stack)](https://libcamera.org)
* [Video for Linux API (V4L2)](https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html)
* [libjpeg-turbo (JPEG image codec that uses SIMD instructions)](https://www.libjpeg-turbo.org)
* [Boost.Asio (cross-platform C++ library for network and low-level I/O programming)](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html)
* [Meson (open source build system)](https://mesonbuild.com)
