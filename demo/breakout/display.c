#include <emscripten.h>
// ST7735 Constants
#define ST7735_NOP      0x0
#define ST7735_SWRESET  0x01
#define ST7735_RDDID    0x04
#define ST7735_RDDST    0x09
#define ST7735_SLPIN    0x10
#define ST7735_SLPOUT   0x11
#define ST7735_PTLON    0x12
#define ST7735_NORON    0x13
#define ST7735_INVOFF   0x20
#define ST7735_INVON    0x21
#define ST7735_DISPOFF  0x28
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_RAMRD    0x2E
#define ST7735_COLMOD   0x3A
#define ST7735_MADCTL   0x36
#define ST7735_FRMCTR1  0xB1
#define ST7735_FRMCTR2  0xB2
#define ST7735_FRMCTR3  0xB3
#define ST7735_INVCTR   0xB4
#define ST7735_DISSET5  0xB6
#define ST7735_PWCTR1   0xC0
#define ST7735_PWCTR2   0xC1
#define ST7735_PWCTR3   0xC2
#define ST7735_PWCTR4   0xC3
#define ST7735_PWCTR5   0xC4
#define ST7735_VMCTR1   0xC5
#define ST7735_RDID1    0xDA
#define ST7735_RDID2    0xDB
#define ST7735_RDID3    0xDC
#define ST7735_RDID4    0xDD
#define ST7735_PWCTR6   0xFC
#define ST7735_GMCTRP1  0xE0
#define ST7735_GMCTRN1  0xE1
//Constants
#define D0    16
#define D1    5
#define D2    4
#define D3    0
#define D4    2
#define D5    14
#define D6    12
#define D7    13
#define D8    15
#define D9    3
#define D10   1
//PIN configurations
#define CS    D1
#define RESET D3
#define DC    D2
#define SDA   D7
#define SCK   D5
//Arduino constants
#define OUTPUT 1
#define INPUT  0
#define LOW    0
#define HIGH   1
//Type declarations 
typedef int bool;
//Imports from WARDuino
extern void write_spi_byte(int c);
extern void spi_begin();
extern void chip_digital_write(int  a,int b);
extern int  chip_digital_read(int  a);
extern void chip_pin_mode(int a,int b);
extern void chip_delay_us(int x);
//extern void chip_fill_rect(int a, int b, int c, int d,int e);




//Writing LCD pins
void LCD_SCK(bool b)
{
    chip_digital_write(SCK, b);
}

void LCD_SDO(bool b)
{
    chip_digital_write(SDA, b);
}

void LCD_RS(bool b)
{
    chip_digital_write(DC, b);
}

void LCD_CS(bool b)
{
    chip_digital_write(CS, b);
}

void LCD_RESET(bool b)
{
    chip_digital_write(RESET, b);
}

void writecommand(unsigned char c)
{
    LCD_RS(LOW);
    LCD_CS(LOW);
    write_spi_byte(c);
    LCD_CS(HIGH);
}

void writedata(unsigned char c)
{
    LCD_RS(HIGH);
    LCD_CS(LOW);
    write_spi_byte(c);
    LCD_CS(HIGH);
}

void setAddrWindow(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1)
{
    writecommand(ST7735_CASET);  // column addr set
    writedata(0x00);
    writedata(x0+0);   // XSTART
    writedata(0x00);
    writedata(x1+0);   // XEND
    
    writecommand(ST7735_RASET);  // row addr set
    writedata(0x00);
    writedata(y0+0);    // YSTART
    writedata(0x00);
    writedata(y1+0);    // YEND
    
    writecommand(ST7735_RAMWR);  // write to RAM
}

#define SCREEN_WIDTH  128 
#define SCREEN_HEIGHT 160

//extern void chip_fill_screen(unsigned int color);
extern void write_spi_bytes_16(int times, int color);

void chip_fill_rect(int x, int y, int w, int h,int color) {
	setAddrWindow(x, y, x+w-1, y+h-1);
	// setup for data
	LCD_RS(HIGH);
	LCD_CS(LOW);
        write_spi_bytes_16(w*h, color);
	/*unsigned char colorB = color >> 8;
	for (x=0; x < w; x++) {
		for (y=0; y < h; y++) {
			write_spi_byte(colorB);    
			write_spi_byte(color);    
		}
	}*/
	LCD_CS(HIGH);
}

