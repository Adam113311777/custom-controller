#include <WiFi.h>              // wifi connection to robot
#include <WiFiMulti.h>         // wifi connection to robot
#include <WiFiClientSecure.h>  // wifi connection to robot
#include <Arduino.h>
#include <WebSocketsClient.h>   // wifi connection to robot
#include "soc/soc.h"            //disable brownout problems
#include "soc/rtc_cntl_reg.h"   //disable brownout problems
#include <Adafruit_GFX.h>       // display
#include <Adafruit_SSD1306.h>   // display
#include <Adafruit_MAX1704X.h>  // battery fuel gauge
#include <Adafruit_BNO08x.h>    // IMU

#define SDA_PIN 21
#define SCL_PIN 22

#define RJX_PIN 36
#define RJY_PIN 39
#define RJZ_PIN 33
#define LJX_PIN 35
#define LJY_PIN 34
#define LJZ_PIN 32

#define VMOTOR1 5
#define VMOTOR2 15
#define VMOTOR3 4
#define VMOTOR4 23

// #define TOUCH_PIN_1 14
// #define TOUCH_PIN_2 12
// #define TOUCH_PIN_3 13

#define BUTTON_1 12
#define BUTTON_2 14
#define BUTTON_3 13
#define BUTTON_4 16

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SSD1306_I2C_ADDRESS 0x3C

// 9Dof
#define BNO08X_RESET -1
struct euler_t {
  float yaw;
  float pitch;
  float roll;
} ypr, yprStart, yprRequired;
int last5yprYaws[5];
unsigned long last5yprYawsTime[5];
int yawCorrection = 0;
Adafruit_BNO08x bno08x(BNO08X_RESET);
sh2_SensorValue_t sensorValue;
sh2_SensorId_t reportType = SH2_ARVR_STABILIZED_RV;
long reportIntervalUs = 50000;
bool run9Dof = false;
bool is9DofConnected = false;
TaskHandle_t taskCompassCheck;
// 9Dof

// Create the display object (width 128, height 64)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
bool isDisplayConnected = true;

struct JoysticksState {
  int Rx;
  int Ry;
  int Rz;
  int Lx;
  int Ly;
  int Lz;
};

struct JoysticksAxisEnabled {
  bool Rx;
  bool Ry;
  bool Rz;
  bool Lx;
  bool Ly;
  bool Lz;
};

struct ButtonsState {
  bool A;
  bool B;
  bool X;
  bool Y;
};


Adafruit_MAX17048 max17048;

ButtonsState myButtonsState = { false, false, false, false };

JoysticksState myJoysticksState;
JoysticksState myJoysticksLastState;
JoysticksAxisEnabled myJoysticksAxisEnabled;

int roviGear = 1;

const char* ssid = "rovi_1";
const char* password = "123456789";
bool connectedToWiFiRovi = false;

TaskHandle_t everySecondTask;
TaskHandle_t everyFortyMillisTask;
TaskHandle_t everyTenSecondsTask;
TaskHandle_t vibrationMotorsTask;

struct vibrationMotorsTaskParameters {
  bool vMotor1 = false;
  bool vMotor2 = false;
  bool vMotor3 = false;
  bool vMotor4 = false;
  int duration = 1000;
};

unsigned long lastAcknowledgement = 0;
bool isConnectedToRovi = false;
bool hasConnectedToRoviSinceAppStart = false;
bool isConnectedToWebSocket = false;

WebSocketsClient webSocket;

bool test = false;

int batteryPercentage = -400;
int roviBatteryPercentage = 0;
int lastRoviBatteryPercentage = 0;

// game stuff
unsigned long gameOnOffLast = 0;
TaskHandle_t gameTask;
bool gameMode = false;
bool gameModeSelection = false;
int gameSelection = 0;
int dinoY = SCREEN_HEIGHT - 20;  // Dino's initial vertical position
int dinoJumpHeight = 13;         // Jump height
int dinoWidth = 10, dinoHeight = 10;
bool isJumping = false;
int jumpCounter = 0;

int obstacleX = SCREEN_WIDTH;  // Obstacle's starting horizontal position
int obstacleY = SCREEN_HEIGHT - 20;
int obstacleWidth = 10, obstacleHeight = 10;
int obstacleSpeed = random(2, 5);

int score = 0;



float ballX = SCREEN_WIDTH / 2;  // Initial ball position (center)
float ballY = SCREEN_HEIGHT / 2;
float lastBallX = ballX;
float lastBallY = ballY;
int ballRadius = 4;  // Ball size
// game stuff

// menu

const char* menuItems[] = { "Option 1", "Option 2", "Option 3", "Exit" };
bool menuStates[] = { false, false, false };  // Store toggle states for options
int menuSize = sizeof(menuItems) / sizeof(menuItems[0]);
int selectedMenuItem = 0;


