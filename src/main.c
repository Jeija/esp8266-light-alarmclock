// Configuration
#include "user_config.h"

// System
#include <esp8266.h>

// Libesphttpd
#include "webpages-espfs.h"
#include "httpdespfs.h"
#include "httpd.h"
#include "espfs.h"

// Project
#include "httpclient.h"
#include "main.h"

// systime: Seconds, minutes, hours, day of week from 1-7 - don't care about year / month / ...
struct TimeSpec {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t dow;
} systime;
bool systime_correct = false;
uint16_t update_systime_skip = 0;

struct WakeTimeSpec {
	uint8_t minutes;
	uint8_t hours;
	uint8_t dow;
	bool enabled;
} waketimes[WAKETIMES_MAX];

os_timer_t pwm_timer;
os_timer_t alarm_timer;
os_timer_t inc_systime_timer;
os_timer_t update_systime_timer;

uint8_t pwm_dutycycle = 0;
bool pwm_active = FALSE;

// Manual intensity setting overrides alarm clock
bool manual_override = FALSE;

/*
 * Utility functions
 */

// Parse string of IP address to uint32_t IP for ip_info
uint32_t ICACHE_FLASH_ATTR string_to_ip(char *ipstring) {
	uint32_t addr = 0;
	uint8_t octet = 0;
	uint8_t i = 0;
	uint8_t ioctet = 0;
	char octet_str[4] = {0x00};

	while (octet < 4) {
		if (ipstring[i] == '.' || ipstring[i] == 0x00) {
			octet_str[ioctet] = 0x00;
			addr += ((atoi(octet_str) & 0xff) << (8 * octet++));

			i++;
			ioctet = 0;
		} else {
			octet_str[ioctet++] = ipstring[i++];
		}
	}

	return addr;
}

// Read waketimes from flash, make sure flash stores valid waketimes
void ICACHE_FLASH_ATTR read_alarmflash(void) {
	char magic[4];
	spi_flash_read(ALARM_FLASH_OFFSET, (uint32 *)&magic, 4);
	if (magic[0] == ALARM_FLASH_MAGIC[0] && magic[1] == ALARM_FLASH_MAGIC[1]
			&& magic[2] == ALARM_FLASH_MAGIC[2] && magic[3] == ALARM_FLASH_MAGIC[3]) {
		spi_flash_read(ALARM_FLASH_OFFSET + 4, (uint32 *)&waketimes, WAKETIMES_MAX * 4);
	}
}

void ICACHE_FLASH_ATTR write_alarmflash(void) {
	spi_flash_erase_sector(ALARM_FLASH_SECTOR);
	spi_flash_write(ALARM_FLASH_OFFSET, (uint32 *)ALARM_FLASH_MAGIC, 4);
	spi_flash_write(ALARM_FLASH_OFFSET + 4, (uint32 *)&waketimes, WAKETIMES_MAX * 4);
}

// Based on http://stackoverflow.com/questions/6054016/c-program-to-find-day-of-week-given-date
// Convert date to day of week from Monday (1) to Sunday (7)
uint8_t ICACHE_FLASH_ATTR date2dow(uint16_t year, uint16_t month, uint16_t day) {
	uint32_t JND = day
		+ ((153 * (month + 12 * ((14 - month) / 12) - 3) + 2) / 5)
		+ (365 * (year + 4800 - ((14 - month) / 12)))
		+ ((year + 4800 - ((14 - month) / 12)) / 4)
		- ((year + 4800 - ((14 - month) / 12)) / 100)
		+ ((year + 4800 - ((14 - month) / 12)) / 400)
		- 32045;
	return JND % 7 + 1;
}

/**
 * Systime update callbacks
 */
void ICACHE_FLASH_ATTR http_callback_time(char *res, int status, char *res_full)
{
	if (status == HTTP_STATUS_GENERIC_ERROR) return;

	// Frugal "JSON" parser: We only want to get the contents of the "formatted" field,
	// so we don't need the full-blown JSON decoder from the IoT SDK here.
	char *start = os_strstr(res, TIMEZONEDB_FIELD);

	// TIMEZONEDB_FIELD not in response: Failed to retrieve a valid JSON from the timezonedb server, ignore
	if (start == NULL) {
		os_printf("Failed to retrieve valid data from timezonedb, got %s\r\n", res);
		return;
	}

	// The JSON should contain an entry that looks like TIMEZONEDB_FIELD : "2017-01-01 00:00:00"
	// `start` currently points to where the TIMEZONEDB_FIELD string begins
	// Increment `timestr` until it points to the first character of the time string.
	char *timestr = start + os_strlen(TIMEZONEDB_FIELD);
	while (*timestr != ':') timestr++;
	while (*timestr != '"') timestr++;
	timestr++;

	// Insert 0x00 string terminators into the ISO time and convert hour, minute, second string to numbers
	uint8_t i = 0;
	while (timestr[i] != 0x00) {
		if (timestr[i] == ' ' || timestr[i] == '-' || timestr[i] == ':') timestr[i] = 0x00;
		i++;
	}

	systime.hours = atoi(timestr + 11);
	systime.minutes = atoi(timestr + 14);
	systime.seconds = atoi(timestr + 17);

	uint16_t year = atoi(timestr);
	uint16_t month = atoi(timestr + 5);
	uint16_t day = atoi(timestr + 8);

	systime.dow = date2dow(year, month, day);
	systime_correct = true;

	os_printf("Retrieved time: %d-%d-%d %d:%d:%d, day of week %d\r\n", year, month, day, systime.hours, systime.minutes, systime.seconds, systime.dow);
}

