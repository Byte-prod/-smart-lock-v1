/*
 * lcd.c
 *
 *  Created on: Dec 30, 2022
 
 */
#include "debug.h"
#include "font_ascii_16x8.h"
#include "pic.h"
#include "driver_all.h"

void  SPI3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));


void SPI_LCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOD, ENABLE );
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3,ENABLE);

    //CS DC BL 背光控制
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_SetBits(GPIOD, GPIO_Pin_3);   //DC
    GPIO_SetBits(GPIOD, GPIO_Pin_4);   //CS
    GPIO_SetBits(GPIOD, GPIO_Pin_5);   //BL

    //SPI接口初始化
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_15);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOB, &GPIO_InitStructure );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;// GPIO_Mode_IN_FLOATING;
    GPIO_Init( GPIOB, &GPIO_InitStructure );

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOB, &GPIO_InitStructure );

    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI3, &SPI_InitStructure);

    SPI_Cmd(SPI3, ENABLE);

}

//if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET);

void SPI3_IRQHandler(void)
{





}





u8 spi_readwrite(u8 TxData)
{
    u8 i=0;

    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET)
    {
        i++;
        if(i>200)return 0;
    }

    SPI_I2S_SendData(SPI3, TxData);
    i=0;

    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_RXNE) == RESET)
    {
        i++;
        if(i>200)return 0;
    }

    return SPI_I2S_ReceiveData(SPI3);
}

/******************************************************************************
      向LCD写入8位数据
      输入参数: dat 要写入的数据
      返回值: 无
******************************************************************************/
void LCD_WR_DATA8(u8 dat)
{
    LCD_CS_Clr();
    LCD_DC_Set();
//    LCD_Writ_Bus(dat);
//    SPI_DataSizeConfig(SPI3,SPI_DataSize_8b);
    spi_readwrite(dat);
    LCD_CS_Set();
}


/******************************************************************************
      向LCD写入16位数据
      输入参数: dat 要写入的数据
      返回值: 无
******************************************************************************/
void LCD_WR_DATA(u16 dat)
{
//    LCD_Writ_Bus(dat>>8);
//    LCD_Writ_Bus(dat);
//    SPI_DataSizeConfig(SPI3,SPI_DataSize_16b);
    LCD_CS_Clr();
    LCD_DC_Set();
    spi_readwrite(dat>>8);
    spi_readwrite(dat);

    LCD_CS_Set();
}


/******************************************************************************
      向LCD写入寄存器命令
      输入参数: dat 要写入的命令
      返回值: 无
******************************************************************************/
void LCD_WR_REG(u8 dat)
{
    LCD_CS_Clr();
    LCD_DC_Clr();//写命令模式
//    LCD_Writ_Bus(dat);
//    SPI_DataSizeConfig(SPI3,SPI_DataSize_8b);
    spi_readwrite(dat);

    LCD_CS_Set();
}