void setup() {
  pinMode(VMOTOR1, OUTPUT);
  pinMode(VMOTOR2, OUTPUT);
  pinMode(VMOTOR3, OUTPUT);
  pinMode(VMOTOR4, OUTPUT);

  analogWrite(VMOTOR1, 0);
  analogWrite(VMOTOR2, 0);
  analogWrite(VMOTOR3, 0);
  analogWrite(VMOTOR4, 0);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  //disable brownout detector
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  //
  Serial.println("Hello");

  randomSeed(analogRead(0));  // Seed random generator with analog noise

  pinMode(RJX_PIN, INPUT);
  pinMode(RJY_PIN, INPUT);
  pinMode(RJZ_PIN, INPUT_PULLUP);
  pinMode(LJX_PIN, INPUT);
  pinMode(LJY_PIN, INPUT);
  pinMode(LJZ_PIN, INPUT_PULLUP);

  // pinMode(TOUCH_PIN_1, INPUT);
  // pinMode(TOUCH_PIN_2, INPUT);
  // pinMode(TOUCH_PIN_3, INPUT);

  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);

  myJoysticksAxisEnabled.Ry = true;  // enable which joysticks you want to use (just a formality)
  myJoysticksAxisEnabled.Ly = true;  // enable which joysticks you want to use (just a formality)

  // myJoysticksAxisEnabled.Rx = true; // enable which joysticks you want to use (just a formality)
  // myJoysticksAxisEnabled.Lx = true; // enable which joysticks you want to use (just a formality)

  // myJoysticksAxisEnabled.Rz = true; // enable which joysticks you want to use (just a formality)
  // myJoysticksAxisEnabled.Lz = true; // enable which joysticks you want to use (just a formality)

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  int tryNum = 0;

  // if (!max17048.begin(&Wire)) {
  //   Serial.println("MAX17048 not detected. Check connections.");
  //   while (1)
  //     ;
  // }

  Serial.println("AAAAAAAA");
  // Serial.println(display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS));
  while (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS) && tryNum < 10) {
    Serial.println("SH1107 not found. Check connections!");
    tryNum++;
    delay(500);
  }
  Serial.print("tryNum");
  Serial.println(tryNum);
  if (tryNum == 10) {
    Serial.println("Failed to connect to display");
    isDisplayConnected = false;
  }
  tryNum = 0;
  if (isDisplayConnected) {
    Serial.println("connected to display");
    // Clear the buffer
    display.clearDisplay();

    // Set text size and color
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setRotation(2);
    // display.print("Hello");
    display.display();
  }

  while (!max17048.begin() && tryNum < 10) {
    Serial.println("MAX17048 not detected. Check connections.");
    tryNum++;
    delay(500);
  }
  if (tryNum == 10) {
    Serial.println("Failed to connect to MAX17048");
  } else {
    Serial.println("MAX17048 connected");
  }
  tryNum = 0;
  // Initialize the display
  vibrationMotorsTaskParameters* taskParams = new vibrationMotorsTaskParameters;
  taskParams->vMotor1 = true;
  taskParams->vMotor2 = true;
  taskParams->vMotor3 = true;
  taskParams->vMotor4 = true;
  taskParams->duration = 300;

  xTaskCreatePinnedToCore(vibrationMotorsTaskMethod, "vibrationMotorsTask", 1024, taskParams, 1, &vibrationMotorsTask, 0);

  // WiFi.begin(ssid, password);
  // delay(500);
  // if (WiFi.status() != WL_CONNECTED) {
  //   int x = 93;  // Center X
  //   int y = 5;   // Center Y
  //   display.clearDisplay();
  //   // display.setCursor(0, 0);
  //   // display.print("not connected to robot");       // Print the current value
  //   // display.display();

  //   // Outer arc (top line)
  //   display.drawLine(x - 4, y - 4, x + 4, y - 4, SSD1306_WHITE);  // Short horizontal line
  //   // Middle arc (middle line)
  //   display.drawLine(x - 3, y - 2, x + 3, y - 2, SSD1306_WHITE);  // Shorter horizontal line
  //   // Inner arc (bottom line)
  //   display.drawLine(x - 2, y, x + 2, y, SSD1306_WHITE);  // Shortest horizontal line
  //   // Dot (signal source)
  //   display.drawPixel(x, y + 2, SSD1306_WHITE);  // Single pixel for the dot
  //   display.drawLine(x - 3, y + 3, x + 5, y - 5, SSD1306_WHITE);
  //   display.display();  // Update display
  // } else {
  //   connectedToRoviProcedure();
  // }


  attachInterrupt(digitalPinToInterrupt(BUTTON_1), BUTTON_1Change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2), BUTTON_2Change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_3), BUTTON_3Change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_4), BUTTON_4Change, CHANGE);

  int compassStartI;
  for (compassStartI = 0; !bno08x.begin_I2C(); compassStartI++) {
    if (compassStartI > 10) {
      break;
    }
    Serial.println("bno085 does not respond");
    delay(500);
  }
  if (compassStartI <= 10) {  //  && zed_f9pReady != 0
    Serial.println("BNO08x Found!");
    setReports(reportType, reportIntervalUs);
    is9DofConnected = true;
    run9Dof = true;
    // xTaskCreatePinnedToCore(compassCheck, "taskCompassCheck", 8192, NULL, 1, &taskCompassCheck, 0);
  }
  xTaskCreatePinnedToCore(everyTenSecondsLoop, "everyTenSecondsTask", 4096, NULL, 1, &everyTenSecondsTask, 0);
  xTaskCreatePinnedToCore(everySecondLoop, "everySecondTask", 4096, NULL, 1, &everySecondTask, 0);
  xTaskCreatePinnedToCore(everyFortyMillisLoop, "everyFortyMillisLoop", 4096, NULL, 1, &everyFortyMillisTask, 0);
}

