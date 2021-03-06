/*** to use the SD Card datalogger shield with an Arduino Mega:
     change: #define MEGA_SOFT_SPI 0
         to: #define MEGA_SOFT_SPI 1
     in the file libraries\SD\utility\Sd2Card.h 
***/
 
/*** Configuration options ***/

/* Sensors */
#define ENABLE_BMP085
//#define ENABLE_ADXL
#define ENABLE_SDLOG
#define ENABLE_TFT
#define ENABLE_GPS
#define ENABLE_THERM

/* GPS */
#ifdef ENABLE_GPS
  #include <Adafruit_GPS.h>
  #include <SoftwareSerial.h>
  SoftwareSerial mySerial(50, 40);
  //RX40 TX50
  #define GPSECHO false
  Adafruit_GPS GPS(&mySerial);
  boolean usingInterrupt = false;
#endif

/* SD Shield */
#ifdef ENABLE_SDLOG
  #include <SD.h>
  #include "RTClib.h"
  int redLEDpin = 2;
  int greenLEDpin = 3;
  RTC_DS1307 RTC;
  const int sdCS = 10;
  char filename[] = "LOG00.csv";
  File logFile;
#endif

/* TFT Screen */
#ifdef ENABLE_TFT
  // 47 = red
  // 51 = yellow
  // 52 = green
  // 53 = orange
  #define tftcs 53
  #define tftdc 47
  #define rst 8
  #include <Adafruit_GFX.h>     //Core graphics library
  #include <Adafruit_ST7735.h>  //Hardware specific library
  #include <SPI.h>
  Adafruit_ST7735 tft = Adafruit_ST7735(tftcs, tftdc, rst);
#endif

/* I2C */
#if defined(ENABLE_BMP085) || defined(ENABLE_ADXL)
  #include <Wire.h>
  #include <Math.h>
#endif

/* Barometer */
#ifdef ENABLE_BMP085
  #include <Adafruit_BMP085.h>
  Adafruit_BMP085 dps;
  float Temperature = 0, Pressure = 0, Altitude = 0, calcAltitude = 0;
  uint32_t nextBmp085 = 0;
  //#define curPress 101964
  uint8_t j = 0;
#endif /* ENABLE_BMP085 */

/* Accelerometer */
#ifdef ENABLE_ADXL
  #include <ADXL345.h>
  ADXL345 accel;
#endif

/* Thermistor */
#ifdef ENABLE_THERM
  #define THERMISTORPIN A15
  #define THERMISTORNOMINAL 10000
  #define TEMPERATURENOMINAL 25
  #define NUMSAMPLES 5
  #define BCOEFFICIENT 3950
  #define SERIESRESISTOR 10000
  int samples[NUMSAMPLES];
  float k = 0;
#endif

