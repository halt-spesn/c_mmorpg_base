package com.mmo.client;

import org.libsdl.app.SDLActivity;

/**
 * Main Activity for C MMO Client
 * 
 * This activity extends SDLActivity to provide the Android entry point
 * for the SDL2-based C game client.
 */
public class MainActivity extends SDLActivity {
    
    /**
     * Called by SDL before loading the native shared libraries.
     * Override this method to load additional libraries.
     */
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "SDL2_mixer",
            "client"  // The native library built from client_sdl.c
        };
    }
}