void BUTTON_1Change() {
  myButtonsState.A = !digitalRead(BUTTON_1);
}

void BUTTON_2Change() {
  myButtonsState.B = !digitalRead(BUTTON_2);
}

void BUTTON_3Change() {
  myButtonsState.X = !digitalRead(BUTTON_3);
}

void BUTTON_4Change() {
  myButtonsState.Y = !digitalRead(BUTTON_4);
}

void dinoGameLoop(void* parameter) {
  for (;;) {
    // display.fillRect(0, 10, 128, 128, SH110X_BLACK);

    // display.fillRect(0, dinoY, dinoWidth, dinoHeight, SH110X_BLACK);
    // // Draw obstacle
    // display.fillRect(obstacleX, obstacleY, obstacleWidth, obstacleHeight, SH110X_BLACK);

    display.setTextColor(SSD1306_BLACK);
    display.setCursor(0, dinoY);
    // // display.fillRect(0, dinoY, dinoWidth, dinoHeight, SH110X_WHITE);
    display.print("D");

    // // Draw obstacle
    display.setCursor(obstacleX, obstacleY);
    display.print("#");

    // Handle jumping
    if (myButtonsState.A && !isJumping) {
      isJumping = true;
      jumpCounter = 0;
    }

    if (isJumping) {
      if (jumpCounter < dinoJumpHeight) {
        dinoY -= 2;  // Move up
      } else if (jumpCounter < 2 * dinoJumpHeight) {
        dinoY += 2;  // Move down
      } else {
        isJumping = false;
      }
      jumpCounter++;
    }
    // Serial.println(millis());
    // Move obstacle
    obstacleX -= random(2 - score / 15, 4 + score / 15);
    if (obstacleX < 0) {  //-obstacleWidth
      obstacleX = SCREEN_WIDTH - 6;
      score++;
      obstacleSpeed = random(2, 5 + score / 5);
    }

    // Collision detection
    if (obstacleX < dinoWidth && obstacleX + obstacleWidth > 0 && dinoY + dinoHeight > obstacleY) {
      dinoGameOver();
      // vTaskDelay(3500 / portTICK_PERIOD_MS);
    } else {
      // Draw dino
      // display.fillRect(0, dinoY, dinoWidth, dinoHeight, SH110X_WHITE);
      // Draw obstacle
      // display.fillRect(obstacleX, obstacleY, obstacleWidth, obstacleHeight, SH110X_WHITE);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, dinoY);
      // // display.fillRect(0, dinoY, dinoWidth, dinoHeight, SH110X_WHITE);
      display.print("D");

      // // Draw obstacle
      display.setCursor(obstacleX, obstacleY);
      display.print("#");


      // Draw score
      // display.fillRect(30, 10, 10, 10, SH110X_BLACK);  // Clear area for dynamic text
      // display.setCursor(30, 10);
      // display.print(score);

      long time = millis();
      display.display();
      // Serial.println(millis() - time);
    }
    // int timeDelay = 50;
    // if(score > 5){
    //   timeDelay = 40;
    // }
    // if(score > 10){
    //   timeDelay = 30;
    // }
    // if(score > 15){
    //   timeDelay = 20;
    // }
    // if(score > 25){
    //   timeDelay = 20;
    // }
    vTaskDelay(40 - score / 2 / portTICK_PERIOD_MS);
  }
}