void setup() 
{
  Serial.begin(115200);
  Serial.println("SAPHE Telemetry Payload");
  #ifdef ENABLE_THERM
    analogReference(EXTERNAL);
  #endif
  
    /* Setup the SD Log */
  #ifdef ENABLE_SDLOG
    Wire.begin();
    RTC.begin();
    digitalWrite(greenLEDpin, HIGH);
    digitalWrite(redLEDpin, HIGH);

    if (! RTC.isrunning()) {
      Serial.println("RTC is NOT running!");
      // following line sets the RTC to the date & time this sketch was compiled
      //RTC.adjust(DateTime(__DATE__, __TIME__));
    }

    Serial.print("Initializing SD Card...");
    pinMode(sdCS, OUTPUT);
    if (!SD.begin(sdCS)) {
      digitalWrite(greenLEDpin, LOW);
      Serial.println("Card failed, or not present");
      delay(500);
      digitalWrite(redLEDpin, LOW);
      return;
    }
    digitalWrite(redLEDpin, LOW);
    Serial.println("card initialized.");
    delay(500);
    digitalWrite(greenLEDpin, LOW);
    
    for (uint8_t i = 0; i < 100; i++) {
      filename[3] = i/10 + '0';
      filename[4] = i%10 + '0';
      if (! SD.exists(filename)) {
        logFile = SD.open(filename, FILE_WRITE);
        break;
      }
    }
    
    if (! logFile)
    {
      digitalWrite(redLEDpin, HIGH);
      Serial.println("Couldn't open log file");
    }
    else
    {
      digitalWrite(greenLEDpin, HIGH);
      String header = "ID,RTCtimestamp";
      #if defined(ENABLE_ADXL)
        header += ",X,Y,Z";
      #endif
      #if defined(ENABLE_BMP085)
        header += ",TempC,PressurePA,AltitudeM,CAltitudeM";
      #endif
      #if defined(ENABLE_GPS)
        header += ",Latitude,Longitude,Knots,GPSAltitude,Satelites,Fix,Quality,Angle,GPStimestamp";
      #endif
      #if defined(ENABLE_THERM)
        header += ",Resistance,ThermTempC";
      #endif
      logFile.println(header);
      logFile.close();
      digitalWrite(greenLEDpin, LOW);
      //Serial.println(header);
    }
  #endif /* ENABLE_SDLOG */

  /* GPS Setup */
  #if defined(ENABLE_GPS)
    GPS.begin(9600);
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    useInterrupt(true);
    delay(1000);
  #endif
  
  /* Setup the TFT */
  #if defined(ENABLE_TFT)
    tft.initR(INITR_REDTAB);
    tft.fillScreen(ST7735_BLACK);
    tft.setRotation(1);
    tft.drawString(0, 0, "Project SAPHE", ST7735_WHITE);
  #endif
  
  #if defined(ENABLE_TFT) && defined(ENABLE_GPS)
    tft.setRotation(1);
    tft.drawString(0, 50, "Lat", ST7735_WHITE);
    tft.drawString(80, 50, "Lon", ST7735_WHITE);
    tft.drawString(0, 80, "Speed", ST7735_WHITE);
    tft.drawString(50, 80, "Altitude", ST7735_WHITE);
    tft.drawString(120, 80, "Sats", ST7735_WHITE);
  #endif
  
  #if defined(ENABLE_TFT) && defined(ENABLE_BMP085)
    tft.setRotation(1);
    tft.drawString(0, 110, "Pa", ST7735_WHITE);
    tft.drawString(50, 110, "Int Temp", ST7735_WHITE);
  #endif
  
  #if defined(ENABLE_TFT) && defined(ENABLE_THERM)
    tft.drawString(110, 110, "Ext Temp", ST7735_WHITE);
  #endif
        
  /* I2C */
  #if defined(ENABLE_BMP085) || defined(ENABLE_ADXL)
    Wire.begin();
    delay(1);
  #endif
  
  #if defined(ENABLE_BMP085)
    dps.begin(BMP085_STANDARD); // Initialize for relative altitude    
    ;
    nextBmp085 = millis() + 1000;
    delay(500);
  #endif /* ENABLE_BMP085 */

  #if defined(ENABLE_ADXL)
    accel = ADXL345();
    if(accel.EnsureConnected()){
      Serial.println("Connected to ADXL345");
    }
    else {
      Serial.println("Could not connect to ADXL");
    }
    accel.EnableMeasurements();
  #endif
  delay(1000);
}

#ifdef ENABLE_GPS
  /* GPS Interupt */
  // Interrupt is called once a millisecond, looks for any new GPS data, and stores it
  SIGNAL(TIMER0_COMPA_vect) {
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    if (GPSECHO)
      if (c) UDR0 = c;  
      // writing direct to UDR0 is much much faster than Serial.print 
      // but only one character can be written at a time. 
  }

  void useInterrupt(boolean v) {
    if (v) {
      // Timer0 is already used for millis() - we'll just interrupt somewhere
      // in the middle and call the "Compare A" function above
      OCR0A = 0xAF;
      TIMSK0 |= _BV(OCIE0A);
      usingInterrupt = true;
    } else {
      // do not call the interrupt function COMPA anymore
      TIMSK0 &= ~_BV(OCIE0A);
      usingInterrupt = false;
    }
  }

  uint16_t timer = millis();
