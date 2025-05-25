#include <WiFi.h>
#include "SCMD.h"
#include "Wire.h"
#include <ESP32Servo.h>
#include <ezButton.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>    
#include <Adafruit_ST7789.h> 
#include <SPI.h>  

const char* ssid = "Tejas";         // Replace with your Wi-Fi SSID
const char* password = "8379936465"; // Replace with your Wi-Fi password

// Motor driver object
SCMD myMotorDriver;
Servo myServo;

//define all pins here
const int servoPin = A0;
const int limitSwitchX1Pin = 4;  // Limit switch for X axis
const int limitSwitchY1Pin = 6;  // Limit switch for Y axis
const int limitSwitchX2Pin = 12;  // Limit switch for X axis
const int limitSwitchY2Pin = 13;  // Limit switch for Y axis
#define TFT_CS    10   // ST7789 TFT module connections for ESP32-S2
#define TFT_DC    11  // ST7789 TFT module connections for ESP32-S2
#define TFT_RST    8  // ST7789 TFT module connections for ESP32-S2
const int emergencyButtonPin = A2; // Emergency stop pin

// Motor speeds
float speed1 = 255;
float speed2 = 255;
float speed3 = 200;
float speed4 = 160;

#define DOWN_MOTOR 0
#define UP_MOTOR 1

const long interval = 50; // Interval for motor control (milliseconds)
unsigned long previousMillis = 0; // Store the last time movement was updated
unsigned long currentInterval = interval; // New variable for interval control
int currentMovementStep = 0; // Track the current movement step

bool isDrawing = false; // Indicates if the pen plotter is currently drawing
int currentLine = 0; // Keeps track of the current line being drawn

// Hall effect sensor
const int hallEffectSensorPin = 3;  // Pin where the Hall Effect sensor is connected
int sensorState = 0;                // Variable to store the state of the Hall Effect sensor

// Flag to track if homing has been done
bool homingDone = false;

// Flag to track Emergency stop activated
bool emergencyStopActivated = false;

// Assuming you have already initialized the emergency stop button
ezButton emergencyStopButton(emergencyButtonPin); // Pin where the emergency stop button is connected

// Initialize Adafruit ST7789 TFT library
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Create limit switch objects using ezButton
ezButton limitSwitchX1(limitSwitchX1Pin);
ezButton limitSwitchY1(limitSwitchY1Pin);
ezButton limitSwitchX2(limitSwitchX2Pin);
ezButton limitSwitchY2(limitSwitchY2Pin);

// Create a web server
WebServer server(80);

// Function to stop motors
void stop_motors() {
    myMotorDriver.setDrive(UP_MOTOR, 0, 0);
    myMotorDriver.setDrive(DOWN_MOTOR, 0, 0);
    Serial.println("Motors stopped.");
}

// Function to check the emergency button state
void check_emergency_stop() {
    // If the emergency stop button is pressed
    emergencyStopButton.loop();
    
    if (digitalRead(emergencyButtonPin) == LOW) {  // Assuming LOW means the button is pressed
        Serial.println("Emergency stop button pressed! Stopping all motors.");
        stop_motors();  // Stop motors immediately
        isDrawing = false;  // Stop drawing
        penUp();  // Raise the pen to avoid damage

        // Stay in this loop until the emergency stop button is released
        while (digitalRead(emergencyButtonPin) == LOW) {
            // You can blink an LED or update the display with "EMERGENCY STOP"
            delay(100);  // Small delay to prevent overwhelming the processor
        }

        // Emergency stop button released
        Serial.println("Emergency stop button released.");
        // Optionally reset the system state here, or wait for user input to resume
        emergencyStopActivated = false;  // Clear the emergency stop flag
    }
}


// Function to check limit switches
void check_limit_switches() {
    // Update the state of the buttons
    limitSwitchX1.loop();
    limitSwitchY1.loop();
    limitSwitchX2.loop();
    limitSwitchY2.loop();

    // Check if any limit switch is pressed
    if (limitSwitchX1.getState() == HIGH || limitSwitchY1.getState() == HIGH || limitSwitchX2.getState() == HIGH || limitSwitchY2.getState() == HIGH) {
        stop_motors();
        Serial.println("Limit switch pressed! Stopping motors.");
        //while (true);  // Stop further execution
    }
}

