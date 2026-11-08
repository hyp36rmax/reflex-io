#pragma once
#include <stdint.h>
#include "oled_pixel_surface.h"

// Pixel primitives shared by the renderer and native preview/tests. No product
// identities or controller mappings belong here. Bounds are inclusive of ink.
namespace oled_button_art {
enum class Shape : uint8_t { Hidden, Round, Pill, Slant, KidneyH, KidneyV, Arrow, Bar, Rect, Fillet, TriggerWell, RoundedRect, Tombstone, Glyph, Play };
struct Button { int8_t x, y; uint8_t w, h; Shape shape; uint8_t rotation = 0; };
inline Button labelBounds(Button b) {
  if (b.shape!=Shape::Bar && b.shape!=Shape::Hidden && (b.w<9 || b.h<9)) {
    b.x+=(int(b.w)-5)/2;
    if (b.h>7) b.y+=(b.h-7)/2;
    b.w=5;b.h=7;
  }
  return b;
}
inline bool ellipse(int x, int y, int w, int h) {
  if (x < 0 || y < 0 || x >= w || y >= h) return false;
  const int32_t dx = 2*x-w+1, dy = 2*y-h+1;
  return dx*dx*h*h + dy*dy*w*w <= int32_t(w)*w*h*h;
}
inline bool inside(const Button& b, int x, int y) {
  if (x < 0 || y < 0 || x >= b.w || y >= b.h) return false;
  switch (b.shape) {
    case Shape::Hidden: case Shape::Glyph: return false;
    case Shape::Rect: return true;
    case Shape::Fillet:
      return !((x==0 || x==b.w-1) && (y==0 || y==b.h-1));
    case Shape::RoundedRect: {
      const int cx=x<b.w-1-x?x:b.w-1-x, cy=y<b.h-1-y?y:b.h-1-y;
      return cx+cy>=2;
    }
    case Shape::Tombstone:
      return y>=b.w/2 || ellipse(x,y,b.w,b.w);
    case Shape::TriggerWell: {
      // Flat top, grip-aligned left edge and rounded lower-right finger guard.
      const int left=(b.h-1-y)*2/(b.h-1);
      if (x<left) return false;
      const int r=b.h/2;
      return x<b.w-r || y<b.h-r ||
             ellipse(x-b.w+2*r,y-b.h+2*r,2*r,2*r);
    }
    case Shape::Bar: return true;
    case Shape::Round: return ellipse(x,y,b.w,b.h);
    case Shape::KidneyH:
      return ellipse(x,y,b.w,b.h) && !ellipse(x-3,y-b.h+2,b.w-6,5);
    case Shape::KidneyV:
      return ellipse(x,y,b.w,b.h) && !ellipse(x+3,y-3,5,b.h-6);
    case Shape::Slant: {
      const int offset = (b.w-1-x)*2/(b.w-1);
      return y >= offset && y < offset+b.h-2 &&
             (x > 0 || y > offset) && (x < b.w-1 || y < offset+b.h-3);
    }
    case Shape::Pill: {
      const int r=b.h/2;
      return (x >= r && x < b.w-r) ||
             ellipse(x,y,b.h,b.h) || ellipse(x-b.w+b.h,y,b.h,b.h);
    }
    case Shape::Arrow: {
      // A real arrowhead and shaft, rather than another D-pad cross segment.
      int u=x, v=y;
      if (b.rotation==1) {u=b.w-1-y; v=x;}
      if (b.rotation==2) {u=b.w-1-x; v=b.h-1-y;}
      if (b.rotation==3) {u=y; v=b.h-1-x;}
      const int c=b.w/2;
      return v<=c ? (u>=c-v && u<=c+v) : (u>=c-1 && u<=c+1);
    }
    case Shape::Play: {
      const int edge=y<b.h-1-y?y:b.h-1-y;
      return x<=edge*(b.w-1)/((b.h-1)/2);
    }
  }
  return false;
}
inline bool pixel(const Button& b, int x, int y, bool pressed, uint8_t fill=0) {
  if (!inside(b,x,y)) return false;
  if (b.shape==Shape::TriggerWell) {
    const bool rim=!inside(b,x-1,y) || !inside(b,x+1,y) ||
                   !inside(b,x,y-1) || !inside(b,x,y+1);
    // A thin curved trigger, pulled toward the grip when pressed. Neither state
    // fills the finger opening; only the curved line changes position.
    const int dy=2*y-b.h+1;
    const int curve=(pressed?2:4)+(dy*dy*2)/((b.h-1)*(b.h-1));
    return rim || x==curve; // Both ends meet the guard; no floating trigger.
  }
  if (pressed || (b.shape==Shape::Bar && x>0 && x<=fill)) return true;
  return !inside(b,x-1,y) || !inside(b,x+1,y) ||
         !inside(b,x,y-1) || !inside(b,x,y+1);
}
template<class Display> void draw(Display& display, Button b, bool pressed,
                                  int16_t col, int8_t panelOffset, uint8_t row=3,
                                  uint8_t fill=0, const uint8_t* label=nullptr) {
  if (!b.w || !b.h || b.shape==Shape::Hidden) return;
  const bool labelOnly=label && b.shape!=Shape::Bar && (b.w<9 || b.h<9);
  if (labelOnly) b=labelBounds(b);
  const int16_t top=row*8+panelOffset+b.y;
  // Clip the write to this button's box, not the whole SSD1306 page. Updating a
  // lower button must never erase an adjacent upper button on the same page.
  oled_pixel::Origin origin(panelOffset+b.y, top<25?25:top,
                            top+b.h-1>63?63:top+b.h-1);
  for (uint8_t page=0;page<(b.h+7)/8;++page) {
    for (uint8_t x=0;x<b.w;++x) {
      const int16_t screenX=col+b.x+x;
      if (screenX<0 || screenX>127) continue;
      uint8_t bits=0;
      for (uint8_t bit=0;bit<8;++bit) {
        const uint8_t y=page*8+bit;
        bool ink=y<b.h && pixel(b,x,y,pressed,fill);
        if (labelOnly) ink=y<7 && bool(label[x]&(1u<<y))!=pressed;
        else if (label && b.w>=7 && b.h>=9) {
          const int lx=int(x)-(b.w-5)/2, ly=int(y)-(b.h-7)/2;
          if (lx>=0 && lx<5 && ly>=0 && ly<7 && (label[lx]&(1<<ly))) ink=!pressed;
        }
        if (ink) bits|=1u<<bit;
      }
      display.setCursor(screenX,row+page);
      display.ssd1306WriteRamBuf(bits);
    }
  }
}
} // namespace oled_button_art
