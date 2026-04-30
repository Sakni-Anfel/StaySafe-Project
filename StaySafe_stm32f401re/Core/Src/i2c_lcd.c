#include "i2c_lcd.h"
#include "main.h"

#define LCD_EN  0x04
#define LCD_RS  0x01
#define LCD_RW  0x02
#define LCD_BL  0x08

static void lcd_write4(I2C_LCD_HandleTypeDef *lcd, uint8_t data)
{
    HAL_I2C_Master_Transmit(lcd->hi2c, lcd->address, &data, 1, 10);
    HAL_Delay(1);
    data |= LCD_EN;
    HAL_I2C_Master_Transmit(lcd->hi2c, lcd->address, &data, 1, 10);
    HAL_Delay(1);
    data &= ~LCD_EN;
    HAL_I2C_Master_Transmit(lcd->hi2c, lcd->address, &data, 1, 10);
    HAL_Delay(1);
}

void lcd_send_cmd(I2C_LCD_HandleTypeDef *lcd, char cmd)
{
    uint8_t hi = (cmd & 0xF0) | LCD_BL;
    uint8_t lo = ((cmd << 4) & 0xF0) | LCD_BL;
    lcd_write4(lcd, hi);
    lcd_write4(lcd, lo);
}

void lcd_send_data(I2C_LCD_HandleTypeDef *lcd, char data)
{
    uint8_t hi = (data & 0xF0) | LCD_BL | LCD_RS;
    uint8_t lo = ((data << 4) & 0xF0) | LCD_BL | LCD_RS;
    lcd_write4(lcd, hi);
    lcd_write4(lcd, lo);
}

void lcd_init(I2C_LCD_HandleTypeDef *lcd)
{
    HAL_Delay(50);

    // wake up sequence — 3x send 0x30 as raw nibble
    lcd_write4(lcd, 0x30 | LCD_BL);
    HAL_Delay(5);
    lcd_write4(lcd, 0x30 | LCD_BL);
    HAL_Delay(1);
    lcd_write4(lcd, 0x30 | LCD_BL);
    HAL_Delay(1);

    // switch to 4-bit
    lcd_write4(lcd, 0x20 | LCD_BL);
    HAL_Delay(1);

    // configure
    lcd_send_cmd(lcd, 0x28); HAL_Delay(1);  // 4-bit, 2 lines, 5x8
    lcd_send_cmd(lcd, 0x08); HAL_Delay(1);  // display off
    lcd_send_cmd(lcd, 0x01); HAL_Delay(2);  // clear
    lcd_send_cmd(lcd, 0x06); HAL_Delay(1);  // entry mode
    lcd_send_cmd(lcd, 0x0C); HAL_Delay(1);  // display on
}

void lcd_gotoxy(I2C_LCD_HandleTypeDef *lcd, int col, int row)
{
    uint8_t address;
    switch (row) {
        case 0: address = 0x80 + col; break;
        case 1: address = 0xC0 + col; break;
        case 2: address = 0x94 + col; break;
        case 3: address = 0xD4 + col; break;
        default: return;
    }
    lcd_send_cmd(lcd, address);
}

void lcd_puts(I2C_LCD_HandleTypeDef *lcd, char *str)
{
    while (*str) lcd_send_data(lcd, *str++);
}

void lcd_putchar(I2C_LCD_HandleTypeDef *lcd, char ch)
{
    lcd_send_data(lcd, ch);
}

void lcd_clear(I2C_LCD_HandleTypeDef *lcd)
{
    lcd_send_cmd(lcd, 0x01);
    HAL_Delay(2);
}