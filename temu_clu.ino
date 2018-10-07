#include <SendOnlySoftwareSerial.h> //http://forum.arduino.cc/index.php?topic=112013.0
#include <PS2Keyboard.h>
#include <avr/pgmspace.h>
#include <C:\3x5font\font_caps.c>
#include <C:\3x5font\font_small.c>
#include <C:\3x5font\symbol_font.c>
#include <C:\3x5font\font_num.c>

#define ENABLE_INVERSE //slows it down a bit

#define UNDERLINE 4
#define INVERSE 2
#define BLINK 1

char text[320]; //Text buffer
char attrib[160]; //Text attribute buffer. 4 bits per character
char vram[1024]; //Raw image data buffer
#define TEXT(x,y) text[((y) * 32) + (x)]
int cx,cy;
unsigned char c;
char call;
char modified;
char cur_atr;
SendOnlySoftwareSerial s(5);
PS2Keyboard keyboard;
char blocking_read(){
  while(!Serial.available());
  return Serial.read();
}
void scrollup(){
  memmove(text, text+32, 288); //Copy all but last line of text up
  memmove(attrib, attrib+16, 144); //Copy all but last line of text atttribute up
  memset(text+288, 0, 32); //Clear last line
}
void scrolldn(){
  memmove(text+32, text, 288); //Copy page of text down
  memmove(attrib+16, attrib, 144); //Copy page of atrributes down
}
  