/******************************************************************************
      设置LCD的显示区域
      输入参数: x1,x2 设置列的起始和结束地址
                y1,y2 设置行的起始和结束地址
      返回值: 无
******************************************************************************/
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2)
{
    if(USE_HORIZONTAL==0)
    {
        LCD_WR_REG(0x2a);//列地址设置
        LCD_WR_DATA(x1+2);
        LCD_WR_DATA(x2+2);
        LCD_WR_REG(0x2b);//行地址设置
        LCD_WR_DATA(y1+1);
        LCD_WR_DATA(y2+1);
        LCD_WR_REG(0x2c);//开始写入
    }
    else if(USE_HORIZONTAL==1)
    {
        LCD_WR_REG(0x2a);//列地址设置
        LCD_WR_DATA(x1+2);
        LCD_WR_DATA(x2+2);
        LCD_WR_REG(0x2b);//列地址设置
        LCD_WR_DATA(y1+3);
        LCD_WR_DATA(y2+3);
        LCD_WR_REG(0x2c);//开始写入
    }
    else if(USE_HORIZONTAL==2)
    {
        LCD_WR_REG(0x2a);//列地址设置
        LCD_WR_DATA(x1+1);
        LCD_WR_DATA(x2+1);
        LCD_WR_REG(0x2b);//列地址设置
        LCD_WR_DATA(y1+2);
        LCD_WR_DATA(y2+2);
        LCD_WR_REG(0x2c);//开始写入
    }
    else
    {
        LCD_WR_REG(0x2a);//列地址设置
        LCD_WR_DATA(x1+3);
        LCD_WR_DATA(x2+3);
        LCD_WR_REG(0x2b);//列地址设置
        LCD_WR_DATA(y1+2);
        LCD_WR_DATA(y2+2);
        LCD_WR_REG(0x2c);//开始写入
    }
}
//--------------------------------------------------------------------------------------------------------/
/*********************************************************************************************************/
/*******************         ST7735驱动初始化                          **************************************/
/*********************************************************************************************************/
void LCD_Init(void)
{
    SPI_LCD_Init();         //初始化GPIO

    LCD_RES_Clr();           //复位

    Delay_Ms(100);
    LCD_RES_Set();
    Delay_Ms(100);

    LCD_BLK_Set();          //打开背光
    Delay_Ms(100);

    LCD_WR_REG(0x11);       //Sleep out
    Delay_Ms(120);          //Delay 120ms
    //------------------------------------ST7735S Frame rate-------------------------------------------------//
    LCD_WR_REG(0xB1);       //Frame rate 80Hz
    LCD_WR_DATA8(0x05);  //02
    LCD_WR_DATA8(0x3C);  //35
    LCD_WR_DATA8(0x3C);  //36

    LCD_WR_REG(0xB2);       //Frame rate 80Hz
    LCD_WR_DATA8(0x05); //02
    LCD_WR_DATA8(0x3C); //35
    LCD_WR_DATA8(0x3C); //36
    LCD_WR_REG(0xB3);       //Frame rate 80Hz
    LCD_WR_DATA8(0x05); //02
    LCD_WR_DATA8(0x3C); //35
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x3C);
    //------------------------------------End ST7735S Frame rate-------------------------------------------//
    LCD_WR_REG(0xB4);      //Dot inversion
    LCD_WR_DATA8(0x03);

    LCD_WR_REG(0x3a);     // Interface pixel format
  //LCD_WR_DATA8(0x03);    // 12-bit/pixel RGB 4-4-4 (4k colors)
    LCD_WR_DATA8(0x05);    // 16-bit/pixel RGB 5-6-5 (65k colors)
  //LCD_WR_DATA8(0x06);    // 18-bit/pixel RGB 6-6-6 (256k colors)

    //------------------------------------ST7735S Power Sequence-----------------------------------------//
    LCD_WR_REG(0xC0);
    LCD_WR_DATA8(0xA2);
    LCD_WR_DATA8(0x02);
    LCD_WR_DATA8(0x84);
    LCD_WR_REG(0xC1);
    LCD_WR_DATA8(0xC5);
    LCD_WR_REG(0xC2);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xC3);
    LCD_WR_DATA8(0x8D);
    LCD_WR_DATA8(0x2A);
    LCD_WR_REG(0xC4);
    LCD_WR_DATA8(0x8D);
    LCD_WR_DATA8(0xEE);
    //---------------------------------End ST7735S Power Sequence---------------------------------------//
    LCD_WR_REG(0xC5);                                      //VCOM
    LCD_WR_DATA8(0x0a);

    LCD_WR_REG(0x36);                                      //显示方向设置
    if(USE_HORIZONTAL==0)LCD_WR_DATA8(0x08);
    else if(USE_HORIZONTAL==1)LCD_WR_DATA8(0xC8);          //水平方向 0XC8
    else if(USE_HORIZONTAL==2)LCD_WR_DATA8(0x78);
    else LCD_WR_DATA8(0xA8);
    //---------------------------------ST7735S Gamma Sequence-----------------------------------------//
     LCD_WR_REG(0XE0);
     LCD_WR_DATA8(0x12);
     LCD_WR_DATA8(0x1C);
     LCD_WR_DATA8(0x10);
     LCD_WR_DATA8(0x18);
     LCD_WR_DATA8(0x33);
     LCD_WR_DATA8(0x2C);
     LCD_WR_DATA8(0x25);
     LCD_WR_DATA8(0x28);
     LCD_WR_DATA8(0x28);
     LCD_WR_DATA8(0x27);
     LCD_WR_DATA8(0x2F);
     LCD_WR_DATA8(0x3C);
     LCD_WR_DATA8(0x00);
     LCD_WR_DATA8(0x03);
     LCD_WR_DATA8(0x03);
     LCD_WR_DATA8(0x10);
     LCD_WR_REG(0XE1);
     LCD_WR_DATA8(0x12);
     LCD_WR_DATA8(0x1C);
     LCD_WR_DATA8(0x10);
     LCD_WR_DATA8(0x18);
     LCD_WR_DATA8(0x2D);
     LCD_WR_DATA8(0x28);
     LCD_WR_DATA8(0x23);
     LCD_WR_DATA8(0x28);
     LCD_WR_DATA8(0x28);
     LCD_WR_DATA8(0x26);
     LCD_WR_DATA8(0x2F);
     LCD_WR_DATA8(0x3B);
     LCD_WR_DATA8(0x00);
     LCD_WR_DATA8(0x03);
     LCD_WR_DATA8(0x03);
     LCD_WR_DATA8(0x10);
    //------------------------------------End ST7735S Gamma Sequence-----------------------------------------//
    LCD_WR_REG(0x20);     // Display inversion off
  //LCD_WR_REG(0x21);     // Display inversion on

    LCD_WR_REG(0x13);     // Partial mode off


    LCD_WR_REG(0x2A);  //Column addr set
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x7F);

    LCD_WR_REG(0x2B);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x7F);