// Function to move motor
void moveMotor(int motor, int direction, int speed, int duration) {
    myMotorDriver.setDrive(motor, direction, speed);
    delay(duration); // Block for the duration of the movement
    stop_motors(); // Stop the motors after moving
}

// Function to move pen up and down
void penUp() {
    if (digitalRead(emergencyButtonPin) == LOW) {
        //Stop motors and pause the drawing
      stop_motors();
      return; 
    }
    myServo.write(20);  // Adjust this value if needed for the "pen up" position
    Serial.println("Pen up (Servo at 20 degrees)");
    delay(100);  // Small delay to allow the servo to move
}

void penDown() {
    if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return; 
    }
    myServo.write(0);  // Adjust this value if needed for the "pen down" position
    Serial.println("Pen down (Servo at 0 degrees)");
    delay(100);  // Small delay to allow the servo to move
    
}

// Function to draw the shape of Nicolas' house with diagonals in a specific order
void nikolasHaus(int x, int y, int size) {
    // 1. Draw left line
    for (int i = 0; i < size; i++) {
        tft.drawPixel(x, y + size - 1 - i, ST77XX_BLUE); // Left line
        delay(30); // Delay for effect
    }

    // 2. Draw top line
    for (int i = 0; i < size; i++) {
        tft.drawPixel(x + i, y, ST77XX_BLUE); // Top line
        delay(30); // Delay for effect
    }

    // 3. Draw right line
    for (int i = 0; i < size; i++) {
        tft.drawPixel(x + size - 1, y + i, ST77XX_BLUE); // Right line
        delay(30); // Delay for effect
    }

    // 4. Draw bottom line
    for (int i = 0; i < size; i++) {
        tft.drawPixel(x + size - 1 - i, y + size - 1, ST77XX_BLUE); // Bottom line
        delay(30); // Delay for effect
    }

    // 5. Draw diagonal from bottom-left to top-right
    for (int i = 0; i < size; i++) {
        tft.drawPixel(x + i, y + size - 1 - i, ST77XX_BLUE); // Diagonal
        delay(30); // Delay for effect
    }

    // Calculate the apex position of the triangle
    int apexX = x + size / 2;  // Center of the base
    int apexY = y - size / 2;  // Height above the square's top edge

    // 6. Draw right side of the triangle (reversed: from right base corner to apex)
    for (int i = size / 2; i >= 0; i--) {
        tft.drawPixel(apexX + i, apexY + i, ST77XX_BLUE); // Right side (reversed)
        delay(30); // Delay for effect
    }

    // 7. Draw left side of the triangle (from apex to left base corner)
    for (int i = 0; i <= size / 2; i++) {
        tft.drawPixel(apexX - i, apexY + i, ST77XX_BLUE); // Left side
        delay(30); // Delay for effect
    }

    // 8. Draw diagonal from top-left to bottom-right
    for (int i = 0; i < size; i++) {
        tft.drawPixel(x + i, y + i, ST77XX_BLUE); // Diagonal
        delay(30); // Delay for effect
    }
}

// Function to show "Preview created" and "Successfully" message above the shape
void showSuccessMessageAboveShape(int squareX, int squareY, int squareSize) {
    tft.setTextColor(ST77XX_BLUE);
    tft.setTextSize(2); // Set text size to 2 for larger text

    // Position the success messages above the shape
    String successText1 = "Preview created";
    String successText2 = "Successfully";
    
    // Calculate X positions for centered text above the shape
    int16_t successX1 = (tft.width() - (6 * successText1.length() * 2)) / 2; // Using size 2 for centering
    int16_t successX2 = (tft.width() - (6 * successText2.length() * 2)) / 2; // Using size 2 for centering
    
    // Increase the vertical offset to move the messages higher above the shape
    int16_t successY1 = squareY - 105;  // Positioning first line much higher above the square
    int16_t successY2 = successY1 + 20; // Positioning the second line just below the first one

    // Display the two lines of success messages
    tft.setCursor(successX1, successY1); // Centered horizontally, above the shape
    tft.println(successText1);
    tft.setCursor(successX2, successY2); // Centered horizontally, below the first message
    tft.println(successText2);
}

