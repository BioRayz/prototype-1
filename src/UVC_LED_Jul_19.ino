/***************************************************
  INCLUDES
*/

#include <Wire.h>
/*
   Download the library from https://github.com/JChristensen/Timer
   extract it and then put it in Documents -> Arduino -> Libraries
*/
#include "Timer.h"
/*
   Download the library from https://www.seeedstudio.com/Grove-LCD-RGB-Backlight.html
   extract it and then put it in Documents -> Arduino -> Libraries
*/
#include "rgb_lcd.h"
/*
   Download the library from https://www.seeedstudio.com/Grove-Ultrasonic-Ranger-p-960.html
   extract it and then put it in Documents -> Arduino -> Libraries
*/
#include "Ultrasonic.h"

/***************************************************
  DEFINES
*/

#define UVC_LEDS_PIN_NUMBER                         0 //Connect the MOSFET to D5 connector
#define ULTRASONIC_SENSOR_PIN_NUMBER                2 //Connect ultrasonic sensor to D2 connector
#define TOUCH_SENSOR_PIN                            3 //Connect the touch sensor to the D3 connector
#define COMPLETED_LED_PIN_NUMBER                    4 //Connect RGB LED to the D4 connector

#define TOP_LED_1_PIN_NUMBER                        5 //Top LED 1 pin number
#define TOP_LED_2_PIN_NUMBER                        6 //Top LED 2 pin number
#define BOTTOM_LED_1_PIN_NUMBER                     7 //Bottom LED 1 pin
#define BOTTOM_LED_2_PIN_NUMBER                     8 //Bottom LED 2 pin

#define DECONTAMINATION_TIME_IN_SECONDS             30

/*
   You may play with this colours to get optimal colour
   R-255, G-255, B- 255 = White,
   R-255, G-0, B-0: Red,
   R-0, G-255, B-0: Green
   R-0, G-0, B-255: Blue
*/
#define LCD_RED_COLOUR_PORTION                      255
#define LCD_GREEN_COLOUR_PORTION                    255
#define LCD_BLUE_COLOUR_PORTION                     255
#define COMPANY_NAME                                "    SANILIGHT"
#define TEAM_NAME                                   "IlluminateHealth"

/***************************************************
  FUNCTION DECLARATIONS
*/

//Touch Sensor
void initialiseTouchSensor(short pinNumber);
bool getTouchSensorValue(short pinNumber);

//LCD
void initialiseLCD(short colourRed, short colourGreen, short colourBlue, bool showTeamName);
void showCompanyAndTeamName(void);

//De contamination process
void initialiseUVCLEDs(byte topLEDPin, byte bottomLEDPin);
void startDecontaminationProcess(byte topLEDPin, byte bottomLEDPin);
void showRemainingTime(void);

/***************************************************
  GLOBAL VARIABLES
*/

//For RGB LCD
rgb_lcd lcd;
//To get ultrasonic readings
Ultrasonic ultrasonic(ULTRASONIC_SENSOR_PIN_NUMBER);
//To update the LCD display every second.
Timer LCDRefreshTimer, LEDBlinkTimer;

bool decontaminationInProcess = false, lidOpenMessageDisplayed = false;
unsigned long timeInSecondsFromStartup = 0, decontaminationStartTime = 0;
unsigned int rangeInCentimeters = 0, UpdateLCDEvent = 0;
bool LEDState = HIGH;

/***************************************************
  SETUP CODE
  Code you put in here is a setup code. Typically used to set
  the system up on start-up and will run only once - On power-cycle(restart)
*/

void setup()
{
  //For debugging.
  Serial.begin(115200);

  //Initialise LCD Display
  initialiseLCD(LCD_RED_COLOUR_PORTION, LCD_BLUE_COLOUR_PORTION, LCD_GREEN_COLOUR_PORTION, true);

  //Initialise Touch Sensor
  initialiseTouchSensor(TOUCH_SENSOR_PIN);

  //Initialise indicator LED
  initialiseIndicatorLED(COMPLETED_LED_PIN_NUMBER);

  initialiseUVCLEDs(UVC_LEDS_PIN_NUMBER);
}

