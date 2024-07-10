# Video Service
This repository contains the source code of the Video service, which is part of the [Underwater Camera Project](https://underwater-camera-project.github.io).

## Features

The service provides the following features:

* H.264 video stream (1920 x 1080 pixel)
* MPJPEG video stream (800 x 600 pixel)
* Remote Control Interface for changing the settings of the camera module

## Installation

The following steps describe how to install the monitoring service on your Raspberry Pi.

1.  Clone this repository
2.  Execute `compile.sh`

## Starting the service

Execute `start.sh`.

video service of the OctoWatch underwater camera

## Environment variables

|variable              |range                               |default        |description                                  |
|----------------------|------------------------------------|---------------|---------------------------------------------|
|OCTOWATCH_LOG_LEVEL   | [DEBUG, INFO, WARNING, ERROR, OFF] | INFO          | log level                                   |
|OCTOWATCH_JPEG_QUALITY| integer in the range [0, 100]      | 95            | JPEG image quality                          |
|OCTOWATCH_JPEG_ENCODER| [CPU, emptyString]                 | emptyString   | whether to use CPU or hardware JPEG encoder |

## References

* [libcamera (open source camera stack)](https://libcamera.org)
* [Video for Linux API (V4L2)](https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html)
* [libjpeg-turbo (JPEG image codec that uses SIMD instructions)](https://www.libjpeg-turbo.org)
* [Boost.Asio (cross-platform C++ library for network and low-level I/O programming)](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html)
* [meson (open source build system)](https://mesonbuild.com)
