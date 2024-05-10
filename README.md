# octowatch-videoservice

video service of the OctoWatch underwater camera

## Environment variables

|variable              |range                               |default|description         |
|----------------------|------------------------------------|-------|--------------------|
|OCTOWATCH_LOG_LEVEL   | [DEBUG, INFO, WARNING, ERROR, OFF] | INFO  | log level          |
|OCTOWATCH_JPEG_QUALITY| integer in the range [0, 100]      | 95    | JPEG image quality |

## References

* [libcamera (open source camera stack)](https://libcamera.org)
* [libjpeg-turbo (JPEG image codec that uses SIMD instructions)](https://www.libjpeg-turbo.org)
* [Boost.Asio (cross-platform C++ library for network and low-level I/O programming)](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html)
* [meson (open source build system)](https://mesonbuild.com)