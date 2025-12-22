# Building iOS .ipa with Theos

This guide explains how to build the MMO Test Client as an iOS .ipa file using the Theos build system.

## Prerequisites

### 1. Install Theos

First, install Theos on your system:

```bash
# Install Theos
export THEOS=/opt/theos
sudo git clone --recursive https://github.com/theos/theos.git $THEOS
```

### 2. Install iOS SDK

You need an iOS SDK to build iOS applications. You can extract this from Xcode:

```bash
# Download iOS SDK from Xcode (if on macOS)
# The SDK will be in: /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/

# Or use theos-sdks repository for pre-packaged SDKs
cd $THEOS
git clone https://github.com/theos/sdks.git sdks
```

### 3. Install Required Tools

On macOS:
```bash
brew install ldid
```

On Linux:
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y fakeroot git perl clang build-essential

# Install ldid
git clone https://github.com/sbingner/ldid.git
cd ldid
./make.sh
sudo cp ldid /usr/local/bin/
```

## Building the .ipa

### Quick Build

To build the iOS application with Theos:

```bash
# Use the Theos Makefile
make -f Makefile.theos

# Package into .deb (Debian package)
make -f Makefile.theos package
```

This will create:
- A `.deb` file in the `packages/` directory
- Build artifacts in `.theos/` directory

### Converting .deb to .ipa

The Theos build system creates `.deb` packages by default. To convert to `.ipa`:

```bash
# Extract the .deb package
dpkg-deb -x packages/com.halt.mmotest_1.0.0-1_iphoneos-arm.deb extracted/

# Create Payload directory structure
mkdir -p Payload
cp -r extracted/Applications/mmotest.app Payload/

# Create .ipa
zip -r mmotest.ipa Payload/
```

### Advanced: Direct .ipa Creation

Alternatively, you can modify the Theos Makefile to create an .ipa directly by adding custom rules.

## Build Configuration

### Target iOS Version

The default target is iOS 13.0+. To change this, edit `Makefile.theos`:

```makefile
TARGET = iphone:clang:latest:14.0  # Change to desired minimum iOS version
```

### Architectures

The build targets `arm64` and `arm64e` by default:

```makefile
ARCHS = arm64 arm64e
```

### Vulkan Support

If you want to enable Vulkan rendering, uncomment this line in `Makefile.theos`:

```makefile
mmotest_FILES += renderer_vulkan.c
```

Then add the Vulkan compiler flag:

```makefile
mmotest_CFLAGS = -DUSE_SDL2 -D__IPHONEOS__ -DUSE_VULKAN -I./xcode_sdl_headers -Wno-error
```

## Project Structure

The Theos build uses the following files:

- **Makefile.theos**: Main Theos build configuration
- **control**: Package metadata (name, version, dependencies)
- **Info-theos.plist**: iOS application metadata
- **mach-o_deps/ios/**: Pre-compiled SDL2 static libraries for iOS

## Cleaning Build Artifacts

To clean build artifacts:

```bash
make -f Makefile.theos clean
```

To remove all Theos-generated files:

```bash
rm -rf .theos/ packages/ obj/
```

## Troubleshooting

### "THEOS not found" Error

Make sure `$THEOS` environment variable is set:

```bash
export THEOS=/opt/theos
```

Add this to your `~/.bashrc` or `~/.zshrc` to make it permanent.

### SDK Not Found

Ensure you have iOS SDKs installed in `$THEOS/sdks/`:

```bash
ls $THEOS/sdks/
# Should show: iPhoneOS13.0.sdk, iPhoneOS14.0.sdk, etc.
```

### Signing Issues

For development builds, Theos uses `ldid` for fake-signing. For App Store or TestFlight distribution, you'll need to properly sign the .ipa with Xcode or `codesign`.

### Missing Resources

If the app fails to load resources (fonts, images), ensure they're being copied in the `after-stage` section of `Makefile.theos`.

## Deployment

### Install on Jailbroken Device

If you have a jailbroken iOS device:

```bash
# Install via SSH
make -f Makefile.theos package install THEOS_DEVICE_IP=192.168.1.xxx THEOS_DEVICE_PORT=22
```

### Install via Sideloading

For non-jailbroken devices:

1. Convert the .deb to .ipa (see above)
2. Use tools like AltStore, Sideloadly, or Xcode to sideload the .ipa
3. Trust the developer certificate on your device

## Notes

- The Theos build system is primarily designed for jailbreak development
- For App Store distribution, it's recommended to use Xcode
- Theos is excellent for rapid prototyping and testing on jailbroken devices
- The `.deb` package format is the native output of Theos builds

## References

- [Theos Documentation](https://theos.dev/)
- [Theos GitHub](https://github.com/theos/theos)
- [iOS SDKs for Theos](https://github.com/theos/sdks)
