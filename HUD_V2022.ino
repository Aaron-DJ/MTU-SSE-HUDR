//*********************************************************************************************//
// Program Name: HUDR_V2022.ino
// Written By: Aaron Jacobsen, Sean Smith, Trevor McFadden, and Cari Janssen
// Function:  This program runs all the logic of the Heads-Up Display System and VPT System
// More in-depth documentation of the code can be viewed at: https://docs.google.com/document/d/1ebYYVj_0Im3cEVpizzCzw-A3DkRRQ7Kt3Cyyll7n5Is
//*********************************************************************************************//

// Libraries -- See section 1
#include <LiquidCrystal.h>
#include <SPI.h>
#include <SdFat.h>
#include <NMEAGPS.h>
#include <GPSport.h>

// Check configuration

// #ifndef NMEAGPS_INTERRUPT_PROCESSING
//     #error You must define NMEAGPS_INTERRUPT_PROCESSING in NMEAGPS_cfg.h!
// #endif

// Global Variables
const String version = "V5.2"; // Used for the LCD and SD log. Probably unnecessary

SdFat SD;
NMEAGPS gps;
File logFile;

// times -- See section 2.a
unsigned long startTime; // Maintain the time in millis at the start of running the program
unsigned long trackMinutes;
unsigned long trackSeconds;
unsigned long lapTime; // Current lap time
unsigned long lapMinutes;
unsigned long lapSeconds;
unsigned long prevLapTime;  // Previously completed lap time
unsigned long prevLapsTime; // All the time of the previous laps - current lap
unsigned int lapCount = 0;  // Laps completed

String prevLapString;

// IO -- See section 2.b
const byte lapResetPin = 18; // Button to reset lap timer
const byte CS = 53; // Pin used for MISO of SD card reader. byte to save memory, may not be necessary
const byte RS = 30, E1 = 31, E2 = 32, RW = 33, DB0 = 34, DB1 = 35, DB2 = 36, DB3 = 37, DB4 = 38, DB5 = 39, DB6 = 40, DB7 = 41; // Pins used for the HUD LCD. byte to save memory, may not be necessary

// LCD Screen Declaration:
LiquidCrystal top(RS, E1, DB4, DB5, DB6, DB7);
LiquidCrystal bottom(RS, E2, DB4, DB5, DB6, DB7);


void GPSLoop()
{
    if(gps.available()) {
        gps_fix fix = gps.read();

        if (fix.valid.location && fix.valid.time)
        {
            //Log / update data and stuff in here
        }
    }
}

void GPSisr(uint8_t c) 
{
    gps.handle(c);
}