//  LCD_WR_REG(0x21);                  //????????????????????
    LCD_WR_REG(0x29); //Display on

}


/******************************************************************************
      在指定区域内填充颜色
      输入参数: xsta,ysta   起始坐标
                xend,yend   结束坐标
                                color       填充颜色
      返回值: 无
******************************************************************************/
void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color)
{
    u16 i,j;
    LCD_Address_Set(xsta,ysta,xend-1,yend-1);//设置显示区域
    for(i=ysta;i<yend;i++)
    {
        for(j=xsta;j<xend;j++)
        {
            LCD_WR_DATA(color);
        }
    }
}

/******************************************************************************
      在指定位置画一个点
      输入参数: x,y 坐标
                color 颜色
      返回值: 无
******************************************************************************/
void LCD_DrawPoint(u16 x,u16 y,u16 color)
{
    LCD_Address_Set(x,y,x,y);//设置单个像素地址
    LCD_WR_DATA(color);
}


/******************************************************************************
      在两点之间画一条直线
      输入参数: x1,y1   起点坐标
                x2,y2   终点坐标
                color   颜色
      返回值: 无
******************************************************************************/
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color)
{
    u16 t;
    int xerr=0,yerr=0,delta_x,delta_y,distance;
    int incx,incy,uRow,uCol;
    delta_x=x2-x1; //计算x方向增量
    delta_y=y2-y1;
    uRow=x1;//设置直线起点坐标
    uCol=y1;
    if(delta_x>0)incx=1; //设置x方向增量
    else if (delta_x==0)incx=0;//垂直线
    else {incx=-1;delta_x=-delta_x;}
    if(delta_y>0)incy=1;
    else if (delta_y==0)incy=0;//水平线
    else {incy=-1;delta_y=-delta_x;}
    if(delta_x>delta_y)distance=delta_x; //选择较长的轴作为步进基准
    else distance=delta_y;
    for(t=0;t<distance+1;t++)
    {
        LCD_DrawPoint(uRow,uCol,color);//画点
        xerr+=delta_x;
        yerr+=delta_y;
        if(xerr>distance)
        {
            xerr-=distance;
            uRow+=incx;
        }
        if(yerr>distance)
        {
            yerr-=distance;
            uCol+=incy;
        }
    }
}


/******************************************************************************
      在指定位置画一个矩形
      输入参数: x1,y1   起点坐标
                x2,y2   终点坐标
                color   矩形边框颜色
      返回值: 无
******************************************************************************/
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color)
{
    LCD_DrawLine(x1,y1,x2,y1,color);
    LCD_DrawLine(x1,y1,x1,y2,color);
    LCD_DrawLine(x1,y2,x2,y2,color);
    LCD_DrawLine(x2,y1,x2,y2,color);
}


