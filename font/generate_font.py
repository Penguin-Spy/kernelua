import sys
from PIL import Image
import pypdn

inputPath = sys.argv[1]
outputPath = sys.argv[2]

# Kernelua Standard Code for Information Interchange
img = Image.open(inputPath)

foregroundIndex = -1

for i in range(0, len(img.getpalette()), 3):
    chunk = img.getpalette()[i: i + 3]
    print(chunk)
    if (chunk == [255, 255, 255]):
        foregroundIndex = int(i/3)
        print(f"Foreground pallete index: {foregroundIndex}")
        break

pixels = img.load()  # this is not a list, nor is it list()'able
img_width, img_height = img.size
color = pixels[0, 0]

glyph = '''// THIS FILE IS GENERATED, DO NOT EDIT MANUALLY, CHANGES WILL BE LOST
#ifndef FONT_H
#define FONT_H

// bytes are rows top-to-bottom
// bits are columns right-to-left
unsigned char font[256][8] = {
'''

# Row, Column of the glyph in the image
for r in range(16):
    for c in range(16):

        glyph += "    { "
        for y in range(8):
            row = 0
            for x in range(8):
                cpixel = pixels[c * 8 + x, r * 8 + y]
                if cpixel == foregroundIndex:  # not all(x == y for x, y in zip(cpixel, (255, 255, 255, 0))):
                    row += 1 << x
            # print("".join(hex(row)))
            glyph += "{0:#0{1}x}".format(row, 4)
            if not y == 7:
                glyph += ", "

        glyph += " }, \n"
glyph += '''};

#endif'''

RPI_Term = open(outputPath, 'w')
RPI_Term.write(glyph)
RPI_Term.close()

print(f"Generated {outputPath}")