// Function to initialize the display with Hochschule, Schmalkalden, Team 20, creators' names, and guidance information
void displayInitialText() {
    tft.fillScreen(ST77XX_BLACK); // Fill the screen with black color
    tft.setTextColor(ST77XX_BLUE);

    // Display the first screen with "Hochschule" and "Schmalkalden"
    tft.setTextSize(3);
    String text1 = "Hochschule";
    String text2 = "Schmalkalden";
    tft.setCursor((tft.width() - (6 * text1.length() * 3)) / 2, 100);
    tft.println(text1);
    tft.setCursor((tft.width() - (6 * text2.length() * 3)) / 2, 150);
    tft.println(text2);
    delay(3000);

    // Display the second screen with "Team 20"
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(5);
    String text3 = "Team 20";
    tft.setCursor((tft.width() - (6 * text3.length() * 5)) / 2, 80);
    tft.println(text3);

    // Now display the names of the creators below "Team 20"
    tft.setTextSize(2); // Set the text size smaller for names

    // Display "Created by"
    String createdBy = "Created by";
    tft.setCursor((tft.width() - (6 * createdBy.length() * 2)) / 2, 150); // Positioned below "Team 20"
    tft.println(createdBy);

    // Display names one below another
    String name1 = "Santosh K";
    String name2 = "Onkar N";
    String name3 = "Anand K";
    String name4 = "Tejas S";

    tft.setCursor((tft.width() - (6 * name1.length() * 2)) / 2, 180); // Centered at y = 200
    tft.println(name1);
    tft.setCursor((tft.width() - (6 * name2.length() * 2)) / 2, 200); // Centered at y = 220
    tft.println(name2);
    tft.setCursor((tft.width() - (6 * name3.length() * 2)) / 2, 220); // Centered at y = 240
    tft.println(name3);
    tft.setCursor((tft.width() - (6 * name4.length() * 2)) / 2, 240); // Centered at y = 260
    tft.println(name4);

    delay(3000); // Keep the text displayed for 3 seconds

    // Create a new screen for guidance information
    tft.fillScreen(ST77XX_BLACK); // Clear the screen for the next display

    // Set a larger text size for "Mentored by"
    tft.setTextSize(3); // Set text size to 3 for "Mentored by"
    String name5 = "Mentored by";
    tft.setCursor(0, 100); // Left-aligned at x = 0, y = 100
    tft.println(name5);

    // Set the text size back to 2 for the rest of the names
    tft.setTextSize(2); // Set text size to 2 for the mentors' names
    String name6 = "Prof.Dr-Ing.Stefan R";
    String name7 = "Prof.Dr-Ing.Silvio B";
    String name8 = "Mr.Joshua Voll";

    // Left-align the mentor names
    tft.setCursor(0, 140); // Left-aligned at x = 0, y = 140 (adjusted y for spacing)
    tft.println(name6);
    tft.setCursor(0, 160); // Left-aligned at x = 0, y = 160 (adjusted y for spacing)
    tft.println(name7);
    tft.setCursor(0, 180); // Left-aligned at x = 0, y = 180 (adjusted y for spacing)
    tft.println(name8);

    delay(3000); // Keep the text displayed for 3 seconds

    // New screen for "X - Y" and "Pen-Plotter"
    tft.fillScreen(ST77XX_BLACK); // Clear the screen for the next display

    // Display "X - Y"
    tft.setTextSize(4); // Set the text size to 4 for "X - Y"
    String xyText = "X - Y";
    tft.setCursor((tft.width() - (6 * xyText.length() * 4)) / 2, 100); // Centered at y = 100
    tft.println(xyText);

    // Display "Pen-Plotter"
    tft.setTextSize(2);
    String penPlotterText = "Pen-Plotter";
    tft.setCursor((tft.width() - (6 * penPlotterText.length() * 2)) / 2, 160); // Centered at y = 160
    tft.println(penPlotterText);

    delay(3000); // Keep the text displayed for 3 seconds

    // New screen for "Press Start to" and "Initialize"
    tft.fillScreen(ST77XX_BLACK); // Clear the screen for the next display
}