/******************************************************************************
      在指定位置画一个圆
      输入参数: x0,y0   圆心坐标
                r       半径
                color   颜色
      返回值: 无
******************************************************************************/
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color)
{
    int a,b;
    a=0;b=r;
    while(a<=b)
    {
        LCD_DrawPoint(x0-b,y0-a,color);             //3
        LCD_DrawPoint(x0+b,y0-a,color);             //0
        LCD_DrawPoint(x0-a,y0+b,color);             //1
        LCD_DrawPoint(x0-a,y0-b,color);             //2
        LCD_DrawPoint(x0+b,y0+a,color);             //4
        LCD_DrawPoint(x0+a,y0-b,color);             //5
        LCD_DrawPoint(x0+a,y0+b,color);             //6
        LCD_DrawPoint(x0-b,y0+a,color);             //7
        a++;
        if((a*a+b*b)>(r*r))//判断是否需要调整半径
        {
            b--;
        }
    }
}

/******************************************************************************
      显示中文字符串
      输入参数: x,y 坐标
                *s 要显示的字符串
                fc 前景色
                bc 背景色
                sizey 字号: 可选 16 24 32
                mode:  0非叠加显示  1叠加显示
      返回值: 无
******************************************************************************/
void LCD_Show_Chinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    while (*s)
    {
     LCD_ShowChinese( x, y, s, fc, bc, sizey, mode);
     x+=16;
     if (x>127)
     {
        x=0;
        y+=16;
     }
     s+=2;

    }

}


void LCD_ShowChinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    while(*s!=0)
    {
        if(sizey==16) LCD_ShowChinese16x16(x,y,s,fc,bc,sizey,mode);
        else if(sizey==24) LCD_ShowChinese24x24(x,y,s,fc,bc,sizey,mode);
        else if(sizey==32) LCD_ShowChinese32x32(x,y,s,fc,bc,sizey,mode);
        else return;
        s+=2;
        x+=sizey;
    }
}

