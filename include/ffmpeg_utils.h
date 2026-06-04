#ifndef FFMPEG_UTILS_H
#define FFMPEG_UTILS_H

#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <stdio.h>
#include <string.h>

#define MAX_TRACKS 32

/**
 * @struct StreamMeta
 * @brief Metadata for one audio or subtitle stream.
 *
 * Stores the stream language tag, display title, and the original stream
 * index within the input media file.
 */
typedef struct {
    char lang[4];
    char title[64];
    int index;
} StreamMeta;

/**
 * @struct TrackInfo
 * @brief Summary of media streams detected in a file.
 *
 * This structure is returned by get_track_counts() and is used by the HLS
 * generation path to decide how to map audio, subtitle, and video tracks.
 */
typedef struct {
    int video_count;       //!< Number of video streams found.
    int audio_count;       //!< Number of audio streams found.
    int subtitle_count;    //!< Number of subtitle streams that can be
                           //!< extracted.
    int error;             //!< Non-zero when stream analysis fails.
    StreamMeta
        audio[MAX_TRACKS];    //!< Audio stream metadata up to MAX_TRACKS.
    StreamMeta
        subs[MAX_TRACKS];    //!< Subtitle stream metadata up to MAX_TRACKS.
} TrackInfo;

/**
 * @brief Inspect an input media file and collect stream metadata.
 *
 * Opens the file with FFmpeg, reads stream information, and fills a
 * TrackInfo structure with counts and per-track metadata.
 *
 * @param filename Path to the input media file.
 * @return A TrackInfo object. The error field is non-zero on failure.
 */
TrackInfo get_track_counts(const char* filename);

/**
 * @brief Generate an HLS output directory with track-aware stream mapping.
 *
 * Uses FFmpeg to create HLS playlists and segment files while also extracting
 * subtitle tracks to WebVTT sidecar files.
 *
 * @param mkv_path Path to the source Matroska/MP4 input file.
 * @param hls_dir  Target directory where the HLS assets will be written.
 * @return Exit status returned by the underlying FFmpeg/system command.
 */
int generate_hls_with_tracks(const char* mkv_path, const char* hls_dir);

#endif    // FFMPEG_UTILS_H