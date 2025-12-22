#include "client.h"

int get_cursor_pos_from_click(const char *text, int mouse_x, int rect_x) {
    if (!text || strlen(text) == 0) return 0;
    
    int len = strlen(text);
    int best_index = 0;
    int min_dist = 10000;

    for (int i = 0; i <= len; ) {
        char temp[256];
        strncpy(temp, text, i);
        temp[i] = 0;
        
        int w, h;
        TTF_SizeText(font, temp, &w, &h);
        
        int text_screen_x = rect_x + 5 + w;
        int dist = abs(mouse_x - text_screen_x);
        
        if (dist < min_dist) {
            min_dist = dist;
            best_index = i;
        }

        if (i == len) break;
        do { i++; } while (i < len && (text[i] & 0xC0) == 0x80);
    }
    return best_index;
}

void delete_selection(char *buffer) {
    if (selection_len == 0) return;

    int start = selection_start;
    int len = selection_len;
    
    if (len < 0) {
        start += len;
        len = -len;
    }

    int total_len = strlen(buffer);
    if (start < 0) start = 0;
    if (start + len > total_len) len = total_len - start;

    memmove(buffer + start, buffer + start + len, total_len - start - len + 1);
    
    cursor_pos = start;
    selection_len = 0;
}

// Get the active field buffer for the currently active text field
char* get_active_field_buffer_for_touch(int *max_len_out) {
    if (is_chat_open) {
        if (max_len_out) *max_len_out = 63;
        return input_buffer;
    }
    
    switch (active_field) {
        case 0: // auth_username
            if (max_len_out) *max_len_out = MAX_INPUT_LEN;
            return auth_username;
        case 1: // auth_password
            if (max_len_out) *max_len_out = MAX_INPUT_LEN;
            return auth_password;
        case 2: // input_ip
            if (max_len_out) *max_len_out = 15;
            return input_ip;
        case 3: // input_port
            if (max_len_out) *max_len_out = 7;
            return input_port;
        case 10: // nick_new
            if (max_len_out) *max_len_out = 31;
            return nick_new;
        case 11: // nick_confirm
            if (max_len_out) *max_len_out = 31;
            return nick_confirm;
        case 12: // nick_pass
            if (max_len_out) *max_len_out = 31;
            return nick_pass;
        case FIELD_PASSWORD_CURRENT:
            if (max_len_out) *max_len_out = 63;
            return password_current;
        case FIELD_PASSWORD_NEW:
            if (max_len_out) *max_len_out = 63;
            return password_new;
        case FIELD_PASSWORD_CONFIRM:
            if (max_len_out) *max_len_out = 63;
            return password_confirm;
        case FIELD_FRIEND_ID:
            if (max_len_out) *max_len_out = 8;
            return input_friend_id;
        case FIELD_SANCTION_REASON:
            if (max_len_out) *max_len_out = 63;
            return input_sanction_reason;
        case FIELD_BAN_TIME:
            if (max_len_out) *max_len_out = 15;
            return input_ban_time;
        default:
            return NULL;
    }
}

#if defined(__ANDROID__) || defined(__IPHONEOS__)
// Position the mobile text menu above touch point, avoiding keyboard
void position_mobile_text_menu(int touch_x, int touch_y, int scaled_w, int scaled_h, float ui_scale_val) {
    mobile_text_menu_x = touch_x - (MOBILE_TEXT_MENU_WIDTH / 2);
    mobile_text_menu_y = touch_y - MOBILE_TEXT_MENU_HEIGHT - 10;  // Position above touch with small gap
    
    // Clamp to screen bounds
    if (mobile_text_menu_x < 0) mobile_text_menu_x = 0;
    if (mobile_text_menu_y < 0) mobile_text_menu_y = 0;
    if (mobile_text_menu_x + MOBILE_TEXT_MENU_WIDTH > scaled_w) {
        mobile_text_menu_x = scaled_w - MOBILE_TEXT_MENU_WIDTH;
    }
    
    // Ensure menu is visible above keyboard
    int max_y = scaled_h - MOBILE_TEXT_MENU_HEIGHT - 10;
    if (keyboard_height > 0) {
        max_y = scaled_h - (keyboard_height / ui_scale_val) - MOBILE_TEXT_MENU_HEIGHT - 10;
    }
    if (mobile_text_menu_y > max_y) mobile_text_menu_y = max_y;
}
#endif