// Function to initialize the display (clear, display initial text, draw Nikolas' house)
void initializeDisplay() {
    tft.init(240, 320); // Initialize display with 240x240 resolution
    tft.setRotation(2); // Set display orientation (rotate 90 degrees)
    tft.fillScreen(ST77XX_BLACK); // Clear the screen with black color

    displayInitialText(); // Show all the initial text slides
}

// Function to initialize the display (clear, display initial text, draw Nikolas' house)
void preview() {

  tft.fillScreen(ST77XX_BLACK); // Clear the screen with black color
    // Define size and position of the shape
    int squareSize = 120;
    int squareX = (tft.width() - squareSize) / 2; // Center horizontally
    int squareY = (tft.height() - squareSize) / 2 + 40; // Center vertically

    nikolasHaus(squareX, squareY, squareSize); // Draw Nikolas' house

    showSuccessMessageAboveShape(squareX, squareY, squareSize); // Show success message
}

// Drawing functions for lines
void Line_1(int direction, int speed) {
    check_limit_switches();
    if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
      stop_motors();
      return;  
    }
    myMotorDriver.setDrive(UP_MOTOR, direction, speed);
    for (int i = 0; i < 481; i++) {
        check_limit_switches();
        delay(1);
    }
    stop_motors();
}

void Line_2(int direction, int speed) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return;
  }  
  myMotorDriver.setDrive(DOWN_MOTOR, direction, speed);

  for (int i = 0; i < 330; i++) {  // Move for 325 ms
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void Line_3(int direction, int speed) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return;
  }  
  myMotorDriver.setDrive(UP_MOTOR, direction, speed);

  for (int i = 0; i < 481; i++) {
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void Line_4(int direction, int speed) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return;
  } 
  myMotorDriver.setDrive(DOWN_MOTOR, direction, speed);

  for (int i = 0; i < 330; i++) {
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void Line_5(int direction_up, int direction_down, int speed3, int speed4) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return; 
  }
  myMotorDriver.setDrive(UP_MOTOR, direction_up, speed3);
  myMotorDriver.setDrive(DOWN_MOTOR, direction_down, speed4);

  for (int i = 0; i < 675; i++) {
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void Line_6(int direction_up, int direction_down, int speed3, int speed4) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return;
  }
  myMotorDriver.setDrive(UP_MOTOR, direction_up, speed3);
  myMotorDriver.setDrive(DOWN_MOTOR, direction_down, speed4);

  for (int i = 0; i < 337; i++) {
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void Line_7(int direction_up, int direction_down, int speed3, int speed4) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return;
  } 
  myMotorDriver.setDrive(UP_MOTOR, direction_up, speed3);
  myMotorDriver.setDrive(DOWN_MOTOR, direction_down, speed4);

  for (int i = 0; i < 337; i++) {
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void Line_8(int direction_up, int direction_down, int speed3, int speed4) {
  check_limit_switches();
  if (digitalRead(emergencyButtonPin) == LOW) {
      //Stop motors and pause the drawing
    stop_motors();
    return;
  }
  myMotorDriver.setDrive(UP_MOTOR, direction_up, speed3);
  myMotorDriver.setDrive(DOWN_MOTOR, direction_down, speed4);

  for (int i = 0; i < 675; i++) {
    check_limit_switches();
    delay(1);
  }

  stop_motors();
}

void homingFunction() {
  // Step 1: Down motor moves clockwise until X1 or X2 limit switch is pressed
  Serial.println("Starting homing sequence...");

  myMotorDriver.setDrive(DOWN_MOTOR, 1, speed2);  // Move down motor in direction 1
  while (true) {
    limitSwitchX1.loop();
    limitSwitchX2.loop();
    
    // Check for emergency stop during the homing sequence
    if (digitalRead(emergencyButtonPin) == LOW) {  // Emergency button is pressed
      check_emergency_stop();  // Call the emergency stop function
      return;  // Exit the homing function early
    }

    // If any X-axis limit switch is pressed, stop down motor
    if (limitSwitchX1.getState() == HIGH || limitSwitchX2.getState() == HIGH) {
      stop_motors();
      Serial.println("X-axis limit switch pressed, stopping down motor.");
      break;
    }
  }

  // Step 2: Up motor moves counter-clockwise until Y1 or Y2 limit switch is pressed
  myMotorDriver.setDrive(UP_MOTOR, 1, speed2);  // Move up motor in direction 1
  while (true) {
    limitSwitchY1.loop();
    limitSwitchY2.loop();
    
    // Check for emergency stop
    if (digitalRead(emergencyButtonPin) == LOW) {
      check_emergency_stop();
      return;
    }

    // If any Y-axis limit switch is pressed, stop up motor
    if (limitSwitchY1.getState() == HIGH || limitSwitchY2.getState() == HIGH) {
      stop_motors();
      Serial.println("Y-axis limit switch pressed, stopping up motor.");
      break;
    }
  }

  // Step 3: Down motor moves counter-clockwise until Hall effect sensor is triggered
  myMotorDriver.setDrive(DOWN_MOTOR, 0, 150);  // Move down motor in direction 0 with speed 100
  while (true) {
    // Read Hall effect sensor state
    sensorState = digitalRead(hallEffectSensorPin);

    // Check for emergency stop
    if (digitalRead(emergencyButtonPin) == LOW) {
      check_emergency_stop();
      return;
    }

    if (sensorState == HIGH) {  // Magnetic field detected
      stop_motors();
      Serial.println("Hall effect sensor triggered, stopping down motor.");
      break;
    }
  }

  // Step 4: Up motor moves clockwise for 500ms at speed 255
  myMotorDriver.setDrive(UP_MOTOR, 0, 255);  // Move up motor in direction 0 with speed 255
  unsigned long startTime = millis();
  while (millis() - startTime < 500) {
    // Check for emergency stop during the delay period
    if (digitalRead(emergencyButtonPin) == LOW) {
      check_emergency_stop();
      return;
    }
  }
  stop_motors();
  Serial.println("Homing sequence complete.");

  homingDone = true;  // Mark homing as done
}

const char* html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>2D Pen Plotter Control</title>
    <style>
        body {
            font-family: 'Arial', sans-serif;
            text-align: center;
            background-color: #f7f9fc;
            margin: 0;
            padding: 0;
            background-image: url('https://www.hs-schmalkalden.de/fileadmin/processed/1/1/csm_P3150356_7df3c7b570.jpg');
            background-size: cover;
            background-position: center;
        }
        h1 {
            color: #333;
            margin-bottom: 5px;
        }
        h2 {
            color: #333;
            margin-bottom: 20px;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            padding: 20px;
            background: rgba(255, 255, 255, 0.8); /* White background with transparency */
            border-radius: 10px; /* Rounded corners for the container */
            position: relative; /* To position the logo */
        }
        .button-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center; /* Center the buttons */
        }
        button {
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 15px 30px;
            font-size: 16px;
            margin: 10px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            transition: all 0.3s ease;
            width: 120px;
            height: 100px;
            flex-direction: column;
        }
        button:hover {
            background-color: #45a049;
            transform: translateY(-3px);
            box-shadow: 0 6px 12px rgba(0, 0, 0, 0.2);
        }
        button i {
            font-size: 24px;
            margin-bottom: 8px;
        }
        button span {
            font-size: 16px;
        }
        .status {
            margin-top: 20px;
            font-size: 18px;
            color: #666;
        }
        .logo {
            position: absolute;
            top: 20px;
            left: 20px;
            width: 250px; /* Increased size */
            height: auto; /* Maintain aspect ratio */
        }
        .emergency-button {
            background-color: red; /* Red background for emergency button */
            flex-basis: 100%; /* Full width for emergency button */
            margin-top: 20px; /* Margin to separate from other buttons */
        }
        .emergency-button:hover {
            background-color: darkred; /* Darker shade on hover */
        }
    </style>
    <!-- Add FontAwesome for icons -->
    <script src="https://kit.fontawesome.com/a076d05399.js" crossorigin="anonymous"></script>
