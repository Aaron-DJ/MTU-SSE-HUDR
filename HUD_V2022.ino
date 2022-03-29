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
#include <NMEAGPS.h> // Make sure all of the cfg files are setup for this application. See documentation
#include <GPSport.h> // This must be setup correctly. See documentation


// Global Variables
const String version = "V5.2"; // Used for the LCD and SD log. Probably unnecessary

SdFat SD;
NMEAGPS gps;
File logFile;

// GPS acquired variables -- All are initialized to zero just incase gps data is never received
// Stored in a struct to avoid any name collisions and ease of access
struct GPSdata {
    float lat = 0;
    float lng = 0;
    int alt = 0;
    float speedMPH = 0;
    NeoGPS::time_t dateTime = {}; // Structure that holds all time and date information
    float latErr = 0;
    float lngErr = 0;
    float altErr = 0;
} currentGPSData, lastGPSData;

// Store how far the vehicle has travelled on current run
float travelledDistance = 0;

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

        // Found a valid GPS signal with proper location and time, any logging and data updating should be done within this if statement
        if (fix.valid.location && fix.valid.time)
        {
            uint16_t lastLoggingTime = 0;
            uint16_t startLogTime = millis();

            updateGPSdata(fix);

            logGPSdata();
            lastLoggingTime = millis() - startLogTime;

            // Flush the file buffer ever second. Necessary to get data into log
            // This should be updated at somepoint with a file.close() attached to some sort of paramater
            uint16_t lastFlushTime = 0;

            if(startLogTime - lastFlushTime > 1000) {
                lastFlushTime = startLogTime;
                logFile.flush();
            }

            // Update the last point to the current to be used at next interval
            lastGPSData = currentGPSData;
        }

    }
}

// Write all of the required GPS data to the logFile
void logGPSdata() 
{
    // Print all of the time data into the first spot
    logFile.print(
        String(currentGPSData.dateTime.month) + "/" +
        String(currentGPSData.dateTime.date) + "/" +
        String(currentGPSData.dateTime.year) + " " +
        String(currentGPSData.dateTime.hours) + ":");

    logFile.print(normalizeTen(currentGPSData.dateTime.minutes) + ":");
    
    logFile.print(normalizeTen(currentGPSData.dateTime.seconds) + ",");

    logFile.print(String(trackMinutes) + ":");
   
    logFile.print(normalizeTen(trackSeconds) + ",");

    logFile.print(String(lapCount) + ",");

    logFile.print(String(currentGPSData.lat, 6) + ",");
    logFile.print(String(currentGPSData.lng, 6) + ",");
    logFile.print(String(currentGPSData.alt) + ",");

    logFile.print(String(currentGPSData.speedMPH));
    logFile.println();
}

// Updates all the data in the gpsdata struct by taking in a gps_fix
void updateGPSdata(gps_fix fix) 
{
    // Update all of the data in currentGPSData
    currentGPSData.lat = fix.latitude();
    currentGPSData.lng = fix.longitude();
    currentGPSData.alt = (fix.altitude() * 3.281f); // Convert meters to feet
    currentGPSData.speedMPH = fix.speed_mph();
    currentGPSData.dateTime = fix.dateTime;
    currentGPSData.latErr = fix.lat_err() * 3.281f;
    currentGPSData.lngErr = fix.lon_err() * 3.281f;
    currentGPSData.altErr = fix.alt_err() * 3.281f;
}

// Called whenever the gps sends an interrupt to the arduino
void GPSisr(uint8_t c) 
{
    gps.handle(c);
}

