// Sodaq_DS3231 Class is a modified version of DS3231.
// DS3231 Class is by Seeed Technology Inc(http://www.seeedstudio.com) and used
// in Seeeduino Stalker v2.1 for battery management(MCU power saving mode)
// & to generate timestamp for data logging. DateTime Class is a modified
// version supporting day-of-week.

// Original DateTime Class and its utility code is by Jean-Claude Wippler at JeeLabs
// http://jeelabs.net/projects/cafe/wiki/RTClib
// Released under MIT License http://opensource.org/licenses/mit-license.php

#include <Wire.h>
#include <avr/pgmspace.h>
#include "Sodaq_DS3231.h"
#include "Arduino.h"

#define EPOCH_TIME_OFF 946684800  // This is 2000-jan-01 00:00:00 in epoch time
#define SECONDS_PER_DAY 86400L

using namespace sodaq_DS3231_nm;

#define DS3231_ADDRESS	      0x68 //I2C Slave address

/* DS3231 Registers. Refer Sec 8.2 of application manual */
#define DS3231_SEC_REG        0x00
#define DS3231_MIN_REG        0x01
#define DS3231_HOUR_REG       0x02
#define DS3231_WDAY_REG       0x03
#define DS3231_MDAY_REG       0x04
#define DS3231_MONTH_REG      0x05
#define DS3231_YEAR_REG       0x06

#define DS3231_AL1SEC_REG     0x07
#define DS3231_AL1MIN_REG     0x08
#define DS3231_AL1HOUR_REG    0x09
#define DS3231_AL1WDAY_REG    0x0A

#define DS3231_AL2MIN_REG     0x0B
#define DS3231_AL2HOUR_REG    0x0C
#define DS3231_AL2WDAY_REG    0x0D

#define DS3231_CONTROL_REG          0x0E
#define DS3231_STATUS_REG           0x0F
#define DS3231_AGING_OFFSET_REG     0x10
#define DS3231_TMP_UP_REG           0x11
#define DS3231_TMP_LOW_REG          0x12

////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

