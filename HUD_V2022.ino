//*********************************************************************************************\\
//Program Name: HUDR_V2022.ino
// Written By: Aaron Jacobsen, Sean Smith, Trevor McFadden, and Cari Janssen
// Function:  This program runs all the logic of the Heads-Up Display System and VPT System
//*********************************************************************************************//

// Libraries
#include <LiquidCrystal.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS.h>

// Global Vars
const String version = "V5.2"; // Used for the LCD and SD log. Probably unnecessary

// times
unsigned long startTime;
unsigned long trackMinutes;
unsigned long trackSeconds;
unsigned long lapTime; // Current lap time
unsigned long lapMinutes;
unsigned long lapSeconds;
unsigned long prevLapTime;  // Previously completed lap time
unsigned long prevLapsTime; // All the time of the previous laps - current lap
unsigned int lapCount = 0;  // Laps completed

String prevLapString;

// IO
const byte lapResetPin = 18;                                                                                                   // Button to reset lap timer
const byte CS = 53;                                                                                                            // Pin used for MISO of SD card reader. byte to save memory, may not be necessary
const byte RS = 30, E1 = 31, E2 = 32, RW = 33, DB0 = 34, DB1 = 35, DB2 = 36, DB3 = 37, DB4 = 38, DB5 = 39, DB6 = 40, DB7 = 41; // Pins used for the HUD LCD. byte to save memory, may not be necessary

// LCD Screen Declaration:
LiquidCrystal top(RS, E1, DB4, DB5, DB6, DB7);
LiquidCrystal bottom(RS, E2, DB4, DB5, DB6, DB7);

void setup()
{

    Serial.begin(9600);
    while (!Serial)
        ; // Wait for the serial port to connect. Needed

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

    // Initialize the SD card module. Print error if it doesnt begin properly
    if (!SD.begin(CS))
    {
        Serial.println("Failed to initialize the SD card...");
    }
    else
        writeToSD(true);
}

void loop()
{
    displayLCD();
}

// Handle all lapping logic whenever a lap is complete
void lapReset()
{
    Serial.println("Lap reset!");
    lapCount++;
    prevLapTime = lapTime;
    prevLapsTime += prevLapTime;
    prevLapString = String(prevLapTime / 60000) + ":" + ((prevLapTime % 60000) / 1000 < 10 ? ("0" + String((prevLapTime % 60000) / 1000)) : String((prevLapTime % 60000) / 1000)); // Creates a string for the previous lap time
}

// Function to handle displaying data to the LCD
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

// Function to control writing to the SD card
void writeToSD(bool firstStart)
{
    File file = SD.open("Data.txt", FILE_WRITE); // TODO change filename to the date. Add folders for each day its tested? SD.mkdir()
    // File fileCSV = SD.open("Data.csv", FILE_WRITE); // TODO - write a CSV file to the sd card that contains time/lat/lon/general info. Use this file to map each lap/testing to a real map for analysis

    // Check file opened and write to it
    if (file)
    {
        // When the HUD is turned on, print a statement to create a break
        if (firstStart)
        {
            file.println("----------------");
            file.println("Starting log - " + String(" 99:99 mm/dd/yy") + String(" - program ver: " + version)); // TODO add date and time of log
            file.println("----------------");
            file.close();
            return;
        }

        // TODO get the propper formatting for the output
        file.println("Testing writing to an SD card...");
    }
    else
        Serial.println("Couldn't open the file in the SD card");

    file.close();
}