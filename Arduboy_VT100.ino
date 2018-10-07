//============================================
// adapted from the code by smeezekitty at:
//
// https://www.youtube.com/watch?v=VEhVO7zY0RI
//============================================

#include <Arduboy2.h>
#include "st7565_font.h"

Arduboy2 arduboy;
BeepPin1 beep;

#define UNDERLINE 4
#define INVERSE 2
#define BLINK 1

#define BUFFERSIZE 128
#define COLUMNS 16
#define ROWS 8

char text[BUFFERSIZE]; //Text buffer
// char attrib[BUFFERSIZE/2]; //Text attribute buffer, 4 bits per character
#define TEXT(x,y) text[((y) * COLUMNS) + (x)]
int8_t cx, cy;
unsigned char c;
char cur_atr;

char blocking_read(){
  while(!Serial.available());
  return Serial.read();
}

void scrollup(){
  memmove(text, text+COLUMNS, BUFFERSIZE-COLUMNS); //Copy all but last line of text up
// memmove(attrib, attrib+(COLUMNS/2), (BUFFERSIZE-COLUMNS)/2); //Copy all but last line of text atttribute up
  memset(text+(BUFFERSIZE-COLUMNS), 0x00, COLUMNS); //Clear last line
}

void scrolldn(){
  memmove(text+COLUMNS, text, BUFFERSIZE-COLUMNS); //Copy page of text down
// memmove(attrib+(COLUMNS/2), attrib, (BUFFERSIZE-COLUMNS)/2); //Copy page of atrributes down
}

void handle_escape(){
  c = blocking_read();
  byte x,val,val2;
  if(c == 'D'){ //Move down one line
    cy++;
    if(cy > (ROWS-1)){
      scrollup();
      cy = ROWS-1;
    }
  }
  else if(c == 'M'){ //Move up one line
    cy--;
    if(cy < 0){cy = 0;}
  }
  else if(c == 'E'){ //Move to next line
    cy++;
    cx=0;
    if(cy > (ROWS-1)){
      scrollup();
      cy = ROWS-1;
    }
  }
  else if(c == '(' || c == ')'){ // character set
    c = blocking_read();
  }
  else if(c == '['){
    c = blocking_read();
    val = 255;
    if(isdigit(c)){
      val = c - '0';
      c = blocking_read();
      if(isdigit(c)){
        val *= 10;
        val += c - '0';
        c = blocking_read();
      }
    }
    switch(c){
      case ';':
        val2 = blocking_read() - '0';
        c = blocking_read();
        if(isdigit(c)){
          val2 *= 10;
          val2 += c - '0';
          c = blocking_read();
        }
        if(c == 'f' || c == 'H'){ //Move cursor to screen location v,h
          cy = val-1; cx = val2-1;
          if(cx > (COLUMNS-1)){cx = COLUMNS-1;}
          if(cx < 0){cx = 0;}
          if(cy > (ROWS-1)){cy = ROWS-1;}
          if(cy < 0){cy = 0;}
        }
        break;
      case 'A':cy-=(val==255)?1:val; if(cy < 0){cy = 0;} break; //Move cursor up n lines
      case 'B':cy+=(val==255)?1:val; if(cy > (ROWS-1)){cy = ROWS-1;} break; //Move cursor down n lines
      case 'C':cx+=(val==255)?1:val; if(cx > (COLUMNS-1)){cx = COLUMNS-1;} break; //Move cursor right n lines
      case 'D':cx-=(val==255)?1:val; if(cx < 0){cx = 0;} break; //Move cursor left n lines
      case 'H':cx = 0; cy = 0; break; //Move cursor to upper left corner
      case 'd':cy = val - 1; break; //Move to line n
      case 'q':
        if(val == 0 || val == 255){ //Turn off all four leds
          arduboy.digitalWriteRGB(RGB_OFF, RGB_OFF, RGB_OFF);
        }
        if(val == 1){ //Turn on LED #1
          arduboy.digitalWriteRGB(RED_LED, RGB_ON);
        }
        if(val == 2){ //Turn on LED #2
          arduboy.digitalWriteRGB(GREEN_LED, RGB_ON);
        }
        if(val == 3){ //Turn on LED #3
          arduboy.digitalWriteRGB(BLUE_LED, RGB_ON);
        }
        break;
      case 'K':
        if(val == 1){ //Clear line from cursor left
          for(x = cx;x;x--){
            TEXT(x,cy) = 0x00;
          }
        }
        if(val == 0 || val == 255){ //Clear line from cursor right
          for(x = cx;x < COLUMNS;x++){
            TEXT(x,cy) = 0x00;
          }
        }
        if(val == 2){
          for(x = 0;x < COLUMNS;x++){ //Clear entire line
            TEXT(x, cy) = 0x00;
          }
        }
        break;
      case 'J':
        if(val == 0 || val == 255){ //Clear screen from cursor down
          for(uint8_t i=((cy*COLUMNS)+cx); i < BUFFERSIZE; i++){
            text[i] = 0x00;
          }
        }
        if(val == 1){ //Clear screen from cursor up
          memset(text, 0x00, (cy*COLUMNS)+cx);
        }
        if(val == 2){ //Clear entire screen
          memset(text, 0x00, BUFFERSIZE);
        }
        break;
      case 'm':
        if(val == 0 || val == 255){
          cur_atr = 0;
        }
        if(val == 4){
          cur_atr |= UNDERLINE;
        }
        if(val == 7){
          cur_atr |= INVERSE;
        }
        if(val == 5){
          cur_atr |= BLINK;
        }
        break;
    }
  }
}

