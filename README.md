# kevmo314/ffmpeg-webrtc

It's ffmpeg with WebRTC support! This fork uses [webrtc-rs](https://github.com/webrtc-rs/webrtc) to publish WebRTC streams using the [WHIP](https://datatracker.ietf.org/doc/draft-ietf-wish-whip/) protocol.

This work is inspired by a [pull request](https://github.com/obsproject/obs-studio/pull/7926) to add WebRTC support to OBS Studio, which also did most of the heavy lifting for this implementation.

This work is sponsored by [Muxable](https://www.muxable.com/).

## Compiling

Build FFmpeg:

```
./configure
make -j
```

The build step requires that Rust is installed, as it will call `cargo build` to build the WebRTC bindings.

## Running

A WHIP server is necessary. If you want one to test with, check out [TinyWHIP](https://github.com/kevmo314/tinywhip).

Then, run:

```
./ffmpeg  -re -stream_loop -1 -i ~/video.mp4 -c:v copy -c:a copy -f whip http://localhost:8080
```

Where `~/video.mp4` is an h264 + opus encoded video file. Some sample videos can be downloaded [here](https://test-videos.co.uk/bigbuckbunny/mp4-h264).
