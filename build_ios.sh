#!/bin/bash
# Script to build iOS .ipa using Theos
# Usage: ./build_ios.sh [clean|package|ipa]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if dpkg-deb is available
if ! command -v dpkg-deb &> /dev/null; then
    echo -e "${RED}Error: dpkg-deb is not installed${NC}"
    echo "Please install dpkg with: sudo apt-get install dpkg"
    echo "Or on macOS with: brew install dpkg"
    exit 1
fi

# Check if THEOS is set
if [ -z "$THEOS" ]; then
    echo -e "${RED}Error: THEOS environment variable is not set${NC}"
    echo "Please set it with: export THEOS=/opt/theos"
    echo "Or install Theos first: https://theos.dev/docs/installation"
    exit 1
fi

# Check if Theos exists
if [ ! -d "$THEOS" ]; then
    echo -e "${RED}Error: THEOS directory not found at: $THEOS${NC}"
    echo "Please install Theos first: https://theos.dev/docs/installation"
    exit 1
fi

ACTION=${1:-all}

case $ACTION in
    clean)
        echo -e "${YELLOW}Cleaning Theos build artifacts...${NC}"
        make -f Makefile.theos clean
        rm -rf .theos/ packages/ obj/
        echo -e "${GREEN}Clean complete${NC}"
        ;;
    
    package)
        echo -e "${YELLOW}Building iOS application with Theos...${NC}"
        make -f Makefile.theos package
        echo -e "${GREEN}Package created in packages/ directory${NC}"
        ;;
    
    ipa)
        echo -e "${YELLOW}Building and converting to .ipa...${NC}"
        
        # Build the package
        make -f Makefile.theos package
        
        # Find the latest .deb file
        DEB_FILE=$(ls -t packages/*.deb 2>/dev/null | head -1)
        
        if [ -z "$DEB_FILE" ] || [ ! -f "$DEB_FILE" ]; then
            echo -e "${RED}Error: No .deb file found in packages/${NC}"
            echo "Make sure the build completed successfully."
            exit 1
        fi
        
        echo -e "${YELLOW}Found package: $DEB_FILE${NC}"
        
        # Clean up old extraction
        rm -rf extracted/ Payload/
        
        # Extract the .deb
        echo -e "${YELLOW}Extracting .deb package...${NC}"
        dpkg-deb -x "$DEB_FILE" extracted/
        
        # Check if the app bundle exists
        if [ ! -d "extracted/Applications/mmotest.app" ]; then
            echo -e "${RED}Error: mmotest.app not found in extracted package${NC}"
            exit 1
        fi
        
        # Create Payload directory
        echo -e "${YELLOW}Creating .ipa structure...${NC}"
        mkdir -p Payload
        cp -r extracted/Applications/mmotest.app Payload/
        
        # Create .ipa
        echo -e "${YELLOW}Creating .ipa file...${NC}"
        zip -r mmotest.ipa Payload/ > /dev/null
        
        # Clean up
        rm -rf extracted/ Payload/
        
        echo -e "${GREEN}Success! Created mmotest.ipa${NC}"
        echo -e "${GREEN}File size: $(du -h mmotest.ipa | cut -f1)${NC}"
        ;;
    
    all|*)
        echo -e "${YELLOW}Building iOS application with Theos...${NC}"
        make -f Makefile.theos
        echo -e "${GREEN}Build complete${NC}"
        echo ""
        echo "To create a package, run: $0 package"
        echo "To create an .ipa, run: $0 ipa"
        ;;
esac

echo ""
echo -e "${GREEN}Done!${NC}"
