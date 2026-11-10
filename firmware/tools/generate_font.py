"""Optional: regenerate the built-in Russian font; not needed to build firmware."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math
root = Path(__file__).resolve().parents[1]
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 13)
codes = sorted(set(range(32, 127)) | set(range(0x410, 0x450)) | {0x401, 0x451})
bitmaps, glyphs = [], []
for code in codes:
    ch = chr(code)
    w = max(2, math.ceil(font.getlength(ch)))
    img = Image.new('L', (w, 16), 0)
    ImageDraw.Draw(img).text((0, -1), ch, font=font, fill=255)
    offset = len(bitmaps)
    for y in range(16):
        for byte in range((w + 7) // 8):
            bits = 0
            for b in range(8):
                x = byte * 8 + b
                if x < w and img.getpixel((x, y)) >= 100:
                    bits |= 1 << (7 - b)
            bitmaps.append(bits)
    glyphs.append((code, w, offset))
src = '''// Generated from DejaVu Sans. See docs/FONT-LICENSE.txt.
#include "ui_font.hpp"
#include <cstddef>
namespace {
struct Glyph { uint16_t code; uint8_t width; uint16_t offset; };
const uint8_t pixels[] = {
'''
src += '\n'.join(','.join(str(v) for v in bitmaps[i:i+32]) + ',' for i in range(0, len(bitmaps), 32))
src += '\n};\nconst Glyph glyphs[] = {\n'
src += '\n'.join('{%d,%d,%d},' % item for item in glyphs)
src += '''
};
uint16_t nextCode(const char*& p)
{
    const uint8_t first = static_cast<uint8_t>(*p++);
    if (first < 128) return first;
    if ((first & 0xE0) == 0xC0 && *p) {
        const uint8_t second = static_cast<uint8_t>(*p++);
        return static_cast<uint16_t>(((first & 31) << 6) | (second & 63));
    }
    return '?';
}
const Glyph& find(uint16_t code)
{
    for (const auto& g : glyphs) if (g.code == code) return g;
    return glyphs['?' - 32];
}
}
int UiFont::width(const char* text, uint8_t scale)
{
    int result = 0;
    while (*text) result += (find(nextCode(text)).width + 1) * scale;
    return result;
}
void UiFont::draw(Adafruit_GFX& surface, int x, int y, const char* text,
                  uint16_t color, uint8_t scale, int maxWidth)
{
    const int end = x + maxWidth;
    while (*text) {
        const auto& g = find(nextCode(text));
        if (x + g.width * scale > end) break;
        const int rowBytes = (g.width + 7) / 8;
        for (int row = 0; row < 16; ++row) {
            for (int col = 0; col < g.width; ++col) {
                if (pixels[g.offset + row * rowBytes + col / 8] & (0x80 >> (col % 8)))
                    surface.fillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
        x += (g.width + 1) * scale;
    }
}
'''
(root / 'src/ui_font.cpp').write_text(src)
print(f'Generated {len(glyphs)} glyphs, {len(bitmaps)} bitmap bytes')
