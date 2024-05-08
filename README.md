https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/main/libjpeg.txt




YUV Image Format Notes (https://rawcdn.githack.com/libjpeg-turbo/libjpeg-turbo/2.1.x/doc/html/group___turbo_j_p_e_g.html)

Technically, the JPEG format uses the YCbCr colorspace (which is technically not a colorspace but a color transform), but per the convention of the digital video community, the TurboJPEG API uses "YUV" to refer to an image format consisting of Y, Cb, and Cr image planes.

Each plane is simply a 2D array of bytes, each byte representing the value of one of the components (Y, Cb, or Cr) at a particular location in the image. The width and height of each plane are determined by the image width, height, and level of chrominance subsampling. The luminance plane width is the image width padded to the nearest multiple of the horizontal subsampling factor (1 in the case of 4:4:4, grayscale, or 4:4:0; 2 in the case of 4:2:2 or 4:2:0; 4 in the case of 4:1:1.) Similarly, the luminance plane height is the image height padded to the nearest multiple of the vertical subsampling factor (1 in the case of 4:4:4, 4:2:2, grayscale, or 4:1:1; 2 in the case of 4:2:0 or 4:4:0.) This is irrespective of any additional padding that may be specified as an argument to the various YUV functions. The chrominance plane width is equal to the luminance plane width divided by the horizontal subsampling factor, and the chrominance plane height is equal to the luminance plane height divided by the vertical subsampling factor.

For example, if the source image is 35 x 35 pixels and 4:2:2 subsampling is used, then the luminance plane would be 36 x 35 bytes, and each of the chrominance planes would be 18 x 35 bytes. If you specify a row alignment of 4 bytes on top of this, then the luminance plane would be 36 x 35 bytes, and each of the chrominance planes would be 20 x 35 bytes. 



https://github.com/libjpeg-turbo/libjpeg-turbo/tree/main

tjCompressFromYUV


tjInitCompress()
# octowatch-videoservice