static const uint8_t daysInMonth [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
        days += pgm_read_byte(daysInMonth + i - 1);
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

// Calculate the day of the week given the date
// https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
// Implementation due to Tomohiko Sakamoto
// y > 1752, 1 <= m <= 12
byte DayOfWeek(int y, byte m, byte d) {   // y > 1752, 1 <= m <= 12
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return ((y + y/4 - y/100 + y/400 + t[m-1] + d) % 7) + 1; // 01 - 07, 01 = Sunday
}

static uint32_t time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (long t) {
    ss = t % 60;
    t /= 60;  // now t is minutes
    mm = t % 60;
    t /= 60;  // now t is hours
    hh = t % 24;
    int16_t days = t / 24;
    uint8_t leap;
    for (yOff = 0; ; ++yOff) {
        leap = yOff % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }
    for (m = 1; ; ++m) {
        uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
    wday = DayOfWeek(yOff+2000, m, d);
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec, uint8_t wd) {
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = date;
    hh = hour;
    mm = min;
    ss = sec;
    wday = wd;
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using PSTR would further reduce the RAM footprint
DateTime::DateTime (const char* date, const char* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    yOff = conv2d(date + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (date[0]) {
        case 'J': m = date[1] == 'a' ? 1 : date[2] == 'n' ? 6 : 7; break;
        case 'F': m = 2; break;
        case 'A': m = date[2] == 'r' ? 4 : 8; break;
        case 'M': m = date[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(date + 4);
    hh = conv2d(time);
    mm = conv2d(time + 3);
    ss = conv2d(time + 6);
    wday = DayOfWeek(yOff+2000, m, d);
}

// A convenient constructor for using "the compiler's time":
// This version will save RAM by using PROGMEM to store it by using the F macro.
//   DateTime now (F(__DATE__), F(__TIME__));
DateTime::DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    char buff[11];
    memcpy_P(buff, date, 11);
    yOff = conv2d(buff + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (buff[0]) {
        case 'J': m = (buff[1] == 'a') ? 1 : ((buff[2] == 'n') ? 6 : 7); break;
        case 'F': m = 2; break;
        case 'A': m = buff[2] == 'r' ? 4 : 8; break;
        case 'M': m = buff[2] == 'r' ? 3 : 5; break;
        case 'S': m = 9; break;
        case 'O': m = 10; break;
        case 'N': m = 11; break;
        case 'D': m = 12; break;
    }
    d = conv2d(buff + 4);
    memcpy_P(buff, time, 8);
    hh = conv2d(buff);
    mm = conv2d(buff + 3);
    ss = conv2d(buff + 6);
}

uint32_t DateTime::get() const {
    uint16_t days = date2days(yOff, m, d);
    return time2long(days, hh, mm, ss);
}

uint32_t DateTime::getEpoch() const
{
    return get() + EPOCH_TIME_OFF;
}
uint32_t DateTime::getY2k_secs() const
{
    return get();
}

/*
 * Format an integer as %0*d
 *
 * Arduino formatting sucks.
 */
static void add0Nd(String &str, uint16_t val, size_t width)
{
    if (width >= 5 && val < 1000) {
	str += '0';
    }
    if (width >= 4 && val < 100) {
	str += '0';
    }
    if (width >= 3 && val < 100) {
	str += '0';
    }
    if (width >= 2 && val < 10) {
	str += '0';
    }
    str += val;
}
static inline void add04d(String &str, uint16_t val) { add0Nd(str, val, 4); }
static inline void add02d(String &str, uint16_t val) { add0Nd(str, val, 2); }

void DateTime::addToString(String & str) const
{
    add04d(str, year());
    str += '-';
    add02d(str, month());
    str += '-';
    add02d(str, date());
    str += ' ';
    add02d(str, hour());
    str += ':';
    add02d(str, minute());
    str += ':';
    add02d(str, second());
}

// Binary-Coded-Decimal (BCD)-to-Decimal conversion
static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
// Decimal-to-BCD (Binary-Coded-Decimal) conversion
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

////////////////////////////////////////////////////////////////////////////////
// RTC DS3231 implementation

uint8_t Sodaq_DS3231::readRegister(uint8_t regaddress)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write((byte)regaddress);
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 1);
    return Wire.read();
}

void Sodaq_DS3231::writeRegister(uint8_t regaddress,uint8_t value)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write((byte)regaddress);
    Wire.write((byte)value);
    Wire.endTransmission();
}

uint8_t Sodaq_DS3231::begin(void) {

  unsigned char ctReg=0;

  Wire.begin();
  ctReg |= 0b00011100;
  writeRegister(DS3231_CONTROL_REG, ctReg);     //CONTROL Register Address
  delay(10);

  // set the clock to 24hr format
  uint8_t hrReg = readRegister(DS3231_HOUR_REG);
  hrReg &= 0b10111111;
  writeRegister(DS3231_HOUR_REG, hrReg);

  delay(10);

  return 1;
}

//set the time-date specified in DateTime format
//writing any non-existent time-data may interfere with normal operation of the RTC
void Sodaq_DS3231::setDateTime(const DateTime& dt) {

  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)DS3231_SEC_REG);  //beginning from SEC Register address

  Wire.write((byte)bin2bcd(dt.second()));
  Wire.write((byte)bin2bcd(dt.minute()));
  Wire.write((byte)bin2bcd((dt.hour()) & 0b10111111)); //Make sure clock is still 24 Hour
  Wire.write((byte)dt.dayOfWeek());
  Wire.write((byte)bin2bcd(dt.date()));
  Wire.write((byte)bin2bcd(dt.month()));
  Wire.write((byte)bin2bcd(dt.year() - 2000));
  Wire.endTransmission();

}

DateTime Sodaq_DS3231::makeDateTime(unsigned long t)
{
  if (t < EPOCH_TIME_OFF)
    return DateTime(0);
  return DateTime(t - EPOCH_TIME_OFF);
}

// Set the RTC using timestamp (seconds since epoch)
void Sodaq_DS3231::setEpoch(uint32_t ts)
{
  setDateTime(makeDateTime(ts));
}

//Read the current time-date and return it in DateTime format
DateTime Sodaq_DS3231::now() {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x00);
  Wire.endTransmission();

  Wire.requestFrom(DS3231_ADDRESS, 8);
  uint8_t ss = bcd2bin(Wire.read());
  uint8_t mm = bcd2bin(Wire.read());

  uint8_t hrreg = Wire.read();
  uint8_t hh = bcd2bin((hrreg & ~0b11000000)); //Ignore 24 Hour bit

  uint8_t wd =  Wire.read();
  uint8_t d = bcd2bin(Wire.read());
  uint8_t m = bcd2bin(Wire.read());
  uint16_t y = bcd2bin(Wire.read()) + 2000;

  return DateTime (y, m, d, hh, mm, ss, wd);
}

