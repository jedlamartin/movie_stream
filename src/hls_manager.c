#define _POSIX_C_SOURCE 200809L
#include "hls_manager.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ffmpeg_utils.h"

typedef struct {
    char mkv_path[PATH_MAX];
    char hls_dir[PATH_MAX];
} ConversionTask;

// Helper: Check if FFmpeg has created at least one .ts chunk yet
int has_any_ts_file(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if(!dir) return 0;
    struct dirent* entry;
    int found = 0;
    while((entry = readdir(dir)) != NULL) {
        if(strstr(entry->d_name, ".ts") != NULL) {
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

    // Remove the lock file to signal FFmpeg is done
    char lock_file[PATH_MAX + 16];
    snprintf(lock_file, sizeof(lock_file), "%s/.processing", task->hls_dir);
    unlink(lock_file);

    if(ret != 0) {
        char error_file[PATH_MAX + 16];
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

    char master_pl[PATH_MAX];
    snprintf(master_pl, sizeof(master_pl), "%s/master.m3u8", out_hls_dir);

    char lock_file[PATH_MAX];
    snprintf(lock_file, sizeof(lock_file), "%s/.processing", out_hls_dir);

    char error_file[PATH_MAX];
    snprintf(error_file, sizeof(error_file), "%s/error.txt", out_hls_dir);

    // If it completely failed previously, show the error state
    if(file_exists(error_file)) {
        return -1;    // ERROR
    }

    // If the master playlist AND at least one video chunk exist, we can start
    // streaming!
    if(file_exists(master_pl) && has_any_ts_file(out_hls_dir)) {
        return 0;    // READY
    }

    // If it's not ready, but the lock file is there, FFmpeg is actively working
    // on it.
    if(file_exists(lock_file)) {
        return 1;    // PROCESSING
    }

    // If neither exist, we need to start FFmpeg. Clean up any broken folders
    // first.
    if(file_exists(out_hls_dir)) {
        char cmd[PATH_MAX + 16];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", out_hls_dir);
        system(cmd);
    }

#ifdef _WIN32
    _mkdir(out_hls_dir);
#else
    mkdir(out_hls_dir, 0755);
#endif

    // Create lock file
    FILE* f = fopen(lock_file, "w");
    if(f) fclose(f);

    // Create the initial access file so the Janitor knows when we started
    char access_file[PATH_MAX];
    snprintf(access_file, sizeof(access_file), "%s/.access", out_hls_dir);
    FILE* af = fopen(access_file, "w");
    if(af) fclose(af);

    // Start background thread
    ConversionTask* task = malloc(sizeof(ConversionTask));
    snprintf(task->mkv_path, PATH_MAX, "%s", mkv_path);
    snprintf(task->hls_dir, PATH_MAX, "%s", out_hls_dir);

    pthread_t thread;
    if(pthread_create(&thread, NULL, conversion_worker, task) == 0) {
        pthread_detach(thread);
        return 1;    // PROCESSING
    }

    free(task);
    return -1;    // ERROR
}

int handle_hls_request(int client_fd,
                       Header* header,
                       const char* abs_path,
                       const char* content_type_str) {
    if(strstr(content_type_str, "video") && strstr(abs_path, ".mkv") &&
       !header->range_request && strcmp(header->query, "mode=hls") == 0) {
        char hls_dir[PATH_MAX];
        int status = check_or_start_hls(abs_path, hls_dir);

        char resp[BUFFER_SIZE * 4];

        if(status == 1) {    // PROCESSING
            snprintf(
                resp,
                sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
                "close\r\n\r\n"
                "<html><head><meta http-equiv='refresh' content='2'></head>"
                "<body "
                "style='background:#111;color:white;text-align:center;padding-"
                "top:20%%;font-family:sans-serif;'>"
                "<h1>Starting Transcoder...</h1><p>Buffering the first few "
                "seconds...</p></body></html>");
            write(client_fd, resp, strlen(resp));
        } else if(status == -1) {    // ERROR
            snprintf(resp,
                     sizeof(resp),
                     "HTTP/1.1 500 Error\r\nContent-Type: "
                     "text/html\r\nConnection: close\r\n\r\n"
                     "<html><body "
                     "style='background:#111;color:red;text-align:center;"
                     "padding-top:20%%;'>"
                     "<h1>Conversion Failed</h1><p>Check server "
                     "logs.</p></body></html>");
            write(client_fd, resp, strlen(resp));
        } else {    // READY
            char playlist_url[PATH_MAX + 128];
            snprintf(
                playlist_url, sizeof(playlist_url), "/%s/master.m3u8", hls_dir);

            int n = snprintf(
                resp,
                sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nCache-Control: "
                "no-cache\r\nConnection: close\r\n\r\n"
                "<!DOCTYPE html><html><head><title>Play</title><script "
                "src=\"https://cdn.jsdelivr.net/npm/hls.js@latest\"></script>"
                "<style>body{background:#111;color:white;text-align:center;"
                "font-family:sans-serif;} "
                "select{padding:10px;margin:10px;background:#333;color:white;"
                "border:1px solid #555;}</style></head>"
                "<body><h2>%s</h2><div><label>Audio: <select "
                "id='audioSelect'></select></label><label>Subs: <select "
                "id='subSelect'></select></label></div>"
                "<video id='video' controls "
                "style='width:80%%;max-width:1000px;margin-top:20px'></video>"
                "<script>"
                "var v=document.getElementById('video');var src='%s';"
                "if(Hls.isSupported()){var h=new "
                "Hls();h.loadSource(src);h.attachMedia(v);"
                "h.on(Hls.Events.MANIFEST_PARSED,function(){v.play();"
                "updateTracks();});"
                "h.on(Hls.Events.AUDIO_TRACKS_UPDATED, updateTracks);"
                "h.on(Hls.Events.SUBTITLE_TRACKS_UPDATED, updateTracks);"
                "function updateTracks(){"
                "var as=document.getElementById('audioSelect');as.innerHTML='';"
                "h.audioTracks.forEach((t,i)=>{var "
                "o=document.createElement('option');o.value=i;o.text=t.name||t."
                "lang||'Track "
                "'+(i+1);if(i===h.audioTrack)o.selected=true;as.add(o);});"
                "var ss=document.getElementById('subSelect');ss.innerHTML='';"
                "var "
                "off=document.createElement('option');off.value=-1;off.text='"
                "Off';if(h.subtitleTrack===-1)off.selected=true;ss.add(off);"
                "h.subtitleTracks.forEach((t,i)=>{var "
                "o=document.createElement('option');o.value=i;o.text=t.name||t."
                "lang||'Sub "
                "'+(i+1);if(i===h.subtitleTrack)o.selected=true;ss.add(o);});}"
                "document.getElementById('audioSelect').onchange=function(){h."
                "audioTrack=parseInt(this.value);};"
                "document.getElementById('subSelect').onchange=function(){h."
                "subtitleTrack=parseInt(this.value);};"
                "}else "
                "if(v.canPlayType('application/vnd.apple.mpegurl')){v.src=src;}"
                "</script></body></html>",
                abs_path,
                playlist_url);

            if(n > 0) write(client_fd, resp, n);
        }
        return 1;    // Signal to site.c that we handled the request
    }
    return 0;    // Not an HLS request
}

// --- PHASE 3 MAGIC: THE CLEANUP JANITOR ---
void* janitor_worker(void* arg) {
    (void) arg;
    while(1) {
        sleep(10);    // Check every 10 seconds

        DIR* dir = opendir(".");
        if(!dir) continue;

        struct dirent* entry;
        while((entry = readdir(dir)) != NULL) {
            // Look for any folder ending in .hls
            if(strstr(entry->d_name, ".hls") != NULL) {
                char access_file[PATH_MAX];
                snprintf(access_file,
                         sizeof(access_file),
                         "%s/.access",
                         entry->d_name);

                struct stat st;
                if(stat(access_file, &st) == 0) {
                    time_t now = time(NULL);
                    // If it has been more than 60 seconds since the browser
                    // asked for a chunk...
                    if(now - st.st_mtime > 60) {
                        printf("[Janitor] Connection lost. Cleaning up: %s\\n",
                               entry->d_name);

                        // 1. Read the PID file and assassinate FFmpeg
                        char pid_file[PATH_MAX];
                        snprintf(pid_file,
                                 sizeof(pid_file),
                                 "%s/.pid",
                                 entry->d_name);
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

                        // 2. Delete the directory
                        char rm_cmd[PATH_MAX + 16];
                        snprintf(rm_cmd,
                                 sizeof(rm_cmd),
                                 "rm -rf \"%s\"",
                                 entry->d_name);
                        system(rm_cmd);
                    }
                }
            }
        }
        closedir(dir);
    }
    return NULL;
}

void init_hls_janitor() {
    pthread_t thread;
    if(pthread_create(&thread, NULL, janitor_worker, NULL) == 0) {
        pthread_detach(thread);
        printf("[Manager] Janitor thread started.\\n");
    }
}