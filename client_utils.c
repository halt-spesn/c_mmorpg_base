#include "client.h"

int is_blocked(int id) {
    for(int i=0; i<blocked_count; i++) if(blocked_ids[i] == id) return 1;
    return 0;
}

void reset_avatar_cache(void) {
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(avatar_cache[i]) SDL_DestroyTexture(avatar_cache[i]);
        avatar_cache[i] = NULL;
        avatar_status[i] = 0;
    }
}

void push_chat_line(const char *text) {
    for(int i=0; i<CHAT_HISTORY-1; i++) strcpy(chat_log[i], chat_log[i+1]);
    strncpy(chat_log[CHAT_HISTORY-1], text, 63);
    chat_scroll = 0;
}

void toggle_block(int id) {
    if (is_blocked(id)) {
        for(int i=0; i<blocked_count; i++) {
            if(blocked_ids[i] == id) {
                for(int j=i; j<blocked_count-1; j++) blocked_ids[j] = blocked_ids[j+1];
                blocked_count--; return;
            }
        }
    } else if(blocked_count < 50) blocked_ids[blocked_count++] = id;
}

SDL_Color get_status_color(int status) {
    switch(status) {
        case STATUS_AFK: return col_status_afk;
        case STATUS_DND: return col_status_dnd;
        case STATUS_ROLEPLAY: return col_status_rp;
        case STATUS_TALK: return col_status_talk;
        default: return col_status_online;
    }
}

int scale_ui(int value) {
    return (int)(value * ui_scale);
}

SDL_Rect scale_rect(int x, int y, int w, int h) {
    return (SDL_Rect){scale_ui(x), scale_ui(y), scale_ui(w), scale_ui(h)};
}

void reload_font_for_ui_scale(void) {
    // Close existing font
    if (font) {
        TTF_CloseFont(font);
        font = NULL;
    }
    
    // Calculate scaled font size with proper rounding
    // Base font size is FONT_SIZE (14), scale it with ui_scale
    // Cast to float for consistent precision, then round and cast to int
    int scaled_font_size = (int)((float)FONT_SIZE * ui_scale + 0.5f);
    
    // Ensure minimum readable size
    if (scaled_font_size < MIN_FONT_SIZE) scaled_font_size = MIN_FONT_SIZE;
    
    // Reload font at scaled size
    font = TTF_OpenFont(FONT_PATH, scaled_font_size);
    if (!font) {
        printf("Failed to reload font '%s' at size %d (ui_scale=%.2f): %s\n", 
               FONT_PATH, scaled_font_size, ui_scale, TTF_GetError());
        // Fallback to default size if scaled size fails
        font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
        if (!font) {
            printf("Critical: Font '%s' could not be loaded at default size: %s\n", 
                   FONT_PATH, TTF_GetError());
        }
    }
}

SDL_Color get_color_from_code(char code) {
    switch(code) {
        case '1': return (SDL_Color){255, 50, 50, 255};
        case '2': return (SDL_Color){50, 255, 50, 255};
        case '3': return (SDL_Color){80, 150, 255, 255};
        case '4': return (SDL_Color){255, 255, 0, 255};
        case '5': return (SDL_Color){0, 255, 255, 255};
        case '6': return (SDL_Color){255, 0, 255, 255};
        case '7': return (SDL_Color){255, 255, 255, 255};
        case '8': return (SDL_Color){150, 150, 150, 255};
        case '9': return (SDL_Color){0, 0, 0, 255};
        default: return (SDL_Color){255, 255, 255, 255};
    }
}

