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
                InputStream inputStream = null;
                try {
                    inputStream = getContentResolver().openInputStream(uri);
                    if (inputStream != null) {
                        // Check file size first to avoid OutOfMemoryError
                        long fileSize = -1;
                        try {
                            android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null);
                            if (cursor != null) {
                                int sizeIndex = cursor.getColumnIndex(android.provider.OpenableColumns.SIZE);
                                if (sizeIndex >= 0) {
                                    cursor.moveToFirst();
                                    fileSize = cursor.getLong(sizeIndex);
                                }
                                cursor.close();
                            }
                        } catch (Exception e) {
                            Log.w(TAG, "Could not determine file size, will validate during read");
                        }
                        
                        // MAX_AVATAR_SIZE is 16384 bytes
                        final int MAX_AVATAR_SIZE = 16384;
                        if (fileSize > MAX_AVATAR_SIZE) {
                            Log.e(TAG, "File too large: " + fileSize + " bytes (max: " + MAX_AVATAR_SIZE + ")");
                            return;
                        }
                        
                        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
                        byte[] temp = new byte[1024];
                        int bytesRead;
                        int totalRead = 0;
                        
                        while ((bytesRead = inputStream.read(temp)) != -1) {
                            // Validate size during read as well
                            if (totalRead + bytesRead > MAX_AVATAR_SIZE) {
                                Log.e(TAG, "File exceeds maximum size during read");
                                return;
                            }
                            buffer.write(temp, 0, bytesRead);
                            totalRead += bytesRead;
                        }
                        
                        byte[] imageData = buffer.toByteArray();
                        
                        // Send the image data to native code
                        nativeOnImageSelected(imageData, imageData.length);
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Error reading file: " + e.getMessage());
                } finally {
                    // Ensure stream is closed even on error
                    if (inputStream != null) {
                        try {
                            inputStream.close();
                        } catch (Exception e) {
                            Log.w(TAG, "Error closing stream: " + e.getMessage());
                        }
                    }
                }
            }
        }
    }
    
    /**
     * Native method to pass selected image data to C code
     */
    private native void nativeOnImageSelected(byte[] data, int size);
}
