#include <LiquidCrystal.h>
#include <Wire.h>
#include "RTClib.h"
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);
RTC_DS1307 rtc;
// pin setup
const int trigPin1 = 9;
const int echoPin1 = 10;
const int trigPin2 = 11;
const int echoPin2 = 12;
const int lightPin = 8;
const int joyXPin = A0;
const int joyYPin = A1;
const int joySWPin = 13;
const int menuButtonPin = A2;
// occupancy variables
int thresholdDistance = 30;
int occupancyCount = 0;
int sensorState = 0;
// menu variables
int menuState = 0;
int menuCursor = 0;
int editCursor = 0;
// timeout/dnd controls
int timeoutMinutes = 0;
int timeoutSeconds = 0;
int dndStartHour = 0;
int dndStartMinute = 0;
int dndEndHour = 0;
int dndEndMinute = 0;
// real time control
unsigned long lastTimeUpdate = 0;
const int timeUpdateInterval = 1000;
// input debounce
unsigned long lastInputTime = 0;
const int inputDebounce = 200;
// timeout timer
unsigned long occupancyZeroTime = 0;
void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Occupancy: 0");
  lcd.setCursor(0, 1);
  if (!rtc.begin()) {
    lcd.print("rtc failed!");
    while (1);
  }
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
  pinMode(lightPin, OUTPUT);
  digitalWrite(lightPin, LOW);
  pinMode(joySWPin, INPUT_PULLUP);
  pinMode(menuButtonPin, INPUT_PULLUP);
  displayHomeScreen();
}
void loop() {
  handleInputs();
  if (menuState == 0) {
    handleOccupancy();
    handleTimeout();
    updateClockDisplay();
  }
}
// handle joystick and push button
void handleInputs() {
  if (millis() - lastInputTime < inputDebounce) return;
  if (checkButtons()) return;
  if (menuState > 0) {
    checkJoystick();
  }
}
bool checkButtons() {
  if (digitalRead(menuButtonPin) == LOW) {
    lastInputTime = millis();
    toggleMenu();
    return true;
  }
  if (digitalRead(joySWPin) == LOW) {
    lastInputTime = millis();
    handleJoySWButton();
    return true;
  }
  return false;
}
void toggleMenu() {
  if (menuState == 0) {
    menuState = 1;
    menuCursor = 0;
    displayMainMenu();
  } else {
    menuState = 0;
    displayHomeScreen();
  }
}
/* 
menu state notes
0: back to homescreen
1: menu list display
2: timeout setting
3: dndstart setting
4: dndend setting
5: set occupancy
menu cursor notes
0: timeout
1: dnd
2: occupancy
*/
void handleJoySWButton() {
  if (menuState == 1) {
    if (menuCursor == 0) { menuState = 2; editCursor = 0; displayTimeoutMenu(); }
    else if (menuCursor == 1) { menuState = 3; editCursor = 0; displaySetDndStart(); }
    else if (menuCursor == 2) { menuState = 5; editCursor = 0; displaySetOccupancyMenu(); }
  } 
  else if (menuState == 3) { 
    menuState = 4; editCursor = 0; displaySetDndEnd();
  } 
  else {
    menuState = 1;
    displayMainMenu();
  }
}
void checkJoystick() {
  int xVal = analogRead(joyXPin);
  int yVal = analogRead(joyYPin);
  if (xVal < 100) {// left
    lastInputTime = millis();
    editCursor = 0;
    updateSubMenuDisplay();
  } 
  else if (xVal > 900) {// right
    lastInputTime = millis();
    editCursor = 1;
    updateSubMenuDisplay();
  }
  if (yVal < 100) { //up
    lastInputTime = millis();
    if (menuState == 1) {
      menuCursor--;
      if (menuCursor < 0) menuCursor = 2;
      displayMainMenu();
    } else {
      adjustValue(1); // increase value for the menu features
    }
  }
  else if (yVal > 900) { // down
    lastInputTime = millis();
    if (menuState == 1) {
      menuCursor++;
      if (menuCursor > 2) menuCursor = 0;
      displayMainMenu();
    } else {
      adjustValue(-1); // decrease value
    }
  }
}
void adjustValue(int direction) {
  switch (menuState) {
    case 2: // Timeout
      if (editCursor == 0) timeoutMinutes = constrain(timeoutMinutes + direction, 0, 60);
      else {
        timeoutSeconds += direction;
        if (timeoutSeconds > 59) timeoutSeconds = 0;
        else if (timeoutSeconds < 0) timeoutSeconds = 59;
      }
      displayTimeoutMenu();
      break;
    case 3: // DnD Start
      if (editCursor == 0) {
        dndStartHour += direction;
        if (dndStartHour > 23) dndStartHour = 0;
        else if (dndStartHour < 0) dndStartHour = 23;
      } else {
        dndStartMinute += direction;
        if (dndStartMinute > 59) dndStartMinute = 0;
        else if (dndStartMinute < 0) dndStartMinute = 59;
      }
      displaySetDndStart();
      break;
    case 4: // DnD End
      if (editCursor == 0) {
        dndEndHour += direction;
        if (dndEndHour > 23) dndEndHour = 0;
        else if (dndEndHour < 0) dndEndHour = 23;
      } else {
        dndEndMinute += direction;
        if (dndEndMinute > 59) dndEndMinute = 0;
        else if (dndEndMinute < 0) dndEndMinute = 59;
      }
      displaySetDndEnd();
      break;
    case 5: // Occupancy
      occupancyCount = constrain(occupancyCount + direction, 0, 20);
      if (occupancyCount == 0) digitalWrite(lightPin, LOW);
      else digitalWrite(lightPin, HIGH);
      displaySetOccupancyMenu();
      break;
  }
}
void updateSubMenuDisplay() {
  if (menuState == 2) displayTimeoutMenu();
  else if (menuState == 3) displaySetDndStart();
  else if (menuState == 4) displaySetDndEnd();
}
void handleOccupancy() {
  long dist1 = getDistance(trigPin1, echoPin1);
  delay(30);
  long dist2 = getDistance(trigPin2, echoPin2);
  bool s1_active = (dist1 < thresholdDistance && dist1 > 0);
  bool s2_active = (dist2 < thresholdDistance && dist2 > 0);
  switch (sensorState) {
    case 0:
      if (s1_active && !s2_active) sensorState = 1;
      else if (s2_active && !s1_active) sensorState = 2;
      break;
    case 1:
      if (s2_active) {
        occupancyCount++;
        updateLightAndOccupancy();
        sensorState = 3;
      } else if (!s1_active) sensorState = 0;
      break;
    case 2:
      if (s1_active) {
        occupancyCount--;
        if (occupancyCount < 0) occupancyCount = 0;
        updateLightAndOccupancy();
        sensorState = 3;
      } else if (!s2_active) sensorState = 0;
      break;
    case 3:
      if (!s1_active && !s2_active) sensorState = 0;
      break;
  }
}
void updateLightAndOccupancy() {
  if (occupancyCount > 0) {
    if (digitalRead(lightPin) == LOW) digitalWrite(lightPin, HIGH);
    occupancyZeroTime = 0;
  } else {
    if (digitalRead(lightPin) == HIGH) occupancyZeroTime = millis();
  }
  if (menuState == 0) updateOccupancyLCD();
}
void handleTimeout() {
  if (occupancyCount == 0 && occupancyZeroTime > 0) {
    unsigned long timeoutMillis = ((unsigned long)timeoutMinutes * 60000) + ((unsigned long)timeoutSeconds * 1000);
    if (timeoutMillis == 0) {
      if (checkDnDAndTurnOff()) occupancyZeroTime = 0;
    } else if (millis() - occupancyZeroTime >= timeoutMillis) {
      if (checkDnDAndTurnOff()) occupancyZeroTime = 0;
    }
  }
}
bool checkDnDAndTurnOff() {
  DateTime now = rtc.now();
  int currentTotalMinutes = now.hour() * 60 + now.minute();
  int dndStartTotalMinutes = dndStartHour * 60 + dndStartMinute;
  int dndEndTotalMinutes = dndEndHour * 60 + dndEndMinute;
  bool isDnD = false;
  if (dndStartTotalMinutes > dndEndTotalMinutes) {
    if (currentTotalMinutes >= dndStartTotalMinutes || currentTotalMinutes < dndEndTotalMinutes) isDnD = true;
  }
  else if (dndStartTotalMinutes < dndEndTotalMinutes) {
    if (currentTotalMinutes >= dndStartTotalMinutes && currentTotalMinutes < dndEndTotalMinutes) isDnD = true;
  }
  if (isDnD) return false;
  else {
    digitalWrite(lightPin, LOW);
    return true;
  }
}
void displayHomeScreen() {
  lcd.clear();
  updateOccupancyLCD();
  updateClockDisplay();
}
void displayMainMenu() {
  lcd.clear();
  switch (menuCursor) {
    case 0:
      lcd.setCursor(0, 0); lcd.print(">Set Timeout");
      lcd.setCursor(0, 1); lcd.print(" Set DnD");
      break;
    case 1:
      lcd.setCursor(0, 0); lcd.print(" Set Timeout");
      lcd.setCursor(0, 1); lcd.print(">Set DnD");
      break;
    case 2:
      lcd.setCursor(0, 0); lcd.print(" Set DnD");
      lcd.setCursor(0, 1); lcd.print(">Set Occupancy");
      break;
  }
}
void displayTimeoutMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Timeout");
  lcd.setCursor(0, 1);
  char buf[20];
  if (editCursor == 0) sprintf(buf, ">Min:%02d  Sec:%02d", timeoutMinutes, timeoutSeconds);
  else sprintf(buf, " Min:%02d >Sec:%02d", timeoutMinutes, timeoutSeconds);
  lcd.print(buf);
}
void displaySetDndStart() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set DnD Start");
  lcd.setCursor(0, 1);
  char buf[20];
  if (editCursor == 0) sprintf(buf, ">Hr:%02d  Min:%02d", dndStartHour, dndStartMinute);
  else sprintf(buf, " Hr:%02d >Min:%02d", dndStartHour, dndStartMinute);
  lcd.print(buf);
}
void displaySetDndEnd() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set DnD End");
  lcd.setCursor(0, 1);
  char buf[20];
  if (editCursor == 0) sprintf(buf, ">Hr:%02d  Min:%02d", dndEndHour, dndEndMinute);
  else sprintf(buf, " Hr:%02d >Min:%02d", dndEndHour, dndEndMinute);
  lcd.print(buf);
}
void displaySetOccupancyMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Occupancy");
  lcd.setCursor(0, 1);
  char buf[20];
  sprintf(buf, "Count:%d", occupancyCount);
  lcd.print(buf);
}
void updateClockDisplay() {
  if (menuState != 0) return;
  if (millis() - lastTimeUpdate > timeUpdateInterval) {
    lastTimeUpdate = millis();
    DateTime now = rtc.now();
    char timeString[9];
    sprintf(timeString, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(timeString);
  }
}
void updateOccupancyLCD() {
  if (menuState != 0) return;
  lcd.setCursor(0, 0);
  lcd.print("Occupancy:    ");
  lcd.setCursor(11, 0);
  lcd.print(occupancyCount);
}
long getDistance(int trig, int echo) {
  long duration, distance;
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  duration = pulseIn(echo, HIGH, 30000);
  distance = duration * 0.034 / 2;
  return distance;
}