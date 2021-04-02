//PINOUTS FOR MEGA:
//GPS
//RX -> 14 (TX3)
//TX -> 15 (RX3)
//
//SD
//CS -> 10
//DI -> 51
//DO -> 50
//CLK -> 52
//
//PRESSURE
//SIGNAL -> A0
//
//WATER LEVEL
//SIGNAL -> A1

#include <Adafruit_GPS.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <utility/imumaths.h>
#include <math.h>

#define BAUD_RATE 115200
#define BNO055_SAMPLERATE_DELAY_MS 100

//define pin assignments
#define GPSSerial Serial3
#define PRESSUREPIN A0
#define WATERPIN A1
#define CHIPSELECT 10

//define global variables
uint32_t timer = millis();
String outputString = "";
String filename = "";

//Initialize the hardware
//GPS
Adafruit_GPS GPS(&GPSSerial);
//Orientation
Adafruit_BNO055 bno = Adafruit_BNO055();
//Barometric Pressure
Adafruit_BMP085 bmp = Adafruit_BMP085();

void setup() {
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(BAUD_RATE);

  //initialize orientation sensor
  bno.begin();
  delay(1000);
  bno.setExtCrystalUse(true);

  //initialize barometric pressure sensor
  bmp.begin();

  initializeGPS();

  initializeSD();
}

void loop() {

  readAndParseGPS();

  // approximately every second or so, print out the current stats
  if (millis() - timer > 1000) {
    timer = millis(); // reset the timer

    //add sensor data to a single output string
    gpsString();
    pressureString();
    waterIntrusionString();
    calibrationString();
    quatString();
    eulerString();

    writeToSD();
    outputString = "";
  }
}

void initializeGPS() {
  //initialize GPS
  Serial.println("Initializing GPS...");
  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate

  //wait until first fix to continue initialization
  Serial.print("Awaiting GPS fix... ");
  while (GPS.fix == 0) {
    readAndParseGPS();
  }
  Serial.println("GPS fix found.");
}

void initializeSD() {
  //initialize SD
  Serial.println("Initializing SD...");
  SD.begin(CHIPSELECT);
  Serial.println("SD Initialized");

  //generate a filename
  Serial.println("Generating filename from GPS date/time data...");
  while (filename == "00000000" || filename == "") {
    char c = GPS.read();
    GPS.parse(GPS.lastNMEA());

    filename = "";
    if (GPS.month < 10) { filename.concat("0"); }
    filename.concat(GPS.month);
    if (GPS.day < 10) { filename.concat("0"); }
    filename.concat(GPS.day);
    if (GPS.hour < 10) { filename.concat("0"); }
    filename.concat(GPS.hour);
    if (GPS.minute < 10) { filename.concat("0"); }
    filename.concat(GPS.minute);
  }
  filename.concat(".csv");
  Serial.print("Filename ");
  Serial.print(filename);
  Serial.println(" created");

  //write header data to SD
  File dataFile = SD.open(filename, FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println("Date, Time, Latitude, Longitude, GPS Fix, Force, Water Level, Accel, Gyro, Magne, Sys, Quat W, Quat X, Quat Y, Quat Z, Euler X, Euler Y, Euler Z");
    Serial.print("Header data written to ");
    Serial.println(filename);
  }
  else {
    Serial.print("Unable to open ");
    Serial.println(filename);
  }
  dataFile.close();
}

void readAndParseGPS() {
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    //Serial.print(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }
}

void writeToSD() {
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(filename, FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(outputString);
    Serial.print("Printed to ");
    Serial.print(filename);
    Serial.print(" : ");
    Serial.println(outputString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.print("error opening ");
    Serial.println(filename);
  }
  dataFile.close();
}

void gpsString() {
  outputString.concat(GPS.day);
  outputString.concat("/");
  outputString.concat(GPS.month);
  outputString.concat("/20");
  outputString.concat(GPS.year);
  outputString.concat(", ");
  if (GPS.hour < 10) { outputString.concat("0"); }
  outputString.concat(GPS.hour);
  outputString.concat(":");
  if (GPS.minute < 10) { outputString.concat("0"); }
  outputString.concat(GPS.minute);
  outputString.concat(":");
  if (GPS.seconds < 10) { outputString.concat("0"); }
  outputString.concat(GPS.seconds);
  outputString.concat(", ");

  if (GPS.fix) {
    outputString.concat(String(GPS.latitudeDegrees, 5));
    outputString.concat(", ");
    outputString.concat(String(GPS.longitudeDegrees, 5));
  }
  else {
    outputString.concat("0");
    outputString.concat(", ");
    outputString.concat("0");
  }

  outputString.concat(", ");
  outputString.concat(GPS.fix);
}

void pressureString() {
  outputString.concat(", ");
  outputString.concat(analogRead(PRESSUREPIN));
}

void waterIntrusionString() {
  outputString.concat(", ");
  outputString.concat(analogRead(WATERPIN));
}

void calibrationString() {
  uint8_t accel, gyro, magne, sys = 0;
  bno.getCalibration(&sys, &gyro, &accel, &magne);
  outputString.concat(", ");
  outputString.concat(int(accel));
  outputString.concat(", ");
  outputString.concat(int(gyro));
  outputString.concat(", ");
  outputString.concat(int(magne));
  outputString.concat(", ");
  outputString.concat(int(sys));
  outputString.concat(", ");
}

void quatString() {
  imu::Quaternion quat = bno.getQuat();
  outputString.concat(quat.w());
  outputString.concat(", ");
  outputString.concat(quat.x());
  outputString.concat(", ");
  outputString.concat(quat.y());
  outputString.concat(", ");
  outputString.concat(quat.z());
  outputString.concat(", ");
}

void eulerString() {
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  outputString.concat(euler.x());
  outputString.concat(", ");
  outputString.concat(euler.y());
  outputString.concat(", ");
  outputString.concat(euler.z());
}
