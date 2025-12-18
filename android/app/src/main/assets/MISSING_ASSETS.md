# Missing Game Assets

Before building the Android APK, you need to add the following image files to this directory:

## Required Images

1. **player.png** - Player character sprite
   - Recommended size: 32x32 pixels or larger
   - Format: PNG with transparency
   - This is the sprite that represents the player in the game

2. **map.jpg** - Game world map
   - Required size: 2000x2000 pixels (as defined in common.h)
   - Format: JPG or JPEG
   - This is the background map/world where players move around

## Creating Placeholder Images

If you don't have the actual game graphics yet, you can create simple placeholders:

### Using ImageMagick (command line):
```bash
# Create a simple player sprite (32x32 red square)
convert -size 32x32 xc:red player.png

# Create a simple map (2000x2000 green background)
convert -size 2000x2000 xc:green map.jpg
```

### Using Python with PIL:
```python
from PIL import Image

# Create player sprite
player = Image.new('RGBA', (32, 32), (255, 0, 0, 255))
player.save('player.png')

# Create map
map_img = Image.new('RGB', (2000, 2000), (0, 255, 0))
map_img.save('map.jpg')
```

### Using GIMP or Photoshop:
1. Create a new image with the specified dimensions
2. Fill with any color or pattern
3. Export as PNG (for player) or JPG (for map)

## Where to Place Files

Once created, place these files in:
```
android/app/src/main/assets/
├── player.png
└── map.jpg
```

The game will load these assets at runtime using SDL2's image loading functions.