//Enable periodic interrupt at /INT pin. Supports only the level interrupt
//for consistency with other /INT interrupts. All interrupts works like single-shot counter
//Use refreshINTA() to re-enable interrupt.
void Sodaq_DS3231::enableInterrupts(uint8_t periodicity)
{
    // Turn in Alarm 1 at the control register
    unsigned char ctReg=0;
    ctReg |= 0b00011101;  // Alarm 1 on
    writeRegister(DS3231_CONTROL_REG, ctReg);     //CONTROL Register Address

   switch(periodicity)
   {
       case EverySecond:
       // Set all four alarm masks (on bit 7) - Alarm once per second
       writeRegister(DS3231_AL1SEC_REG,  0b10000000 ); //Set A1M1
       writeRegister(DS3231_AL1MIN_REG,  0b10000000 ); //Set A1M2
       writeRegister(DS3231_AL1HOUR_REG, 0b10000000 ); //Set A1M3
       writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //Set A1M4

       break;

       case EveryMinute:
       // Set 3 masks - Alarm when seconds match
       // seconds = 0, thus alarms on the minute
       writeRegister(DS3231_AL1SEC_REG,  0b00000000 ); //Clr A1M1
       writeRegister(DS3231_AL1MIN_REG,  0b10000000 ); //Set A1M2
       writeRegister(DS3231_AL1HOUR_REG, 0b10000000 ); //Set A1M3
       writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //Set A1M4

       break;

       case EveryHour:
       // Set 2 masks - Alarm when minutes and seconds match
       // seconds = 0 and minutes = 0, thus alarms on the hour
       writeRegister(DS3231_AL1SEC_REG,  0b00000000 ); //Clr A1M1
       writeRegister(DS3231_AL1MIN_REG,  0b00000000 ); //Clr A1M2
       writeRegister(DS3231_AL1HOUR_REG, 0b10000000 ); //Set A1M3
       writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //Set A1M4

       break;
   }
}

// Enable HH/MM/SS interrupt on /INTA pin. All interrupts works like single-shot counter
// This will only alarm ONE TIME PER DAY AT EXACT HH:MM:SS MATCH!!
void Sodaq_DS3231::enableInterrupts(uint8_t hh24, uint8_t mm, uint8_t ss)
{
    // Turn in Alarm 1 at the control register
    unsigned char ctReg=0;
    ctReg |= 0b00011101;
    writeRegister(DS3231_CONTROL_REG, ctReg);

    writeRegister(DS3231_AL1SEC_REG,  0b00000000 | bin2bcd(ss) ); //Clr AM1
    writeRegister(DS3231_AL1MIN_REG,  0b00000000 | bin2bcd(mm)); //Clr AM2
    writeRegister(DS3231_AL1HOUR_REG, (0b00000000 | (bin2bcd(hh24) & 0b10111111))); //Clr AM3
    writeRegister(DS3231_AL1WDAY_REG, 0b10000000 ); //Set AM4 - Alarm when hours, minutes, and seconds match
}


