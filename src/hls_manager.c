#define _POSIX_C_SOURCE 200809L
#include "hls_manager.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ffmpeg_utils.h"

typedef struct {
    char mkv_path[PATH_MAX];
    char hls_dir[PATH_MAX];
} ConversionTask;

int has_any_ts_file(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if(!dir) return 0;
    struct dirent* entry;
    int found = 0;
    while((entry = readdir(dir)) != NULL) {
        if(strstr(entry->d_name, ".ts") != NULL ||
           strstr(entry->d_name, ".m4s") != NULL) {
            found = 1;
            break;
        }
    }
    closedir(dir);
    return found;
}

void* conversion_worker(void* arg) {
    ConversionTask* task = (ConversionTask*) arg;
    printf("[Transcoder] Starting background worker for: %s\n", task->mkv_path);

    int ret = generate_hls_with_tracks(task->mkv_path, task->hls_dir);

    char lock_file[PATH_MAX + 32];
    snprintf(lock_file, sizeof(lock_file), "%s/.processing", task->hls_dir);
    unlink(lock_file);

    if(ret != 0) {
        char error_file[PATH_MAX + 32];
        snprintf(error_file, sizeof(error_file), "%s/error.txt", task->hls_dir);
        FILE* f = fopen(error_file, "w");
        if(f) {
            fprintf(f, "Failed: %d\n", ret);
            fclose(f);
        }
    } else {
        printf("[Transcoder] Finished processing: %s\n", task->mkv_path);
    }

    free(task);
    return NULL;
}

int check_or_start_hls(const char* mkv_path, char* out_hls_dir) {
    snprintf(out_hls_dir, PATH_MAX, "%s.hls", mkv_path);

    char master_pl[PATH_MAX + 32];
    snprintf(master_pl, sizeof(master_pl), "%s/master.m3u8", out_hls_dir);

    char lock_file[PATH_MAX + 32];
    snprintf(lock_file, sizeof(lock_file), "%s/.processing", out_hls_dir);

    char error_file[PATH_MAX + 32];
    snprintf(error_file, sizeof(error_file), "%s/error.txt", out_hls_dir);

    if(file_exists(error_file)) return -1;
    if(file_exists(master_pl) && has_any_ts_file(out_hls_dir)) return 0;
    if(file_exists(lock_file)) return 1;

    if(file_exists(out_hls_dir)) {
        char pid_file[PATH_MAX + 32];
        snprintf(pid_file, sizeof(pid_file), "%s/.pid", out_hls_dir);
        FILE* pf = fopen(pid_file, "r");
        if(pf) {
            int pid = 0;
            if(fscanf(pf, "%d", &pid) == 1 && pid > 0) {
                char kill_cmd[64];
                snprintf(kill_cmd,
                         sizeof(kill_cmd),
                         "kill -9 %d > /dev/null 2>&1",
                         pid);
                system(kill_cmd);
            }
            fclose(pf);
        }

        char cmd[PATH_MAX + 16];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", out_hls_dir);
        system(cmd);
    }

    mkdir(out_hls_dir, 0755);

    FILE* f = fopen(lock_file, "w");
    if(f) fclose(f);

    char access_file[PATH_MAX + 32];
    snprintf(access_file, sizeof(access_file), "%s/.access", out_hls_dir);
    FILE* af = fopen(access_file, "w");
    if(af) fclose(af);

    ConversionTask* task = malloc(sizeof(ConversionTask));
    snprintf(task->mkv_path, PATH_MAX, "%s", mkv_path);
    snprintf(task->hls_dir, PATH_MAX, "%s", out_hls_dir);

    pthread_t thread;
    if(pthread_create(&thread, NULL, conversion_worker, task) == 0) {
        pthread_detach(thread);
        return 1;
    }

    free(task);
    return -1;
}

