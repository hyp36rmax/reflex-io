#pragma once

#include "menu_pad_layouts_internal.h"
#include "oled_button_art.h"

namespace mouse_art {
using namespace menu_pad_layouts_internal;
using oled_button_art::Button;
using oled_button_art::Shape;

// A little animal mouse: ears are left/right click, Saturn's nose is middle
// click and its bow tie is Start. These are display masks, not USB mappings.
inline const PadButton snes[] = {
  {GPAD_A,0,10,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_B,0,40,PAD_FACE_ON,PAD_FACE_OFF},
};
inline const PadButton saturn[] = {
  {GPAD_A,0,10,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_B,0,40,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_X,3,30,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_START,4,28,PAD_RECT_ON,PAD_RECT_OFF},
};
inline const PadButton three[] = {
  {GPAD_A,0,10,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_B,0,40,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_X,3,30,PAD_FACE_ON,PAD_FACE_OFF},
};
inline const PadButton pce[] = {
  {GPAD_A,0,10,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_B,0,40,PAD_FACE_ON,PAD_FACE_OFF},
  {GPAD_START,4,28,PAD_RECT_ON,PAD_RECT_OFF},
  {GPAD_SELECT,3,30,PAD_FACE_ON,PAD_FACE_OFF},
};
inline bool selected(const PadButton* layout) { return layout==snes || layout==saturn || layout==three || layout==pce; }
inline bool button(const PadButton* layout, const PadButton& key, Button& art) {
  if (!selected(layout)) return false;
  if (key.mask==GPAD_A || key.mask==GPAD_B)
    art={int8_t(key.mask==GPAD_A?10:40),0,14,14,Shape::Round};
  else if (key.mask==GPAD_X || key.mask==GPAD_SELECT) art={30,24,5,5,Shape::Round};
  else art={28,32,9,5,Shape::Pill};
  return true;
}

template<class Display>
void decoration(Display& display, const PadButton* layout, uint8_t col,
                int8_t offset, uint8_t row) {
  if (!selected(layout)) return;
  auto* surface=oled_pixel::homeSurface();
  if (!surface || !surface->active) return;
  auto pixel=[&](int x,int y) {
    const int px=col+x, py=row*8+offset+y;
    if (px<0 || px>=128 || py<25 || py>63) return;
    oled_pixel::Origin origin(0,25,63);
    display.setCursor(px,py/8);
    display.ssd1306WriteRamBuf(surface->pixels[(py/8)*128+px]|(1u<<(py%8)));
  };
  auto line=[&](int x,int y,int tx,int ty) {
    const int dx=tx>x?tx-x:x-tx, dy=ty>y?y-ty:ty-y;
    const int sx=x<tx?1:-1, sy=y<ty?1:-1;
    int error=dx+dy;
    while (true) {
      pixel(x,y);
      if (x==tx && y==ty) break;
      const int e=2*error;
      if (e>=dy) {error+=dy;x+=sx;}
      if (e<=dx) {error+=dx;y+=sy;}
    }
  };
  constexpr int8_t face[][2]={{18,14},{45,14},{45,20},{36,29},{27,29},{18,20},{18,14}};
  for (uint8_t n=1;n<sizeof(face)/sizeof(face[0]);++n)
    line(face[n-1][0],face[n-1][1],face[n][0],face[n][1]);
  for (uint8_t eye=0;eye<2;++eye) {
    const uint8_t x=eye?37:25;
    pixel(x,18);pixel(x+1,18);pixel(x,19);pixel(x+1,19);
  }
  line(24,23,9,20);line(24,25,7,25);line(26,27,10,30);
  line(39,23,54,20);line(39,25,56,25);line(37,27,53,30);
  if (layout==snes) {line(30,25,34,25);line(31,26,33,26);pixel(32,27);}
  line(32,29,32,31);line(29,31,35,31);
}
} // namespace mouse_art
