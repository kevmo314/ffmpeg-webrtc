# kevmo314/ffmpeg-whip

It's ffmpeg with [WHIP](https://datatracker.ietf.org/doc/draft-ietf-wish-whip/) support!

## Compiling

```
./configure --extra-ldflags="-L./libavformat/webrtc/target/debug -lwebrtc_rs_lib"
make -j
```