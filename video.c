#include "video.h"

char** generate_subtitles(const char* video_path){
	AVFormatContext* fmt_ctx = NULL;
	if (avformat_open_input(&fmt_ctx, video_path, NULL, NULL) != 0 || avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "ffmpeg opening failed");
		return NULL;
	}

	char subtitle_path[PATH_MAX];
	int length = strchr(video_path, '.') - video_path;
	strncpy(subtitle_path, video_path, length);
	subtitle_path[length] = '\0';


	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
		if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			char tmp_path[PATH_MAX];
			strncpy(tmp_path, subtitle_path, PATH_MAX - 1 - 3);

			AVDictionaryEntry* tag = av_dict_get(fmt_ctx->streams[i]->metadata, "language", NULL, 0);
			if (tag == NULL) {
				strcat(tmp_path, ".vtt");
			}
			else {
				strcat(tmp_path, ".");
				strcat(tmp_path, tag->value);
				strcat(tmp_path, ".vtt");
			}

			int subtitle_fd = -1;
			if ((subtitle_fd = open(tmp_path, O_CREAT)) < 0) {
				fprintf(stderr, "Could not create subtitle file '%s'!", tmp_path);
				return NULL;
			}

			// Write the subtitle file

		}
	}
}
