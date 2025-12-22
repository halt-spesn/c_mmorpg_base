# Quick Start: Building iOS .ipa with Theos

## Prerequisites
```bash
# Set THEOS environment variable
export THEOS=/opt/theos

# Install Theos (if not already installed)
sudo git clone --recursive https://github.com/theos/theos.git $THEOS

# Install iOS SDK
cd $THEOS
git clone https://github.com/theos/sdks.git sdks

# Install ldid (on macOS)
brew install ldid

# Or on Linux
sudo apt-get install dpkg fakeroot
```

## Building

### Option 1: Using the build script (Recommended)
```bash
# Build and create .ipa
./build_ios.sh ipa

# Or just build without packaging
./build_ios.sh

# Or create only .deb package
./build_ios.sh package

# Clean build artifacts
./build_ios.sh clean
```

### Option 2: Manual build
```bash
# Build the application
make -f Makefile.theos

# Create package
make -f Makefile.theos package

# Clean
make -f Makefile.theos clean
```

## Output

- **Debian package**: `packages/com.halt.mmotest_*.deb`
- **IPA file** (when using `./build_ios.sh ipa`): `mmotest.ipa`

## Documentation

For detailed information, see:
- [THEOS_BUILD_GUIDE.md](THEOS_BUILD_GUIDE.md) - Complete guide
- [README.md](README.md) - Main project documentation

## Troubleshooting

**Error: THEOS not found**
```bash
export THEOS=/opt/theos
```

**Error: dpkg-deb not installed**
```bash
# macOS
brew install dpkg

# Linux
sudo apt-get install dpkg
```

**Error: SDK not found**
```bash
cd $THEOS
git clone https://github.com/theos/sdks.git sdks
```
