#ifndef FFMPEG_UTILS_H
#define FFMPEG_UTILS_H

#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int video_count;
    int audio_count;
    int subtitle_count;
    int error;    // 0 = OK, 1 = Error
} TrackInfo;

// 1. Function to inspect the MKV file
TrackInfo get_track_counts(const char* filename);

// 2. Function to generate the dynamic FFmpeg command
// Returns 0 on success, -1 on failure
int generate_hls_with_tracks(const char* mkv_path, const char* hls_dir);

#endif    // FFMPEG_UTILS_H