// More flexible setting of interrupts
void Sodaq_DS3231::enableInterrupts(ALARM_TYPES_t alarmType, uint8_t daydate, uint8_t hh24, uint8_t minutes, uint8_t seconds)
{
    unsigned char ctReg=0;
    ctReg |= 0b00011101;  // Alarm 1 on
    writeRegister(DS3231_CONTROL_REG, ctReg);     //CONTROL Register Address

    seconds = bin2bcd(seconds);
    minutes = bin2bcd(minutes);
    hh24 = bin2bcd(hh24);
    daydate = bin2bcd(daydate);
    if (alarmType & 0x01) seconds |= 0b10000000;  // To alarm every second, set the alarm mask on seconds
    if (alarmType & 0x02) minutes |= 0b10000000;  // To match seconds, need to set the alarm mask on minutes
    if (alarmType & 0x04) hh24 |= 0b10000000;  // To match minutes *and* seconds, need to add the alarm mask on hours
    if (alarmType & 0x10) hh24 |= 0b01000000;  // To match day *and* hours, minutes, seconds, need clear all alarm masks, but set the DY/DT bit
    if (alarmType & 0x08) daydate |= 0b10000000;  // To match hours *and* minutes, seconds, need to add the alarm mask on days
    // To match date *and* hours, minutes, seconds, need no alarm masks or DY/DT bits

    writeRegister(DS3231_AL1SEC_REG, seconds);
    writeRegister(DS3231_AL1MIN_REG, minutes);
    writeRegister(DS3231_AL1HOUR_REG, hh24);
    writeRegister(DS3231_AL1WDAY_REG, daydate);
}

//Disable Interrupts. This is equivalent to begin() method.
void Sodaq_DS3231::disableInterrupts()
{
    begin(); //Restore to initial value.
}

//Clears the interrrupt flag in status register.
//This is equivalent to preparing the DS3231 /INT pin to high for MCU to get ready for recognizing the next INT0 interrupt
void Sodaq_DS3231::clearINTStatus()
{
    // Clear interrupt flag
    uint8_t statusReg = readRegister(DS3231_STATUS_REG);
    statusReg &= 0b11111110;
    writeRegister(DS3231_STATUS_REG, statusReg);

}

//force temperature sampling and converting to registers. If this function is not used the temperature is sampled once 64 Sec.
void Sodaq_DS3231::convertTemperature(bool waitToFinish)
{
    // Set the CONV register - this forces a new conversion
    uint8_t ctReg = readRegister(DS3231_CONTROL_REG);
    ctReg |= 0b00100000;
    writeRegister(DS3231_CONTROL_REG,ctReg);

    //wait until CONV is cleared. Indicates new temperature value is available in register.
    if (!waitToFinish)
        while ((readRegister(DS3231_CONTROL_REG) & 0b00100000) == 0b00100000 ) {}

}

//Read the temperature value from the register and convert it into float (deg C)
float Sodaq_DS3231::getTemperature()
{
    float fTemperatureCelsius;
    uint8_t tUBYTE  = readRegister(DS3231_TMP_UP_REG);  //Two's complement form
    uint8_t tLRBYTE = readRegister(DS3231_TMP_LOW_REG); //Fractional part

    if(tUBYTE & 0b10000000) //check if -ve number
    {
       tUBYTE  ^= 0b11111111;
       tUBYTE  += 0x1;
       fTemperatureCelsius = tUBYTE + ((tLRBYTE >> 6) * 0.25);
       fTemperatureCelsius = fTemperatureCelsius * -1;
    }
    else
    {
        fTemperatureCelsius = tUBYTE + ((tLRBYTE >> 6) * 0.25);
    }

    return (fTemperatureCelsius);

}

Sodaq_DS3231 rtcExtPhy;