</head>
<body>
    <img class="logo" src="https://www.hs-schmalkalden.de/typo3conf/ext/hsm_sitepackage/Resources/Public/Images/logo.jpg" alt="Hochschule Schmalkalden Logo">
    <div class="container">
        <h1>2D Pen Plotter Control</h1>
        <h2>Team 20</h2>
        <div class="button-container">
            <button onclick="sendCommand('START')">
                <i class="fas fa-play-circle"></i>
                <span>START DRAWING</span>
            </button>
            <button onclick="sendCommand('STOP')">
                <i class="fas fa-stop-circle"></i>
                <span>STOP DRAWING</span>
            </button>
            <button onclick="sendCommand('PENDOWN')">
                <i class="fas fa-pencil-alt"></i>
                <span>PEN DOWN</span>
            </button>
            <button onclick="sendCommand('PENUP')">
                <i class="fas fa-pen-nib"></i>
                <span>PEN UP</span>
            </button>
            <button onclick="sendCommand('RESET')">
                <i class="fas fa-redo"></i>
                <span>RESET</span>
            </button>
            <button onclick="sendCommand('HOMING')">
                <i class="fas fa-home"></i>
                <span>HOMING</span>
            </button>
            <button onclick="sendCommand('INTRO')">
                <i class="fas fa-info-circle"></i>
                <span>INTRO</span>
            </button>
            <button onclick="sendCommand('MOTOR_PREVIEW')">
                <i class="fas fa-eye"></i>
                <span>MOTOR PREVIEW</span>
            </button>
            <button class="emergency-button" onclick="sendCommand('EMERGENCY_STOP')">
                <i class="fas fa-exclamation-triangle"></i>
                <span>EMERGENCY STOP!!!!</span>
            </button>
        </div>
        <div class="status" id="status">Status: Waiting for command...</div>
    </div>

    <script>
        function sendCommand(command) {
            fetch(/command?cmd=${command})
                .then(response => response.text())
                .then(data => {
                    console.log(data);
                    document.getElementById('status').innerText = Status: ${command} executed;
                })
                .catch(error => {
                    console.error('Error:', error);
                    document.getElementById('status').innerText = Error: ${error.message};
                });
        }
    </script>
