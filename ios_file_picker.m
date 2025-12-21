#ifdef __IPHONEOS__

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "client.h"

// Callback to send selected image data to C code
static void (*g_image_selected_callback)(const uint8_t* data, size_t size) = NULL;

// Document picker delegate
@interface FilePickerDelegate : NSObject <UIDocumentPickerDelegate>
@end

@implementation FilePickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    if (urls.count > 0) {
        NSURL *url = urls[0];
        
        // Start accessing the security-scoped resource
        if ([url startAccessingSecurityScopedResource]) {
            NSData *imageData = [NSData dataWithContentsOfURL:url];
            [url stopAccessingSecurityScopedResource];
            
            if (imageData && g_image_selected_callback) {
                g_image_selected_callback((const uint8_t*)[imageData bytes], [imageData length]);
            }
        }
    }
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    NSLog(@"File picker cancelled");
}

@end

static FilePickerDelegate *g_delegate = nil;

// C function to set the callback
void ios_set_image_callback(void (*callback)(const uint8_t* data, size_t size)) {
    g_image_selected_callback = callback;
}

// C function to open the file picker
void ios_open_file_picker(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        // Get the key window (compatible with iOS 13+)
        UIWindow *keyWindow = nil;
        if (@available(iOS 13.0, *)) {
            NSSet *connectedScenes = [UIApplication sharedApplication].connectedScenes;
            for (UIScene *scene in connectedScenes) {
                if ([scene isKindOfClass:[UIWindowScene class]]) {
                    UIWindowScene *windowScene = (UIWindowScene *)scene;
                    for (UIWindow *window in windowScene.windows) {
                        if (window.isKeyWindow) {
                            keyWindow = window;
                            break;
                        }
                    }
                }
                if (keyWindow) break;
            }
        } else {
            keyWindow = [UIApplication sharedApplication].keyWindow;
        }
        
        UIViewController *rootViewController = keyWindow.rootViewController;
        
        if (rootViewController) {
            UIDocumentPickerViewController *documentPicker = nil;
            
            if (@available(iOS 14.0, *)) {
                // Use UTType for iOS 14+
                NSArray<UTType *> *contentTypes = @[UTTypeImage];
                documentPicker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:contentTypes];
            } else {
                // Fallback for older iOS versions
                NSArray *documentTypes = @[@"public.image"];
                documentPicker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:documentTypes
                                                                                       inMode:UIDocumentPickerModeImport];
            }
            
            // Create delegate if needed
            if (!g_delegate) {
                g_delegate = [[FilePickerDelegate alloc] init];
            }
            
            documentPicker.delegate = g_delegate;
            documentPicker.allowsMultipleSelection = NO;
            
            [rootViewController presentViewController:documentPicker animated:YES completion:nil];
            
            NSLog(@"iOS file picker opened");
        } else {
            NSLog(@"Failed to get root view controller");
        }
    });
}

#endif // __IPHONEOS__