void chip_fill_screen(int color)
{
	setAddrWindow(0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1);
	// setup for data
	LCD_RS(HIGH);
	LCD_CS(LOW);
        write_spi_bytes_16(SCREEN_WIDTH*SCREEN_HEIGHT, color);
	/*unsigned char colorB = color >> 8;
	for (int x=0; x < SCREEN_WIDTH*SCREEN_HEIGHT ; x++) {
		write_spi_byte(colorB);    
		write_spi_byte(color);    
	}*/
	LCD_CS(HIGH);
}

void ST7735_initR(void) {
    LCD_RESET(HIGH);
    chip_delay_us(500);
    LCD_RESET(LOW);
    chip_delay_us(500);
    LCD_RESET(HIGH);
    chip_delay_us(500);
    LCD_CS(LOW);
    
    writecommand(ST7735_SWRESET); // software reset
    chip_delay_us(150);
    
    writecommand(ST7735_SLPOUT);  // out of sleep mode
    chip_delay_us(500);
    
    writecommand(ST7735_COLMOD);  // set color mode
    writedata(0x05);        // 16-bit color
    chip_delay_us(10);
    
    writecommand(ST7735_FRMCTR1);  // frame rate control - normal mode
    writedata(0x01);  // frame rate = fosc / (1 x 2 + 40) * (LINE + 2C + 2D)
    writedata(0x2C);
    writedata(0x2D);
    
    writecommand(ST7735_FRMCTR2);  // frame rate control - idle mode
    writedata(0x01);  // frame rate = fosc / (1 x 2 + 40) * (LINE + 2C + 2D)
    writedata(0x2C);
    writedata(0x2D);
    
    writecommand(ST7735_FRMCTR3);  // frame rate control - partial mode
    writedata(0x01); // dot inversion mode
    writedata(0x2C);
    writedata(0x2D);
    writedata(0x01); // line inversion mode
    writedata(0x2C);
    writedata(0x2D);
    
    writecommand(ST7735_INVCTR);  // display inversion control
    writedata(0x07);  // no inversion
    
    writecommand(ST7735_PWCTR1);  // power control
    writedata(0xA2);
    writedata(0x02);      // -4.6V
    writedata(0x84);      // AUTO mode
    
    writecommand(ST7735_PWCTR2);  // power control
    writedata(0xC5);      // VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    
    writecommand(ST7735_PWCTR3);  // power control
    writedata(0x0A);      // Opamp current small
    writedata(0x00);      // Boost frequency
    
    writecommand(ST7735_PWCTR4);  // power control
    writedata(0x8A);      // BCLK/2, Opamp current small & Medium low
    writedata(0x2A);
    
    writecommand(ST7735_PWCTR5);  // power control
    writedata(0x8A);
    writedata(0xEE);
    
    writecommand(ST7735_VMCTR1);  // power control
    writedata(0x0E);
    
    writecommand(ST7735_INVOFF);    // don't invert display
    
    writecommand(ST7735_MADCTL);  // memory access control (directions)
    
    // http://www.adafruit.com/forums/viewtopic.php?f=47&p=180341
    
    // R and B byte are swapped
    // madctl = 0xC8;
    
    // normal R G B order
    // madctl = 0xC0;
    writedata(0xC0);  // row address/col address, bottom to top refresh
    
    writecommand(ST7735_COLMOD);  // set color mode
    writedata(0x05);        // 16-bit color
    
    writecommand(ST7735_CASET);  // column addr set
    writedata(0x00);
    writedata(0x00);   // XSTART = 0
    writedata(0x00);
    writedata(0x7F);   // XEND = 127
    
    writecommand(ST7735_RASET);  // row addr set
    writedata(0x00);
    writedata(0x00);    // XSTART = 0
    writedata(0x00);
    writedata(0x9F);    // XEND = 159
    writecommand(ST7735_GMCTRP1);
    writedata(0x0f);
    writedata(0x1a);
    writedata(0x0f);
    writedata(0x18);
    writedata(0x2f);
    writedata(0x28);
    writedata(0x20);
    writedata(0x22);
    writedata(0x1f);
    writedata(0x1b);
    writedata(0x23);
    writedata(0x37);
    writedata(0x00);
    writedata(0x07);
    writedata(0x02);
    writedata(0x10);
    writecommand(ST7735_GMCTRN1);
    writedata(0x0f);
    writedata(0x1b);
    writedata(0x0f);
    writedata(0x17);
    writedata(0x33);
    writedata(0x2c);
    writedata(0x29);
    writedata(0x2e);
    writedata(0x30);
    writedata(0x30);
    writedata(0x39);
    writedata(0x3f);
    writedata(0x00);
    writedata(0x07);
    writedata(0x03);
    writedata(0x10);
    writecommand(ST7735_DISPON);
    chip_delay_us(100);
    writecommand(ST7735_NORON);  // normal display on
    chip_delay_us(10);
    LCD_CS(HIGH);
}


