#include "./webrtc/bindings.h"

#include "avformat.h"

#include "libavutil/thread.h"

typedef struct
{
    AVFormatContext *ctx;

    pthread_mutex_t write_mutex;
    int64_t audio_timestamp;
    int64_t video_timestamp;

    OBSWebRTCOutput *obsrtc;
} WHIPMuxContext;

static void whip_deinit(AVFormatContext *s)
{
    WHIPMuxContext *output = s->priv_data;
    if (output->obsrtc)
        obs_webrtc_output_free(output->obsrtc);
    pthread_mutex_destroy(&output->write_mutex);
    av_free(output);
}

static int whip_write_header(AVFormatContext *s)
{
    WHIPMuxContext *output = s->priv_data;
    const char *key = "";

    output->obsrtc = obs_webrtc_output_new();
    if (!output->obsrtc)
    {
        return AVERROR_INVALIDDATA;
    }

    // TODO: support stream key authorization header
    obs_webrtc_output_connect(output->obsrtc, s->url, key);

    return 0;
}

static void webrtc_output_close_unsafe(WHIPMuxContext *output)
{
    if (output)
    {
        obs_webrtc_output_close(output->obsrtc);
        obs_webrtc_output_free(output->obsrtc);
        output->obsrtc = NULL;
    }
}

static int whip_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    WHIPMuxContext *output = s->priv_data;
    int64_t duration = 0;
    bool is_audio = false;
    int codec_type = s->streams[pkt->stream_index]->codecpar->codec_type;

    if (codec_type == AVMEDIA_TYPE_VIDEO)
    {
        duration = pkt->dts - output->video_timestamp;
        output->video_timestamp = pkt->dts;
    }

    if (codec_type == AVMEDIA_TYPE_AUDIO)
    {
        is_audio = true;
        duration = pkt->dts - output->audio_timestamp;
        output->audio_timestamp = pkt->dts;
    }

    pthread_mutex_lock(&output->write_mutex);
    if (output->obsrtc)
    {
        if (!obs_webrtc_output_write(output->obsrtc, pkt->data, pkt->size, duration, is_audio))
        {
            // For now, all write errors are treated as connectivity issues that cannot be recovered from
            webrtc_output_close_unsafe(output);
        }
    }
    pthread_mutex_unlock(&output->write_mutex);
    return 0;
}

static int whip_write_trailer(AVFormatContext *s)
{
    WHIPMuxContext *output = s->priv_data;

    pthread_mutex_lock(&output->write_mutex);
    webrtc_output_close_unsafe(output);
    pthread_mutex_unlock(&output->write_mutex);
    return 0;
}

static int whip_init(struct AVFormatContext *s)
{
    WHIPMuxContext *output = s->priv_data;

    output->ctx = s;
    output->obsrtc = NULL;

    if (pthread_mutex_init(&output->write_mutex, NULL) != 0)
        goto fail;

    return 0;
fail:
    pthread_mutex_destroy(&output->write_mutex);
    av_free(output);
    return 0;
}

static const AVClass whip_muxer_class = {
    .class_name = "whip/webm muxer",
    .item_name = av_default_item_name,
    .version = LIBAVUTIL_VERSION_INT,
};

// exposed as a muxer because there's no valid muxers.
const AVOutputFormat ff_whip_muxer = {
    .name = "whip",
    .long_name = NULL_IF_CONFIG_SMALL("WebRTC HTTP Ingest Protocol"),
    .mime_type = "video/x-whip",
    .extensions = "whip",
    .priv_data_size = sizeof(WHIPMuxContext),
    .audio_codec = AV_CODEC_ID_OPUS,
    .video_codec = AV_CODEC_ID_H264,
    .init = whip_init,
    .deinit = whip_deinit,
    .write_header = whip_write_header,
    .write_packet = whip_write_packet,
    .write_trailer = whip_write_trailer,
    .flags = AVFMT_NOFILE,
    .priv_class = &whip_muxer_class,
};