</body>
</html>
)rawliteral";



// Function to handle incoming commands from the web HMI
void handleCommand() {
    if (server.hasArg("cmd")) {
        String cmd = server.arg("cmd");
        Serial.println("Command received: " + cmd);
        if (cmd == "START") {
          isDrawing = true;  // Start drawing
          currentMovementStep = 0;
        } else if (cmd == "STOP") {
            isDrawing = false;  // Stop drawing
            stop_motors();  // Ensure motors are stopped
        } else if (cmd == "PENUP") {
            penUp(); // Put the pen up
            Serial.println("Pen is up.");
        } else if (cmd == "PENDOWN") {
            penDown(); // Put the pen down
            Serial.println("Pen is down.");
        } else if (cmd == "EMERGENCY_STOP") {
            check_emergency_stop();  // Immediately stop the motors
            isDrawing = false;  // Stop any ongoing drawing operation
            penUp(); // Optionally raise the pen to avoid damage
            Serial.println("EMERGENCY BUTTON PRESSED. Motors stopped and pen raised.");
        } else if (cmd == "INTRO") {
            initializeDisplay(); // Optionally raise the pen to avoid damage
            Serial.println("INTRO");
        } else if (cmd == "MOTOR_PREVIEW") {
            preview(); // Optionally raise the pen to avoid damage
            Serial.println("MOTOR_PREVIEW");
        } else if (cmd == "RESET") {
            Serial.println("Reset command received. Restarting ESP32...");
            server.send(200, "text/plain", "Resetting ESP32...");
            delay(100); // Optional delay before restart
            ESP.restart(); // Reset the ESP32
        } else if (cmd == "HOMING") {
            homingFunction(); // Call the homing function
        }
        server.send(200, "text/plain", "Command executed: " + cmd);
    } else {
        server.send(400, "text/plain", "No command provided");
    }
}


