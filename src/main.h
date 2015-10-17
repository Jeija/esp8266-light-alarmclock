#ifndef _MAIN_H
#define _MAIN_H

// Utility functions:
// string_to_ip: Convert IP Address from string (e.g. "123.234.123.234") to uint32_t
// read_alarmflash: Read flash and fill wakteimes
// write_alarmflash: Write waketimes to flash
uint32_t ICACHE_FLASH_ATTR string_to_ip(char *ipstring);
void ICACHE_FLASH_ATTR read_alarmflash(void);
void ICACHE_FLASH_ATTR write_alarmflash(void);

// HTTP request callbacks
void ICACHE_FLASH_ATTR http_callback_time(char *res, int status, char *res_full);

// Timer callback functions
static void pwm_timer_cb(void);
static void inc_systime_timer_cb(void);
void ICACHE_FLASH_ATTR update_systime_timer_cb(void);
void ICACHE_FLASH_ATTR alarm_timer_cb(void);

// PWM functions
void ICACHE_FLASH_ATTR pwm_init(void);
void ICACHE_FLASH_ATTR pwm_deinit(void);
void ICACHE_FLASH_ATTR pwm_setintensity(uint8_t intensity);
uint8_t ICACHE_FLASH_ATTR pwm_getintensity(void);

// HTTP server callbacks
int ICACHE_FLASH_ATTR cmd_intensity_set(HttpdConnData *conn);
int ICACHE_FLASH_ATTR cmd_intensity_get(HttpdConnData *conn);
int ICACHE_FLASH_ATTR cmd_waketimes_get(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cmd_waketime_del(HttpdConnData *conn);
int ICACHE_FLASH_ATTR cmd_waketime_add(HttpdConnData *conn);

void ICACHE_FLASH_ATTR user_init(void);

#endif