// Extension code placed here to keep compatibility from upstream fork.
//#define Sodaq_DS3231_DEBUG
#if defined Sodaq_DS3231_DEBUG
#define SODAQ_DBG2(parm1,parm2)     Serial.print(parm1,HEX); Serial.print("/");Serial.print(parm2,HEX);Serial.print(", ")
#define SODAQ_DBGT(parm1) Serial.print(parm1);
#define SODAQ_DBGN(parm1) Serial.println(parm1);
#else
#define SODAQ_DBG2(parm1,parm2)     
#define SODAQ_DBGT(parm1) 
#define SODAQ_DBGN(parm1)
#endif 

#if defined(__AVR__)
#define SODAQ_PROGMEM PROGMEM
#define SDOAQ_rd_pgm(param1) pgm_read_byte_near(param1)
#else
#define SODAQ_PROGMEM
#define SDOAQ_rd_pgm(param1) param1
#endif

#define DS3231_ALM1_SZ 4
//Alarm1 has four consecutive registers  A1M1 A1M2 A1M3 A1M4
const uint8_t alm1Ref_1second_pm[DS3231_ALM1_SZ] SODAQ_PROGMEM  = {0x80,0x80,0x80, 0x80};
const uint8_t alm1Ref_1minute_pm[DS3231_ALM1_SZ] SODAQ_PROGMEM  = {0x00,0x80,0x80, 0x80};
const uint8_t alm1Ref_1hour_pm[DS3231_ALM1_SZ]   SODAQ_PROGMEM  = {0x00,0x00,0x80, 0x80};

//Enable periodic interrupt at /INT pin. Supports only the level interrupt
//for consistency with other /INT interrupts. All interrupts works like single-shot counter
//Use refreshINTA() to re-enable interrupt.
//return 0 if true or else bit position of non-compliant register
// Check the ALM1 is enabled in two reads 
uint8_t Sodaq_DS3231::enableInterruptsCheckAlm1(uint8_t periodicity)
{

    uint8_t cmp_result = 0;
    uint8_t devReg;
    uint8_t *p_almReg_pm= 0; //Pointer ref program space

    SODAQ_DBGT("SODAQalm1 reg/diff ");  

    //Check CONTROL_REG
    #define DS3231_ALM1_EN 0b00000101
    devReg =(uint8_t)readRegister(DS3231_CONTROL_REG);
    devReg &= DS3231_ALM1_EN;
    cmp_result |= ( (bool)( devReg ^ DS3231_ALM1_EN )?  0x01:0); //
    SODAQ_DBG2(devReg,cmp_result);

    switch(periodicity)
    {    
        case EverySecond: p_almReg_pm =(uint8_t *)alm1Ref_1second_pm; break;
        case EveryMinute: p_almReg_pm =(uint8_t *)alm1Ref_1minute_pm; break;
        case EveryHour: p_almReg_pm   =(uint8_t *)alm1Ref_1hour_pm;   break;        
    }
    if (0==p_almReg_pm) {
        SODAQ_DBGT(periodicity);
        SODAQ_DBGN(" invalid periodicity!");
        return 0xFF;
    }

    //Check the ALM1 Regs by reading 4 consecutive registers
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_AL1SEC_REG);
    Wire.endTransmission();

    uint8_t sz_read=Wire.requestFrom(DS3231_ADDRESS, DS3231_ALM1_SZ);
    if (DS3231_ALM1_SZ == sz_read) {
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg ^ SDOAQ_rd_pgm(&p_almReg_pm[0]) )?  0x02:0); //07 Check Seconds
        SODAQ_DBG2(devReg,cmp_result);
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg  ^ SDOAQ_rd_pgm(&p_almReg_pm[1]) )? 0x04:0); //08 Check Minutes    
        SODAQ_DBG2(devReg,cmp_result);
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg  ^ SDOAQ_rd_pgm(&p_almReg_pm[2]) )? 0x08:0); //09 Check Hours
        SODAQ_DBG2(devReg,cmp_result);
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg  ^ SDOAQ_rd_pgm(&p_almReg_pm[3]) )? 0x10:0); //0A Check Day   
        SODAQ_DBG2(devReg,cmp_result);
        SODAQ_DBGN("");
    } else {
        SODAQ_DBGN(" not read!");
    }

   return cmp_result;
}

