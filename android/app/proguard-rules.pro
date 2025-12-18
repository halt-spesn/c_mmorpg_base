# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Keep SDL classes
-keep class org.libsdl.app.** { *; }
-keepclassmembers class org.libsdl.app.** { *; }

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}