void poll_serial(){ //We need to do this often to avoid dropping bytes in the tiny buffer
  while(Serial.available()){ //Check if there is incoming serial and if there is, handle it
    c = Serial.read();
    if(c == 27){handle_escape(); continue;} //Escape
    if(c == '\n' || cx > (COLUMNS-1)){ //Line Feed
      cx = 0; cy ++;
      if(cy > (ROWS-1)){
        scrollup();
        cy = ROWS-1;
      }
    }
    else if(c == '\r'){cx = 0;} //Carriage Return
    else if(c == '\t' && cx < (COLUMNS-9)){cx = cx + 8;} //Tab
    else if(c == 8 && cx > 0){cx--;} //Back Space
    else if(c == 7){beep.tone(1116, 30);} //Bell (Play a 895Hz tone)
    else if(c > 31){
      text[(cy * COLUMNS) + cx] = c - 32;
//    attrib[(cy * (COLUMNS/2)) + cx/2] &= cx&1?0xF0:0x0F;
//    attrib[(cy * (COLUMNS/2)) + cx/2] |= cx&1?cur_atr:cur_atr<<4;
      cx++;
    }
  }
}

// adapted from C++ version of Arduboy2Core::paintScreen()
void update_display(){
  uint8_t d;
  uint8_t i = 0;

  for(uint8_t n = 0; n < BUFFERSIZE; n++)
  {
    SPDR = st7565_font[text[n]][i++]; // set the first SPI data byte to get things started

    // the code to iterate the loop and get the next byte from the buffer is
    // executed while the previous byte is being sent out by the SPI controller
    while (i < 8)
    {
      // get the next byte. It's put in a local variable so it can be sent as
      // as soon as possible after the sending of the previous byte has completed
      d = st7565_font[text[n]][i++];

      while (!(SPSR & _BV(SPIF))) { } // wait for the previous byte to be sent

      // put the next byte in the SPI data register. The SPI controller will
      // clock it out while the loop continues and gets the next byte ready
      SPDR = d;
    }

    i = 0;

    while (!(SPSR & _BV(SPIF))) { } // wait for the last byte to be sent
  }
}

void setup(){
  arduboy.boot();
  arduboy.audio.begin();
  beep.begin();
  SPCR |= _BV(DORD); // LSB of the data word is transmitted first

  Serial.begin(9600);
  memset(text, 0x00, BUFFERSIZE);
// memset(attrib, 0, BUFFERSIZE/2);
}

void loop(){
  poll_serial();
  if (arduboy.nextFrame())
  {
    beep.timer(); // handle tone duration
    update_display();
  }
}
