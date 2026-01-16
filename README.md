# FFMPEG-Media-Server

Small C++ example that reads a video stream with FFmpeg, decodes it, converts frames
to RGB24 rawvideo, and pushes the result to a streaming URL (RTSP/RTMP/RTP/UDP/TCP).

## Features
- Input from RTSP, RTMP, local files (MP4), and other FFmpeg-supported sources.
- Output as rawvideo RGB24 to RTSP/RTMP/RTP/UDP/TCP destinations.
- Simple single-binary build with CMake.

## Requirements
- CMake 3.15+
- C++14 compiler
- FFmpeg development libraries: `libavcodec`, `libavformat`, `libavutil`, `libavdevice`, `libswscale`

On Debian/Ubuntu, typical packages are:
```
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libavdevice-dev libswscale-dev
```

## Build
```
git clone https://github.com/matinfazel/FFMPEG-Media-Server.git
cd FFMPEG-Media-Server
mkdir build
cd build
cmake ..
make
```

The output binary is `test`.

## Usage
```
./test <input_url_or_file> <output_url>
```

Examples:
```
./test rtsp://user:pass@camera-ip/stream1 rtsp://localhost:8554/live
./test input.mp4 rtmp://localhost/live/stream
```

## Notes
- The output stream is always encoded as `rawvideo` in RGB24.
- The process stops after 150 frames in the current example code.
- Press `Ctrl+C` to pause/resume processing (SIGINT toggles a pause flag).
- A local file named `camera.mp4` is written as raw packet data for debugging.

## Limitations
- The current loop does not filter out non-video packets. Inputs with audio streams
  may cause decoder errors unless you adapt the code to handle stream selection.
- No audio is encoded or pushed.

## Troubleshooting
- If CMake cannot find FFmpeg headers or libraries, install the `-dev` packages
  or set `CMAKE_PREFIX_PATH` to your FFmpeg build.
- If the output URL is not one of `rtsp://`, `rtmp://`, `rtp://`, `udp://`, or `tcp://`,
  the pusher will report it as unsupported.
