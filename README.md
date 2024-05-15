# octowatch-videoservice

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