// Waits until GPSisr provides a valid location
void waitForGPS()
{
    DEBUG_PORT.println("Waiting for GPS data..."); // Print to serial debug for verification of GPS
    top.setCursor(0, 0);
    top.print("Waiting for GPS data..."); // Print to HUD so user can see what is happening

    int lastToggle = millis(); // Start time of the waiting for GPS

    while(1) { // Loop indefinitely until GPS signal is acquired
        if(gps.available()) {
            gps_fix fix = gps.read();
            if (fix.valid.location && fix.valid.time) { // Valid GPS signal is found
                updateGPSdata(fix); // Set initial GPS data
                lastGPSData = currentGPSData; // Set the lastGPSData so that it has a starting value
                break; // GPS verified location, can now break out of loop
            }
        }

        // Flash LED until gps is found
        if(millis() - lastToggle > 500) {
            lastToggle += 500;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
    }

    // Turn off debug led
    digitalWrite(LED_BUILTIN, LOW);

    // Reset GPS overrun
    gps.overrun(false);
}

/*
 *  This is the main setup function that is ran when the arduino first runs.
 *  For more information see section 3 of documentation
 */
void setup()
{
    DEBUG_PORT.begin(9600);
    while (!DEBUG_PORT); // Wait for the serial port to connect. Needed

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

    // Wait for valid GPS location
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
    GPSLoop();
    displayLCD();

    if (gps.overrun())
    {
        gps.overrun(false);
        DEBUG_PORT.println(F("DATA OVERRUN: fix data lost!"));
    }
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
    prevLapString = String(prevLapTime / 60000) + ":" + normalizeTen((prevLapTime % 60000) / 1000); // Creates a string for the previous lap time
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
    top.print(normalizeTen(trackSeconds)); // Add a zero when the seconds are below 10 to display properly

    // Prints speed to driver
    top.setCursor(18, 1);
    top.print("Speed: ");
    top.print(currentGPSData.speedMPH); // Display speed starting at 22

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
    bottom.print(travelledDistance);

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
    bottom.print(normalizeTen(lapSeconds)); // Add a zero when the seconds are below 10 to display properly

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
    
    if (!SD.begin(CS)) // SD doesnt initialize, likely not inserted. Print error
    {
        DEBUG_PORT.println("Failed to initialize the SD card...    Will not log this run");
    }
    else
    {
        String date = String(currentGPSData.dateTime.month) + "_" + String(currentGPSData.dateTime.date) + "_" + String(currentGPSData.dateTime.year); // Create a string of the data EX: 03_29_2022
        DEBUG_PORT.println("Date: " + date); // print the date for debug purposes.
        if (!SD.exists("/" + date)) // Check if there has been a folder made today. If not, create a new folder with the date.
        {
            SD.mkdir(date);
        } 
        SD.chdir("/" + date); // Move into the folder with todays date

        String time = String(currentGPSData.dateTime.hours) + String(currentGPSData.dateTime.minutes) + String(currentGPSData.dateTime.seconds); // Create a string for time in hhmmss

        logFile = SD.open(time + ".txt", FILE_WRITE); // Open/create a new file with the name of time within the folder for the current date

        logFile.println("Date_Time,Elapsed_Time,Lap,Latitude,Longitude,Elevation,Speed");
        // Temp remove this, look into for future... Reading and closing file is not feasable. Add switch for logging?
        // writeToSD(true);
    }
}

// Helper function for when displaying times and there may be a leading 0 to compensate
String normalizeTen(int time) {
    return time < 10 ? ("0" + String(time)) : (String(time));
}

// --- NOT BEING USED AS OF NOW --- //
// Function to control writing to the SD card
// More information in section 6 of documentation
void writeToSD(bool firstStart)
{
    logFile = SD.open("Data.txt", FILE_WRITE); // TODO change filename to the date.
    // File fileCSV = SD.open("Data.csv", FILE_WRITE); // TODO - write a CSV file to the sd card that contains time/lat/lon/general info. Use this file to map each lap/testing to a real map for analysis

    // Check file opened and write to it
    if (logFile)
    {
        // When the HUD is turned on, print a statement to create a break
        if (firstStart)
        {
            logFile.println("Date_Time,Elapsed_Time,Lap,Latitude,Longitude,Elevation,Speed");
            logFile.close();
            return;
        }

    }
    else
        DEBUG_PORT.println("Couldn't open the file in the SD card");

    logFile.close();
}