int handle_hls_request(int client_fd,
                       Header* header,
                       const char* abs_path,
                       const char* content_type_str) {
    if(strcmp(header->query, "mode=hls_ping") == 0) {
        char hls_dir[PATH_MAX + 16];
        snprintf(hls_dir, sizeof(hls_dir), "%s.hls", abs_path);

        char access_file[PATH_MAX + 32];
        snprintf(access_file, sizeof(access_file), "%s/.access", hls_dir);

        FILE* f = fopen(access_file, "w");
        if(f) fclose(f);

        char* ok = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
        write(client_fd, ok, strlen(ok));
        return 1;
    }

    if(strstr(content_type_str, "video") && strstr(abs_path, ".mkv") &&
       !header->range_request && strcmp(header->query, "mode=hls") == 0) {
        char hls_dir[PATH_MAX];
        int status = check_or_start_hls(abs_path, hls_dir);
        char resp[BUFFER_SIZE * 4];

        const char* hls_folder_name = strrchr(hls_dir, '/');
        if(hls_folder_name) hls_folder_name++;
        else
            hls_folder_name = hls_dir;

        if(status == 1) {
            snprintf(
                resp,
                sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: text/html; "
                "charset=utf-8\r\nConnection: close\r\n\r\n"
                "<html><head><meta http-equiv='refresh' content='2'></head>"
                "<body "
                "style='background:#111;color:white;text-align:center;padding-"
                "top:20%%;font-family:sans-serif;'>"
                "<h1>Starting Transcoder...</h1><p>Buffering the first few "
                "seconds...</p></body></html>");
            write(client_fd, resp, strlen(resp));
        } else if(status == -1) {
            snprintf(resp,
                     sizeof(resp),
                     "HTTP/1.1 500 Error\r\nContent-Type: text/html; "
                     "charset=utf-8\r\nConnection: close\r\n\r\n"
                     "<html><body "
                     "style='background:#111;color:red;text-align:center;"
                     "padding-top:20%%;'>"
                     "<h1>Conversion Failed</h1><p>Check server "
                     "logs.</p></body></html>");
            write(client_fd, resp, strlen(resp));
        } else {
            char playlist_url[PATH_MAX + 128];
            snprintf(
                playlist_url, sizeof(playlist_url), "/%s/master.m3u8", hls_dir);

            TrackInfo info = get_track_counts(abs_path);
            char tracks_html[8192] = "";
            for(int i = 0; i < info.subtitle_count; i++) {
                char t_buf[PATH_MAX + 512];
                snprintf(t_buf,
                         sizeof(t_buf),
                         "<track src=\"/%s/sub_%d.vtt\" kind=\"subtitles\" "
                         "srclang=\"%s\" label=\"%s\">",
                         hls_dir,
                         i,
                         info.subs[i].lang,
                         info.subs[i].title);
                strcat(tracks_html, t_buf);
            }

            int n =
                snprintf(resp,
                         sizeof(resp),
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html; "
                         "charset=utf-8\r\nCache-Control: "
                         "no-cache\r\nConnection: close\r\n\r\n"
                         "<!DOCTYPE "
                         "html><html><head><title>Play</title><script "
                         "src=\"https://cdn.jsdelivr.net/npm/hls.js@latest\"></"
                         "script>"
                         "<style>body{background:#111;color:white;text-align:"
                         "center;font-family:sans-serif;} "
                         "select{padding:10px;margin:10px;background:#333;"
                         "color:white;border:1px solid #555;}</style></head>"
                         "<body><h2>%s</h2><div><label>Audio Track: <select "
                         "id='audioSelect'></select></label></div>"
                         "<video id='video' controls "
                         "style='width:80%%;max-width:1000px;margin-top:20px' "
                         "crossorigin='anonymous'>"
                         "%s"
                         "</video>"
                         "<script>"
                         "var v=document.getElementById('video');var src='%s';"
                         "setInterval(function() { fetch('?mode=hls_ping'); }, "
                         "30000);"
                         "if(Hls.isSupported()){"
                         "  var h=new Hls({autoStartLoad: false});"
                         "  h.loadSource(src);h.attachMedia(v);"
                         "  h.on(Hls.Events.MANIFEST_PARSED,function(){"
                         "    h.startLoad(0);"
                         "    v.play(); updateAudio();"
                         "  });"
                         "  h.on(Hls.Events.AUDIO_TRACKS_UPDATED, updateAudio);"
                         "  function updateAudio(){"
                         "    var as=document.getElementById('audioSelect'); "
                         "as.innerHTML='';"
                         "    h.audioTracks.forEach((t,i)=>{"
                         "      var o=document.createElement('option'); "
                         "o.value=i;"
                         "      var lang = t.lang || 'und';"
                         "      o.text = 'Track ' + (i+1) + ' - [' + lang + "
                         "']';"
                         "      if(i===h.audioTrack) o.selected=true; "
                         "as.add(o);"
                         "    });"
                         "  }"
                         "  "
                         "document.getElementById('audioSelect').onchange="
                         "function(){h.audioTrack=parseInt(this.value);};"
                         "}else "
                         "if(v.canPlayType('application/"
                         "vnd.apple.mpegurl')){v.src=src;}"
                         "</script></body></html>",
                         hls_folder_name,
                         tracks_html,
                         playlist_url);

            if(n > 0) write(client_fd, resp, n);
        }
        return 1;
    }
    return 0;
}

void* janitor_worker(void* arg) {
    (void) arg;
    while(1) {
        sleep(10);

        FILE* fp = popen("find . -type d -name \"*.hls\"", "r");
        if(!fp) continue;

        char hls_dir[PATH_MAX];
        while(fgets(hls_dir, sizeof(hls_dir), fp) != NULL) {
            hls_dir[strcspn(hls_dir, "\r\n")] = 0;    // Strip newline

            char access_file[PATH_MAX + 32];
            snprintf(access_file, sizeof(access_file), "%s/.access", hls_dir);

            struct stat st;
            if(stat(access_file, &st) == 0) {
                time_t now = time(NULL);

                if(now - st.st_mtime > 1800) {
                    printf(
                        "[Janitor] Connection lost for 30 minutes. Cleaning "
                        "up: %s\n",
                        hls_dir);

                    // FIX: Buffer increased here too
                    char pid_file[PATH_MAX + 32];
                    snprintf(pid_file, sizeof(pid_file), "%s/.pid", hls_dir);
                    FILE* pf = fopen(pid_file, "r");
                    if(pf) {
                        int pid = 0;
                        if(fscanf(pf, "%d", &pid) == 1 && pid > 0) {
                            char kill_cmd[64];
                            snprintf(kill_cmd,
                                     sizeof(kill_cmd),
                                     "kill -9 %d > /dev/null 2>&1",
                                     pid);
                            system(kill_cmd);
                        }
                        fclose(pf);
                    }

                    char rm_cmd[PATH_MAX + 16];
                    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", hls_dir);
                    system(rm_cmd);
                }
            }
        }
        pclose(fp);
    }
    return NULL;
}

void init_hls_janitor() {
    pthread_t thread;
    if(pthread_create(&thread, NULL, janitor_worker, NULL) == 0) {
        pthread_detach(thread);
        printf("[Manager] Janitor thread started.\n");
    }
}