void setup() {
    Serial.begin(115200);
    myMotorDriver.settings.commInterface = I2C_MODE;
    myMotorDriver.settings.I2CAddress = 0x5D; // Address of the motor driver

    WiFi.begin(ssid, password); // Connect to Wi-Fi
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000); // Wait for 1 second
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi!");
    Serial.println(WiFi.localIP()); // Print the ESP32's IP address

    // Initialize motor driver
    while (myMotorDriver.begin() != 0xA9) {
        Serial.println("Motor driver initialization failed. Retrying...");
        delay(1000);
    }
    Serial.println("Motor driver initialized successfully.");
    while (myMotorDriver.ready() == false);
    Serial.println("Motors ready.");
    myMotorDriver.enable();

    myServo.attach(servoPin); // Attach the servo to GPIO 36
    penUp(); // Start with the pen up

    // Set up limit switch pins
    pinMode(limitSwitchX1Pin, INPUT_PULLUP);
    pinMode(limitSwitchY1Pin, INPUT_PULLUP);
    pinMode(limitSwitchX2Pin, INPUT_PULLUP);
    pinMode(limitSwitchY2Pin, INPUT_PULLUP);

    pinMode(hallEffectSensorPin, INPUT);

       // Set up emergency button pin
    pinMode(emergencyButtonPin, INPUT_PULLUP);  // Use internal pull-up resistor

    // Set up web server routes
    server.on("/", []() {
        server.send(200, "text/html", html);  // Serve the HTML content
    });

    server.on("/command", handleCommand); // Handle incoming commands
    server.begin(); // Start the server
}

void loop() {
    // Handle web server client requests
    server.handleClient(); 
    check_limit_switches(); // Continuously check limit switches
    check_emergency_stop(); // Continuously check emergency switches
    //if (digitalRead(emergencyButtonPin) == LOW) {
        //Stop motors and pause the drawing
        //stop_motors();
        //isDrawing = false;
       // return;
    //}
    // Execute movement logic based on the drawing status
    if (isDrawing) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= currentInterval) {
            previousMillis = currentMillis; // Save the last update time

            switch (currentMovementStep) {
                case 0:
                    penDown(); // Put the pen down
                    delay(1000); // Small delay for pen positioning
                    currentMovementStep++;
                    break;
                case 1:
                    Serial.println("Moving X-axis backward");
                    Line_1(0, speed2); // Move X-axis backward
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 2:
                    Serial.println("Moving X-axis forward");
                    Line_2(1, speed2); // Move X-axis forward
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 3:
                    Serial.println("Moving X-axis backward");
                    Line_3(1, speed2); // Move X-axis backward
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 4:
                    Serial.println("Moving X-axis forward");
                    Line_4(0, speed1); // Move X-axis forward
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 5:
                    Serial.println("Moving Diagonal 1");
                    Line_5(0, 1, speed3, speed4); // Move diagonally
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 6:
                    Serial.println("Moving Diagonal 2");
                    Line_6(0, 0, speed3, speed4); // Move diagonally
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 7:
                    Serial.println("Moving Diagonal 3");
                    Line_7(1, 0, speed3, speed4); // Move diagonally
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 8:
                    Serial.println("Moving Diagonal 4");
                    Line_8(1, 1, speed3, speed4); // Move diagonally
                    delay(2000); // 2-second delay after drawing the line
                    currentMovementStep++;
                    break;
                case 9:
                    penUp(); // Raise the pen after finishing
                    Serial.println("Pen up after drawing.");
                    delay(2000); // 2-second delay after drawing is complete
                    currentMovementStep = 0; // Reset to the beginning for the next drawing
                    ESP.restart(); // Reset the ESP32
                    break;
            }
        }
    }
}