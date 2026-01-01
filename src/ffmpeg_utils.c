#include "ffmpeg_utils.h"

TrackInfo get_track_counts(const char* filename) {
    TrackInfo info = {0, 0, 0, 0};
    AVFormatContext* fmt_ctx = NULL;

    // Suppress FFmpeg logs
    av_log_set_level(AV_LOG_QUIET);

    // Open video file
    if(avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", filename);
        info.error = 1;
        return info;
    }

    // Retrieve stream information
    if(avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info\n");
        avformat_close_input(&fmt_ctx);
        info.error = 1;
        return info;
    }

    // Count streams
    for(unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if(fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.video_count++;
        } else if(fmt_ctx->streams[i]->codecpar->codec_type ==
                  AVMEDIA_TYPE_AUDIO) {
            info.audio_count++;
        } else if(fmt_ctx->streams[i]->codecpar->codec_type ==
                  AVMEDIA_TYPE_SUBTITLE) {
            info.subtitle_count++;
        }
    }

    avformat_close_input(&fmt_ctx);
    return info;
}

int generate_hls_with_tracks(const char* mkv_path, const char* hls_dir) {
    TrackInfo info = get_track_counts(mkv_path);

    if(info.error || info.video_count == 0) {
        return -1;
    }

    char cmd[4096];
    char var_stream_map[1024] = "";
    char map_args[1024] = "";

    // Step A: Build the Mapping Arguments
    // We want to map Video:0 and ALL audio tracks.
    // Example: "-map 0:v:0 -map 0:a:0 -map 0:a:1"

    strcpy(map_args, "-map 0:v:0");    // Always map the first video track

    // Map every audio track found
    for(int i = 0; i < info.audio_count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), " -map 0:a:%d", i);
        strcat(map_args, buf);
    }

    // Step B: Build the var_stream_map string
    // This tells HLS how to pair them.
    // We want pairs like: "v:0,a:0 v:0,a:1 v:0,a:2"
    // This creates a separate stream for every audio language, all sharing
    // video track 0.

    for(int i = 0; i < info.audio_count; i++) {
        char buf[64];
        // v:0 refers to the first mapped video stream
        // a:i refers to the i-th mapped audio stream
        snprintf(buf, sizeof(buf), "v:0,a:%d ", i);
        strcat(var_stream_map, buf);
    }

    // Step C: Construct the final command
    snprintf(cmd,
             sizeof(cmd),
             "ffmpeg -i \"%s\" "
             "%s "           // Insert the -map arguments
             "-c:v copy "    // Copy video (fast)
             "-c:a aac "     // Convert audio to AAC (compatible)
             "-f hls "
             "-hls_time 10 "
             "-hls_list_size 0 "
             "-hls_segment_filename \"%s/segment_%%v_%%03d.ts\" "
             "-master_pl_name master.m3u8 "
             "-var_stream_map \"%s\" "    // Insert the stream map
             "\"%s/stream_%%v.m3u8\" > /dev/null 2>&1",
             mkv_path,
             map_args,
             hls_dir,
             var_stream_map,
             hls_dir);

    printf("[HLS] Generating streams... Video: %d, Audio: %d\n",
           info.video_count,
           info.audio_count);

    int ret = system(cmd);
    return (ret == 0) ? 0 : -1;
}