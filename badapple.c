#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for
// information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5
#define PIEZO 20

#define OLED_ADDR 0x3C

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

void oled_command(uint8_t command) {
  uint8_t buf[] = {0x00, command};
  i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, 2, false);
}

void oled_init(void) {
  oled_command(0xAE); // Display off

  oled_command(0xD5);
  oled_command(0x80); // Clock divide ratio

  oled_command(0xA8);
  oled_command(0x3F); // Multiplex ratio: 64

  oled_command(0xD3);
  oled_command(0x00); // Display offset

  oled_command(0x40); // Start line = 0

  oled_command(0x8D);
  oled_command(0x14); // Enable charge pump

  oled_command(0x20);
  oled_command(0x00); // Horizontal addressing mode

  oled_command(0xA1); // Segment remap
  oled_command(0xC8); // COM scan direction

  oled_command(0xDA);
  oled_command(0x12); // COM pins configuration

  oled_command(0x81);
  oled_command(0xCF); // Contrast

  oled_command(0xD9);
  oled_command(0xF1); // Pre-charge

  oled_command(0xDB);
  oled_command(0x40); // VCOMH

  oled_command(0xA4); // Display follows RAM
  oled_command(0xA6); // Normal display

  oled_command(0xAF); // Display on
}

void oled_update(uint8_t *buffer) {
  uint8_t data[1025];

  data[0] = 0x40;

  for (int i = 0; i < 1024; i++) {
    data[i + 1] = buffer[i];
  }

  i2c_write_blocking(I2C_PORT, OLED_ADDR, data, 1025, false);
}

void core1_main() {
  uint slice_num = pwm_gpio_to_slice_num(PIEZO);
  uint chan = pwm_gpio_to_channel(PIEZO);
  for (int i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
    if (messages[i].note_on) {
      pwm_set_freq_duty(slice_num, chan, messages[i].freq, 50);
      pwm_set_enabled(slice_num, false);
      sleep_us(messages[i].us);
    } else {
      pwm_set_enabled(slice_num, true);
      sleep_us(messages[i].us);
    }
  }
  pwm_set_enabled(slice_num, false);
}

int main() {
  stdio_init_all();

  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SCL);
  gpio_pull_up(I2C_SDA);

  oled_init();

  gpio_set_function(PIEZO, GPIO_FUNC_PWM);
  multicore_launch_core1(core1_main);

  uint8_t framebuffer[1024];
  for (int i = 0; i < 1024; i++) {
    framebuffer[i] = 0x00;
  }
  oled_update(framebuffer);

  while (true) {
    sleep_ms(1000);
  }
}