struct Coord { int x,y; };

struct Snake { 
	struct Coord body[20];
        int size;
} snake;

typedef struct Snake snakeT;

void  setCoord(snakeT s,int index,int x, int y) {
	s.body[index].x = x;
	s.body[index].y = y;
}

void initSnake() {
	snake.size = 5;
	int x = 0;
	while(x < 5) {
 	 setCoord(snake,x,x,6);
         x++;
	}
}

void renderSnake() {
	int x = 0;
        chip_fill_screen(0);
	while(x < 5) {
		struct Coord coord = snake.body[x++];
    		chip_fill_rect(coord.x*10,coord.y*10,10,10,0x0FF);
	}	
}


int max(int a, int b) {
	return a>b ?  a : b; 
}

int min(int a, int b) {
	return a <b ? a : b;
}


/* 
	Ball Abstraction 
*/
#define BSIZE 10
#define BCOL  0x0F0
#define BGCOL 0xF00
/*
void update() {
    	chip_fill_rect((int)(b_x/100),(int)(b_y/100),BSIZE,BSIZE,BGCOL); 
	b_x += s_x;
	b_y += s_y;	
   	chip_fill_rect((int)(b_x/100),(int)(b_y/100),BSIZE,BSIZE,BCOL); 
	if(b_x > 12800) {
		s_x = -s_x;
	}
	if(b_y > 16400) {
		s_y = s_y*-1;
	}
	if(b_y < 0) {
		s_y = s_y*-1;
	}
	if(b_x < 0) {
		s_x = s_x*-1;
	}
} 

void initBall() {
	b_x = 1000;
	b_y = 1000;
	s_x = 50;
	s_y = 50;
}
*/

int EMSCRIPTEN_KEEPALIVE main() {
    //ball state
    int b_x,b_y;
    int s_x,s_y;
    //end ball state
    //pin mode
    chip_pin_mode(CS,    OUTPUT);
    chip_pin_mode(DC,    OUTPUT);
    chip_pin_mode(SDA,   OUTPUT);
    chip_pin_mode(SCK,   OUTPUT);
    chip_pin_mode(RESET, OUTPUT);
    chip_pin_mode(D4, INPUT);

    spi_begin();
    ST7735_initR();
    chip_fill_screen(0xF00);
    int x = 64-20; 
    //init ball
	b_x = 1000;
	b_y = 1000;
	s_x = 50;
	s_y = 50;
    //end init ball
    while(1) {
	//update ball
	chip_fill_rect((int)(b_x/100),(int)(b_y/100),BSIZE,BSIZE,BGCOL); 
	b_x += s_x;
	b_y += s_y;	
   	chip_fill_rect((int)(b_x/100),(int)(b_y/100),BSIZE,BSIZE,BCOL); 
	//bounce of the wall
	if(b_x > 12800) { s_x = -s_x; }
	if(b_y > 16400) { s_y = s_y*-1; }
	if(b_y < 0) { s_y = s_y*-1; }
	if(b_x < 0) { s_x = s_x*-1; }
	//end update ball
	if(x > 128) {
		chip_fill_rect(x,150,40,5,0xF00); 
		x = 0;
   		chip_fill_rect(x,150,40,5,0x00F); 
	}
	else{
		if(chip_digital_read(D4)) {
			chip_fill_rect(x,150,1,5,0xF00); 
			x = x+1;
   			chip_fill_rect(x+39,150,1,5,0x00F); 
		} else {
			chip_fill_rect(x+39,150,1,5,0xF00); 
			x = x-1;
   			chip_fill_rect(x,150,1,5,0x00F); 
		}
	}	
	//chip_delay_us(500);
    }
}