void handle_escape(){
  c = blocking_read();
  byte x,y,val;
  if(c == 'D'){
    cy++;
    if(cy > 9){
      scrollup();
      cy = 0;
    }
  }
  if(c == 'M'){
    cy--;
    if(cy < 0){cy = 0;}
  }
  if(cy == 'E'){
    cy++;
    cx=0;
    if(cy > 9){
      scrollup();
      cy = 9;
    }
  }
  if(c == '['){
    c = blocking_read();
    val = 255;
    if(isdigit(c)){
      val = c - '0';
      c = blocking_read();
      if(isdigit(c)){
        val*=10;
        val+=c-'0';
        c = blocking_read();
      }
    }
    switch(c){
      case ';':
        int val2;
        val2 = blocking_read() - '0';
        c = blocking_read();
        if(isdigit(c)){
          val2 *= 10;
          val2 += c-'0';
          c = blocking_read();
        }
        if(c == 'f' || c == 'H'){
          cy = val-1; cx = val2-1;
          if(cx > 31){cx = 31;}
          if(cx < 0){cx = 0;}
          if(cy > 9){cy = 9;}
          if(cy < 0){cy = 0;}
        }
        break;
      case 'A':cy-=(val==255)?1:val; if(cy < 0){cy = 0;} break;
      case 'B':cy+=(val==255)?1:val; if(cy > 9){cy = 9;} break;
      case 'C':cx+=(val==255)?1:val; if(cx > 31){cx = 31;} break;
      case 'D':cx-=(val==255)?1:val; if(cx < 0){cx = 0;} break;
      case 'H':cx = 0; cy =0 ; break;
      case 'K':{
        if(val == 1){
          for(x = cx;x;x--){
            TEXT(x,cy) = 0;
          }
        }
        if(val == 0 || val == 255){
        for(x = cx; x < 32;x++){
          TEXT(x,cy) = 0;
        }
        }
        if(val == 2){
          for(x = 0;x < 32;x++){
            TEXT(x, cy) = 0;
          }
        }
          break;
      case 'J':
        if(val == 0 || val == 255){
          memset(text + cy*32+cx, 0, 320 - (cy*32)+cx);
        }
        if(val == 1){
          memset(text, 0, cy*32+cx);
        }
        if(val == 2){
          memset(text, 0, 320);
        }
        break;
      }
      case 'n':
      if(val == 5){
        s.write(27);
        s.write("[0n");
      }
      if(val == 6){
        s.write(27);
        s.write("[");
        s.print((int)cx);
        s.write(";");
        s.print((int)cy);
        s.write("n");
      }
      break;
      case 'm':
        if(val == 0 || val == 255){
          cur_atr = 0;
        }
        if(val == 4){
          cur_atr |= UNDERLINE;
        }
        #ifdef ENABLE_INVERSE
        if(val == 7){
          cur_atr |= INVERSE;
        }
        #endif
        if(val == 5){
          cur_atr |= BLINK;
        }
        break;
      case 'Z': case 'c':
        s.write(27);
        s.write("[?1;0c");
        break;
    }
}
}  
void poll_serial(){ //We need to do this often to avoid dropping bytes in the tiny buffer
     while(keyboard.available()){ //Handle keyboard input
       c=keyboard.read();
       if(c == PS2_UPARROW){
         s.write(27);
         s.write("[A");
       }
       else if(c == PS2_LEFTARROW){
         s.write(27);
         s.write("[D");
       }
       else if(c == PS2_RIGHTARROW){
         s.write(27);
         s.write("[C");
       }
       else if(c == PS2_DOWNARROW){
         s.write(27);
         s.write("[B");
       }
       else s.write(c);
     }
     while(Serial.available()){ //Check if there is incoming serial and if there is, handle it
      c = Serial.read();
      if(c == 27){handle_escape(); continue;}
      if(c == '\n' || cx > 31){
      cx = 0; cy ++;
      if(cy > 9){
        scrollup();
        cy = 9;
      }
    }
    if(c == '\r'){
        cx = 0;
      }
    if(c == 8 && cx > 0){cx--;}
    if(c > 31){
    text[(cy * 32) + cx] = c;
    attrib[(cy * 16) + cx/2] &= cx&1?0xF0:0x0F;
    attrib[(cy * 16) + cx/2] |= cx&1?cur_atr:cur_atr<<4;
    cx++;
    }
  }
}
void inline putpixel(int x, int y, int c){
  vram[x + (y / 8)*128] &= ~(1 << (y % 8));
  vram[x + (y / 8)*128] |= (!!c)*(1 << (y % 8));
  modified = 1; //Set modified flag
}
void render_char(byte x, byte y, prog_uchar *data, char attrib){
     byte sy,d;
      for(sy = 0;sy < 5;sy++){
       d = pgm_read_byte(data + sy);
       #ifdef ENABLE_INVERSE
       if(attrib & INVERSE){
         d = ~d; //Invert the pixels
       }
       putpixel(x+3, sy+y, attrib & INVERSE);
       #endif
       putpixel(x, sy+y, d & 1);
       putpixel(x+1, sy+y, d & 2);
       putpixel(x+2, sy+y, d & 4);
       poll_serial();
     }
        putpixel(x, y+5, call && cx*4 == x && cy*6 == y);
        putpixel(x+1, y+5, call && cx*4 == x && cy*6 == y);
        putpixel(x+2, y+5, call && cx*4 == x && cy*6 == y);
        putpixel(x+3, y+5, call && cx*4 == x && cy*6 == y);
        if(attrib & UNDERLINE){ //Draw the underline
        putpixel(x, y+5, 1);
        putpixel(x+1, y+5, 1);
        putpixel(x+2, y+5, 1);
        putpixel(x+3, y+5, 1);
        }

     
}  
void eraseBlock(int x1, int y1, int x2, int y2) 
{	
  int x,y;
  for(x = x1;x < x2;x++){
    poll_serial();
    for(y = y1;y < y2;y++){
      putpixel(x,y,0);
    }
    
  }
        putpixel(cx*4, cy*6+5, call);
        putpixel(cx*4+1, cy*6+5, call);
        putpixel(cx*4+2, cy*6+5, call);
        putpixel(cx*4+3, cy*6+5, call);
}
void print_character(byte x, byte y, char c, char atr){ //Map character to font bitmap
  unsigned char *d;
  switch(c){
    case '3': d = _3_bits; break;
    case '4': d = _4_bits; break;
    case '6': d = _6_bits; break;
    case '7': d = _7_bits; break;
    case '8': d = _8_bits; break;
    case '9': d = _9_bits; break;
    case '0': d = _0_bits; break;
    case 'a': d = as_bits; break;
    case 'b': d = bs_bits; break;
    case 'c': d = cs_bits; break;
    case 'd': d = ds_bits; break;
    case 'e': d = es_bits; break;
    case 'f': d = fs_bits; break;
    case 'g': d = gs_bits; break;
    case 'h': d = hs_bits; break;
    case 'i': d = is_bits; break;
    case 'j': d = js_bits; break;
    case 'l': case'1': d = ls_bits; break;
    case 'n': d = ns_bits; break;  
    case 'o': d = os_bits; break;
    case 'p': d = ps_bits; break;
    case 'q': d = qs_bits; break;
    case 'r': d = rs_bits; break;
    case 's': d = ss_bits; break;
    case 't': d = ts_bits; break;
    case 'u': d = us_bits; break;
    case 'v': d = vs_bits; break;
    case 'w': d = ws_bits; break;
    case 'x': d = xs_bits; break;
    case 'y': d = ys_bits; break;
    case 'A': d = A_bits; break;
    case 'B': d = B_bits; break; 
    case 'C': d = C_bits; break;
    case 'D': d = D_bits; break; 
    case 'E': d = E_bits; break;
    case 'F': d = F_bits; break; 
    case 'G': d = G_bits; break;
    case 'H':  d = H_bits; break; 
    case 'I': d = I_bits; break; 
    case 'J': d = J_bits; break;
    case 'K': case 'k': d = K_bits; break; 
    case 'L': d = L_bits; break; 
    case 'M': case 'm': d = M_bits; break;
    case 'N': d = N_bits; break; 
    case 'O': d = O_bits; break;
    case 'P': d = P_bits; break; 
    case 'Q': d = Q_bits; break;
    case 'R': d = R_bits; break; 
    case 'S': case'5': d = S_bits; break;
    case 'T': d = T_bits; break; 
    case 'U': d = U_bits; break;
    case 'V': d = V_bits; break;
    case 'W': d = W_bits; break; 
    case 'X': d = X_bits; break;
    case 'Y': d = Y_bits; break; 
    case 'Z': case 'z': case'2': d = Z_bits; break;
    case '#': d=hash_bits; break;
    case '$': d=dollar_bits; break;
    case '@': d=at_bits; break;
    case '?': d=question_bits; break;
    case '_': d=underscore_bits; break;
    case '-': d=dash_bits; break;
    case '!': d=exclaimation_bits; break;
    case '.': d=period_bits; break;
    case ',': d=comma_bits; break;
    case '(': d=leftp_bits; break;
    case ')': d=rightp_bits; break;
    case ':': d=colon_bits; break;
    case '\\': d=backslash_bits; break;
    case '/': d=slash_bits; break;
    case '~': d=tilde_bits; break;
    case '"': d=quote_bits; break;
    case '\'': d=squote_bits; break;
    case '|': d=bar_bits; break;
    case '>': d=gt_bits; break;
    case '<': d=lt_bits; break;
    case '+': d=plus_bits; break;
    case '*': d=star_bits; break;
    case '%': d=percent_bits; break;
    case '=': d=equals_bits; break;
    case ';': d=semicolon_bits; break;
    case '{': d=leftbrace_bits; break;
    case '}': d=rightbrace_bits; break;
    case '[': d=leftsquare_bits; break;
    case ']': d=rightsquare_bits; break;
    case '`': d=backtick_bits; break;
    case '^': d=caret_bits; break;
    case ' ': d=space_bits; break;
    default: d = NULL;
  }
  if(d){
    render_char(x, y, d,  atr);
  } else {
    eraseBlock(x, y, x+4, y+6);
  }
}
/*void print_string(byte x, byte y, char *s){
  byte cx = x;
  int i;
  for(i = 0;s[i];i++){
    print_character(cx, y, s[i]);
    cx+=4;
    delay(10);
  }
}*/
void update_display(){
  int x,y;
  char atr;
  call=!call; //Hacky. For blinking, only draw blinking elements every other call
  for(x = 0;x < 32;x++){
    for(y = 0;y < 10;y++){
      poll_serial();
     atr = x&1? attrib[(y * 16) + (x / 2)] & 0x0F: (attrib[(y * 16) + (x / 2)] & 0xF0) >> 4; //Get the attribute for the character
      c = text[(y * 32) + x];
      if((atr & BLINK) && call){c = 0;} 
      print_character(x*4, y * 6, c, atr);
    }
  }
  if(modified){ //Write data to serial display if it was updated since last call
  Serial.write(0x7C);
  Serial.write(0x16);
  Serial.write((byte)0);
  Serial.write((byte)0);
  Serial.write((byte)7);
  Serial.write(128);
  Serial.write(64);
	for(int x = 0; x < 1024; x++) {
		Serial.write(vram[x]);
                poll_serial();
	}
        modified = 0;
     }
}
void setup(){
  call = 1;
  modified = 1;
  s.begin(115200);
  Serial.begin(115200);
  keyboard.begin(6, 3);
  Serial.write(0x7C); //Display initiation sequence
  Serial.write(0x06);
  Serial.write(0x7C);
  Serial.write(0x02);
  Serial.write(0xFF);
  memset(text, 0, 320);
  memset(attrib, 0, 160);
  memset(vram, 0,1024);
}

void loop(){
  poll_serial();
  update_display();
}


