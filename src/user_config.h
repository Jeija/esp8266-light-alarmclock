#define BAUD 921600

// PWM requires microsecond timing
#define USE_US_TIMER

// PWM configuration
#define PWM_PERIOD_US 30
#define PWM_RESOLUTION 100

// Timezone configuration, UPDATE_SYSTIME_INTERVAL in seconds
#define TIMEZONE "Europe/Berlin"
#define TIMEZONEDB_URL "http://api.timezonedb.com/v2/get-time-zone?key=" TIMEZONEDB_KEY "&format=json&by=zone&fields=formatted&zone=" TIMEZONE
#define TIMEZONEDB_FIELD "\"formatted\""
#define UPDATE_SYSTIME_MININTERVAL 5
#define UPDATE_SYSTIME_MAXSKIP 50

// Make sure waketimes doesn't overflow the response buffer of "waketimes_get"
// 3 alarms per weekday are propably more than enough
#define WAKETIMES_MAX 21

// Alarm / wakeup light dimming configuration (in seconds or %)
#define ALARM_HOLD_DURATION 120	// seconds
#define ALARM_DIM_DURATION 240	// seconds
#define ALARM_DIM_INTERVAL 3	// seconds
#define ALARM_DIM_START 11	// %
#define ALARM_DIM_STOP 50	// %
#define ALARM_DIM_FINAL 100	// %

// Network configuration, comment this section out for DHCP
#define USE_STATIC
#define IP_ADDR	"192.168.0.50"
#define IP_GATEWAY "192.168.0.2"
#define IP_SUBNET "255.255.0.0"

// Store alarm times in flash, so that they can be restored after power loss
// In order for the system to recognize the alarm section in flash, the magic
// 4 bytes must be set at the very beginning of ALARM_FLASH_OFFSET
// Then, 4 bytes * WAKETIMES_MAX are used
#define ALARM_FLASH_OFFSET 0x3c000
#define ALARM_FLASH_SECTOR (ALARM_FLASH_OFFSET / 0x1000)
#define ALARM_FLASH_MAGIC "ALRM"

// Make sure to set your Wi-Fi network and passwort on the make command line (WIFI_SSID, WIFI_PASS)!
// Also make sure to provide a timezonedb API key when compiling!
