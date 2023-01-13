# kevmo314/ffmpeg-whip

It's ffmpeg with WebRTC support! This fork uses [webrtc-rs](https://github.com/webrtc-rs/webrtc) to publish WebRTC streams using the [WHIP](https://datatracker.ietf.org/doc/draft-ietf-wish-whip/) protocol.

## Compiling

Build the Rust library:

```
cd libavformat/webrtc && cargo build && cd -
```

Build FFmpeg:

```
./configure --extra-ldflags="-L$(pwd)/libavformat/webrtc/target/debug" --extra-libs="-lwebrtc_rs_lib"
make -j
```

## Running

A WHIP server is necessary. If you want one to test with, check out [TinyWHIP](https://github.com/kevmo314/tinywhip).

Then, run:

```
./ffmpeg  -re -stream_loop -1 -i ~/video.mp4 -c:v copy -c:a copy -f whip http://localhost:8080
```

Where `~/video.mp4` is an h264 + opus encoded video file. Some sample videos can be downloaded [here](https://test-videos.co.uk/bigbuckbunny/mp4-h264).