/******************************************************************************
      显示单个16x16中文字符
      输入参数: x,y 坐标
                *s 要显示的汉字
                fc 前景色
                bc 背景色
                sizey 字号
                mode:  0非叠加显示  1叠加显示
      返回值: 无
******************************************************************************/
void LCD_ShowChinese16x16(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    u8 i,j;
    u16 k;
    u16 HZnum;//汉字个数
    u16 TypefaceNum;//一个字符所需的字节数
    u16 x0=x;
    TypefaceNum=sizey/8*sizey;//一个字符占用的字节数 = 字号/8*字号
                              //16x16需要32个字节,因为每个字节8位,一行需要2字节
    HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);  //统计汉字个数
    for(k=0;k<HZnum;k++)
    {
        if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
        {
            LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
            for(i=0;i<TypefaceNum;i++)
            {
                for(j=0;j<8;j++)
                {
                    if(!mode)//非叠加显示
                    {
                        if(tfont16[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
                        else LCD_WR_DATA(bc);
                    }
                    else//叠加显示
                    {
                        if(tfont16[k].Msk[i]&(0x01<<j)) LCD_DrawPoint(x,y,fc);//画一个点
                        x++;
                        if((x-x0)==sizey)
                        {
                            x=x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;  //查找下一个汉字,跳过已找到的继续搜索
    }
}


/******************************************************************************
      显示单个24x24中文字符
      输入参数: x,y 坐标
                *s 要显示的汉字
                fc 前景色
                bc 背景色
                sizey 字号
                mode:  0非叠加显示  1叠加显示
      返回值: 无
******************************************************************************/
void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    u8 i,j;
    u16 k;
    u16 HZnum;//汉字个数
    u16 TypefaceNum;//一个字符所需的字节数
    u16 x0=x;
    TypefaceNum=sizey/8*sizey;//一个字符占用的字节数 = 字号/8*字号
                              //24x24需要72个字节,因为每个字节8位,一行需要3字节
    HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);  //统计汉字个数
    for(k=0;k<HZnum;k++)
    {
        if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
        {
            LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
            for(i=0;i<TypefaceNum;i++)
            {
                for(j=0;j<8;j++)
                {
                    if(!mode)//非叠加显示
                    {
                        if(tfont24[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
                        else LCD_WR_DATA(bc);
                    }
                    else//叠加显示
                    {
                        if(tfont24[k].Msk[i]&(0x01<<j)) LCD_DrawPoint(x,y,fc);//画一个点
                        x++;
                        if((x-x0)==sizey)
                        {
                            x=x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;  //查找下一个汉字,跳过已找到的继续搜索
    }
}


/******************************************************************************
      显示单个32x32中文字符
      输入参数: x,y 坐标
                *s 要显示的汉字
                fc 前景色
                bc 背景色
                sizey 字号
                mode:  0非叠加显示  1叠加显示
      返回值: 无
******************************************************************************/
void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    u8 i,j;
    u16 k;
    u16 HZnum;//汉字个数
    u16 TypefaceNum;//一个字符所需的字节数
    u16 x0=x;
    TypefaceNum=sizey/8*sizey;//一个字符占用的字节数 = 字号/8*字号
                              //32x32需要128个字节,因为每个字节8位,一行需要4字节
    HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);  //统计汉字个数
    for(k=0;k<HZnum;k++)
    {
        if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
        {
            LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
            for(i=0;i<TypefaceNum;i++)
            {
                for(j=0;j<8;j++)
                {
                    if(!mode)//非叠加显示
                    {
                        if(tfont32[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
                        else LCD_WR_DATA(bc);
                    }
                    else//叠加显示
                    {
                        if(tfont32[k].Msk[i]&(0x01<<j)) LCD_DrawPoint(x,y,fc);//画一个点
                        x++;
                        if((x-x0)==sizey)
                        {
                            x=x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;  //查找下一个汉字,跳过已找到的继续搜索
    }
}


/******************************************************************************
      在指定位置显示一个ASCII字符
      输入参数: x,y 坐标
                num 要显示的字符
                fc 前景色
                bc 背景色
                sizey 字号
                mode:  0非叠加显示  1叠加显示
      返回值: 无
******************************************************************************/
void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    u8 temp,sizex,t;
    u16 i,TypefaceNum;//一个字符所需的字节数
    u16 x0=x;
    sizex=sizey/2;
    TypefaceNum=sizex/8*sizey;
    num=num-' ';    //得到偏移量
    LCD_Address_Set(x,y,x+sizex-1,y+sizey-1);  //设置单个字符地址
    for(i=0;i<TypefaceNum;i++)
    {
        if(sizey==16)temp=ascii_1608[num][i];              //选择8x16字体
        else if(sizey==32)temp=ascii_3216[num][i];       //选择16x32字体
        else return;
        for(t=0;t<8;t++)
        {
            if(!mode)//非叠加显示
            {
                if(temp&(0x01<<t))LCD_WR_DATA(fc);
                else LCD_WR_DATA(bc);
            }
            else//叠加显示
            {
                if(temp&(0x01<<t)) LCD_DrawPoint(x,y,fc);//画一个点
                x++;
                if((x-x0)==sizex)
                {
                    x=x0;
                    y++;
                    break;
                }
            }
        }
    }
}


/******************************************************************************
      显示ASCII字符串
      输入参数: x,y 坐标
                *p 要显示的字符串
                fc 前景色
                bc 背景色
                sizey 字号
                mode:  0非叠加显示  1叠加显示
      返回值: 无
******************************************************************************/
void LCD_ShowString(u16 x,u16 y,const u8 *p,u16 fc,u16 bc,u8 sizey,u8 mode)
{
    while(*p!='\0')
    {
        LCD_ShowChar(x,y,*p,fc,bc,sizey,mode);
        x+=sizey/2;
        p++;
    }
}


/******************************************************************************
      m的n次方计算
      输入参数: m底数 n指数
      返回值: 计算结果
******************************************************************************/
u32 mypow(u8 m,u8 n)
{
    u32 result=1;
    while(n--)result*=m;
    return result;
}


/******************************************************************************
      在指定位置显示整数
      输入参数: x,y 坐标
                num 要显示的整数
                len 显示的数字位数
                fc 前景色
                bc 背景色
                sizey 字号
      返回值: 无
******************************************************************************/
void LCD_ShowIntNum(u16 x,u16 y,u16 num,u8 len,u16 fc,u16 bc,u8 sizey)
{
    u8 t,temp;
    u8 enshow=0;
    u8 sizex=sizey/2;
    for(t=0;t<len;t++)
    {
        temp=(num/mypow(10,len-t-1))%10;
        if(enshow==0&&t<(len-1))
        {
            if(temp==0)
            {
                LCD_ShowChar(x+t*sizex,y,' ',fc,bc,sizey,0);
                continue;
            }else enshow=1;

        }
        LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
    }
}


/******************************************************************************
      在指定位置显示浮点数
      输入参数: x,y 坐标
                num 要显示的浮点数
                len 显示的数字位数
                fc 前景色
                bc 背景色
                sizey 字号
      返回值: 无
******************************************************************************/
void LCD_ShowFloatNum1(u16 x,u16 y,float num,u8 len,u16 fc,u16 bc,u8 sizey)
{
    u8 t,temp,sizex;
    u16 num1;
    sizex=sizey/2;
    num1=num*100;
    for(t=0;t<len;t++)
    {
        temp=(num1/mypow(10,len-t-1))%10;
        if(t==(len-2))
        {
            LCD_ShowChar(x+(len-2)*sizex,y,'.',fc,bc,sizey,0);
            t++;
            len+=1;
        }
        LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
    }
}


/******************************************************************************
      在指定位置显示图片
      输入参数: x,y      起始坐标
                length   图片长度
                width    图片宽度
                pic[]    图片数据
      返回值: 无
******************************************************************************/
void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width,const u8 pic[])
{
    u16 i,j,k=0;
    LCD_Address_Set(x,y,x+length-1,y+width-1);
    for(i=0;i<length;i++)
    {
        for(j=0;j<width;j++)
        {
            LCD_WR_DATA8(pic[k*2]);
            LCD_WR_DATA8(pic[k*2+1]);
            k++;
        }
    }
}

u8 show_flag;//系统显示状态标志
u8 show_flag_old;

void lcd_task () 
{ 
   
    if (key_index != key_index_old)//按键输入变化检测
    { u8 i = key_index;
      LCD_Fill(16,45,112,66,GBLUE);//清空输入区域
      if (key_index == 7) key_index = 6;//防越界
     
     while (i--)//显示星号
     {
        if (i<6)
        {
        LCD_ShowChar(20+16*i,45,'*',RED,GBLUE,16,0);
        }
        
     }
    
      key_index_old = key_index;
    }

    if (show_flag != show_flag_old)//显示状态变化检测
    {
        LCD_Fill(0,0,127,32,WHITE);//清空顶部区域

        show_flag_old = show_flag;
        
    }
    switch (show_flag) 
    {
        case 0://密码验证主页
            LCD_Show_Chinese(32,0,"门已上锁",BRRED,WHITE,16,0);
            LCD_Show_Chinese(24,16,"请输入密码",BRRED,WHITE,16,0);
            
        break;
        case 1://门已解锁
            LCD_Show_Chinese(32,0,"门已解锁",BRRED,WHITE,16,0);
            LCD_Show_Chinese(32,16,"欢迎回家",BRRED,WHITE,16,0);   
        break;
        case 2://密码锁定倒计时
            LCD_ShowIntNum(58,16,time_30000ms,2,RED,WHITE,16);
        break;
        case 3://请输入管理员密码
        LCD_Show_Chinese(0,0,"请输入管理员密码",BRRED,WHITE,16,0);
        break;
        case 4://请输入新密码
        LCD_Show_Chinese(8,0,"管理员密码正确",BRRED,WHITE,16,0);
        LCD_Show_Chinese(16,16,"请输入新密码",BRRED,WHITE,16,0);
        break;
        case 5://管理员密码错误
        LCD_Show_Chinese(8,0,"管理员密码错误",BRRED,WHITE,16,0);
        LCD_Show_Chinese(24,16,"请重新输入",BRRED,WHITE,16,0);
        break;
        case 6://设置成功
        LCD_Show_Chinese(32,0,"设置成功",BRRED,WHITE,16,0);
        break;
        case 7://请放置卡片
        LCD_Show_Chinese(24,0,"请放置卡片",BRRED,WHITE,16,0);
        break;
        case 8://刷卡成功
        LCD_Show_Chinese(32,0,"刷卡成功",BRRED,WHITE,16,0);
        LCD_Show_Chinese(32,16,"欢迎回家",BRRED,WHITE,16,0);   
        break;
        case 9://请放置手指
        LCD_Show_Chinese(24,0,"请放置手指",BRRED,WHITE,16,0);
        break;
        case 12: // WiFi connected
        LCD_Show_Chinese(24,0,"网络已连接",BRRED,WHITE,16,0);
        break;
        case 13: // WiFi disconnected
        LCD_Show_Chinese(24,0,"网络已断开",BRRED,WHITE,16,0);
        break;

    }



}