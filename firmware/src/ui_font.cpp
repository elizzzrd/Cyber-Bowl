#include "ui_font.hpp"
#include "ui_font_data.hpp"

namespace
{
    uint16_t nextCode(const char*& text)
    {
        const uint8_t first = static_cast<uint8_t>(*text++);
        if (first < 128)
        {
            return first;
        }

        if ((first & 0xE0) == 0xC0 && *text)
        {
            const uint8_t second = static_cast<uint8_t>(*text++);
            return static_cast<uint16_t>(((first & 31) << 6) | (second & 63));
        }

        return '?';
    }

    const UiFontData::Glyph& findGlyph(uint16_t code)
    {
        for (const auto& glyph : UiFontData::GLYPHS)
        {
            if (glyph.code == code)
            {
                return glyph;
            }
        }

        return UiFontData::GLYPHS['?' - 32];
    }

    void drawGlyph(Adafruit_GFX& surface, int x, int y, const UiFontData::Glyph& glyph,
                   uint16_t color, uint8_t scale)
    {
        const int rowBytes = (glyph.width + 7) / 8;
        for (int row = 0; row < 16; ++row)
        {
            for (int column = 0; column < glyph.width; ++column)
            {
                const int byteIndex = glyph.offset + row * rowBytes + column / 8;
                const uint8_t mask = 0x80 >> (column % 8);
                if (UiFontData::BITMAP_DATA[byteIndex] & mask)
                {
                    surface.fillRect(x + column * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
} // namespace

int UiFont::width(const char* text, uint8_t scale)
{
    int result = 0;
    while (*text)
    {
        const auto& glyph = findGlyph(nextCode(text));
        result += (glyph.width + 1) * scale;
    }
    return result;
}

void UiFont::draw(Adafruit_GFX& surface, int x, int y, const char* text, uint16_t color,
                  uint8_t scale, int maxWidth)
{
    const int rightEdge = x + maxWidth;
    while (*text)
    {
        const auto& glyph = findGlyph(nextCode(text));
        if (x + glyph.width * scale > rightEdge)
        {
            break;
        }

        drawGlyph(surface, x, y, glyph, color, scale);
        x += (glyph.width + 1) * scale;
    }
}
