#pragma once

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

typedef struct Subtitle {
	char language[16];
	char path[PATH_MAX];
} Subtitle;

Subtitle* generate_subtitles(const char* video_path);