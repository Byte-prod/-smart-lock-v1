/*
 * lcd.h â€? LCD Display Driver (ST7735 or compatible)
 *
 * Hardware:
 *   GND=GND  VCC=3.3V
 *   SCL=PB3  SDA=PB5  RES=3V3
 *   DC=PD3   CS=PD4   BLK=3V3
 */

#ifndef USER_LCD_H_
#define USER_LCD_H_

/* Display orientation: 0/1=horizontal, 2/3=vertical */
#define USE_HORIZONTAL 1

#if USE_HORIZONTAL==0 || USE_HORIZONTAL==1
#define LCD_W 128
#define LCD_H 128
#else
#define LCD_W 128
#define LCD_H 128
#endif

/* LCD Control Pins */
#define LCD_RES_Clr()  GPIO_ResetBits(GPIOB, GPIO_Pin_4)  /* RES */
#define LCD_RES_Set()  GPIO_SetBits(GPIOB, GPIO_Pin_4)
#define LCD_DC_Clr()   GPIO_ResetBits(GPIOD, GPIO_Pin_3)  /* DC  */
#define LCD_DC_Set()   GPIO_SetBits(GPIOD, GPIO_Pin_3)
#define LCD_CS_Clr()   GPIO_ResetBits(GPIOD, GPIO_Pin_4)  /* CS  */
#define LCD_CS_Set()   GPIO_SetBits(GPIOD, GPIO_Pin_4)
#define LCD_BLK_Clr()  GPIO_ResetBits(GPIOD, GPIO_Pin_5)  /* BLK */
#define LCD_BLK_Set()  GPIO_SetBits(GPIOD, GPIO_Pin_5)

/*
 * 16-bit TFT Color Definitions (RGB565)
 *   R(5bit) + G(6bit) + B(5bit) = 16bit
 *   Formula: Color = (R<<11) | (G<<5) | B
 */

/* Primary Colors */
#define WHITE            0xFFFF  /* R:31 G:63 B:31 */
#define BLACK            0x0000  /* R:0  G:0  B:0  */
#define BLUE             0x001F  /* R:0  G:0  B:31 */
#define RED              0xF800  /* R:31 G:0  B:0  */
#define GREEN            0x07E0  /* R:0  G:63 B:0  */
#define MAGENTA          0xF81F  /* R:31 G:0  B:31 */
#define CYAN             0x7FFF  /* R:0  G:63 B:31 */
#define YELLOW           0xFFE0  /* R:31 G:63 B:0  */

/* Brown/Brick Tones */
#define BROWN            0xBC40
#define BRRED            0xFC07
#define GRAY             0x8430

/* Blue Tones */
#define DARKBLUE         0x01CF
#define LIGHTBLUE        0x7D7C
#define GRAYBLUE         0x5458
#define LBBLUE           0x2B12  /* selected item background */
#define LGRAYBLUE        0xA651  /* middle background */

/* Red/Pink Tones */
#define BRED             0xF81F
#define GRED             0xFFE0

/* Green Tones */
#define LIGHTGREEN       0x841F

/* Gray Tones */
#define LGRAY            0xC618  /* panel background */

/* Other */
#define GBLUE            0x07FF

/* API Functions */
void LCD_GPIO_Init(void);
void LCD_WR_DATA8(u8 dat);
void LCD_WR_DATA(u16 dat);
void LCD_WR_REG(u8 dat);
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_Init(void);

void LCD_Fill(u16 xsta, u16 ysta, u16 xend, u16 yend, u16 color);
void LCD_DrawPoint(u16 x, u16 y, u16 color);
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void Draw_Circle(u16 x0, u16 y0, u8 r, u16 color);

void LCD_Show_Chinese(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese16x16(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese24x24(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese32x32(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);

void LCD_ShowChar(u16 x, u16 y, u8 num, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowString(u16 x, u16 y, const u8 *p, u16 fc, u16 bc, u8 sizey, u8 mode);
u32 mypow(u8 m, u8 n);
void LCD_ShowIntNum(u16 x, u16 y, u16 num, u8 len, u16 fc, u16 bc, u8 sizey);
void LCD_ShowFloatNum1(u16 x, u16 y, float num, u8 len, u16 fc, u16 bc, u8 sizey);

void LCD_ShowPicture(u16 x, u16 y, u16 length, u16 width, const u8 pic[]);

void lcd_task(void);

#endif /* USER_LCD_H_ */
