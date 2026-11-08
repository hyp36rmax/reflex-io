#pragma once
#include "oled_button_art.h"
#include "menu_pad_layouts.h"

// Physical wheel artwork, separate from input decoding and USB mappings. Saturn
// pedals are contacts; Dreamcast paddles retain their analog pressure range.
namespace classic_wheel_art {
inline void direction(uint8_t steering,int& c,int& s) {
  static const int8_t cosine[17]={0,6,12,18,23,27,30,31,32,31,30,27,23,18,12,6,0};
  static const int8_t sine[17]={-32,-31,-30,-27,-23,-18,-12,-6,0,6,12,18,23,27,30,31,32};
  const uint8_t angle=(uint16_t(steering)*16+127)/255;
  c=cosine[angle];s=sine[angle];
}
inline bool yokeInside(int x,int y) {
  // Compact local silhouette around the steering pivot. Its swept radius fits
  // between the stationary buttons and above Start, even at either lock.
  const int a=x<0?-x:x;
  const int top=a<=8?-3-a*6/8:-9+(a-8)*2;
  if (a>11 || y<top || y>6+a/6) return false;
  return !(y>=-3 && y<=4 && a>=7+(y+3)/4 && a<=9+(y+3)/4);
}
inline int roundedYokeCoordinate(int value,int scale=32) {
  return (value+(value<0?-scale/2:scale/2))/scale;
}
inline bool capsuleInside(int u,int v,int ax,int ay,int bx,int by,int radius) {
  // Coordinates stay in 1/32-pixel units through rotation. Round caps and joins
  // are evaluated before rasterizing, rather than magnifying a jagged bitmap.
  u-=ax*32;v-=ay*32;
  const int dx=bx-ax,dy=by-ay,span=dx*dx+dy*dy;
  // Subpixel coverage keeps the cap extrema three pixels wide instead of
  // leaving a lone "spike" where an integer-radius circle just touches a row.
  const int r=radius*32+12;
  const int along=u*dx+v*dy,r2=r*r;
  if (along<=0) return u*u+v*v<=r2;
  if (along>=span*32) {u-=dx*32;v-=dy*32;return u*u+v*v<=r2;}
  const int cross=u*dy-v*dx;
  return cross*cross<=r2*span;
}
inline bool wideYokeInside(int u,int v) {
  const int a=u<0?-u:u;
  if (a>19*32 || v<-19*32 || v>19*32) return false;
  // Broad rounded grips, a shallow scooped top, and a gently bowed lower rim.
  // The center face joins both rims but leaves open hand grips on either side.
  if (v>=-4*32-a/3 && v<=6*32 && a<=8*32-(v+7*32)*2/13) return true;
  return capsuleInside(u,v,-10,-8,-13,7,3) ||
         capsuleInside(u,v,10,-8,13,7,3) ||
         capsuleInside(u,v,-10,-8,-5,-5,2) ||
         capsuleInside(u,v,-5,-5,0,-4,2) ||
         capsuleInside(u,v,0,-4,5,-5,2) ||
         capsuleInside(u,v,5,-5,10,-8,2) ||
         capsuleInside(u,v,-13,7,-6,5,3) ||
         capsuleInside(u,v,-6,5,6,5,3) ||
         capsuleInside(u,v,6,5,13,7,3);
}
inline bool rotatedYokeInside(int x,int y,int c,int s,bool wide=false) {
  x-=wide?64:30;y-=wide?19:16;
  const int u=x*c+y*s,v=-x*s+y*c;
  if (wide) return wideYokeInside(u,v);
  return yokeInside(roundedYokeCoordinate(u),roundedYokeCoordinate(v));
}
inline bool yokePixel(int x,int y,int c,int s,bool wide=false) {
  if (!rotatedYokeInside(x,y,c,s,wide)) return false;
  // Outline after rotation, rather than rotating a thin raster outline: this
  // keeps the rim and hand openings closed at diagonal steering angles.
  return !rotatedYokeInside(x-1,y,c,s,wide) || !rotatedYokeInside(x+1,y,c,s,wide) ||
         !rotatedYokeInside(x,y-1,c,s,wide) || !rotatedYokeInside(x,y+1,c,s,wide) ||
         (!wide && x==30 && y==16);
}
inline bool yokeStartInside(int x,int y,int c,int s) {
  x-=64;y-=19;
  if (x<-6 || x>6 || y<-6 || y>6) return false;
  return capsuleInside(x*c+y*s,-x*s+y*c,-2,0,2,0,2);
}
inline bool yokeStartPixel(int x,int y,int c,int s,bool pressed,bool labels,const uint8_t* font) {
  if (labels) {
    x-=64;y-=19;
    const int bx=roundedYokeCoordinate(x*c+y*s)+2;
    const int by=roundedYokeCoordinate(-x*s+y*c)+3;
    if (bx<0 || bx>=5 || by<0 || by>=7) return false;
    const uint8_t* glyph=font+6+('S'-font[4])*5;
    return bool(glyph[bx]&(1u<<by))!=pressed;
  }
  if (!yokeStartInside(x,y,c,s)) return false;
  return pressed || !yokeStartInside(x-1,y,c,s) || !yokeStartInside(x+1,y,c,s) ||
                    !yokeStartInside(x,y-1,c,s) || !yokeStartInside(x,y+1,c,s);
}
inline bool dreamcastPixel(int x,int y,int c,int s,bool wide=false) {
  x-=wide?64:30;y-=wide?19:17;
  const int radius=4*(x*x+y*y);
  const int outer=wide?35:29,inner=wide?29:25;
  if (radius>outer*outer) return false;
  // Half-pixel radii produce broad rounded cardinal extrema, not lone spikes.
  // The three solid spokes meet a hollow horn boss; Start is outside the wheel.
  if (radius>=inner*inner) return true;
  const int boss=wide?11:9,hole=wide?7:5;
  if (radius<=boss*boss) return radius>=hole*hole || radius<=4;
  const int u=x*c+y*s,v=-x*s+y*c;
  const int au=u<0?-u:u,av=v<0?-v:v;
  const int spoke=wide?64:32;
  return (av<=spoke && au>=96) || (au<=spoke && v>=96);
}
struct Key {oled_button_art::Button button;uint32_t mask;char name;};
inline constexpr Key saturnWideKeys[]={
  // Six-pixel circles, four-pixel gaps and a half-row rise. Keep the outer
  // anchors fixed; compact the inner keys outward for more yoke clearance.
  {{14,12,6,6,oled_button_art::Shape::Round},GPAD_L1,'Z'},
  {{24,16,6,6,oled_button_art::Shape::Round},GPAD_Y,'Y'},
  {{34,20,6,6,oled_button_art::Shape::Round},GPAD_X,'X'},
  {{109,12,6,6,oled_button_art::Shape::Round},GPAD_R1,'C'},
  {{99,16,6,6,oled_button_art::Shape::Round},GPAD_B,'B'},
  {{89,20,6,6,oled_button_art::Shape::Round},GPAD_A,'A'}};
inline constexpr Key dreamcastWideKeys[]={
  {{22,2,9,9,oled_button_art::Shape::Round},GPAD_DOWN,'-'},
  {{22,16,9,9,oled_button_art::Shape::Round},GPAD_B,'B'},
  {{97,2,9,9,oled_button_art::Shape::Round},GPAD_UP,'+'},
  {{97,16,9,9,oled_button_art::Shape::Round},GPAD_A,'A'},
  {{22,31,9,5,oled_button_art::Shape::Pill},GPAD_START,'S'}};

inline oled_button_art::Button wheelKeyBounds(oled_button_art::Button b,bool labels) {
  if (labels) {
    // Wheel label mode uses an unscaled 5x7 glyph, including larger 1P keys.
    b.x+=(int(b.w)-5)/2;
    if (b.h>7) b.y+=(b.h-7)/2;
    b.w=5;b.h=7;
  }
  return b;
}

template<class Display,class Background> void panel(Display& display, uint32_t state,
    uint8_t leftPedal,uint8_t rightPedal,uint8_t col,uint8_t row,bool labels,
    const uint8_t* font,const Key* keys,uint8_t count,Background background,
    uint8_t width=61,uint8_t pedalInset=0,uint8_t pedalWidth=5,
    uint8_t pedalTop=4,uint8_t pedalHeight=25) {
  // Compose in the existing Home framebuffer. The OLED receives one diff flush,
  // not a clear-then-redraw sequence. Overlaid spokes/buttons cannot erase peers.
  oled_pixel::Origin origin(1,row*8+1,row*8+39);
  const uint8_t rightPedalX=width-pedalInset-pedalWidth;
  for (uint8_t page=0;page<5;++page) for (uint8_t x=0;x<width;++x) {
    uint8_t bits=0;
    for (uint8_t bit=0;bit<8;++bit) {
      const uint8_t y=page*8+bit;
      if (y>=39) continue;
      bool ink=background(x,y);
      const bool onLeftPedal=x>=pedalInset && x<pedalInset+pedalWidth;
      const bool onRightPedal=x>=rightPedalX && x<rightPedalX+pedalWidth;
      if ((onLeftPedal || onRightPedal) && y>=pedalTop && y<pedalTop+pedalHeight) {
        const uint8_t px=x-(onLeftPedal?pedalInset:rightPedalX),py=y-pedalTop;
        const uint8_t fill=onLeftPedal?leftPedal:rightPedal;
        const oled_button_art::Button pedal={0,0,pedalWidth,pedalHeight,oled_button_art::Shape::Fillet};
        ink=oled_button_art::pixel(pedal,px,py,false) ||
            (px>0 && px<pedalWidth-1 && py>0 && py<pedalHeight-1 && py>=pedalHeight-1-fill);
      }
      for (uint8_t i=0;i<count;++i) {
        auto b=wheelKeyBounds(keys[i].button,labels);
        const int bx=int(x)-b.x,by=int(y)-b.y;
        if (bx<0 || bx>=b.w || by<0 || by>=b.h) continue;
        const bool pressed=bool(state&keys[i].mask);
        if (labels) {
          const uint8_t* glyph=font+6+(uint8_t(keys[i].name)-font[4])*5;
          ink=by<7 && bool(glyph[bx]&(1u<<by))!=pressed;
        } else ink=oled_button_art::pixel(b,bx,by,pressed);
      }
      if (ink) bits|=1u<<bit;
    }
    display.setCursor(col+x,row+page);display.ssd1306WriteRamBuf(bits);
  }
}
template<class Display> void draw(Display& display,uint8_t steering,uint32_t state,
    uint8_t col,uint8_t row,bool pedals,bool buttons,bool axis,bool labels,const uint8_t* font,
    bool fullWidth=false) {
  if (!pedals && !buttons && !axis) return;
  int c,s;direction(steering,c,s);
  if (fullWidth) {
    // One wheel owns all 128 columns below the unchanged logo/status rows.
    // Start is part of the center face and rotates with the yoke.
    panel(display,state,(state&GPAD_UP)?27:0,(state&GPAD_DOWN)?27:0,0,row,labels,font,
          saturnWideKeys,6,[&](int x,int y){
            return yokePixel(x,y,c,s,true) || yokeStartPixel(x,y,c,s,bool(state&GPAD_START),labels,font);
          },128,5,7,3,29);
    return;
  }
  using oled_button_art::Shape;
  static const Key keys[]={
    {{7,4,7,7,Shape::Round},GPAD_L1,'Z'},{{7,14,7,7,Shape::Round},GPAD_Y,'Y'},
    {{7,24,7,7,Shape::Round},GPAD_X,'X'},{{47,4,7,7,Shape::Round},GPAD_R1,'C'},
    {{47,14,7,7,Shape::Round},GPAD_B,'B'},{{47,24,7,7,Shape::Round},GPAD_A,'A'},
    {{27,32,7,5,Shape::Pill},GPAD_START,'S'}};
  panel(display,state,(state&GPAD_UP)?23:0,(state&GPAD_DOWN)?23:0,col,row,labels,font,
        keys,7,[&](int x,int y){return yokePixel(x,y,c,s);});
}
template<class Display> void drawDreamcast(Display& display,uint8_t steering,uint32_t state,
    uint8_t leftPedal,uint8_t rightPedal,uint8_t col,uint8_t row,bool labels,const uint8_t* font,
    bool fullWidth=false) {
  int c,s;direction(steering,c,s);
  if (fullWidth) {
    panel(display,state,leftPedal,rightPedal,0,row,labels,font,dreamcastWideKeys,5,
          [&](int x,int y){return dreamcastPixel(x,y,c,s,true);},128,2,7,3,29);
    return;
  }
  using oled_button_art::Shape;
  static const Key keys[]={
    {{8,7,7,7,Shape::Round},GPAD_DOWN,'-'},{{8,22,7,7,Shape::Round},GPAD_B,'B'},
    {{46,7,7,7,Shape::Round},GPAD_UP,'+'},{{46,22,7,7,Shape::Round},GPAD_A,'A'},
    {{7,32,9,5,Shape::Pill},GPAD_START,'S'}};
  panel(display,state,leftPedal,rightPedal,col,row,labels,font,keys,5,
        [&](int x,int y){return dreamcastPixel(x,y,c,s);});
}
} // namespace classic_wheel_art
