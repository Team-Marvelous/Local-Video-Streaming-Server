# Local-Video-Streaming-Server
This c code performs the function of reading a video stored in the server and streaming it to a proxy server (nginx in our case) while changing the pts and dts of each packet it reads.
Here we have used the 'libav' library which is the c based library for ffmpeg to handle the video streaminging. 
