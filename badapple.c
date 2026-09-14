// SPDX-License-Identifier: MIT
/*
 * badapple.c
 *
 * badapple-picobricks
 *
 * Copyright (c) 2026 Tuna Necmi İnal
 * I took the display controlling functions from the pico examples here:
 * https://github.com/raspberrypi/pico-examples/blob/master/i2c/ssd1306_i2c/ssd1306_i2c.c
 *
 * I wrote this project all by hand EXCEPT the for loop (lines 251-261) where I
 * translate the frame[] into the final buffer the display controller
 * understands. I didn't know my pixels were horizontal, I thought they were
 * vertical and the frame was getting sliced into eight pieces. I just wanted to
 * get it to work so I asked Claude for help. That is the only part of this
 * project which has been written by an LLM.
 */

#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>

#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5
#define PIEZO 20
#define FPS 30

#define SSD1306_HEIGHT 64
#define SSD1306_WIDTH 128

#define SSD1306_I2C_ADDR _u(0x3C)
#define SSD1306_I2C_CLK 400

#define SSD1306_SET_MEM_MODE _u(0x20)
#define SSD1306_SET_COL_ADDR _u(0x21)
#define SSD1306_SET_PAGE_ADDR _u(0x22)
#define SSD1306_SET_HORIZ_SCROLL _u(0x26)
#define SSD1306_SET_SCROLL _u(0x2E)

#define SSD1306_SET_DISP_START_LINE _u(0x40)

#define SSD1306_SET_CONTRAST _u(0x81)
#define SSD1306_SET_CHARGE_PUMP _u(0x8D)

#define SSD1306_SET_SEG_REMAP _u(0xA0)
#define SSD1306_SET_ENTIRE_ON _u(0xA4)
#define SSD1306_SET_ALL_ON _u(0xA5)
#define SSD1306_SET_NORM_DISP _u(0xA6)
#define SSD1306_SET_INV_DISP _u(0xA7)
#define SSD1306_SET_MUX_RATIO _u(0xA8)
#define SSD1306_SET_DISP _u(0xAE)
#define SSD1306_SET_COM_OUT_DIR _u(0xC0)
#define SSD1306_SET_COM_OUT_DIR_FLIP _u(0xC0)

#define SSD1306_SET_DISP_OFFSET _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV _u(0xD5)
#define SSD1306_SET_PRECHARGE _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG _u(0xDA)
#define SSD1306_SET_VCOM_DESEL _u(0xDB)

#define SSD1306_PAGE_HEIGHT _u(8)
#define SSD1306_NUM_PAGES (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN (SSD1306_NUM_PAGES * SSD1306_WIDTH)

#define SSD1306_WRITE_MODE _u(0xFE)
#define SSD1306_READ_MODE _u(0xFF)

void SSD1306_send_cmd(uint8_t cmd) {
  // I2C write process expects a control byte followed by data
  // this "data" can be a command or data to follow up a command
  // Co = 1, D/C = 0 => the driver expects a command
  uint8_t buf[2] = {0x80, cmd};
  i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buf, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
  for (int i = 0; i < num; i++)
    SSD1306_send_cmd(buf[i]);
}

void SSD1306_send_buf(uint8_t buf[], int buflen) {
  // in horizontal addressing mode, the column address pointer auto-increments
  // and then wraps around to the next page, so we can send the entire frame
  // buffer in one gooooooo!

  // copy our frame buffer into a new buffer because we need to add the control
  // byte to the beginning

  static uint8_t temp_buf[SSD1306_BUF_LEN + 1];

  temp_buf[0] = 0x40;
  memcpy(temp_buf + 1, buf, buflen);

  i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1,
                     false);
}

void SSD1306_init() {
  // Some of these commands are not strictly necessary as the reset
  // process defaults to some of these but they are shown here
  // to demonstrate what the initialization sequence looks like
  // Some configuration values are recommended by the board manufacturer

  uint8_t cmds[] = {
      SSD1306_SET_DISP, // set display off
      /* memory mapping */
      SSD1306_SET_MEM_MODE, // set memory address mode 0 = horizontal, 1 =
                            // vertical, 2 = page
      0x00,                 // horizontal addressing mode
      /* resolution and layout */
      SSD1306_SET_DISP_START_LINE, // set display start line to 0
      SSD1306_SET_SEG_REMAP |
          0x01, // set segment re-map, column address 127 is mapped to SEG0
      SSD1306_SET_MUX_RATIO,          // set multiplex ratio
      SSD1306_HEIGHT - 1,             // Display height - 1
      SSD1306_SET_COM_OUT_DIR | 0x08, // set COM (common) output scan direction.
                                      // Scan from bottom up, COM[N-1] to COM0
      SSD1306_SET_DISP_OFFSET,        // set display offset
      0x00,                           // no offset
      SSD1306_SET_COM_PIN_CFG, // set COM (common) pins hardware configuration.
                               // Board specific magic number. 0x02 Works for
                               // 128x32, 0x12 Possibly works for 128x64. Other
                               // options 0x22, 0x32
#if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
      0x02,
#elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
      0x12,
#else
      0x02,
#endif
      /* timing and driving scheme */
      SSD1306_SET_DISP_CLK_DIV, // set display clock divide ratio
      0x80,                     // div ratio of 1, standard freq
      SSD1306_SET_PRECHARGE,    // set pre-charge period
      0xF1,                     // Vcc internally generated on our board
      SSD1306_SET_VCOM_DESEL,   // set VCOMH deselect level
      0x30,                     // 0.83xVcc
      /* display */
      SSD1306_SET_CONTRAST, // set contrast control
      0xFF,
      SSD1306_SET_ENTIRE_ON,   // set entire display on to follow RAM content
      SSD1306_SET_NORM_DISP,   // set normal (not inverted) display
      SSD1306_SET_CHARGE_PUMP, // set charge pump
      0x14,                    // Vcc internally generated on our board
      SSD1306_SET_SCROLL |
          0x00, // deactivate horizontal scrolling if set. This is necessary as
                // memory writes will corrupt if scrolling was enabled
      SSD1306_SET_DISP | 0x01, // turn display on
  };

  SSD1306_send_cmd_list(cmds, count_of(cmds));
}

