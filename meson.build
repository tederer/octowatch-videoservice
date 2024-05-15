project('video-service', 'c', 'cpp',
  version : '0.1',
  default_options : ['werror=true', 'warning_level=3', 'cpp_std=c++17', 'c_std=c11', 'buildtype=release'])

headersDir        = include_directories(['src/h'])

libcamera_dep     = dependency('libcamera', required : true)
libjpeg_dep       = dependency('libjpeg',   required : true)

video_service_src = [
   'src/cpp/Main.cpp',
   'src/cpp/Camera.cpp',
   'src/cpp/CameraCapabilities.cpp',
   'src/cpp/CameraControl.cpp',
   'src/cpp/DmaHeap.cpp',
   'src/cpp/Logging.cpp',
   'src/cpp/SingleThreadedExecutor.cpp',
   'src/cpp/H264Encoder.cpp',
   'src/cpp/HardwareJpegEncoder.cpp',
   'src/cpp/CpuJpegEncoder.cpp',
   'src/cpp/MultipartJpegHttpStream.cpp',
   'src/cpp/H264Stream.cpp',
   'src/cpp/network/Connection.cpp',
   'src/cpp/network/TcpConnection.cpp',
   'src/cpp/network/TcpServer.cpp',
   'src/cpp/RemoteControl.cpp',
   'src/cpp/SystemTemperature.cpp',
   'src/cpp/StringUtils.cpp']

video_service_dep = [libcamera_dep, libjpeg_dep]

cpp_arguments = ['-pedantic', '-Wno-unused-parameter', '-faligned-new']

add_project_arguments(cpp_arguments, language : 'cpp')

executable(
   'video_service', 
   video_service_src, 
   dependencies : video_service_dep,
   include_directories : headersDir)