void dinoGameOver() {
  display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);  // Clear area for dynamic text
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print("Game Over");
  display.setCursor(0, 20);
  display.print("Score: ");
  display.print(score);
  display.display();
  delay(3000);

  // Reset game
  isJumping = false;
  jumpCounter = 0;
  dinoY = SCREEN_HEIGHT - 20;
  obstacleX = SCREEN_HEIGHT;
  score = 0;
  display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);
  display.setTextSize(1);
  // display.setTextColor(SSD1306_WHITE);
  // display.setCursor(0, 10);
  // display.print("Score: ");
  display.display();
}

void everyFortyMillisLoop(void* parameter) {
  for (;;) {
    long time = millis();
    updateJoysticks();
    if (isConnectedToRovi || true) {
      sendJoystickData();
    }
    // myButtonsState.A  = !digitalRead(BUTTON_1);
    // myButtonsState.B = !digitalRead(BUTTON_2);
    // myButtonsState.X= !digitalRead(BUTTON_3);
    // myButtonsState.Y = !digitalRead(BUTTON_4);
    compassCheck();
    if (isDisplayConnected && !gameMode) {
      drawDefaultUI();
    }
    if (gameMode && gameSelection ==2) {
      gyroGameLoop();
    }
    if (myButtonsState.A) {
      if (!gameMode) {
        analogWrite(VMOTOR1, 5);
      } else {
        if (gameModeSelection && gameOnOffLast+2000 < millis()) {
          gameSelection = 1;
          gameModeSelection = false;
          display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);
          display.setTextSize(1);
          // display.setTextColor(SSD1306_WHITE);
          // display.setCursor(0, 10);
          // display.print("Score: ");
          display.display();
          xTaskCreatePinnedToCore(dinoGameLoop, "dinoGameLoop", 4096, NULL, 1, &gameTask, 0);
        }
      }
    } else {
      analogWrite(VMOTOR1, 0);
    }
    if (myButtonsState.B) {
      if (!gameMode) {
        analogWrite(VMOTOR2, 5);
      }
      if (gameModeSelection && gameOnOffLast+2000 < millis()) {
        display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);
        display.display();
        gameSelection = 2;
        gameModeSelection = false;
      }
    } else {
      analogWrite(VMOTOR2, 0);
    }
    if (myButtonsState.X) {
      if (!gameMode) {
        analogWrite(VMOTOR3, 5);
        roviGear++;
        if (roviGear > 3) {
          roviGear = 1;
        }
      }
      if (gameModeSelection && gameOnOffLast+2000 < millis()) {
        gameMode = false;
        gameModeSelection = false;
        display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);
        display.display();
      }
    } else {
      analogWrite(VMOTOR3, 0);
    }
    if (myButtonsState.Y) {
      if (!gameMode) {
        analogWrite(VMOTOR4, 5);
      }
    } else {
      analogWrite(VMOTOR4, 0);
    }
    if (myButtonsState.A && myButtonsState.B && myButtonsState.X && millis() - gameOnOffLast > 5000) {
      if (!gameMode) {
        Serial.println("starting game mode");
        gameMode = true;
        gameModeSelection = true;
        gameSelectionUI();
      } else {
        if (gameSelection == 1) {
          vTaskDelete(gameTask);
          dinoY = SCREEN_HEIGHT - 20;
          obstacleX = SCREEN_WIDTH;
          score = 0;
          jumpCounter = 0;
        }
        gameSelection = 0;
        gameMode = false;
        gameModeSelection = false;
        display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);  // Clear area for dynamic text
        display.display();
      }
      gameOnOffLast = millis();
    }
    // Serial.println(millis() - time);
    int delay = 40 - (millis() - time);
    if (delay > 5) {
      vTaskDelay(delay / portTICK_PERIOD_MS);
    }
  }
}

void drawDefaultUI() {
  display.fillRect(80, 10, SCREEN_WIDTH, 40, SSD1306_BLACK);  // Clear area for dynamic text
  display.setTextSize(1);
  display.setCursor(80, 13);
  display.print((int)ypr.yaw);
  display.setCursor(80, 23);
  display.print((int)ypr.pitch);
  display.setCursor(80, 33);
  display.print((int)ypr.roll);
  display.fillRect(0, 15, 80, 60, SSD1306_BLACK);  // Clear area for dynamic text

  display.setTextSize(3);                // Larger text for dynamic part
  display.setCursor(0, 15);              // Position for dynamic text
  display.println(myJoysticksState.Ly);  // Print the current value
  display.print(myJoysticksState.Ry);    // Print the current value
  // display.println(A);  // Print the current value
  // display.print(B);    // Print the current value

  display.display();  // Push to the display
}

void gameSelectionUI() {
  display.fillRect(0, 10, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);  // Clear area for dynamic text
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.print("Game Selection");
  display.setCursor(0, 25);
  display.print("1. Dino");
  display.setCursor(0, 35);
  display.print("2. Ball");
  display.setCursor(0, 45);
  display.print("3. Exit");
  display.display();
}

