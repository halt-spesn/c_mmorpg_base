package com.mmo.client;

import org.libsdl.app.SDLActivity;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;

/**
 * Main Activity for C MMO Client
 * 
 * This activity extends SDLActivity to provide the Android entry point
 * for the SDL2-based C game client.
 */
public class MainActivity extends SDLActivity {
    
    private static final int PICK_IMAGE_REQUEST = 1;
    private static final String TAG = "MMOClient";
    
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
    
    /**
     * Open file picker for avatar selection.
     * Called from native C code via JNI.
     */
    public static void openFilePicker() {
        final MainActivity activity = (MainActivity) mSingleton;
        if (activity != null) {
            activity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
                    intent.setType("image/*");
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    try {
                        activity.startActivityForResult(
                            Intent.createChooser(intent, "Select Avatar Image"),
                            PICK_IMAGE_REQUEST);
                    } catch (android.content.ActivityNotFoundException ex) {
                        Log.e(TAG, "No file picker available");
                    }
                }
            });
        }
    }
    
    /**
     * Handle the result from file picker
     */
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        
        if (requestCode == PICK_IMAGE_REQUEST && resultCode == RESULT_OK) {
            if (data != null && data.getData() != null) {
                Uri uri = data.getData();
                try {
                    InputStream inputStream = getContentResolver().openInputStream(uri);
                    if (inputStream != null) {
                        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
                        byte[] temp = new byte[1024];
                        int bytesRead;
                        while ((bytesRead = inputStream.read(temp)) != -1) {
                            buffer.write(temp, 0, bytesRead);
                        }
                        byte[] imageData = buffer.toByteArray();
                        inputStream.close();
                        
                        // Send the image data to native code
                        nativeOnImageSelected(imageData, imageData.length);
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Error reading file: " + e.getMessage());
                }
            }
        }
    }
    
    /**
     * Native method to pass selected image data to C code
     */
    private native void nativeOnImageSelected(byte[] data, int size);
}
