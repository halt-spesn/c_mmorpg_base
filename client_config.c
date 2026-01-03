#include "client.h"

void get_path(char *out, const char *filename, int is_save_file) {
    #ifdef __ANDROID__
        if (is_save_file) {
            const char *internal_path = SDL_AndroidGetInternalStoragePath();
            if (internal_path) {
                snprintf(out, 256, "%s/%s", internal_path, filename);
            } else {
                strcpy(out, filename);
            }
        } else {
            strcpy(out, filename);
        }
    #elif defined(__APPLE__)
        if (is_save_file) {
            const char *home = getenv("HOME");
            if (home) {
                snprintf(out, 256, "%s/Library/Application Support/c_mmorpg/%s", home, filename);
                char dir_path[256];
                snprintf(dir_path, 256, "%s/Library/Application Support/c_mmorpg", home);
                mkdir(dir_path, 0755);
            } else { strcpy(out, filename); }
        } else { strcpy(out, filename); }
    #else
        strcpy(out, filename);
    #endif
}

void ensure_save_file(const char *filename, const char *asset_name) {
    char path[256]; get_path(path, filename, 1);
    if (access(path, F_OK) != -1) return;
    char asset_path[256]; get_path(asset_path, asset_name, 0);
    FILE *src = fopen(asset_path, "rb");
    if (!src) return;
    FILE *dst = fopen(path, "wb");
    if (!dst) { fclose(src); return; }
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
    fclose(src); fclose(dst);
}

void load_servers(void) {
    char path[256]; get_path(path, "servers.txt", 1);
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    server_count = 0;
    while (fscanf(fp, "%s %s %d", server_list[server_count].name, server_list[server_count].ip, &server_list[server_count].port) == 3) {
        server_count++;
        if (server_count >= 10) break;
    }
    fclose(fp);
}

void save_config(void) {
    char path[256]; get_path(path, "config.txt", 1);
    FILE *fp = fopen(path, "w");
    if (fp) {
        int r=255, g=255, b=255;
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(local_players[i].active && local_players[i].id == local_player_id) {
                r = local_players[i].r; g = local_players[i].g; b = local_players[i].b;
            }
        }
        // Config format: r g b r2 g2 b2 afk_min debug fps coords vol unread ui_scale zoom backend use_vk use_nv lang
        fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %.2f %.2f %d %d %d %d\n", 
            r, g, b,                          // Name color (start)
            my_r2, my_g2, my_b2,              // Name color (end)
            afk_timeout_minutes,              // AFK timeout
            show_debug_info, show_fps, show_coords, music_volume,  // Display settings
            show_unread_counter,              // UI settings
            ui_scale,                         // UI scale
            game_zoom,                        // Game zoom
            render_backend,                   // Rendering backend
            config_use_vulkan,                // Vulkan preference
            config_use_nvidia_gpu,            // NVIDIA GPU preference
            current_language                  // Language preference
        );
        fclose(fp);
    }
}