/***************************************************
  CONTINUOUS CODE
*/
void loop()
{
  //Refreshing timer
  LCDRefreshTimer.update();

  //Refreshing LED Timer
  LEDBlinkTimer.update();

  //Getting ultrasonic readings to check if the lid is closed.
  rangeInCentimeters = ultrasonic.MeasureInCentimeters();
  Serial.println(rangeInCentimeters);

  //Seconds from when the board is on.
  timeInSecondsFromStartup = millis() / 1000;

  /*
     We need to turn on the LEDs on the following condition:
     (a. Someone pressed on touch sensor
                      AND
     b. Ultrasonic sensor detected that lid is closed.)
                      AND
     If the process is not already ongoing.

     This logic just cheaks for that case.
  */
  if ((getTouchSensorValue(TOUCH_SENSOR_PIN) && rangeInCentimeters < 5) && decontaminationInProcess == false)
  {
    //Updating time. This will be used to show the time remaining on LCD.
    decontaminationStartTime = timeInSecondsFromStartup;
    //Turning on the MOSFET
    startDecontaminationProcess(UVC_LEDS_PIN_NUMBER);
    //Updating the flag
    decontaminationInProcess = true;
    //Deploying the timer that refreshes the LCD display every 1000 milli-seconds.
    UpdateLCDEvent = LCDRefreshTimer.every(1000, showRemainingTime);
  }
  else if (getTouchSensorValue(TOUCH_SENSOR_PIN) && rangeInCentimeters >= 5 && lidOpenMessageDisplayed == false)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please close lid");
    lidOpenMessageDisplayed = true;
  }
  else if (rangeInCentimeters < 5 && lidOpenMessageDisplayed)
  {
    lidOpenMessageDisplayed = false;
    showCompanyAndTeamName();
  }

  if (decontaminationInProcess)
  {
    //Checking if it's time to stop the process
    if (timeInSecondsFromStartup > decontaminationStartTime +  DECONTAMINATION_TIME_IN_SECONDS)
    {
      stopDecontaminationProcess(UVC_LEDS_PIN_NUMBER);
    }

    //Checking if lid is open
    if (rangeInCentimeters > 15)
    {
      //If lid is opened
      timeInSecondsFromStartup = decontaminationStartTime +  DECONTAMINATION_TIME_IN_SECONDS + 1;
      stopDecontaminationProcess(UVC_LEDS_PIN_NUMBER);
    }
  }
}

/***************************************************
  FUNCTION DEFINITION
*/

/*
   Function Name: initialiseLCD
   Description: Initialises Grove RGB LCD
   Parameters:
    colourRed - Red colour portion of the backlight
    colourGreen - Green colour portion of the backlight
    colourBlue - Blue colour portion of the backlight
    showTeamName - Shows the name of the team for 1 second
   Returns: None
*/

void initialiseLCD(short colourRed, short colourGreen, short colourBlue, bool showTeamName)
{
  Serial.println("Initialising LCD.");
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  lcd.setRGB(colourRed, colourGreen, colourBlue);

  if (showTeamName)
  {
    //Showing your team name.
    showCompanyAndTeamName();
  }
  Serial.println("LCD Initialised.");
}

/*
   Function Name: showCompanyAndTeamName
   Description: Displays company and team names on LCD
   Parameters: None
   Returns: None
*/

void showCompanyAndTeamName(void)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(COMPANY_NAME);
  lcd.setCursor(0, 1);
  lcd.print(TEAM_NAME);
}

/*
   Function Name: initialiseTouchSensor
   Description: Gets the touch sensor reading
   Parameters:
    pinNumber - Digital pin to which sensor is attached
   Returns: None
*/

void initialiseTouchSensor(short pinNumber)
{
  Serial.println("Initialising touch sensor.");
  pinMode(pinNumber, INPUT);
  Serial.println("Touch sensor initialised.");
}