#define DS3231_ALM2_SZ 4
//Alarm2 has four consecutive registers A2M2 A2M3 A2M4 Control  
const uint8_t alm2Ref_1minute_pm[DS3231_ALM2_SZ] SODAQ_PROGMEM  = {0x80,0x80, 0x80, 0b00000110};
const uint8_t alm2Ref_1hour_pm[DS3231_ALM2_SZ]   SODAQ_PROGMEM  = {0x00,0x80, 0x80, 0b00000110};

// Check the ALM2 is enabled in one multi-byte read 
uint8_t Sodaq_DS3231::enableInterruptsCheckAlm2(uint8_t periodicity)
{
    uint8_t cmp_result = 0;
    uint8_t devReg;
    uint8_t *p_almReg_pm= 0; //Pointer ref program space

    SODAQ_DBGT("SODAQalm2 reg/diff ");  

    switch(periodicity)
    {    
        //case EverySecond: p_almReg_pm =(uint8_t *)alm1Ref_1second_pm; break;
        case EveryMinute: p_almReg_pm =(uint8_t *)alm2Ref_1minute_pm; break;
        case EveryHour: p_almReg_pm   =(uint8_t *)alm2Ref_1hour_pm;   break;    
    }
    if (0==p_almReg_pm) {
        SODAQ_DBGT(periodicity);
        SODAQ_DBGN(F(" invalid periodicity!"));
        return 0xFF;
    }

    //Read the ALM2 + CONTROL Regs
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_AL2MIN_REG);
    Wire.endTransmission();

    uint8_t sz_read=Wire.requestFrom(DS3231_ADDRESS, DS3231_ALM2_SZ);
    if (DS3231_ALM2_SZ == sz_read) {
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg ^ SDOAQ_rd_pgm(&p_almReg_pm[0]) )?  0x04:0); //0B Check Minutes
        SODAQ_DBG2(devReg,cmp_result);
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg  ^ SDOAQ_rd_pgm(&p_almReg_pm[1]) )? 0x08:0); //0C Check Hours    
        SODAQ_DBG2(devReg,cmp_result);
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg  ^ SDOAQ_rd_pgm(&p_almReg_pm[2]) )? 0x10:0); //0D Check Hours
        SODAQ_DBG2(devReg,cmp_result);
        devReg =(uint8_t)Wire.read();
        cmp_result |= ( (bool)( devReg  ^ SDOAQ_rd_pgm(&p_almReg_pm[3]) )? 0x01:0); //0E Check Control   
        SODAQ_DBG2(devReg,cmp_result);
        SODAQ_DBGN("");
    } else {
        SODAQ_DBGN(" not read!");
    }

   return cmp_result;
}

//Enable periodic interrupt at /INT pin. Supports only the level interrupt
//for consistency with other /INT interrupts. All interrupts works like single-shot counter
//Use enableInterruptsCheckAlm2 for already setup interrupts
// Practical note: this didn't creae an interrupt when tested. Alm1 worked
void Sodaq_DS3231::enableInterruptsAlm2(uint8_t periodicity)
{
    uint8_t *p_almReg_pm= 0; //Pointer ref program space

    SODAQ_DBGT("SODAQenInt alm2 ");  
    switch(periodicity)
    {    
        case EveryMinute: p_almReg_pm =(uint8_t *)alm2Ref_1minute_pm; break;
        case EveryHour: p_almReg_pm   =(uint8_t *)alm2Ref_1hour_pm;   break;    
    }
    if (0==p_almReg_pm) {
        SODAQ_DBGT(periodicity);
        SODAQ_DBGN(F(" invalid periodicity!"));
        return;
    }
    writeRegister_pm(DS3231_AL2MIN_REG,p_almReg_pm,DS3231_ALM2_SZ);
}

