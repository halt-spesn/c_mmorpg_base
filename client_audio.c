#include "client.h"

void play_next_track(void) {
    if (music_count == 0) return;
    if (bgm) Mix_FreeMusic(bgm);
    current_track++;
    if (current_track >= music_count) current_track = 0;
    char path[256];
    char music_file[256];
    snprintf(music_file, 256, "music/%s", music_playlist[current_track]);
    get_path(path, music_file, 0);
    bgm = Mix_LoadMUS(path);
    if (bgm) Mix_PlayMusic(bgm, 1);
}

void init_audio(void) {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    
    #ifdef __ANDROID__
    const char* android_music_files[] = {"1.mp3", NULL};
    for(int i = 0; android_music_files[i] != NULL && music_count < 20; i++) {
        char path[256];
        char music_file[256];
        snprintf(music_file, 256, "music/%s", android_music_files[i]);
        get_path(path, music_file, 0);
        SDL_RWops *rw = SDL_RWFromFile(path, "rb");
        if (rw) {
            SDL_RWclose(rw);
            strncpy(music_playlist[music_count++], android_music_files[i], 63);
        }
    }
    #else
    DIR *d; struct dirent *dir;
    char music_dir_path[256];
    get_path(music_dir_path, "music", 0);
    d = opendir(music_dir_path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".ogg") || strstr(dir->d_name, ".wav")) {
                if (music_count < 20) strncpy(music_playlist[music_count++], dir->d_name, 63);
            }
        }
        closedir(d);
    }
    #endif
    Mix_VolumeMusic(music_volume);
}
