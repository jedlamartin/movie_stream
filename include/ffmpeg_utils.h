#ifndef FFMPEG_UTILS_H
#define FFMPEG_UTILS_H

#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <stdio.h>
#include <string.h>

#define MAX_TRACKS 32

typedef struct {
    char lang[4];
    char title[64];
} StreamMeta;

typedef struct {
    int video_count;
    int audio_count;
    int subtitle_count;
    int error;
    StreamMeta audio[MAX_TRACKS];
    StreamMeta subs[MAX_TRACKS];
} TrackInfo;

TrackInfo get_track_counts(const char* filename);
int generate_hls_with_tracks(const char* mkv_path, const char* hls_dir);

#endif    // FFMPEG_UTILS_H