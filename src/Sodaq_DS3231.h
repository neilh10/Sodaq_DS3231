// Sodaq_DS3231 Class is a modified version of DS3231.
// DS3231 Class is by Seeed Technology Inc(http://www.seeedstudio.com) and used
// in Seeeduino Stalker v2.1 for battery management(MCU power saving mode)
// & to generate timestamp for data logging. DateTime Class is a modified
// version supporting day-of-week.

// Original DateTime Class and its utility code is by Jean-Claude Wippler at JeeLabs
// http://jeelabs.net/projects/cafe/wiki/RTClib
// Released under MIT License http://opensource.org/licenses/mit-license.php

#ifndef SODAQ_DS3231_H
#define SODAQ_DS3231_H

#include <Arduino.h>
#include <stdint.h>



namespace sodaq_DS3231_nm {
// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
public:
    DateTime (long t =0); //Translates to years, and stores as offset from 2000.
    DateTime (uint16_t year, uint8_t month, uint8_t date,
              uint8_t hour, uint8_t min, uint8_t sec, uint8_t wday);
    DateTime (const char* date, const char* time);
    DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time);

    uint8_t second() const      { return ss; }
    uint8_t minute() const      { return mm; }
    uint8_t hour() const        { return hh; }

    uint8_t date() const        { return d; }
    uint8_t month() const       { return m; }
    uint16_t year() const       { return 2000 + yOff; }		// Notice the 2000 !
    uint8_t year2k() const       { return yOff; }		// Notice the 2000 !

    uint8_t dayOfWeek() const   { return wday;}  /*Su=1 Mo=2 Tu=3 We=4 Th=5 Fr=6 Sa=7 */

    // 32-bit time as seconds since 1/1/2000
    uint32_t get() const;
    // 32-bit number of seconds since Unix epoch (1970-01-01)
    uint32_t getEpoch() const;
    // 32-bit number of seconds since yr2000 UST/GMT (2000-01-01)
    uint32_t getY2k_secs() const;

    void addToString(String & str) const;

protected:
    uint8_t yOff, m, d, hh, mm, ss, wday;
};

// These are the constants for periodicity of enableInterrupts() below.
#define EverySecond     0x01
#define EveryMinute     0x02
#define EveryHour       0x03

//Alarm masks
enum ALARM_TYPES_t {
    EVERY_SECOND = 0x0F,
    MATCH_SECONDS = 0x0E,
    MATCH_MINUTES = 0x0C,     //match minutes *and* seconds
    MATCH_HOURS = 0x08,       //match hours *and* minutes, seconds
    MATCH_DATE = 0x00,        //match date *and* hours, minutes, seconds
    MATCH_DAY = 0x10,         //match day *and* hours, minutes, seconds
};

// RTC DS3231 chip connected via I2C and uses the Wire library.
// Only 24 Hour time format is supported in this implementation
class Sodaq_DS3231 {
public:
    uint8_t begin(void);

    void setDateTime(const DateTime& dt);  //Changes the date-time
    void setEpoch(uint32_t ts); // Set the RTC using timestamp (seconds since epoch)
    DateTime now();            //Gets the current date-time

    DateTime makeDateTime(unsigned long t);

    //Decides the /INT pin's output setting
    //periodicity can be any of following defines: EverySecond, EveryMinute, EveryHour
    void enableInterrupts(uint8_t periodicity);
    void enableInterrupts(uint8_t hh24, uint8_t mm,uint8_t ss);
    void enableInterrupts(ALARM_TYPES_t alarmType, uint8_t daydate, uint8_t hh24, uint8_t mm, uint8_t ss);
    void disableInterrupts();
    void clearINTStatus();

    void convertTemperature(bool waitToFinish=true);
    float getTemperature();
private:
    uint8_t readRegister(uint8_t regaddress);
    void writeRegister(uint8_t regaddress, uint8_t value);
    void writeRegister_pm(uint8_t regaddress, uint8_t *buf, uint8_t len);

public:
    uint8_t enableInterruptsCheckAlm1(uint8_t periodicity);
    //Alm2 in testing didn't create an interrupt. Alm1 created interrupts
    uint8_t enableInterruptsCheckAlm2(uint8_t periodicity);
    void    enableInterruptsAlm2(uint8_t periodicity);
};
//expect to have MS_SAMD_DS3231 defined to enable
extern Sodaq_DS3231 rtcExtPhy;
} //namespace sodaq_DS3231_nm

#if defined ADAFRUIT_FEATHERWING_RTC_SD
// RTC based on the PCF8523 chip connected via I2C and the Wire library
#define PCF8523_ADDRESS       0x68
#define PCF8523_CLKOUTCONTROL 0x0F
#define PCF8523_CONTROL_3     0x02
enum Pcf8523SqwPinMode { PCF8523_OFF = 7, PCF8523_SquareWave1HZ = 6, PCF8523_SquareWave32HZ = 5, PCF8523_SquareWave1kHz = 4, PCF8523_SquareWave4kHz = 3, PCF8523_SquareWave8kHz = 2, PCF8523_SquareWave16kHz = 1, PCF8523_SquareWave32kHz = 0 };

class RTC_PCF8523 {
public:
    boolean begin(void);
    void adjust(const DateTime& dt) { setTimeYear2kT0(dt);}
    void setTimeYear2kT0(const DateTime& dt);
    void setTimeEpochT0(long t);
    boolean initialized(void);
    static DateTime now();

    Pcf8523SqwPinMode readSqwPinMode();
    void writeSqwPinMode(Pcf8523SqwPinMode mode);
};
extern RTC_PCF8523 rtcExtPhy;
#endif //ADAFRUIT_FEATHERWING_RTC_SD
#endif
