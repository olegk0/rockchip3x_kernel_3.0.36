#ifndef __RK29_KEYS_H__
#define __RK29_KEYS_H__
#include <linux/input.h>

#define DEFAULT_DEBOUNCE_INTERVAL	10  //10ms
#define LONG_PRESS_COUNT			100 //100 * 10 = 1000ms
#define ONE_SEC_COUNT				(1000/DEFAULT_DEBOUNCE_INTERVAL)

#define ADC_SAMPLE_TIME				100

#define EV_ENCALL KEY_F4
#define EV_MENU KEY_F1

#define PRESS_LEV_LOW 1
#define PRESS_LEV_HIGH 0

struct rk29_keys_button {
	int code;		
	int code_long_press;
	int gpio;
	int adc_value;
	int adc_state;
	int active_low;
	char *desc;
	int wakeup;	
	int hall_key;
};


struct rk29_keys_platform_data {
	struct rk29_keys_button *buttons;
	int nbuttons;
	int chn;
	int rep;
};

#endif