#endif

void loop()
{
  #if defined(ENABLE_GPS)
    if (! usingInterrupt) {
      // read data from the GPS in the 'main loop'
      char c = GPS.read();
      // if you want to debug, this is a good time to do it!
      if (GPSECHO)
        if (c) UDR0 = c;
        // writing direct to UDR0 is much much faster than Serial.print 
        // but only one character can be written at a time. 
     }
  
    // if a sentence is received, we can check the checksum, parse it...
    if (GPS.newNMEAreceived()) {
      // a tricky thing here is if we print the NMEA sentence, or data
      // we end up not listening and catching other sentences! 
      // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
      //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
      if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
        return;  // we can fail to parse a sentence in which case we should just wait for another
    }
  
    // approximately every 2 seconds or so, print out the current stats
    if (millis() - timer > 2000) { 
      timer = millis(); // reset the timer
//      Serial.print("\nTime: ");
//      Serial.print(GPS.hour, DEC); Serial.print(':');
//      Serial.print(GPS.minute, DEC); Serial.print(':');
//      Serial.print(GPS.seconds, DEC); Serial.print('.');
//      Serial.println(GPS.milliseconds);
//      Serial.print("Date: ");
//      Serial.print(GPS.day, DEC); Serial.print('/');
//      Serial.print(GPS.month, DEC); Serial.print("/20");
//      Serial.println(GPS.year, DEC);
//      Serial.print("Fix: "); Serial.print(GPS.fix);
//      Serial.print(" quality: "); Serial.println(GPS.fixquality); 
//      if (GPS.fix) {
//        Serial.print("Location: ");
//        Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
//        Serial.print(", "); 
//        Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
//        Serial.print("Speed (knots): "); Serial.println(GPS.speed);
//        Serial.print("Angle: "); Serial.println(GPS.angle);
//        Serial.print("Altitude: "); Serial.println(GPS.altitude);
//        Serial.print("Satellites: "); Serial.println(GPS.satellites);
//      }
    }
  #endif

  // IF SD Datalogger shield enabled, Put RTC info into variables //
  #if defined(ENABLE_SDLOG)
    String utime;
    String rtimestamp;
    String comma = ","; String colon = ":";
    String slash = "/"; String space = " ";
    String dot = ".";
    String rsmonth = "0"; String rsday = "0";
    String rsmin = "0"; String rssec = "0";
    DateTime now = RTC.now();
    utime = (String) now.unixtime();
    char ryear[5]; dtostrf(now.year(), 4, 0, ryear);
    char rmonth[3]; 
    if(now.month() <= 9) {
      dtostrf(now.month(), 1, 0, rmonth);
      rsmonth += rmonth;
    }
    else {
      dtostrf(now.month(), 2, 0, rmonth);
      rsmonth = String(rmonth);
    }
    char rday[3]; 
    if(now.day() <= 9) {
      dtostrf(now.day(), 1, 0, rday);
      rsday += rday;
    }
    else {
      dtostrf(now.day(), 1, 0, rday);
      rsday = String(rday);
    }
    char rhour[3]; dtostrf(now.hour(), 2, 0, rhour);
    char rmin[3]; 
    if(now.minute() <= 9) {
      dtostrf(now.minute(), 1, 0, rmin);
      rsmin += rmin;
    }
    else {
      dtostrf(now.minute(), 2, 0, rmin);
      rsmin = String(rmin);
    }      
    char rsec[3]; 
    if(now.second() <= 9) {
      dtostrf(now.second(), 1, 0, rsec);
      rssec += rsec;
    }
    else {
      dtostrf(now.second(), 2, 0, rsec);
      rssec = String(rsec);
    }
    if (! RTC.isrunning()) {
      rtimestamp = "RTC is NOT running!";
    }
    else {
      rtimestamp = (ryear + slash + rsmonth + slash + rsday + space
                  + rhour + colon + rsmin + colon + rssec);
    }
    Serial.println(rtimestamp);
  #endif

  // IF GPS is connected and SD Logger, read into variables //
  #if defined(ENABLE_GPS) && defined(ENABLE_SDLOG)
    char lat[10]; dtostrf(GPS.latitude, 9, 4, lat);
    char lon[10]; dtostrf(GPS.longitude, 9, 4, lon);
    char gspd[10]; dtostrf(GPS.speed, 4, 1, gspd);
    char galt[10]; dtostrf(GPS.altitude, 7, 0, galt);
    char gsat[5]; dtostrf(GPS.satellites, 3, 0, gsat);
    char ghour[3]; dtostrf(GPS.hour, 2, 0, ghour);
    char gmin[3]; dtostrf(GPS.minute, 2, 0, gmin);
    char gsec[4]; dtostrf(GPS.seconds, 2, 0, gsec);
    char gday[4]; dtostrf(GPS.day, 2, 0, gday);
    char gmon[2]; dtostrf(GPS.month, 1, 0, gmon);
    char gyeara[3]; dtostrf(GPS.year, 2, 0, gyeara);
    String milen("20");
    String gyearc = String(milen + gyeara);
    char gfix[5]; dtostrf(GPS.fix, 2, 0, gfix);
    char gqual[5]; dtostrf(GPS.fixquality, 2, 0, gqual);
    char gang[5]; dtostrf(GPS.angle, 3, 0, gang);
  #endif

  // IF BMP enabled, read values into variables //
  #ifdef ENABLE_BMP085
    if (millis() > nextBmp085) {
      Temperature = dps.readTemperature();
      Pressure = dps.readPressure();
      Altitude = dps.readAltitude();
      calcAltitude = dps.readAltitude(101964); //change curPress to actual pressure (PA) as reported weather.com
//      Serial.print("Temp(C):");
//      Serial.print(Temperature);
//      Serial.print("  Pressure(Pa):");
//      Serial.print(Pressure);
//      Serial.print("  Alt(m):");
//      Serial.print(Altitude);
//      Serial.print("  Calc Altitude:");
//      Serial.println(calcAltitude);
    }
  #endif 

  // IF SD Datalogger and BMP085, gather BMP info into variables //
  #if defined(ENABLE_BMP085) && defined(ENABLE_SDLOG)
    char temp[6]; dtostrf(Temperature, 4, 2, temp);
    String tempFs;
    String tempCs; tempCs = String(temp) + " ";
    char tempF[6]; dtostrf((Temperature * 9 / 5 + 32), 4, 2, tempF);
    tempFs = String(tempF) + "F";
    char press[9]; dtostrf(Pressure, 6, 0, press);
    char alt[8]; dtostrf(Altitude, 4, 1, alt); 
    char calt[9]; dtostrf(calcAltitude, 8, 1, calt);
  #endif
  
  // IF ADXL enabled, grab info into variables //
  #if defined(ENABLE_ADXL)
    AccelerometerRaw raw = accel.ReadRawAxis();
    int xAxisRawData = raw.XAxis;
    int yAxisRawData = raw.YAxis;
    int zAxisRawData = raw.ZAxis;
    AccelerometerScaled scaled = accel.ReadScaledAxis();
    float xAxisGs = scaled.XAxis;
//    Serial.print("X: ");
//    Serial.print(raw.XAxis);
//    Serial.print("   Y: ");
//    Serial.print(raw.YAxis);
//    Serial.print("   Z: ");
//    Serial.println(raw.ZAxis);
  #endif
  
  #if defined(ENABLE_THERM)
    float thermR;
    float thermC;
    float thermF;
    char thermRc[6];
    char thermCc[6];
    k = analogRead(THERMISTORPIN);
    Serial.println(k);
    k = (1023 / k) - 1;
    k = SERIESRESISTOR / k;
    thermR = k;
    Serial.println(thermR);
    float steinhart;
    steinhart = thermR / THERMISTORNOMINAL;
    steinhart = log(steinhart);
    steinhart /= BCOEFFICIENT;
    steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15;
    thermC = steinhart;
//    thermC = 25;
    Serial.println(thermC);
    thermF = ((thermC * 9) / 5) + 32;
    Serial.println(thermF);
    dtostrf(thermR, 4, 2, thermRc);
    dtostrf(thermC, 4, 2, thermCc);
    String thermFs;
    String thermCs; thermCs = String(thermCc) + "  ";
    char thermFc[6]; dtostrf(thermF, 4, 2, thermFc);
    k = 0;
  #endif
  
  #if defined(ENABLE_SDLOG)
    String record;

/* RTC */    
    record = (utime + comma + ryear + slash + rmonth + slash + rday + 
              space + rhour + colon + rmin + colon + rsec + comma);
/* AXL */
    #if defined(ENABLE_ADXL)
      record += (xAxisRawData + comma + yAxisRawData + comma + zAxisRawData + comma);
    #endif  

/* BMP */
    #if defined(ENABLE_BMP085)
      record += (temp + comma + press + comma + alt + comma + calt + comma);
    #endif

/* GPS */
    #if defined(ENABLE_GPS)
      record += (lat + comma + lon + comma + gspd + comma + galt + comma + 
                 gsat + comma + gfix + comma + gqual + comma + gang + comma +
                 gyearc + slash + gmon + slash + gday + space + 
                 ghour + colon + gmin + colon + gsec + comma);
    #endif
  
/* THM */
    #if defined(ENABLE_THERM)
      record += (String(thermRc) + comma + String(thermCc));
    #endif
  
//    Serial.println();
//    String testgtime = (gyearc + slash + gmon + slash + gday + space + ghour + colon + gmin + colon + gsec + comma);
//    Serial.println(testgtime);
//    Serial.println();
    if (SD.exists(filename)) {
      logFile = SD.open(filename, FILE_WRITE);
    }
    if (logFile)
    {
      digitalWrite(greenLEDpin, HIGH);
      logFile.println(record);
      logFile.close();
      digitalWrite(greenLEDpin, LOW);
      Serial.println(record);
    }
    else
    {
      digitalWrite(redLEDpin, HIGH);
      Serial.print("error opening log ");
      Serial.println(filename);
    }
  #endif
  
  #if defined(ENABLE_TFT) && defined(ENABLE_SDLOG)
    char time_char[32];
    rtimestamp.toCharArray(time_char, 32);
    tft.setRotation(1);
    tft.drawString(0, 20, time_char, ST7735_WHITE);
    //tft.drawString(0, 20, rtimestamp, ST7735_WHITE);
    tft.drawString(0, 30, filename, ST7735_WHITE);
  #endif
  
  #if defined(ENABLE_TFT) && defined(ENABLE_GPS)
    tft.setRotation(1);
        //4002.2719N, 7503.6269W
    tft.drawString(0, 60, lat, ST7735_WHITE);
    tft.drawString(80, 60, lon, ST7735_WHITE);
    tft.drawString(0, 90, gspd, ST7735_WHITE);
    tft.drawString(50, 90, galt, ST7735_WHITE);
    tft.drawString(120, 90, gsat, ST7735_WHITE);
  #endif

  #if defined(ENABLE_TFT) && defined(ENABLE_BMP085)
    tft.setRotation(1);
    tft.drawString(0, 120, press, ST7735_WHITE);
    
    if(j%2 == 0) {
      char tempCsa[7];
      tempCs.toCharArray(tempCsa, 7);
      tft.drawString(50, 120, tempCsa, ST7735_WHITE);
    }
    else {
      char tempFsa[7];
      tempFs.toCharArray(tempFsa, 7); 
      tft.drawString(50, 120, tempFsa, ST7735_WHITE);
    }
  #endif
  
  #if defined(ENABLE_TFT) && defined(ENABLE_THERM)
    tft.setRotation(1);
    if(j%2 == 0) {
      tft.drawString(110, 120, thermCc, ST7735_WHITE);
    }
    else {
      tft.drawString(110, 120, thermFc, ST7735_WHITE);
    }
    j = j+1;
  #endif
  
//  delay(10);
//  digitalWrite(redLEDpin, LOW);
//  digitalWrite(greenLEDpin, LOW);
  delay(1000);
}
