#define _DEFAULT_SOURCE
#include "ffmpeg_utils.h"

#include <ctype.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/log.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: Ensures string is safe for FFmpeg command (only alphanumeric +
// underscores)
void sanitize_name(char* dst, const char* src, size_t max_len) {
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j < max_len - 1; i++) {
        unsigned char c = src[i];
        if(isalnum(c)) {
            dst[j++] = c;
        } else {
            dst[j++] = '_';    // Replace everything else with underscore
        }
    }
    dst[j] = '\0';

    // Remove trailing underscores
    while(j > 0 && dst[j - 1] == '_') {
        dst[--j] = '\0';
    }

    if(j == 0) strcpy(dst, "und");
}

TrackInfo get_track_counts(const char* filename) {
    TrackInfo info;
    memset(&info, 0, sizeof(TrackInfo));

    AVFormatContext* fmt_ctx = NULL;
    av_log_set_level(AV_LOG_QUIET);    // Keep FFmpeg library quiet

    if(avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) {
        info.error = 1;
        return info;
    }
    if(avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        avformat_close_input(&fmt_ctx);
        info.error = 1;
        return info;
    }

    int a_idx = 0;
    int s_idx = 0;

    for(unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream* st = fmt_ctx->streams[i];
        AVDictionaryEntry* lang =
            av_dict_get(st->metadata, "language", NULL, 0);

        if(st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.video_count++;
        } else if(st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if(a_idx < MAX_TRACKS) {
                // 1. DIRECTLY USE LANGUAGE CODE (eng, jpn)
                if(lang) {
                    strncpy(info.audio[a_idx].lang, lang->value, 3);
                    info.audio[a_idx].lang[3] = '\0';
                } else {
                    strcpy(info.audio[a_idx].lang, "und");
                }

                // Set Title = Language Code (e.g. "jpn")
                strcpy(info.audio[a_idx].title, info.audio[a_idx].lang);

                // Sanitize just in case
                sanitize_name(
                    info.audio[a_idx].title, info.audio[a_idx].title, 63);

                // Handle Duplicates (eng, eng_2, eng_3)
                int dup_count = 1;
                for(int k = 0; k < a_idx; k++) {
                    if(strncmp(info.audio[k].title,
                               info.audio[a_idx].title,
                               63) == 0) {
                        dup_count++;
                    }
                }
                if(dup_count > 1) {
                    char suffix[16];
                    snprintf(suffix, sizeof(suffix), "_%d", dup_count);
                    strcat(info.audio[a_idx].title, suffix);
                }

                a_idx++;
            }
            info.audio_count++;
        } else if(st->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            if(s_idx < MAX_TRACKS) {
                if(lang) {
                    strncpy(info.subs[s_idx].lang, lang->value, 3);
                    info.subs[s_idx].lang[3] = '\0';
                } else {
                    strcpy(info.subs[s_idx].lang, "und");
                }

                strcpy(info.subs[s_idx].title, info.subs[s_idx].lang);
                sanitize_name(
                    info.subs[s_idx].title, info.subs[s_idx].title, 63);

                int dup_count = 1;
                for(int k = 0; k < s_idx; k++) {
                    if(strncmp(
                           info.subs[k].title, info.subs[s_idx].title, 63) == 0)
                        dup_count++;
                }
                if(dup_count > 1) {
                    char suffix[16];
                    snprintf(suffix, sizeof(suffix), "_%d", dup_count);
                    strcat(info.subs[s_idx].title, suffix);
                }

                s_idx++;
            }
            info.subtitle_count++;
        }
    }

    avformat_close_input(&fmt_ctx);
    return info;
}

static int run_ffmpeg_command(const char* mkv_path,
                              const char* hls_dir,
                              TrackInfo info,
                              int include_subs) {
    char cmd[16384];
    char var_stream_map[4096] = "";
    char map_args[2048] = "";
    char abs_mkv_path[PATH_MAX];

    if(!realpath(mkv_path, abs_mkv_path)) strcpy(abs_mkv_path, mkv_path);

    strcpy(map_args, "-map 0:v:0");
    for(int i = 0; i < info.audio_count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), " -map 0:a:%d", i);
        strcat(map_args, buf);
    }
    if(include_subs) {
        for(int i = 0; i < info.subtitle_count; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), " -map 0:s:%d", i);
            strcat(map_args, buf);
        }
    }

    // STREAM MAP
    for(int i = 0; i < info.audio_count; i++) {
        char buf[256];
        snprintf(buf,
                 sizeof(buf),
                 "a:%d,agroup:audio,language:%s,name:%s,default:%s ",
                 i,
                 info.audio[i].lang,
                 info.audio[i].title,
                 (i == 0 ? "yes" : "no"));
        strcat(var_stream_map, buf);
    }

    if(include_subs) {
        for(int i = 0; i < info.subtitle_count; i++) {
            char buf[256];
            snprintf(buf,
                     sizeof(buf),
                     "s:%d,sgroup:subs,language:%s,name:%s ",
                     i,
                     info.subs[i].lang,
                     info.subs[i].title);
            strcat(var_stream_map, buf);
        }
    }

    strcat(var_stream_map, "v:0,agroup:audio");
    if(include_subs) strcat(var_stream_map, ",sgroup:subs");

    // COMMAND (Restored > /dev/null 2>&1 to be silent)
    snprintf(cmd,
             sizeof(cmd),
             "ffmpeg -i \"%s\" %s "
             "-c:v copy -c:a aac %s "
             "-f hls -hls_time 10 -hls_list_size 0 "
             "-hls_playlist_type vod "
             "-hls_flags independent_segments "
             "-hls_segment_filename \"%s/segment_%%v_%%03d.ts\" "
             "-master_pl_name master.m3u8 "
             "-var_stream_map \"%s\" "
             "\"%s/stream_%%v.m3u8\" > /dev/null 2>&1",
             abs_mkv_path,
             map_args,
             include_subs ? "-c:s webvtt" : "",
             hls_dir,
             var_stream_map,
             hls_dir);

    // Removed the printf("[DEBUG] Command: ...")

    return system(cmd);
}

int generate_hls_with_tracks(const char* mkv_path, const char* hls_dir) {
    TrackInfo info = get_track_counts(mkv_path);
    if(info.error || info.video_count == 0) return -1;

    // Try with subtitles first
    if(info.subtitle_count > 0) {
        if(run_ffmpeg_command(mkv_path, hls_dir, info, 1) == 0) return 0;
        // Subtitles failed (likely PGS/ASS). Retry silently without.
    }
    return run_ffmpeg_command(mkv_path, hls_dir, info, 0);
}