/**
 * Timer callbacks
 */
static void pwm_timer_cb(void) {
	static uint16_t dutycount = 0;
	if (dutycount++ >= PWM_RESOLUTION) dutycount = 0;

	if (dutycount > pwm_dutycycle) {
		gpio_output_set(0, BIT2, BIT2, 0); // off
	} else {
		gpio_output_set(BIT2, 0, BIT2, 0); // on
	}
}

static void inc_systime_timer_cb(void) {
	if (++systime.seconds >= 60) {
		systime.seconds = 0;
		if (++systime.minutes >= 60) {
			systime.minutes = 0;
			if (++systime.hours >= 24) {
				systime.hours = 0;
				if (++systime.dow >= 8) {
					systime.dow = 1;
				}
			}
		}
	}
}

void ICACHE_FLASH_ATTR update_systime_timer_cb(void) {
	// Skip systime update up to UPDATE_SYSTIME_MAXSKIP times if time is already correct
	if (systime_correct && ++update_systime_skip < UPDATE_SYSTIME_MAXSKIP) return;

	http_get(TIMEZONEDB_URL, "", http_callback_time);
}

// Warning: The alarm timer won't work at the end of the week (sunday night at 23:59)
// since it counts the seconds since the beginning of the week
void ICACHE_FLASH_ATTR alarm_timer_cb(void) {
	if (manual_override) return;

	// Calculate elapsed minutes seconds this week:
	uint32_t seconds_this_week = (systime.dow - 1) * 86400 + systime.hours * 3600 + systime.minutes * 60 + systime.seconds;

	uint32_t latest_alarm = 0xffffffff;

	uint8_t i;
	for (i = 0; i < WAKETIMES_MAX; ++i) {
		if (!waketimes[i].enabled) continue;

		// Calculate at which time (in seconds) of the week the alarm has to occur:
		uint32_t seconds_alarm = (waketimes[i].dow - 1) * 86400 + waketimes[i].hours * 3600 + waketimes[i].minutes * 60;
		int32_t seconds_since_alarm = seconds_this_week - seconds_alarm;
		if (seconds_since_alarm > 0 && seconds_since_alarm < latest_alarm) latest_alarm = seconds_since_alarm;
	}

	// Turn on / dim light depending on when the last alarm has occured
	if (latest_alarm < ALARM_DIM_DURATION) {
		uint8_t dim = (1000 * latest_alarm / ALARM_DIM_DURATION * (ALARM_DIM_STOP - ALARM_DIM_START)) / 1000 + ALARM_DIM_START;
		pwm_setintensity(dim);
	} else if (latest_alarm < ALARM_DIM_DURATION + ALARM_HOLD_DURATION) {
		pwm_setintensity(ALARM_DIM_FINAL);
	} else {
		pwm_setintensity(0);
	}
}

/**
 * PWM functions
 */
void ICACHE_FLASH_ATTR pwm_init(void) {
	os_timer_setfn(&pwm_timer, (os_timer_func_t *)pwm_timer_cb, NULL);
	os_timer_arm_us(&pwm_timer, PWM_PERIOD_US, 1);
	pwm_active = TRUE;
}

void ICACHE_FLASH_ATTR pwm_deinit(void) {
	os_timer_disarm(&pwm_timer);
	gpio_output_set(0, BIT2, BIT2, 0);
	pwm_active = FALSE;
}

// Intensity between 0 - PWM_RESOLUTION, visible at > 10
void ICACHE_FLASH_ATTR pwm_setintensity(uint8_t intensity) {
	pwm_dutycycle = intensity;
	if (intensity == 0 && pwm_active) pwm_deinit();
	if (intensity > 0 && !pwm_active) pwm_init();
}

uint8_t ICACHE_FLASH_ATTR pwm_getintensity(void) {
	return pwm_dutycycle;
}

/**
 * HTTP Server functions
 */
inline void http_respond(HttpdConnData *conn, int code, char *msg) {
	httpdStartResponse(conn, code);
	httpdEndHeaders(conn);
	httpdSend(conn, msg, -1);
}