typedef struct {
  uint16_t freq;
  uint32_t us;
  uint8_t note_on;
} Message;

#include "messages.txt"

uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d) {
  uint32_t clock = 125000000;
  uint32_t divider16 = clock / f / 4096 + (clock % (f * 4096) != 0);
  if (divider16 / 16 == 0)
    divider16 = 16;
  uint32_t wrap = clock * 16 / divider16 / f - 1;
  pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
  pwm_set_wrap(slice_num, wrap);
  pwm_set_chan_level(slice_num, chan, wrap * d / 100);
  return wrap;
}

void core1_main() {
  uint slice_num = pwm_gpio_to_slice_num(PIEZO);
  uint chan = pwm_gpio_to_channel(PIEZO);
  for (int i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
    const absolute_time_t startingTimestamp = time_us_64();
    if (messages[i].note_on) {
      pwm_set_freq_duty(slice_num, chan, messages[i].freq, 25);
      pwm_set_enabled(slice_num, false);
    } else {
      pwm_set_enabled(slice_num, true);
    }
    const absolute_time_t endingTimestamp =
        delayed_by_us(startingTimestamp, messages[i].us);
    sleep_until(endingTimestamp);
  }
  pwm_set_enabled(slice_num, false);
}

#include "framedata.txt"

int main() {
  stdio_init_all();

  gpio_set_function(PIEZO, GPIO_FUNC_PWM);
  multicore_launch_core1(core1_main);

  i2c_init(i2c_default, SSD1306_I2C_CLK * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);
  SSD1306_init();

  const uint8_t cmds[] = {SSD1306_SET_COL_ADDR,  0, 127,
                          SSD1306_SET_PAGE_ADDR, 0, 7};
  SSD1306_send_cmd_list(cmds, sizeof(cmds));

  uint8_t buf[SSD1306_BUF_LEN];
  memset(buf, 0, SSD1306_BUF_LEN);

  uint32_t frameDataIdx = 0;
  uint16_t frameCount = 0;
  uint8_t frame[1024];
  memset(frame, 0, 1024);
  for (int i = 0; i < sizeof(frameSizes) / sizeof(frameSizes[0]); ++i) {
    absolute_time_t renderStart = get_absolute_time();
    uint8_t bitPattern = startingBits[frameCount];
    uint16_t xorDeltaIdx = 0;
    for (int j = 0; j < frameSizes[i]; ++j) {
      uint16_t bitRunLength = 0;
      if (frameData[frameDataIdx] < 128) {
        bitRunLength = frameData[frameDataIdx];
      } else {
        bitRunLength = ((frameData[frameDataIdx] & ~(1 << 7)) << 8) +
                       frameData[frameDataIdx + 1];
        frameDataIdx++;
      }
      bitRunLength++;
      if (bitPattern) {
        for (int k = 0; k < bitRunLength; ++k) {
          uint16_t byte = xorDeltaIdx / 8;
          uint8_t bit = xorDeltaIdx % 8;
          frame[byte] ^=
              128 >> bit; // could be 1 << bit instead we'll try and see
          xorDeltaIdx++;
        }
      } else {
        xorDeltaIdx += bitRunLength;
      }
      bitPattern ^= 1;
      frameDataIdx++;
    }
    memset(buf, 0, SSD1306_BUF_LEN);
    for (int row = 0; row < SSD1306_HEIGHT; ++row) {
      for (int col = 0; col < SSD1306_WIDTH; ++col) {
        int byteIdx = row * (SSD1306_WIDTH / 8) + col / 8;
        int bitSet = frame[byteIdx] & (128 >> (col % 8));
        if (bitSet) {
          int page = row / 8;
          buf[page * SSD1306_WIDTH + col] |= (1 << (row % 8));
        }
      }
    }
    SSD1306_send_buf(buf, SSD1306_BUF_LEN);
    frameCount++;
    absolute_time_t frameEnd = delayed_by_us(renderStart, 1000000 / FPS);
    sleep_until(frameEnd);
  }
  return 0;
}