//Write a constant buffer from program space
void Sodaq_DS3231::writeRegister_pm(uint8_t regaddress, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write((byte)regaddress);
    for (uint8_t buf_lp=0; buf_lp<len;buf_lp++) {
        Wire.write((byte) SDOAQ_rd_pgm(&buf[buf_lp]));
    }
    Wire.endTransmission();
}

#if defined ADAFRUIT_FEATHERWING_RTC_SD
////////////////////////////////////////////////////////////////////////////////
// RTC_PCF8563 implementation

boolean RTC_PCF8523::begin(void) {
  Wire.begin();
  return true;
}

boolean RTC_PCF8523::initialized(void) {
  Wire.beginTransmission(PCF8523_ADDRESS);
  Wire.write((byte)PCF8523_CONTROL_3);
  Wire.endTransmission();

  Wire.requestFrom(PCF8523_ADDRESS, 1);
  uint8_t ss = Wire.read();
  //returns false if reads 0xE0 battery switch-over function is disabled
  return ((ss & 0xE0) != 0xE0);
}

void RTC_PCF8523::setTimeEpochT0(long t) {
    DateTime dt;
    setTimeYear2kT0(t-EPOCH_TIME_OFF);
}
// Set time - relative to yr2000
void RTC_PCF8523::setTimeYear2kT0(const DateTime& dt) { 
  Wire.beginTransmission(PCF8523_ADDRESS);
  Wire.write((byte)3); // start at location 3
  Wire.write(bin2bcd(dt.second()));
  Wire.write(bin2bcd(dt.minute()));
  Wire.write(bin2bcd(dt.hour()));
  Wire.write(bin2bcd(dt.date()));
  Wire.write(bin2bcd(dt.dayOfWeek())); // skip weekdays
  Wire.write(bin2bcd(dt.month()));
  Wire.write(bin2bcd(dt.year() - 2000));
  Wire.endTransmission();

  // set to battery switchover mode
  Wire.beginTransmission(PCF8523_ADDRESS);
  Wire.write((byte)PCF8523_CONTROL_3);
  Wire.write((byte)0x00);
  Wire.endTransmission();
}

DateTime RTC_PCF8523::now() {
  uint8_t ss, mm, hh, day, wkday, mnth; 
  uint16_t year;
  Wire.beginTransmission(PCF8523_ADDRESS);
  Wire.write((byte)3);	
  Wire.endTransmission();

  Wire.requestFrom(PCF8523_ADDRESS, 7);
  ss = bcd2bin(Wire.read() & 0x7F);
  mm = bcd2bin(Wire.read());
  hh = bcd2bin(Wire.read());
  day = bcd2bin(Wire.read());
  wkday =bcd2bin(Wire.read());  // skip 'weekdays'
  mnth = bcd2bin(Wire.read());
  year = bcd2bin(Wire.read()) + 2000;
  
  return DateTime (year, mnth, day, hh, mm, ss,wkday);
}

Pcf8523SqwPinMode RTC_PCF8523::readSqwPinMode() {
  int mode;

  Wire.beginTransmission(PCF8523_ADDRESS);
  Wire.write(PCF8523_CLKOUTCONTROL);
  Wire.endTransmission();
  
  Wire.requestFrom((uint8_t)PCF8523_ADDRESS, (uint8_t)1);
  mode = Wire.read();

  mode >>= 3;
  mode &= 0x7;
  return static_cast<Pcf8523SqwPinMode>(mode);
}

void RTC_PCF8523::writeSqwPinMode(Pcf8523SqwPinMode mode) {
  Wire.beginTransmission(PCF8523_ADDRESS);
  Wire.write(PCF8523_CLKOUTCONTROL);
  Wire.write(mode << 3);
  Wire.endTransmission();
}

RTC_PCF8523 rtcExtPhy;
#endif //ADAFRUIT_FEATHERWING_RTC_SD
