#ifndef LED_H_
#define LED_H_

#include <stdint.h>

#define LED_GREEN  12
#define LED_ORANGE 13
#define LED_RED    14
#define LED_BLUE   15

void led_init_all(void);
void led_on(uint8_t led_no);
void led_off(uint8_t led_no);

#endif /* LED_H_ */