/*
   Function Name: getTouchSensorValue
   Description: Gets the touch sensor reading
   Parameters:
    pinNumber - Digital pin to which sensor is attached
   Returns:
    1(True) - If touch is detected and
    0(False) - If touch is not detected
*/


bool getTouchSensorValue(short pinNumber)
{
  return digitalRead(pinNumber);
}

/*
   Function Name: initialiseIndicatorLED
   Description: Initiates the indicator LED
   Parameters:
    pinNumber - Digital pin to which LED is attached
   Returns: None
*/

void initialiseIndicatorLED(short pinNumber)
{
  Serial.println("Initialising indicator LED.");
  pinMode(pinNumber, OUTPUT);
  Serial.println("indicator LED initialised.");
}

/*
   Function Name: initialiseUVCLEDs
   Description: Initialised UVC LEDs
   Parameters:
    MOSFETPin - Digital pin to which MOSFET is attached
   Returns: None
*/

void initialiseUVCLEDs(byte MOSFETPin)
{
  pinMode(MOSFETPin, OUTPUT);
  pinMode(BOTTOM_LED_1_PIN_NUMBER, OUTPUT);
  pinMode(BOTTOM_LED_2_PIN_NUMBER, OUTPUT);
  pinMode(TOP_LED_1_PIN_NUMBER, OUTPUT);
  pinMode(TOP_LED_2_PIN_NUMBER, OUTPUT);
}

/*
   Function Name: startDecontaminationProcess
   Description: Turns on the LEDs for decontamination process
   Parameters:
    LEDMOSFETPinNumber - Digital pin to which MOSFET is attached
   Returns: None
*/


void startDecontaminationProcess(byte LEDMOSFETPinNumber)
{
  //Clearing contents on LCD display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("De contaminating");
  lcd.setCursor(0, 1);
  lcd.print("Time Left: ");
  lcd.print(DECONTAMINATION_TIME_IN_SECONDS);
  //  digitalWrite(LEDMOSFETPinNumber, HIGH);
  analogWrite(LEDMOSFETPinNumber, 16);
  analogWrite(TOP_LED_1_PIN_NUMBER, 255);
  analogWrite(TOP_LED_2_PIN_NUMBER, 255);
  analogWrite(BOTTOM_LED_1_PIN_NUMBER, 255);
  analogWrite(BOTTOM_LED_2_PIN_NUMBER, 255);
  //Turning on LED
  LEDState = LOW;
  Serial.println("Starting de-contamination process.");
}

/*
   Function Name: stopDecontaminationProcess
   Description: Turns off the LEDs for decontamination process
   Parameters:
    LEDMOSFETPinNumber - Digital pin to which MOSFET is attached
   Returns: None
*/


void stopDecontaminationProcess(byte LEDMOSFETPinNumber)
{
  digitalWrite(LEDMOSFETPinNumber, LOW);
  analogWrite(TOP_LED_1_PIN_NUMBER, 0);
  analogWrite(TOP_LED_2_PIN_NUMBER, 0);
  analogWrite(BOTTOM_LED_1_PIN_NUMBER, 0);
  analogWrite(BOTTOM_LED_2_PIN_NUMBER, 0);
  lcd.clear();
  //Stopping LCD refresh
  LCDRefreshTimer.stop(UpdateLCDEvent);
  showCompanyAndTeamName();
  decontaminationInProcess = false;
  digitalWrite(COMPLETED_LED_PIN_NUMBER, LOW);
  Serial.println("Stopping de-contamination process.");
}

/*
   Function Name: showRemainingTime
   Description: Displays time remaining in seconds for the decontamination process to finish
   Parameters: None.
   Returns: None
*/


void showRemainingTime(void)
{
  int timeRemainingInSeconds = decontaminationStartTime +  DECONTAMINATION_TIME_IN_SECONDS - timeInSecondsFromStartup;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("De contaminating");
  lcd.setCursor(0, 1);
  lcd.print("Time Left: ");
  lcd.print(timeRemainingInSeconds);

  //Toggle indicator LED
  LEDState = !LEDState;
  digitalWrite(COMPLETED_LED_PIN_NUMBER , LEDState);
}