void load_config(void) {
    char path[256]; get_path(path, "config.txt", 1);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        get_path(path, "configs.txt", 1);
        fp = fopen(path, "r");
    }
    if (!fp) {
        get_path(path, "config.txt", 0);
        fp = fopen(path, "r");
    }
    if (!fp) {
        get_path(path, "configs.txt", 0);
        fp = fopen(path, "r");
    }
    
    // If no config file found on mobile, set default mobile scaling
    #if defined(__ANDROID__) || defined(__IPHONEOS__)
    if (!fp) {
        ui_scale = 1.6f;
        pending_ui_scale = 1.6f;
        game_zoom = 1.5f;
        pending_game_zoom = 1.5f;
        ALOG("First run detected on mobile - setting default UI scale to 1.6x and game zoom to 1.5x\\n");
        return;
    }
    #endif
    
    if (fp) {
        int dbg=0, fps=0, crd=0, vol=64, unread=1, backend=0, use_vk=0, use_nv=0, lang=0;
        float scale=1.0f, zoom=1.0f;
        
        // Config format: r g b r2 g2 b2 afk_min debug fps coords vol unread ui_scale zoom backend use_vk use_nv lang
        int count = fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %f %f %d %d %d %d", 
            &saved_r, &saved_g, &saved_b,       // Name color (start)
            &my_r2, &my_g2, &my_b2,             // Name color (end)
            &afk_timeout_minutes,               // AFK timeout
            &dbg, &fps, &crd, &vol,             // Display settings
            &unread,                            // UI settings
            &scale,                             // UI scale
            &zoom,                              // Game zoom
            &backend,                           // Rendering backend
            &use_vk,                            // Vulkan preference
            &use_nv,                            // NVIDIA GPU preference
            &lang                               // Language preference
        );
        
        if (count >= 11) {
            show_debug_info = dbg;
            show_fps = fps;
            show_coords = crd;
            music_volume = vol;
            Mix_VolumeMusic(music_volume);
        }
        
        if (count >= 12) {
            show_unread_counter = unread;
        }
        
        if (count >= 13) {
            ui_scale = scale;
            if (ui_scale < 0.5f) ui_scale = 0.5f;
            if (ui_scale > 2.0f) ui_scale = 2.0f;
            pending_ui_scale = ui_scale;
        }
        
        if (count >= 14) {
            game_zoom = zoom;
            if (game_zoom < 0.5f) game_zoom = 0.5f;
            if (game_zoom > 2.0f) game_zoom = 2.0f;
            pending_game_zoom = game_zoom;
        }
        
        if (count >= 16) {
            config_use_vulkan = use_vk;
        }
        
        if (count >= 17) {
            config_use_nvidia_gpu = use_nv;
        }
        
        if (count >= 18) {
            set_language((Language)lang);
        }
        
        #ifdef USE_VULKAN
        (void)backend;
        #endif
        
        fclose(fp);
    }
}

void load_triggers(void) {
    trigger_count = 0;
    printf("Client: Waiting to receive triggers from server...\n");
}

void receive_triggers_from_server(Packet *pkt) {
    trigger_count = pkt->trigger_count;
    printf("Client: Received %d triggers from server\n", trigger_count);
    
    for (int i = 0; i < trigger_count && i < 20; i++) {
        strncpy(triggers[i].src_map, pkt->triggers[i].src_map, sizeof(triggers[i].src_map) - 1);
        triggers[i].src_map[sizeof(triggers[i].src_map) - 1] = '\0';
        triggers[i].rect.x = pkt->triggers[i].rect_x;
        triggers[i].rect.y = pkt->triggers[i].rect_y;
        triggers[i].rect.w = pkt->triggers[i].rect_w;
        triggers[i].rect.h = pkt->triggers[i].rect_h;
        strncpy(triggers[i].target_map, pkt->triggers[i].target_map, sizeof(triggers[i].target_map) - 1);
        triggers[i].target_map[sizeof(triggers[i].target_map) - 1] = '\0';
        triggers[i].spawn_x = pkt->triggers[i].spawn_x;
        triggers[i].spawn_y = pkt->triggers[i].spawn_y;
        
        printf("Client: Loaded Trigger %d: %s [%d,%d %dx%d] -> %s\n",
               i, triggers[i].src_map,
               triggers[i].rect.x, triggers[i].rect.y,
               triggers[i].rect.w, triggers[i].rect.h,
               triggers[i].target_map);
    }
}

void save_servers(void) {
    char path[256]; get_path(path, "servers.txt", 1);
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    for(int i=0; i<server_count; i++) {
        fprintf(fp, "%s %s %d\n", server_list[i].name, server_list[i].ip, server_list[i].port);
    }
    fclose(fp);
}

void add_server_to_list(const char* name, const char* ip, int port) {
    if (server_count >= 10) return;
    strcpy(server_list[server_count].name, name);
    strcpy(server_list[server_count].ip, ip);
    server_list[server_count].port = port;
    server_count++;
    save_servers();
}

void load_profiles(void) {
    char path[256]; get_path(path, "profiles.txt", 1);
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    profile_count = 0;
    while (fscanf(fp, "%s %s", profile_list[profile_count].username, profile_list[profile_count].password) == 2) {
        profile_count++;
        if (profile_count >= 10) break;
    }
    fclose(fp);
}

void save_profiles(void) {
    char path[256]; get_path(path, "profiles.txt", 1);
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    for(int i=0; i<profile_count; i++) {
        fprintf(fp, "%s %s\n", profile_list[i].username, profile_list[i].password);
    }
    fclose(fp);
}

void add_profile_to_list(const char* username, const char* password) {
    if (profile_count >= 10) return;
    strcpy(profile_list[profile_count].username, username);
    strcpy(profile_list[profile_count].password, password);
    profile_count++;
    save_profiles();
}
