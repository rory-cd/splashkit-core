//
//  test_raspi_i2c_led_quad_14segment.cpp
//  splashkit
//
//  Created by Olivia McKeon https://github.com/omckeon
//

#include <iostream>
#include "raspi_gpio.h"

#include "basics.h"
#include "utils.h"

using namespace std;
using namespace splashkit_lib;

/** ===== QUAD 7-segment ===== **

    Segment names for 14-segment alphanumeric displays.

    See https://learn.adafruit.com/14-segment-alpha-numeric-led-featherwing/usage

    -------A-------
    |\     |     /|
    | \    J    / |
    |   H  |  K   |
    F    \ | /    B
    |     \|/     |
    |--G1--|--G2--|
    |     /|\     |
    E    / | \    C
    |   L  |   N  |
    | /    M    \ |
    |/     |     \|
    -------D-------  DP
*/

// #define ALPHANUM_SEG_A 0b0000000000000001  ///< Alphanumeric segment A
// #define ALPHANUM_SEG_B 0b0000000000000010  ///< Alphanumeric segment B
// #define ALPHANUM_SEG_C 0b0000000000000100  ///< Alphanumeric segment C
// #define ALPHANUM_SEG_D 0b0000000000001000  ///< Alphanumeric segment D
// #define ALPHANUM_SEG_E 0b0000000000010000  ///< Alphanumeric segment E
// #define ALPHANUM_SEG_F 0b0000000000100000  ///< Alphanumeric segment F
// #define ALPHANUM_SEG_G1 0b0000000001000000 ///< Alphanumeric segment G1
// #define ALPHANUM_SEG_G2 0b0000000010000000 ///< Alphanumeric segment G2
// #define ALPHANUM_SEG_H 0b0000000100000000  ///< Alphanumeric segment H
// #define ALPHANUM_SEG_J 0b0000001000000000  ///< Alphanumeric segment J
// #define ALPHANUM_SEG_K 0b0000010000000000  ///< Alphanumeric segment K
// #define ALPHANUM_SEG_L 0b0000100000000000  ///< Alphanumeric segment L
// #define ALPHANUM_SEG_M 0b0001000000000000  ///< Alphanumeric segment M
// #define ALPHANUM_SEG_N 0b0010000000000000  ///< Alphanumeric segment N
// #define ALPHANUM_SEG_DP 0b0100000000000000 ///< Alphanumeric segment DP

const uint16_t alphafonttable[] = {
    0b0000000000000001,
    0b0000000000000010,
    0b0000000000000100,
    0b0000000000001000,
    0b0000000000010000,
    0b0000000000100000,
    0b0000000001000000,
    0b0000000010000000,
    0b0000000100000000,
    0b0000001000000000,
    0b0000010000000000,
    0b0000100000000000,
    0b0001000000000000,
    0b0010000000000000,
    0b0100000000000000,
    0b1000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0001001011001001,
    0b0001010111000000,
    0b0001001011111001,
    0b0000000011100011,
    0b0000010100110000,
    0b0001001011001000,
    0b0011101000000000,
    0b0001011100000000,
    0b0000000000000000, //
    0b0000000000000110, // !
    0b0000001000100000, // "
    0b0001001011001110, // #
    0b0001001011101101, // $
    0b0000110000100100, // %
    0b0010001101011101, // &
    0b0000010000000000, // '
    0b0010010000000000, // (
    0b0000100100000000, // )
    0b0011111111000000, // *
    0b0001001011000000, // +
    0b0000100000000000, // ,
    0b0000000011000000, // -
    0b0100000000000000, // .
    0b0000110000000000, // /
    0b0000110000111111, // 0
    0b0000000000000110, // 1
    0b0000000011011011, // 2
    0b0000000010001111, // 3
    0b0000000011100110, // 4
    0b0010000001101001, // 5
    0b0000000011111101, // 6
    0b0000000000000111, // 7
    0b0000000011111111, // 8
    0b0000000011101111, // 9
    0b0001001000000000, // :
    0b0000101000000000, // ;
    0b0010010000000000, // <
    0b0000000011001000, // =
    0b0000100100000000, // >
    0b0001000010000011, // ?
    0b0000001010111011, // @
    0b0000000011110111, // A
    0b0001001010001111, // B
    0b0000000000111001, // C
    0b0001001000001111, // D
    0b0000000011111001, // E
    0b0000000001110001, // F
    0b0000000010111101, // G
    0b0000000011110110, // H
    0b0001001000001001, // I
    0b0000000000011110, // J
    0b0010010001110000, // K
    0b0000000000111000, // L
    0b0000010100110110, // M
    0b0010000100110110, // N
    0b0000000000111111, // O
    0b0000000011110011, // P
    0b0010000000111111, // Q
    0b0010000011110011, // R
    0b0000000011101101, // S
    0b0001001000000001, // T
    0b0000000000111110, // U
    0b0000110000110000, // V
    0b0010100000110110, // W
    0b0010110100000000, // X
    0b0001010100000000, // Y
    0b0000110000001001, // Z
    0b0000000000111001, // [
    0b0010000100000000, //
    0b0000000000001111, // ]
    0b0000110000000011, // ^
    0b0000000000001000, // _
    0b0000000100000000, // `
    0b0001000001011000, // a
    0b0010000001111000, // b
    0b0000000011011000, // c
    0b0000100010001110, // d
    0b0000100001011000, // e
    0b0000000001110001, // f
    0b0000010010001110, // g
    0b0001000001110000, // h
    0b0001000000000000, // i
    0b0000000000001110, // j
    0b0011011000000000, // k
    0b0000000000110000, // l
    0b0001000011010100, // m
    0b0001000001010000, // n
    0b0000000011011100, // o
    0b0000000101110000, // p
    0b0000010010000110, // q
    0b0000000001010000, // r
    0b0010000010001000, // s
    0b0000000001111000, // t
    0b0000000000011100, // u
    0b0010000000000100, // v
    0b0010100000010100, // w
    0b0010100011000000, // x
    0b0010000000001100, // y
    0b0000100001001000, // z
    0b0000100101001001, // {
    0b0001001000000000, // |
    0b0010010010001001, // }
    0b0000010100100000, // ~
    0b0011111111111111,
};

void run_gpio_i2c_quad_14_seg_test()
{
    raspi_init();

    cout << "Testing Quad 14-segment alphanumeric display with HT16K33 I2C driver...\n";

    int i2c_device = raspi_i2c_open(1, 0x70);
    raspi_i2c_write(i2c_device, 0b00100001); // Turn on system oscillator
    raspi_i2c_write(i2c_device, 0b10000001); // no blink, display on
    raspi_i2c_write(i2c_device, 0b11100011); // lower brightness

    // clear display
    for (uint8_t i = 0; i < 16; i++)
    {
        raspi_i2c_write(i2c_device, i, 0, 2);
    }

    string message = "HELLO WORLD FROM SPLASHKIT";
    string text = "    " + message + "    ";

    cout << "Starting text scrolling...\n";
    delay(500);
    
    for (int i = 0; i < text.length() - 3; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            raspi_i2c_write(i2c_device, j * 2, alphafonttable[text[i + j]], 2);
        }
        delay(200);
    }

    // Turn off LED device
    raspi_i2c_write(i2c_device, 0b00100000); // Turn off system oscillator

    // Clean up the GPIO
    raspi_cleanup();
}