// Waits until GPSisr provides a valid location
void waitForGPS()
{
    DEBUG_PORT.println("Waiting for GPS data...");

    int lastToggle = millis();

    while(1) {
        if(gps.available()) {
            if(gps.read().valid.location)
                break; // GPS verified location, can now break out of loop
        }

        // Flash LED until gps is found
        if(millis() - lastToggle > 500) {
            lastToggle += 500;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
    }

    digitalWrite(LED_BUILTIN, LOW);

    gps.overrun(false);
}

/*
 *  This is the main setup function that is ran when the arduino first runs.
 *  For more information see section 3 of documentation
 */
void setup()
{
    DEBUG_PORT.begin(9600);
    while (!NeoSerial); // Wait for the serial port to connect. Needed

    gpsPort.attachInterrupt(GPSisr); // Add interrupt so that the gps only fills buffer at intervals
    gpsPort.begin(9600); // start the gps port at 9600 baud

    // Set the start time of the program
    startTime = millis();
    lapTime = startTime; // Set to 0?
    prevLapTime = lapTime;

    // Setup interrupt for the lap reset
    pinMode(lapResetPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(lapResetPin), lapReset, RISING); // Calls the lapReset function whenever pin 18 gets a rising edge signal

    // Set the LCD to write
    pinMode(RW, OUTPUT);
    digitalWrite(RW, LOW);

    // Being the LCDs, sets the size
    top.begin(40, 2);
    bottom.begin(40, 2);

    // Intialize the pin for the built in LED for gps debugging
    pinMode(LED_BUILTIN, OUTPUT);

    // Wait for GPS location
    waitForGPS();

    // Initialize the SD card
    initSD();
}

/*
 *  This is the main Loop function that is ran ever clock cycle
 *  For more information see section 4 of documentation
 */
void loop()
{    
    // GPSLoop();
    // displayLCD();
}

/*  
 *  Function that is attached to a pin interrupt to handle the functionality of counting laps and reseting lap timer
 *  For more information see section 5 of documentation
 */
void lapReset()
{
    DEBUG_PORT.println("Lap reset!");
    lapCount++;
    prevLapTime = lapTime;
    prevLapsTime += prevLapTime;
    prevLapString = String(prevLapTime / 60000) + ":" + ((prevLapTime % 60000) / 1000 < 10 ? ("0" + String((prevLapTime % 60000) / 1000)) : String((prevLapTime % 60000) / 1000)); // Creates a string for the previous lap time
}

// Function to handle displaying data to the LCD
// More information in section 4.a 
void displayLCD()
{

    // Update Time Variables
    trackMinutes = (millis() - startTime) / 60000;          // minutes
    trackSeconds = ((millis() - startTime) % 60000) / 1000; // seconds
    lapTime = millis() - prevLapsTime;                      // Time for the current lap
    lapMinutes = lapTime / 60000;                           // lap minutes
    lapSeconds = (lapTime % 60000) / 1000;                  // lap seconds

    /* Print Top */
    top.setCursor(0, 0);
    top.print("Supermileage Systems Enterprise     " + version);
    top.setCursor(0, 1);
    top.print("TRACK TIME:");
    top.setCursor(12, 1);
    top.print(trackMinutes);
    top.print(":");
    top.print(trackSeconds < 10 ? ("0" + (String)trackSeconds) : trackSeconds); // Add a zero when the seconds are below 10 to display properly

    // TODO print the latitude and longitude (speed may be more benefitial)
    top.setCursor(18, 1);
    top.print("Lat: ");
    // Display lat starting at 22
    top.setCursor(30, 1);
    top.print("Lng:");
    // Displat long starting at 36

    /* Print Bottom */
    // Lap counter
    bottom.setCursor(0, 0);
    bottom.print("COUNT:");
    bottom.setCursor(7, 0);
    bottom.print(lapCount);

    // Lap distance
    bottom.setCursor(12, 0);
    bottom.print("LAP DIST:");
    bottom.setCursor(21, 0);
    // TODO get distance traveled and print it

    // Engine temp
    bottom.setCursor(27, 0);
    bottom.print("ENG TEMP: ");
    bottom.setCursor(37, 0);
    // TODO add engine temp print out

    // Lap time
    bottom.setCursor(0, 1);
    bottom.print("LAP:");
    bottom.setCursor(5, 1);
    bottom.print(lapMinutes);
    bottom.print(":");
    bottom.print(lapSeconds < 10 ? ("0" + (String)lapSeconds) : lapSeconds); // Add a zero when the seconds are below 10 to display properly

    bottom.setCursor(12, 1);
    bottom.print("PREV LAP: ");
    bottom.print(prevLapString);

    bottom.setCursor(28, 1);
    bottom.print("AVG: ");
    // Not working right now.. giving weird time values, should be an easy fix if we want it to work
    // Make sure that lap count isn't zero... dividing by zero doesn't work
    // unsigned int avgLapTime = prevLapsTime/lapCount; // Average time to complete a lap
    // if(lapCount>0) bottom.print(String(avgLapTime/60000) + ":" + (avgLapTime < 10 ? ("0" + String((avgLapTime%60000)/1000)) : String((avgLapTime%60000)/1000)));
}

// Initialize the SD card module. Print error if it doesnt begin properly
void initSD() 
{
    if (!SD.begin(CS))
    {
        DEBUG_PORT.println("Failed to initialize the SD card...");
    }
    else
        writeToSD(true);
}

// Function to control writing to the SD card
// More information in section 6 of documentation
void writeToSD(bool firstStart)
{
    logFile = SD.open("Data.txt", FILE_WRITE); // TODO change filename to the date. Add folders for each day its tested? SD.mkdir()
    // File fileCSV = SD.open("Data.csv", FILE_WRITE); // TODO - write a CSV file to the sd card that contains time/lat/lon/general info. Use this file to map each lap/testing to a real map for analysis

    // Check file opened and write to it
    if (logFile)
    {
        // When the HUD is turned on, print a statement to create a break
        if (firstStart)
        {
            logFile.println("----------------");
            logFile.println("Starting log - " + String(" 99:99 mm/dd/yy") + String(" - program ver: " + version)); // TODO add date and time of log
            logFile.println("----------------");
            logFile.close();
            return;
        }

        // TODO get the propper formatting for the output
        logFile.println("Testing writing to an SD card...");
    }
    else
        DEBUG_PORT.println("Couldn't open the file in the SD card");

    logFile.close();
}
