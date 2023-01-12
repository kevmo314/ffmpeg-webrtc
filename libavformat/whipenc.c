#include <stdint.h>

#include "config_components.h"
#include "./webrtc/bindings.h"

#include "av1.h"
#include "avc.h"
#include "hevc.h"
#include "avformat.h"
#include "avio_internal.h"
#include "avlanguage.h"
#include "dovi_isom.h"
#include "flacenc.h"
#include "internal.h"
#include "isom.h"
#include "mux.h"
#include "riff.h"
#include "version.h"
#include "vorbiscomment.h"
#include "wv.h"

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/crc.h"
#include "libavutil/dict.h"
#include "libavutil/intfloat.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/lfg.h"
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/random_seed.h"
#include "libavutil/rational.h"
#include "libavutil/samplefmt.h"
#include "libavutil/stereo3d.h"

#include "libavcodec/av1.h"
#include "libavcodec/codec_desc.h"
#include "libavcodec/xiph.h"
#include "libavcodec/mpeg4audio.h"
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

    return output;
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

static int whip_query_codec(enum AVCodecID codec_id, int std_compliance)
{
    enum AVMediaType type = avcodec_get_type(codec_id);
    // support any audio and video for now.
    if (type == AVMEDIA_TYPE_VIDEO || type == AVMEDIA_TYPE_AUDIO)
        return 1;

    return 0;
}

// exposed as a muxer because there's no valid muxers.
const AVOutputFormat ff_whip_muxer = {
    .name = "whip",
    .long_name = NULL_IF_CONFIG_SMALL("WebRTC HTTP Ingest Protocol"),
    .mime_type = "video/x-whip",
    .extensions = "whip",
    .priv_data_size = sizeof(WHIPMuxContext),
    .audio_codec = CONFIG_LIBVORBIS_ENCODER ? AV_CODEC_ID_VORBIS : AV_CODEC_ID_AC3,
    .video_codec = CONFIG_LIBX264_ENCODER ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG4,
    .init = whip_init,
    .deinit = whip_deinit,
    .write_header = whip_write_header,
    .write_packet = whip_write_packet,
    .write_trailer = whip_write_trailer,
    .flags = AVFMT_NOFILE | AVFMT_GLOBALHEADER,
    .query_codec = whip_query_codec,
    .priv_class = &whip_muxer_class,
};