int ICACHE_FLASH_ATTR cmd_intensity_set(HttpdConnData *conn) {
	// Parse command from GET parameters
	char intensity_str[4];
	httpdFindArg(conn->getArgs, "intensity", intensity_str, sizeof(intensity_str));
	uint8_t intensity = atoi(intensity_str);
	pwm_setintensity(intensity);

	manual_override = (intensity != 0);

	http_respond(conn, 200, "ok");
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cmd_intensity_get(HttpdConnData *conn) {
	char answer[4];
	os_sprintf(answer, "%d", pwm_getintensity());
	http_respond(conn, 200, answer);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cmd_waketimes_get(HttpdConnData *conn) {
	char buf[1024];
	char section[50];

	strcpy(buf, "[");

	bool first = true;
	uint8_t i = 0;
	for (i = 0; i < WAKETIMES_MAX; ++i) {
		if (waketimes[i].enabled) {
			if (!first) strcat(buf, ",");
			first = FALSE;
			os_sprintf(section, "{\"id\":%d,\"dow\":%d,\"hrs\":%d,\"min\":%d}", i, waketimes[i].dow,
					waketimes[i].hours, waketimes[i].minutes);
			strcat(buf, section);
		}
	}
	strcat(buf, "]");

	http_respond(conn, 200, buf);

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cmd_waketime_del(HttpdConnData *conn) {
	char id_str[4];
	httpdFindArg(conn->getArgs, "id", id_str, sizeof(id_str));
	uint16_t id = atoi(id_str);

	if (id < WAKETIMES_MAX) {
		waketimes[id].enabled = FALSE;
		http_respond(conn, 200, "ok");
	} else {
		http_respond(conn, 400, "error: invalid id");
	}

	write_alarmflash();

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cmd_waketime_add(HttpdConnData *conn) {
	char dow_str[4];
	char hrs_str[4];
	char min_str[4];

	httpdFindArg(conn->getArgs, "dow", dow_str, sizeof(dow_str));
	httpdFindArg(conn->getArgs, "hrs", hrs_str, sizeof(hrs_str));
	httpdFindArg(conn->getArgs, "min", min_str, sizeof(min_str));

	uint8_t dow = atoi(dow_str);
	uint8_t hrs = atoi(hrs_str);
	uint8_t min = atoi(min_str);

	if (dow > 7 || hrs > 24 || min > 59) {
		http_respond(conn, 400, "error: invalid data");
	} else {
		// Look for an empty waketime slot
		uint8_t i;
		for (i = 0; i < WAKETIMES_MAX; ++i) {
			if (waketimes[i].enabled != TRUE) {
				waketimes[i].dow = dow;
				waketimes[i].hours = hrs;
				waketimes[i].minutes = min;
				waketimes[i].enabled = TRUE;
				http_respond(conn, 200, "ok");
				write_alarmflash();
				return HTTPD_CGI_DONE;
			}
		}

		// No empty slot found
		http_respond(conn, 500, "error: no more empty slots");
	}

	return HTTPD_CGI_DONE;
}

HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/index.html"},
	{"/intensity_set", cmd_intensity_set, NULL},
	{"/intensity_get", cmd_intensity_get, NULL},
	{"/waketimes_get", cmd_waketimes_get, NULL},
	{"/waketime_del", cmd_waketime_del, NULL},
	{"/waketime_add", cmd_waketime_add, NULL},
	{"*", cgiEspFsHook, NULL},
	{NULL, NULL, NULL}
};


/**
 * Initialization
 */
void ICACHE_FLASH_ATTR user_init(void)
{
	/*** Initialization ***/
	system_timer_reinit();
	uart_div_modify(0, UART_CLK_FREQ / BAUD);
	gpio_output_set(0, BIT2, BIT2, 0);

	/*** Connect to WiFi ***/
	wifi_set_opmode(STATION_MODE);
	char ssid[32] = WIFI_SSID;
	char pass[64] = WIFI_PASS;
	struct station_config sta_conf;
	sta_conf.bssid_set = 0;   
	os_memcpy(&sta_conf.ssid, ssid, 32);
	os_memcpy(&sta_conf.password, pass, 64);

	wifi_station_set_config(&sta_conf);
	wifi_station_connect();

	/*** Network configuration ***/
	#ifdef USE_STATIC
	wifi_station_dhcpc_stop();

	struct ip_info ip_conf;
	ip_conf.ip.addr = string_to_ip(IP_ADDR);
	ip_conf.gw.addr = string_to_ip(IP_GATEWAY);
	ip_conf.netmask.addr = string_to_ip(IP_SUBNET);
	wifi_set_ip_info(STATION_IF, &ip_conf);
	#else
	wifi_station_dhcpc_start();
	#endif

	/*** Set up HTTP Server ***/
	espFsInit((void*)(webpages_espfs_start));
	httpdInit(builtInUrls, 80);

	os_timer_setfn(&alarm_timer, (os_timer_func_t *)alarm_timer_cb, NULL);
	os_timer_arm(&alarm_timer, ALARM_DIM_INTERVAL * 1000, 1);

	os_timer_setfn(&inc_systime_timer, (os_timer_func_t *)inc_systime_timer_cb, NULL);
	os_timer_arm(&inc_systime_timer, 1000, 1);

	os_timer_setfn(&update_systime_timer, (os_timer_func_t *)update_systime_timer_cb, NULL);
	os_timer_arm(&update_systime_timer, UPDATE_SYSTIME_MININTERVAL * 1000, 1);

	/*** Read alarm configuration ***/
	read_alarmflash();

	os_printf("Init completed!\r\n");
}