void setReports(sh2_SensorId_t reportType, long report_interval) {
  Serial.println("Setting desired reports");
  if (!bno08x.enableReport(reportType, report_interval)) {
    Serial.println("Could not enable stabilized remote vector");
  }
  // if (!bno08x.enableReport(SH2_PICKUP_DETECTOR, report_interval)) {
  //   Serial.println("Could not enable SH2_PICKUP_DETECTOR");
  // }
  // if (!bno08x.enableReport(SH2_LINEAR_ACCELERATION, report_interval)) {
  //   Serial.println("Could not enable SH2_PICKUP_DETECTOR");
  // }
}

void quaternionToEuler(float qr, float qi, float qj, float qk, euler_t* ypr, bool degrees = false) {

  float sqr = sq(qr);
  float sqi = sq(qi);
  float sqj = sq(qj);
  float sqk = sq(qk);

  ypr->yaw = atan2(2.0 * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
  ypr->pitch = asin(-2.0 * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
  ypr->roll = atan2(2.0 * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));

  if (degrees) {
    ypr->yaw *= RAD_TO_DEG;
    ypr->pitch *= RAD_TO_DEG;
    ypr->roll *= RAD_TO_DEG;
    // ypr->yaw += 180;                           // make it from -180 - 180 to 0 - 360
    // ypr->yaw = map(ypr->yaw, 0, 360, 360, 0);  // and reverse it
  }
}

void quaternionToEulerRV(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees = false) {
  quaternionToEuler(rotational_vector->real, rotational_vector->i, rotational_vector->j, rotational_vector->k, ypr, degrees);
}

void compassCheck() {
  if (bno08x.wasReset()) {
    Serial.print("sensor was reset ");
    setReports(reportType, reportIntervalUs);
  }

  if (bno08x.getSensorEvent(&sensorValue)) {
    switch (sensorValue.sensorId) {
      case SH2_ARVR_STABILIZED_RV:
        for (int i = 1; i < 5; i++) {
          last5yprYaws[i] = last5yprYaws[i - 1];
          last5yprYawsTime[i] = last5yprYawsTime[i - 1];
        }
        last5yprYaws[0] = (int)ypr.yaw;
        last5yprYawsTime[0] = millis();
        quaternionToEulerRV(&sensorValue.un.arvrStabilizedRV, &ypr, true);
        Serial.print("yaw: ");
        Serial.print(ypr.yaw);
        Serial.print(", pitch: ");
        Serial.print(ypr.pitch);
        Serial.print(", roll: ");
        Serial.println(ypr.roll);
        break;
      default:
        Serial.println("default");
        break;
    }
  }
  return;
}

void everySecondLoop(void* parameter) {
  for (;;) {
    if (isConnectedToWebSocket) {
      Serial.println("sending ackCC");
      webSocket.sendTXT("^ackCC");
    }
    if (isConnectedToRovi == true && hasConnectedToRoviSinceAppStart == true && lastAcknowledgement < millis() - 2000) {
      Serial.println("lost connection to Rovi - ack time out");
      isConnectedToRovi = false;
      connectedToWiFiRovi = false;
    }

    // int a = touchRead(TOUCH_PIN_1);
    // if (a < 20) {
    //   Serial.print("YES");
    // } else {
    //   Serial.print("NO");
    // }
    // Serial.println("touch");
    // Serial.println(a);

    // Serial.println("Scanning...");
    // byte count = 0;
    // for (byte address = 1; address < 127; address++) {
    //   Wire.beginTransmission(address);
    //   if (Wire.endTransmission() == 0) {
    //     Serial.print("Found I2C device at address 0x");
    //     Serial.println(address, HEX);
    //     count++;
    //   }
    // }
    // if (count == 0) {
    //   Serial.println("No I2C devices found.");
    // } else {
    //   Serial.print("Found ");
    //   Serial.print(count);
    //   Serial.println(" device(s).");
    // }

    // Get battery voltage
    // float voltage = max17048.cellVoltage();
    // Serial.print("Battery Voltage: ");
    // Serial.print(voltage);
    // Serial.println(" V");

    // Get state of charge (SOC)
    int soc = max17048.cellPercent();
    Serial.print("State of Charge: ");
    Serial.print(soc);
    Serial.println(" %");
    soc = map(soc, 0, 100, -8, 105);
    if (soc > 100) {
      soc = 100;
    }
    if (soc < 0) {
      soc = 0;
    }

    if (isDisplayConnected) {
      if (soc != batteryPercentage) {
        display.fillRect(103, 0, SCREEN_WIDTH, 10, SSD1306_BLACK);  // Clear area for dynamic text
        display.setTextSize(1);
        display.setCursor(103, 3);
        display.print(soc);
        display.print("%");
        display.display();
      }
    }
    batteryPercentage = soc;

    // analogWrite(VMOTOR1, 5);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR1, 0);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR2, 5);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR2, 0);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR3, 5);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR3, 0);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR4, 5);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // analogWrite(VMOTOR4, 0);
    // Serial.println(myJoysticksState.Ry);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void everyTenSecondsLoop(void* parameter) {
  for (;;) {
    Serial.println("start 10s loop");
    int delayTime = 10000;
    if (WiFi.status() != WL_CONNECTED) {
      int n = WiFi.scanNetworks();  // Scan for available networks
      for (int i = 0; i < n; i++) {
        // Print found networks
        Serial.printf("SSID: %s, RSSI: %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        // Check if the SSID matches
        if (WiFi.SSID(i) == ssid) {
          Serial.println("Matching SSID found. Connecting...");
        }
      }
      if (n == 0) {
        Serial.println("No wifi detected");
      }
      WiFi.begin(ssid, password);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      delayTime -= 1000;
      int tryNum = 0;
      // Wait for connection with timeout
      while (WiFi.status() != WL_CONNECTED && tryNum < 15) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
        tryNum++;
        delayTime -= 500;
      }
      if (tryNum < 15 && !connectedToWiFiRovi) {
        connectedToRoviProcedure();
        Serial.println("WiFi 1");
      } else {
        Serial.println("WiFi 0");
      }
      // Clean up after scanning
      WiFi.scanDelete();
      Serial.println("deleting scan");
    } else {
      if (!connectedToWiFiRovi) {
        connectedToRoviProcedure();
      }
      Serial.println("Already connected to Wi-Fi.");
    }
    Serial.println("delayStart");
    vTaskDelay(delayTime / portTICK_PERIOD_MS);
    Serial.println("delayEnd");
  }
}

void connectedToRoviProcedure() {
  Serial.println("connectedToRoviProcedure start");
  int x = 93;  // Center X
  int y = 6;   // Center Y

  connectedToWiFiRovi = true;
  // server address, port and URL
  webSocket.begin("192.168.4.1", 80, "/ws");

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);


  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.fillRect(x - 4, 0, y - 4 + 4, y + 4, SSD1306_BLACK);

  display.drawLine(x - 4, y - 4, x + 4, y - 4, SSD1306_WHITE);  // Short horizontal line
  // Middle arc (middle line)
  display.drawLine(x - 3, y - 2, x + 3, y - 2, SSD1306_WHITE);  // Shorter horizontal line
  // Inner arc (bottom line)
  display.drawLine(x - 2, y, x + 2, y, SSD1306_WHITE);  // Shortest horizontal line
  // Dot (signal source)
  display.drawPixel(x, y + 2, SSD1306_WHITE);  // Single pixel for the dot
  display.display();                           // Update display

  vibrationMotorsTaskParameters* taskParams = new vibrationMotorsTaskParameters;
  taskParams->vMotor1 = true;
  taskParams->vMotor2 = true;
  taskParams->vMotor3 = true;
  taskParams->vMotor4 = true;
  taskParams->duration = 300;

  xTaskCreatePinnedToCore(vibrationMotorsTaskMethod, "vibrationMotorsTask", 1024, taskParams, 1, &vibrationMotorsTask, 0);

  Serial.println("ready to go");
}

void vibrationMotorsTaskMethod(void* parameter) {
  vibrationMotorsTaskParameters* params = (vibrationMotorsTaskParameters*)parameter;
  if (params->vMotor1) {
    analogWrite(VMOTOR1, 10);
  }
  if (params->vMotor2) {
    analogWrite(VMOTOR2, 10);
  }
  if (params->vMotor3) {
    analogWrite(VMOTOR3, 10);
  }
  if (params->vMotor4) {
    analogWrite(VMOTOR4, 10);
  }
  vTaskDelay(params->duration / portTICK_PERIOD_MS);
  analogWrite(VMOTOR1, 0);
  analogWrite(VMOTOR2, 0);
  analogWrite(VMOTOR3, 0);
  analogWrite(VMOTOR4, 0);
  vTaskDelete(NULL);
}

void sendJoystickData() {
  String dataToSend = "~";
  bool send = false;
  if (myJoysticksAxisEnabled.Rx && myJoysticksState.Rx != myJoysticksLastState.Rx) {
    if (myJoysticksState.Rx > -1) {
      if (myJoysticksState.Rx == 0 && myJoysticksLastState.Rx < 0) {
        dataToSend += "-";
      } else {
        dataToSend += "+";
      }
    }
    dataToSend += String(myJoysticksState.Rx) + ",";
    send = true;
  } else {
    dataToSend += "n,";  // it's used to represent NULL
  }
  if (myJoysticksAxisEnabled.Ry && myJoysticksState.Ry != myJoysticksLastState.Ry) {
    if (myJoysticksState.Ry > -1) {
      if (myJoysticksState.Ry == 0 && myJoysticksLastState.Ry < 0) {
        dataToSend += "-";
      } else {
        dataToSend += "+";
      }
    }
    dataToSend += String(myJoysticksState.Ry) + ",";
    send = true;
  } else {
    dataToSend += "n,";
  }
  if (myJoysticksAxisEnabled.Rz && myJoysticksState.Rz != myJoysticksLastState.Rz) {
    dataToSend += String(myJoysticksState.Rz) + ",";
    send = true;
  } else {
    dataToSend += "n,";
  }
  if (myJoysticksAxisEnabled.Lx && myJoysticksState.Lx != myJoysticksLastState.Lx) {
    if (myJoysticksState.Lx > -1) {
      if (myJoysticksState.Lx == 0 && myJoysticksLastState.Lx < 0) {
        dataToSend += "-";
      } else {
        dataToSend += "+";
      }
    }
    dataToSend += String(myJoysticksState.Lx) + ",";
    send = true;
  } else {
    dataToSend += "n,";
  }
  if (myJoysticksAxisEnabled.Ly && myJoysticksState.Ly != myJoysticksLastState.Ly) {
    if (myJoysticksState.Ly > -1) {
      if (myJoysticksState.Ly == 0 && myJoysticksLastState.Ly < 0) {
        dataToSend += "-";
      } else {
        dataToSend += "+";
      }
    }
    dataToSend += String(myJoysticksState.Ly) + ",";
    send = true;
  } else {
    dataToSend += "n,";
  }
  if (myJoysticksAxisEnabled.Lz && myJoysticksState.Lz != myJoysticksLastState.Lz) {
    dataToSend += String(myJoysticksState.Lz) + ",";
    send = true;
  } else {
    dataToSend += "n,";
  }
  if (send && isConnectedToRovi) {
    if (isConnectedToWebSocket) webSocket.sendTXT(dataToSend);
    // Serial.println(dataToSend);
  }
  // if(send){
  //   Serial.println(dataToSend);
  // }
}

void updateJoysticks() {
  myJoysticksLastState.Rx = myJoysticksState.Rx;
  myJoysticksLastState.Ry = myJoysticksState.Ry;
  myJoysticksLastState.Rz = myJoysticksState.Rz;
  myJoysticksLastState.Lx = myJoysticksState.Lx;
  myJoysticksLastState.Ly = myJoysticksState.Ly;
  myJoysticksLastState.Lz = myJoysticksState.Lz;

  myJoysticksState.Rx = analogRead(RJX_PIN);
  myJoysticksState.Ry = analogRead(RJY_PIN);
  myJoysticksState.Lx = analogRead(LJX_PIN);
  myJoysticksState.Ly = analogRead(LJY_PIN);

  // myJoysticksState.Rx = map(analogRead(RJX_PIN), 0, 4095, -255, 255);
  // myJoysticksState.Ry = map(analogRead(RJY_PIN), 0, 4095, -255, 255);
  myJoysticksState.Rz = digitalRead(RJZ_PIN);
  // myJoysticksState.Lx = map(analogRead(LJX_PIN), 0, 4095, -255, 255);
  // myJoysticksState.Ly = map(analogRead(LJY_PIN), 0, 4095, -255, 255);
  myJoysticksState.Lz = digitalRead(LJZ_PIN);

  if (myJoysticksState.Rx > 2000) {
    myJoysticksState.Rx = (myJoysticksState.Rx > 3600) ? -255 : map(myJoysticksState.Rx, 2000, 3600, -10, -255);
  } else if (myJoysticksState.Rx < 1800) {
    myJoysticksState.Rx = (myJoysticksState.Rx < 410) ? 255 : map(myJoysticksState.Rx, 1800, 400, 10, 255);
  } else {
    myJoysticksState.Rx = 0;
  }

  if (myJoysticksState.Ry > 2000) {
    myJoysticksState.Ry = (myJoysticksState.Ry > 3600) ? -255 : map(pow(myJoysticksState.Ry / 10, 2) / 100, 400, 1296, -10, -255);
  } else if (myJoysticksState.Ry < 1800) {
    myJoysticksState.Ry = (myJoysticksState.Ry < 410) ? 255 : map(pow(map(myJoysticksState.Ry, 1800, 400, 200, 360), 2) / 100, 400, 1296, 10, 255);  // pow(map(myJoysticksState.Ry, 1800, 400, 200, 360), 2)/100, 400, 1296, -10, -255
  } else {
    myJoysticksState.Ry = 0;
  }

  if (myJoysticksState.Lx > 2050) {
    myJoysticksState.Lx = (myJoysticksState.Lx > 3600) ? -255 : map(myJoysticksState.Lx, 2050, 3600, -10, -255);
  } else if (myJoysticksState.Lx < 1800) {
    myJoysticksState.Lx = (myJoysticksState.Lx < 410) ? 255 : map(myJoysticksState.Lx, 1800, 400, 10, 255);
  } else {
    myJoysticksState.Lx = 0;
  }

  if (myJoysticksState.Ly > 2000) {
    myJoysticksState.Ly = (myJoysticksState.Ly > 3600) ? -255 : map(pow(myJoysticksState.Ly / 10, 2) / 100, 400, 1296, -10, -255);  // map(myJoysticksState.Ly, 2000, 3600, 10, 255)
  } else if (myJoysticksState.Ly < 1800) {
    myJoysticksState.Ly = (myJoysticksState.Ly < 410) ? 255 : map(pow(map(myJoysticksState.Ly, 1800, 400, 200, 360), 2) / 100, 400, 1296, 10, 255);  // map(myJoysticksState.Ly, 1800, 400, -10, -255)
  } else {
    myJoysticksState.Ly = 0;
  }

  // if(myJoysticksState.Rx < 20 & myJoysticksState.Rx > -20){
  //   myJoysticksState.Rx = 0;
  // }
  // if(myJoysticksState.Ry < 20 & myJoysticksState.Ry > -20){
  //   myJoysticksState.Ry = 0;
  // }
  // if(myJoysticksState.Lx < 20 & myJoysticksState.Lx > -20){
  //   myJoysticksState.Lx = 0;
  // }
  // if(myJoysticksState.Ly < 20 & myJoysticksState.Ly > -20){
  //   myJoysticksState.Ly = 0;
  // }
}

void loop() {
  // put your main code here, to run repeatedly:
  webSocket.loop();
  delay(1);
}

void handleWebSocketMessage(uint8_t* data) {
  Serial.println((char*)data);
  String values = (char*)data;
  if (values == "id") {
    webSocket.sendTXT("^idCode-34");
    hasConnectedToRoviSinceAppStart = true;
    isConnectedToRovi = true;
    lastAcknowledgement = millis();
    // display.fillRect(0, 0, 30, 30, SSD1306_BLACK);  // Clear area for dynamic text
    // display.setCursor(0, 0);
    // display.setTextSize(1);
    // display.print("connected to robot");  // Print the current value
    // display.display();
  }
  if (values == "ackR") {
    if (!isConnectedToRovi) {
      isConnectedToRovi = true;
      connectedToWiFiRovi = true;
      Serial.println("connected to Rovi change");
    }
    lastAcknowledgement = millis();
  }
  if (values.substring(0, 17) == "BatteryPercentage") {
    // lastRoviBatteryPercentage = roviBatteryPercentage;
    int percentage = values.substring(18).toInt();
    if (roviBatteryPercentage != percentage) {
      roviBatteryPercentage = percentage;
      display.fillRect(65, 0, 13, 10, SSD1306_BLACK);  // Clear area for dynamic text
      display.setTextSize(1);
      display.setCursor(65, 0);
      display.print(roviBatteryPercentage);
      display.print("%");
      display.display();
    }
  }
}
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected from web socket!");
      isConnectedToWebSocket = false;
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      isConnectedToWebSocket = true;
      // send message to server when Connected
      webSocket.sendTXT("Connected");
      break;
    case WStype_TEXT:
      // Serial.printf("[WSc] get text: %s\n", payload);
      handleWebSocketMessage(payload);
      // send message to server
      // webSocket.sendTXT("message here");
      break;
    case WStype_BIN:
      // Serial.printf("[WSc] get binary text: %u\n", (char*)payload);


      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void gyroGameLoop() {
  // Calculate ball position based on tilt angles
  float xOffset = (ypr.roll * 0.35);    // Scale roll value to fit screen width
  float yOffset = (ypr.pitch * -0.35);  // Scale pitch value to fit screen height

  // Apply offsets to center position
  lastBallX = ballX;
  lastBallY = ballY;
  ballX = ballX + xOffset;
  ballY = ballY + yOffset;

  // Keep ball within screen boundaries
  if (ballX < ballRadius) {
    ballX = ballRadius;
  }
  if (ballX > SCREEN_WIDTH - ballRadius) {
    ballX = SCREEN_WIDTH - ballRadius;
  }
  if (ballY < ballRadius) {
    ballY = ballRadius;
  }
  if (ballY > SCREEN_HEIGHT - ballRadius) {
    ballY = SCREEN_HEIGHT - ballRadius;
  }
  display.fillCircle((int)lastBallX, (int)lastBallY, ballRadius, SSD1306_BLACK);
  display.fillCircle((int)ballX, (int)ballY, ballRadius, SSD1306_WHITE);

  // Update display
  display.display();
}