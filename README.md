# kevmo314/ffmpeg-whip

It's ffmpeg with [WHIP](https://datatracker.ietf.org/doc/draft-ietf-wish-whip/) support!

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