void handle_text_edit(char *buffer, int max_len, SDL_Event *ev) {
    int len = strlen(buffer);
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int shift_pressed = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    int ctrl_pressed = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];

    if (ev->type == SDL_TEXTINPUT) {
        if (selection_len != 0) delete_selection(buffer);
        
        int add_len = strlen(ev->text.text);
        if (strlen(buffer) + add_len <= max_len) {
            memmove(buffer + cursor_pos + add_len, buffer + cursor_pos, strlen(buffer) - cursor_pos + 1);
            memcpy(buffer + cursor_pos, ev->text.text, add_len);
            cursor_pos += add_len;
        }
    } 
    else if (ev->type == SDL_KEYDOWN) {
        if (ctrl_pressed && ev->key.keysym.sym == SDLK_c) {
            if (selection_len != 0) {
                int start = selection_start;
                int slen = selection_len;
                if (slen < 0) { start += slen; slen = -slen; }
                
                char *clip_buf = malloc(slen + 1);
                if (clip_buf) {
                    strncpy(clip_buf, buffer + start, slen);
                    clip_buf[slen] = 0;
                    SDL_SetClipboardText(clip_buf);
                    free(clip_buf);
                }
            }
        }
        else if (ctrl_pressed && ev->key.keysym.sym == SDLK_v) {
            if (SDL_HasClipboardText()) {
                char *text = SDL_GetClipboardText();
                if (text) {
                    if (selection_len != 0) delete_selection(buffer);
                    
                    int add_len = strlen(text);
                    if (strlen(buffer) + add_len <= max_len) {
                        memmove(buffer + cursor_pos + add_len, buffer + cursor_pos, strlen(buffer) - cursor_pos + 1);
                        memcpy(buffer + cursor_pos, text, add_len);
                        cursor_pos += add_len;
                    }
                    SDL_free(text);
                }
            }
        }
        else if (ctrl_pressed && ev->key.keysym.sym == SDLK_x) {
            if (selection_len != 0) {
                int start = selection_start;
                int slen = selection_len;
                if (slen < 0) { start += slen; slen = -slen; }
                char *clip_buf = malloc(slen + 1);
                if (clip_buf) {
                    strncpy(clip_buf, buffer + start, slen);
                    clip_buf[slen] = 0;
                    SDL_SetClipboardText(clip_buf);
                    free(clip_buf);
                }
                delete_selection(buffer);
            }
        }
        else if (ctrl_pressed && ev->key.keysym.sym == SDLK_a) {
            cursor_pos = len;
            selection_start = 0;
            selection_len = len;
        }
        else if (ev->key.keysym.sym == SDLK_LEFT) {
            if (shift_pressed && selection_len == 0) selection_start = cursor_pos;
            
            if (cursor_pos > 0) {
                do { cursor_pos--; if(shift_pressed) selection_len--; } 
                while (cursor_pos > 0 && (buffer[cursor_pos] & 0xC0) == 0x80);
            }
            if (!shift_pressed) selection_len = 0;
        }
        else if (ev->key.keysym.sym == SDLK_RIGHT) {
            if (shift_pressed && selection_len == 0) selection_start = cursor_pos;

            if (cursor_pos < len) {
                do { cursor_pos++; if(shift_pressed) selection_len++; } 
                while (cursor_pos < len && (buffer[cursor_pos] & 0xC0) == 0x80);
            }
            if (!shift_pressed) selection_len = 0;
        }
        else if (ev->key.keysym.sym == SDLK_BACKSPACE) {
            if (selection_len != 0) {
                delete_selection(buffer);
            } else if (cursor_pos > 0) {
                int end = cursor_pos;
                int start = end;
                do { start--; } while (start > 0 && (buffer[start] & 0xC0) == 0x80);
                memmove(buffer + start, buffer + end, len - end + 1);
                cursor_pos = start;
            }
        }
    }
}
