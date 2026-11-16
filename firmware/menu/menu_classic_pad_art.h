#pragma once
#include "menu_pad_layouts_internal.h"
#include "oled_button_art.h"
#include "menu_mouse_art.h"
#include "menu_fishing_pad_art.h"

// Classic-only physical artwork. These masks are display slots, not USB button
// numbers. Existing electrical decoding and output mapping are unchanged.
namespace classic_pad_art {
using oled_button_art::Button;
using oled_button_art::Shape;
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
using namespace menu_pad_layouts_internal;
#ifdef ENABLE_INPUT_PSX
inline const PadButton flight[] = {
 {GPAD_UP,1,6,PAD_UP_ON,PAD_UP_OFF}, {GPAD_DOWN,3,6,PAD_DOWN_ON,PAD_DOWN_OFF},
 {GPAD_LEFT,2,0,PAD_LEFT_ON,PAD_LEFT_OFF}, {GPAD_RIGHT,2,12,PAD_RIGHT_ON,PAD_RIGHT_OFF},
 {GPAD_SELECT,1,15,PAD_RECT_ON,PAD_RECT_OFF}, {GPAD_START,1,27,PAD_RECT_ON,PAD_RECT_OFF},
 {GPAD_L1,2,24,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_X,2,34,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_Y,2,44,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_R1,2,54,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_L2,3,24,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_A,3,34,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_B,3,44,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_R2,3,54,PAD_FACE_ON,PAD_FACE_OFF},
};
// Konami beatmania (PS1 five-key). The turntable is the JogCon dial, drawn
// and turned separately (drawBeatmaniaTurntable) at kBeatmaniaDialX/Y; the
// slots are black keys 2/4 (L1/R1) above white keys 1/3/5 (Square/Cross/
// Circle), with Select and Start on top.
constexpr uint8_t kBeatmaniaDialX = 0;
constexpr uint8_t kBeatmaniaDialY = 4;
constexpr uint8_t kBeatmaniaPanelLast = 24 + 29;  // home row 24 + the keys' last row
inline const PadButton beatmania[] = {
 {GPAD_SELECT,0,30,PAD_RECT_ON,PAD_RECT_OFF}, {GPAD_START,0,40,PAD_RECT_ON,PAD_RECT_OFF},
 {GPAD_L1,1,32,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_R1,1,43,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_X,2,27,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_A,2,38,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_B,2,49,PAD_FACE_ON,PAD_FACE_OFF},
};
inline const PadButton gun[] = {
 {GPAD_A,2,24,PAD_FACE_ON,PAD_FACE_OFF}, // trigger
 {GPAD_B,1,44,PAD_FACE_ON,PAD_FACE_OFF}, // physical A (PSX Start)
 {GPAD_X,2,44,PAD_FACE_ON,PAD_FACE_OFF}, // physical B (PSX Cross), far side
};
inline constexpr int8_t gunOutline[][2]={{7,2},{55,2},{57,5},{57,18},{37,18},{36,25},{35,27},{33,28},
  {19,28},{17,35},{3,35},{10,16},{5,14},{5,7},{7,2}};
inline bool gunBodyContains(int x,int y) {
  bool inside=false;
  for (uint8_t i=1;i<sizeof(gunOutline)/sizeof(gunOutline[0]);++i) {
    const int ax=gunOutline[i-1][0],ay=gunOutline[i-1][1];
    const int bx=gunOutline[i][0],by=gunOutline[i][1];
    if ((x-ax)*(by-ay)==(y-ay)*(bx-ax) &&
        x>=(ax<bx?ax:bx) && x<=(ax>bx?ax:bx) &&
        y>=(ay<by?ay:by) && y<=(ay>by?ay:by)) return true;
    if ((ay>y)!=(by>y) && x<ax+(bx-ax)*(y-ay)/(by-ay)) inside=!inside;
  }
  return inside;
}
#endif
#ifdef ENABLE_INPUT_SATURN
inline const PadButton mission[] = {
 {GPAD_UP,1,6,PAD_UP_ON,PAD_UP_OFF}, {GPAD_DOWN,3,6,PAD_DOWN_ON,PAD_DOWN_OFF},
 {GPAD_LEFT,2,0,PAD_LEFT_ON,PAD_LEFT_OFF}, {GPAD_RIGHT,2,12,PAD_RIGHT_ON,PAD_RIGHT_OFF},
 {GPAD_L2,2,22,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_R2,2,57,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_START,0,39,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_X,1,30,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_Y,1,39,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_L1,1,48,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_A,2,30,PAD_FACE_ON,PAD_FACE_OFF}, {GPAD_B,2,39,PAD_FACE_ON,PAD_FACE_OFF},
 {GPAD_R1,2,48,PAD_FACE_ON,PAD_FACE_OFF},
};
#endif
inline bool get(const PadButton* layout, const PadButton& key, Button& b) {
  if (mouse_art::button(layout,key,b)) return true;
  const uint32_t m=key.mask;
  b={int8_t(key.col),int8_t(key.row*8),5,5,Shape::Round};
  const bool face=m==GPAD_A || m==GPAD_B || m==GPAD_X || m==GPAD_Y;
#ifdef ENABLE_INPUT_VBOY
  if (layout==padLayoutVBoy) {
    if (m==GPAD_L1 || m==GPAD_R1) {b.x-=1;b.y=0; b.w=b.h=7; return true;}
    if (m==GPAD_SELECT) {b.x=17;b.y=9;return true;}
    if (m==GPAD_START) {b.x=22;b.y=17;return true;}
    if (m==GPAD_B) {b.x=33;b.y=17;return true;}
    if (m==GPAD_A) {b.x=38;b.y=9;return true;}
  }
#endif
  if (layout==padLayoutPowerPad) {
    b={int8_t(11+((key.col-6)/12)*10),int8_t(key.row*11),9,9,Shape::Round};return true;
  }
  if (layout==padLayoutNES && (m==GPAD_SELECT || m==GPAD_START)) {
    b={int8_t(m==GPAD_SELECT?20:29),18,7,5,Shape::Pill};return true;
  }
  if (layout==padLayoutNES && (m==GPAD_A || m==GPAD_B)) {
    b={int8_t(m==GPAD_B?40:51),16,9,9,Shape::Round};return true;
  }
  if (layout==padLayoutN64) {
    if (m==GPAD_START) {b={27,8,5,5,Shape::Round};return true;}
    if (m==GPAD_L1 || m==GPAD_R1) {b={int8_t(m==GPAD_L1?18:36),0,5,5,Shape::Rect};return true;}
    if (m==GPAD_L2) {b={26,0,7,5,Shape::Rect};return true;}
    if (m==GPAD_B) {b={33,17,7,7,Shape::Round};return true;}
    if (m==GPAD_A) {b={41,27,7,7,Shape::Round};return true;}
  }
  if (layout==padLayoutGC) {
    if (m==GPAD_SELECT) {b={26,0,9,5,Shape::Pill};return true;}
    if (m==GPAD_A) {b={41,19,11,11,Shape::Round};return true;}
    if (m==GPAD_B) {b={33,28,7,7,Shape::Round};return true;}
    if (m==GPAD_Y) {b={40,10,12,7,Shape::KidneyH};return true;}
    if (m==GPAD_X) {b={54,18,7,12,Shape::KidneyV};return true;}
  }
  bool sega=layout==padLayoutSaturn || layout==padLayoutSaturn3D;
#ifdef ENABLE_INPUT_MEGADRIVE
  sega=sega || layout==padLayoutGenesis6;
  if (layout==padLayoutGenesis3) {
    if (m==GPAD_START) {b={40,8,11,5,Shape::Pill};return true;}
    if (m==GPAD_A || m==GPAD_B || m==GPAD_R1) {
      const int n=m==GPAD_A?0:m==GPAD_B?1:2;
      b={int8_t(30+11*n),int8_t(24-4*n),9,9,Shape::Round};return true;
    }
  }
#endif
  if (sega) {
    if (m==GPAD_START) {
      b={21,18,7,5,Shape::Pill};
      return true;
    }
    const bool upper=m==GPAD_X || m==GPAD_Y || m==GPAD_L1;
    const bool lower=m==GPAD_A || m==GPAD_B || m==GPAD_R1;
    if (upper || lower) {
      const int n=(m==GPAD_X || m==GPAD_A)?0:(m==GPAD_Y || m==GPAD_B)?1:2;
      b={int8_t(32+10*n),int8_t((upper?12:24)-4*n),uint8_t(upper?7:9),uint8_t(upper?7:9),Shape::Round};
      // One six-button face layout for Genesis/Mega Drive and Saturn: the X/Y/Z row
      // sits at the same height on both (Genesis was 1 px higher until 2026-11-10).
      if (upper) b.y+=3;
      return true;
    }
  }
#ifdef ENABLE_INPUT_SATURN
  if (layout==mission) {
    if (m==GPAD_START) {b={39,0,7,5,Shape::Pill};return true;}
    if (m==GPAD_L2 || m==GPAD_R2) {b.y=20;return true;}
    if (face || m==GPAD_L1 || m==GPAD_R1) {b.w=b.h=7;return true;}
  }
#endif
#ifdef ENABLE_INPUT_PCE
  if (layout==padLayoutPCE || layout==padLayoutPCE2) {
    if (m==GPAD_SELECT || m==GPAD_START) {
      b={int8_t((m==GPAD_SELECT?22:31)-(layout==padLayoutPCE?2:0)),17,7,5,Shape::Pill};return true;
    }
    if (face || m==GPAD_L1 || m==GPAD_R1) {
      if (layout==padLayoutPCE2) b={int8_t(m==GPAD_A?42:53),16,9,9,Shape::Round};
      else b={int8_t(40+(key.col-42)/6*8),int8_t(key.row==2?6:16),7,7,Shape::Round};
      return true;
    }
  }
#endif
#ifdef ENABLE_INPUT_3DO
  if (layout==padLayout3DO && (m==GPAD_SELECT || m==GPAD_START)) {
    b={int8_t(m==GPAD_SELECT?19:28),16,7,7,Shape::Round};return true;
  }
  if (layout==padLayout3DO && (m==GPAD_A || m==GPAD_B || m==GPAD_X)) {
    const int n=m==GPAD_A?0:m==GPAD_B?1:2;
    b={int8_t(36+8*n),int8_t(22-4*n),7,7,Shape::Round};return true;
  }
#endif
#ifdef ENABLE_INPUT_JPC
  if (layout==padLayoutJPC && (m==GPAD_SELECT || m==GPAD_START)) {
    b={int8_t(m==GPAD_SELECT?20:29),18,7,5,Shape::Pill};return true;
  }
  if (layout==padLayoutJPC && (m==GPAD_A || m==GPAD_B)) {
    b={int8_t(m==GPAD_A?40:51),16,9,9,Shape::Round};return true;
  }
#endif
#ifdef ENABLE_INPUT_SMS
  if (layout==padLayoutSMS && (m==GPAD_A || m==GPAD_B)) {
    b={int8_t(m==GPAD_A?36:45),16,7,7,Shape::Round};return true;
  }
#endif
#ifdef ENABLE_INPUT_JAGUAR
  if (layout==padLayoutJaguar && key.on==PAD_RECT_ON) {
    b.x=18+((key.col-18)/6)*8;b.w=7;b.h=4;b.shape=Shape::Fillet;return true;
  }
  if (layout==padLayoutJaguar || layout==padLayoutJaguarRotary) {
    if (m==GPAD_START || m==GPAD_SELECT) {
      b={int8_t(m==GPAD_START?21:33),0,5,7,Shape::Glyph};return true;
    }
    // Physical C, B, A are the X, A, B neutral bits respectively.
    if (m==GPAD_X) {b={42,22,5,5,Shape::Round};return true;}
    if (m==GPAD_A) {b={48,16,5,5,Shape::Round};return true;}
    if (m==GPAD_B) {b={54,10,5,5,Shape::Round};return true;}
  }
#endif
#ifdef ENABLE_INPUT_DRIVING
  if (layout==padLayoutDriving) {b={27,28,7,7,Shape::Round};return true;}
#endif
#ifdef ENABLE_INPUT_PSX
  if (m==GPAD_START && key.on==PAD_PS_TRIANGLE_ON) {
    // The old five-column glyph has only four lit columns. Add two real pixels
    // and retain an odd height for a centered, vertically symmetric point.
    b={int8_t(key.col-1),int8_t(key.row*8),6,7,Shape::Play};return true;
  }
  if (layout==padLayoutPSXNeGcon) {
    if (m==GPAD_X || m==GPAD_A) {b.shape=Shape::Hidden;return true;}
    if (m==GPAD_L2) {b={25,16,22,6,Shape::Bar};return true;}
    if (m==GPAD_R2) {b={31,25,22,6,Shape::Bar};return true;}
  }
  if (layout==flight) {
    if (m==GPAD_SELECT || m==GPAD_START) {b.y=8;b.w=7;b.shape=Shape::Pill;return true;}
    if (face || m==GPAD_L1 || m==GPAD_R1 || m==GPAD_L2 || m==GPAD_R2) {
      b.y=key.row==2?16:28;b.w=b.h=7;return true;
    }
  }
  if (layout==gun) {
    if (m==GPAD_A) b={20,17,14,9,Shape::TriggerWell};
    else if (m==GPAD_B) b={43,7,7,7,Shape::Round};
    else b={43,21,7,7,Shape::Round};
    return true;
  }
  if (layout==beatmania) {
    if (m==GPAD_SELECT || m==GPAD_START) {b={int8_t(m==GPAD_SELECT?30:40),0,7,5,Shape::Pill};return true;}
    if (m==GPAD_L1 || m==GPAD_R1) {b={int8_t(m==GPAD_L1?32:43),7,8,10,Shape::Fillet};return true;}
    b={int8_t(m==GPAD_X?27:m==GPAD_A?38:49),19,8,11,Shape::Fillet};return true;
  }
  if (layout==padLayoutPopn) {
    if (m==GPAD_SELECT || m==GPAD_START) {
      b={int8_t(m==GPAD_SELECT?12:36),0,7,5,Shape::Pill};return true;
    }
    b={int8_t(key.col+1),int8_t(key.row==1?9:21),9,9,Shape::Round};return true;
  }
  if (layout==padLayoutGuitarFreaks) {
    if (m==GPAD_Y || m==GPAD_B || m==GPAD_R2) {b={int8_t(key.col+3),5,9,15,Shape::Tombstone};return true;}
    if (m==GPAD_L2) {b={3,0,9,5,Shape::Pill};return true;}
    if (m==(GPAD_UP|GPAD_DOWN)) {b={3,10,20,5,Shape::RoundedRect};return true;}
    b={int8_t(m==GPAD_SELECT?3:12),25,5,5,Shape::Round};return true;
  }
  if (layout==padLayoutDancePad) {
    if (m==GPAD_SELECT || m==GPAD_START) {b={int8_t(m==GPAD_SELECT?14:38),0,7,5,Shape::Pill};return true;}
    b={int8_t(13+(key.col/6-1)*6),int8_t(7+(key.row-1)*11),9,9,Shape::Round};
    if (m==GPAD_UP || m==GPAD_DOWN || m==GPAD_LEFT || m==GPAD_RIGHT) {
      b.shape=Shape::Arrow;b.rotation=m==GPAD_UP?0:m==GPAD_LEFT?1:m==GPAD_DOWN?2:3;
    }
    return true;
  }
#endif
  return false;
}
inline int8_t directionShift(const PadButton* layout) {
#ifdef ENABLE_INPUT_SMS
  if (layout==padLayoutSMS) return 6;
#endif
  return 0;
}
inline int16_t column(const PadButton* layout, int16_t col) {
  // The analog panel owns x=0..53 even where it is blank. Never place digital
  // ink in that erase span; the visible outer margins differ by only one pixel.
  return layout==padLayoutN64 && col==60 ? 54 : col;
}
template<class Display> void decoration(Display& display, const PadButton* layout,
                                       uint8_t col, int8_t offset, uint8_t row) {
  if (mouse_art::selected(layout)) {
    mouse_art::decoration(display,layout,col,offset,row);
    return;
  }
#ifdef ENABLE_INPUT_PSX
  if (layout==padLayoutDancePad) {
    oled_button_art::draw(display,{25,18,9,9,Shape::Round},false,col,offset,row);
  }
  if (layout==gun) {
    // A side silhouette, with the far-side button outside the body. Its dotted
    // horizontal/45-degree leader is a diagram annotation, not another input.
    auto* surface=oled_pixel::homeSurface();
    if (!surface || !surface->active) return;
    auto line=[&](int x,int y,int tx,int ty,bool dotted) {
      const int dx=tx>x?tx-x:x-tx, dy=ty>y?y-ty:ty-y;
      const int sx=x<tx?1:-1, sy=y<ty?1:-1;
      int err=dx+dy, step=0;
      while (true) {
        const int py=row*8+offset+y, px=col+x;
        if ((!dotted || ((step%2)==0 && !gunBodyContains(x,y))) &&
            py>=25 && py<=63 && px>=0 && px<128) {
          oled_pixel::Origin origin(0,25,63);
          display.setCursor(px,py/8);
          display.ssd1306WriteRamBuf(surface->pixels[(py/8)*128+px]|(1u<<(py%8)));
        }
        if (x==tx && y==ty) break;
        const int e=2*err;
        if (e>=dy) {err+=dy;x+=sx;}
        if (e<=dx) {err+=dx;y+=sy;}
        ++step;
      }
    };
    for (uint8_t segment=1;segment<sizeof(gunOutline)/sizeof(gunOutline[0]);++segment) {
      line(gunOutline[segment-1][0],gunOutline[segment-1][1],gunOutline[segment][0],gunOutline[segment][1],false);
    }
    line(50,24,60,24,true);
    // The leader aims at the opposite side's matching location, but is occluded
    // by the opaque body. It must never look like a dotted stripe on the barrel.
    line(60,24,46,10,true);
  }
#else
  (void)display;(void)layout;(void)col;(void)offset;(void)row;
#endif
}
#else
inline bool get(const PadButton*, const PadButton&, Button&) {return false;}
inline int8_t directionShift(const PadButton*) {return 0;}
inline int16_t column(const PadButton*, int16_t col) {return col;}
#endif
} // namespace classic_pad_art
