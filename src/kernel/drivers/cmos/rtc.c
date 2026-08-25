#include "rtc.h"
#include "../ports/IO.h"
#include "../../utils/logging/logger.h"

#define MODULE "RTC"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static int get_update_in_progress_flag() {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static unsigned char get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void rtc_get_time(rtc_time_t *time) {
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char register_b;

    while (get_update_in_progress_flag());

    time->second = get_rtc_register(0x00);
    time->minute = get_rtc_register(0x02);
    time->hour   = get_rtc_register(0x04);
    time->day    = get_rtc_register(0x07);
    time->month  = get_rtc_register(0x08);
    time->year   = get_rtc_register(0x09);

    do {
        last_second = time->second;
        last_minute = time->minute;
        last_hour   = time->hour;
        last_day    = time->day;
        last_month  = time->month;
        last_year   = time->year;

        while (get_update_in_progress_flag());

        time->second = get_rtc_register(0x00);
        time->minute = get_rtc_register(0x02);
        time->hour   = get_rtc_register(0x04);
        time->day    = get_rtc_register(0x07);
        time->month  = get_rtc_register(0x08);
        time->year   = get_rtc_register(0x09);
    } while (last_second != time->second || last_minute != time->minute || 
             last_hour != time->hour || last_day != time->day || 
             last_month != time->month || last_year != time->year);

    register_b = get_rtc_register(0x0B);

    if (!(register_b & 0x04)) {
        time->second = (time->second & 0x0F) + ((time->second / 16) * 10);
        time->minute = (time->minute & 0x0F) + ((time->minute / 16) * 10);
        time->hour   = ((time->hour & 0x0F) + (((time->hour & 0x70) / 16) * 10)) | (time->hour & 0x80);
        time->day    = (time->day & 0x0F) + ((time->day / 16) * 10);
        time->month  = (time->month & 0x0F) + ((time->month / 16) * 10);
        time->year   = (time->year & 0x0F) + ((time->year / 16) * 10);
    }

    if (!(register_b & 0x02) && (time->hour & 0x80)) {
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }

    time->year += 2000;
}

void rtc_initialize(void) {
    log_info(MODULE, "Initialized");
}