void render_raw_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, int center) {
    if (!text || strlen(text) == 0) return;
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int rx = center ? x - (surface->w/2) : x;
    SDL_Rect dst = {rx, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void remove_last_utf8_char(char *str) {
    int len = strlen(str);
    if (len == 0) return;
    while (len > 0) {
        len--;
        if ((str[len] & 0xC0) != 0x80) break; 
    }
    str[len] = '\0';
}

void render_text_gradient(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color c1, SDL_Color c2, int center) {
    if (!text || !*text) return;

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, (SDL_Color){255, 255, 255, 255});
    if (!surface) return;

    SDL_LockSurface(surface);
    
    int w = surface->w;
    int h = surface->h;
    int pitch = surface->pitch;
    Uint8 *pixels = (Uint8 *)surface->pixels;
    SDL_PixelFormat *fmt = surface->format;

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            Uint32 *pixel_addr = (Uint32 *)(pixels + py * pitch + px * 4);
            
            Uint32 pixel_val = *pixel_addr;
            Uint8 r_old, g_old, b_old, a;
            SDL_GetRGBA(pixel_val, fmt, &r_old, &g_old, &b_old, &a);

            if (a > 0) {
                float t = (float)px / (float)w;

                Uint8 r = c1.r + (int)((c2.r - c1.r) * t);
                Uint8 g = c1.g + (int)((c2.g - c1.g) * t);
                Uint8 b = c1.b + (int)((c2.b - c1.b) * t);

                *pixel_addr = SDL_MapRGBA(fmt, r, g, b, a);
            }
        }
    }
    
    SDL_UnlockSurface(surface);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int rx = center ? x - (w / 2) : x;
    SDL_Rect dst = {rx, y, w, h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void render_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color default_color, int center) {
    if (!text || !*text) return;

    int original_style = TTF_GetFontStyle(font);
    int current_style = original_style;
    SDL_Color current_color = default_color;
    
    typedef struct { SDL_Texture *tex; int w, h; } Segment;
    Segment segments[64]; int seg_count = 0;
    int total_width = 0; int max_height = 0;

    char buffer[256]; int buf_idx = 0; int i = 0;

    while (text[i] && seg_count < 64) {
        int flush = 0; int skip = 0;
        
        if (text[i] == '^' && text[i+1] >= '0' && text[i+1] <= '9') {
            flush = 1; skip = 2;
        } 
        else if (text[i] == '*' || text[i] == '#' || text[i] == '~') {
            flush = 1; skip = 1;
        }

        if (flush) {
            if (buf_idx > 0) {
                buffer[buf_idx] = 0;
                TTF_SetFontStyle(font, current_style);
                SDL_Surface *s = TTF_RenderUTF8_Blended(font, buffer, current_color);
                if (s) {
                    segments[seg_count].tex = SDL_CreateTextureFromSurface(renderer, s);
                    segments[seg_count].w = s->w; segments[seg_count].h = s->h;
                    total_width += s->w; if (s->h > max_height) max_height = s->h;
                    SDL_FreeSurface(s); seg_count++;
                }
                buf_idx = 0;
            }

            if (skip == 2) {
                current_color = get_color_from_code(text[i+1]);
            } else if (skip == 1) {
                if (text[i] == '*') current_style ^= TTF_STYLE_ITALIC;
                if (text[i] == '#') current_style ^= TTF_STYLE_BOLD;
                if (text[i] == '~') current_style ^= TTF_STYLE_STRIKETHROUGH;
            }
            i += skip;
        } else {
            buffer[buf_idx++] = text[i++];
        }
    }

    if (buf_idx > 0 && seg_count < 64) {
        buffer[buf_idx] = 0;
        TTF_SetFontStyle(font, current_style);
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, buffer, current_color);
        if (s) {
            segments[seg_count].tex = SDL_CreateTextureFromSurface(renderer, s);
            segments[seg_count].w = s->w; segments[seg_count].h = s->h;
            total_width += s->w; if (s->h > max_height) max_height = s->h;
            SDL_FreeSurface(s); seg_count++;
        }
    }

    int current_x = center ? (x - total_width / 2) : x;
    for (int k = 0; k < seg_count; k++) {
        if (segments[k].tex) {
            SDL_Rect dst = {current_x, y, segments[k].w, segments[k].h};
            SDL_RenderCopy(renderer, segments[k].tex, NULL, &dst);
            current_x += segments[k].w;
            SDL_DestroyTexture(segments[k].tex);
        }
    }
    TTF_SetFontStyle(font, original_style);
}

const char* get_role_name(int role) {
    switch(role) {
        case 1: return "ADMIN";
        case 2: return "DEV";
        case 3: return "CONTRIB";
        case 4: return "VIP";
        default: return "Player";
    }
}

SDL_Color get_role_color(int role) {
    switch(role) {
        case 1: return (SDL_Color){255, 50, 50, 255};
        case 2: return (SDL_Color){50, 150, 255, 255};
        case 3: return (SDL_Color){0, 200, 100, 255};
        case 4: return (SDL_Color){255, 215, 0, 255};
        default: return (SDL_Color){100, 100, 100, 255};
    }
}

void switch_map(const char* new_map, int x, int y) {
    if (SDL_GetTicks() - last_map_switch_time < 2000) return;
    last_map_switch_time = SDL_GetTicks();
    strncpy(current_map_file, new_map, 31);
    
    if (tex_map) SDL_DestroyTexture(tex_map);
    char path[256]; get_path(path, current_map_file, 0);
    SDL_Surface *temp = IMG_Load(path);
    if (temp) {
        tex_map = SDL_CreateTextureFromSurface(global_renderer, temp);
        map_w = temp->w; map_h = temp->h;
        SDL_FreeSurface(temp);
    } else {
        printf("Failed to load map: %s\n", current_map_file);
    }

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(local_players[i].active && local_players[i].id == local_player_id) {
            local_players[i].x = x;
            local_players[i].y = y;
            strncpy(local_players[i].map_name, new_map, 31);
        }
    }

    Packet pkt; 
    pkt.type = PACKET_MAP_CHANGE;
    strncpy(pkt.target_map, new_map, 31);
    pkt.dx = x; 
    pkt.dy = y;
    send_packet(&pkt);
    
    printf("Switched to %s at %d,%d\n", new_map, x, y);
}
