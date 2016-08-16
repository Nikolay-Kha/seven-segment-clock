#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>

#include "pwm.h"
#include "c_gpio.h"

#define CHANNEL 0

// FrontPanel pin mapping
#define PIN_CLOCK 26
#define PIN_DATA 20
#define PIN_BUTTONS 21
#define PIN_WIRELESS 17
#define PIN_DIGIT1 2
#define PIN_DIGIT2 27
#define PIN_DIGIT3 4
#define PIN_DIGIT4 14
#define PIN_DIGIT5 15
#define CLOCK_PERIOD 10

// DATA bits
//   _    7
//  /_/ 2 1 6
//  /_/ 3 4 5
//
// DIGIT1, bit 0 message icon
// DIGIT2, just digit
// DIGIT3, colon, 6 bit bottom dot, 7 bit top dot
// DIGIT4, bit 0 power icon
// DIGIT5, bit 0 music icon

bool m_run = true;
uint32_t *m_buttons_buf[8];

void terminated()
{
    m_run = false;
}

int render_digit(unsigned int value, int offset, bool init)
{
  assert( value < 10 );
  const static unsigned char DIGITTABLE[10] = {0b11111100, 0b01100000, // 0, 1
                                               0b11011010, 0b11110010, // 2, 3
                                               0b01100110, 0b10110110, // 4, 5
                                               0b10111110, 0b11100000, // 6, 7
                                               0b11111110, 0b11110110};// 8, 9
  int i;
  int o = offset;
  for (i = 1; i < 256; i <<= 1) {
      if (init) {
          add_channel_pulse(CHANNEL, PIN_CLOCK, o + CLOCK_PERIOD/2, CLOCK_PERIOD/2);
      }
      if( (DIGITTABLE[value] & i) == 0) {
          add_channel_pulse(CHANNEL, PIN_DATA, o, CLOCK_PERIOD);
      }
      o += 2 * CLOCK_PERIOD;
  }
  return o - offset; // in fact 16 * CLOCK_PERIOD
}

void display_print(unsigned int hours, unsigned int minutes)
{
    assert( hours < 24 && minutes < 60 );
    static bool init = true;
    int offset;
    int i;
    const int digit_time = 16 * CLOCK_PERIOD;
    const int length = (SUBCYCLE_TIME_US_DEFAULT /
                       PULSE_WIDTH_INCREMENT_GRANULARITY_US_DEFAULT -
                       48 * CLOCK_PERIOD) / 5
                       - digit_time - CLOCK_PERIOD;
    pause_all();
    if (init) {
        clear_channel(CHANNEL);
        offset = digit_time;
        add_channel_pulse(CHANNEL, PIN_DIGIT1, offset, length);
        offset += digit_time + length + CLOCK_PERIOD;
        add_channel_pulse(CHANNEL, PIN_DIGIT2, offset, length);
        offset += digit_time + length + CLOCK_PERIOD;
        add_channel_pulse(CHANNEL, PIN_DIGIT3, offset, length);
        offset += digit_time + length + CLOCK_PERIOD;
        add_channel_pulse(CHANNEL, PIN_DIGIT4, offset, length);
        offset += digit_time + length + CLOCK_PERIOD;
        add_channel_pulse(CHANNEL, PIN_DIGIT5, offset, length);
        offset += digit_time + length + CLOCK_PERIOD;
        // generate 0 per at one position and shift it to detect which buttons
        // are pressed.
        for (i = 0; i < 16; i++) {
          if (i != 8) {
              add_channel_pulse(CHANNEL, PIN_DATA, offset, CLOCK_PERIOD);
          }
          if (i >= 8) {
              m_buttons_buf[i-8] = add_measure(CHANNEL, offset +
                                               CLOCK_PERIOD / 2 - 1);
          }
          add_channel_pulse(CHANNEL, PIN_CLOCK, offset + CLOCK_PERIOD/2,
                            CLOCK_PERIOD/2);
          offset += 2 * CLOCK_PERIOD;
        }
    }
    clear_channel_gpio(CHANNEL, PIN_DATA);
    offset = 0;
    render_digit(hours / 10, offset, init);
    offset += digit_time + length + CLOCK_PERIOD;
    render_digit(hours % 10, offset, init);
    offset += digit_time + length + CLOCK_PERIOD;
    render_digit(8, offset, init);
    offset += digit_time + length + CLOCK_PERIOD;
    render_digit(minutes / 10, offset, init);
    offset += digit_time + length + CLOCK_PERIOD;
    render_digit(minutes % 10, offset, init);
    init = false;
    resume_all();
}

int main(int argc, char **argv)
{
    int i;
    int prev_min = -1;
    time_t rawtime;
    struct tm * timeinfo;
    FILE *fd;
    char buf[2];
    bool wstate;

    setupgpiomod();
    setup_gpio(PIN_WIRELESS, OUTPUT, PUD_OFF);
    output_gpio(PIN_WIRELESS, 0);
    input_gpio(PIN_BUTTONS);
    set_loglevel(LOG_LEVEL_ERRORS);

    setup(PULSE_WIDTH_INCREMENT_GRANULARITY_US_DEFAULT, DELAY_VIA_PWM);
    init_channel(CHANNEL, SUBCYCLE_TIME_US_DEFAULT);

    for (i = 0; i < 64; i++) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = terminated;
        sigaction(i, &sa, NULL);
    }

    while(m_run) {
        // update time if necessary
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        if(prev_min != timeinfo->tm_min) {
            display_print(timeinfo->tm_hour, timeinfo->tm_min);
            prev_min = timeinfo->tm_min;
        }

        // check wi-fi state
        wstate = false;
        fd = fopen("/sys/class/net/wlan0/operstate", "r");
        if (fd) {
            if (fread(buf, 2, 1, fd)) {
                wstate = buf[0] == 'u' && buf[1] == 'p';
            }
            fclose(fd);
        }
        output_gpio(PIN_WIRELESS, wstate ? 1 : 0);
        usleep(1000000);
        //printf("%X %X %X %X %X %X %X %X\n", *m_buttons_buf[0], *m_buttons_buf[1], *m_buttons_buf[2], *m_buttons_buf[3],
          //     *m_buttons_buf[4], *m_buttons_buf[5], *m_buttons_buf[6], *m_buttons_buf[7]);
    }

    // cleanup
    shutdown();
    // live something on screen
    setup_gpio(PIN_CLOCK, OUTPUT, PUD_OFF);
    setup_gpio(PIN_DATA, OUTPUT, PUD_OFF);
    setup_gpio(PIN_DIGIT4, OUTPUT, PUD_OFF);
    output_gpio(PIN_CLOCK, 0);

    for (i = 0; i < 8; i++) {
      if (i == 0) {
          output_gpio(PIN_DATA, 0);
      } else {
          output_gpio(PIN_DATA, 1);
      }
      output_gpio(PIN_CLOCK, 1);
      usleep(10);
      output_gpio(PIN_CLOCK, 0);
      usleep(10);
    }

    output_gpio(PIN_DIGIT4, 1); // power icon is here
    output_gpio(PIN_WIRELESS, 0);
    return 0;
}
