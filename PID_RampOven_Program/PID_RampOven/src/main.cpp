#include <Arduino.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h> //SDA A4, SCL A5
//#include "Adafruit_LiquidCrystal.h"

#include <max6675.h>


#define Select_Button 6
#define RIGHT_BUTTON 11
#define LEFT_BUTTON 8

#define EncButton 3
#define OUTA 4  //CLK
#define OUTB 5  //DT

#define AC_IN 2
#define TRIAC_OUT 7

//Max6675
#define S0 12
#define CS 10
#define SCK 13


LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize the LCD with the I2C address and dimensions
MAX6675 thermocouple(SCK, CS, S0);

bool isBlinking = false; // Variable to control blinking state
int currentMode = 0; // 0 for Soak, 1 for Ramp
unsigned long dispTime = 0; // Variable to track the last time the display was updated
bool modeSelected = false; // Variable to track if a mode has been selected
bool prevSB = true; // Variable to track the previous state of the select button

unsigned long lastTime;
double output, Setpoint;
volatile int firingAngle;
float kp=1.0, ki=0.0, kd=0.0;
double error, lastError, P, I, D;

float outMin = 0, outMax = 255;
int temperature = 25; //desired temperature
float actualTemperature = 24.5; //actual temperature
float StartPoint = 0; // Starting point for the ramp

volatile bool ultimoCLK = true;
int cambio = 0;

int SetPoint_mS=1000; //How long for setpoint to move in mS ramp mode
float movingSetPoint=0; //Variable to hold the moving setpoint in ramp mode
double m=0; //Slope of the ramp in ramp mode

int minutes_ramp = 0; // Variable to hold the desired time in minutes
int minutes_stay = 0; // Variable to hold the desired time in minutes
unsigned long runTime = 0; // Variable to hold the start time of the ramp

void PID(unsigned long ActTime, float NowSetPoint) {
float deltaTime = (ActTime - lastTime);
  error = NowSetPoint - actualTemperature;
  P = kp * error;
  I += (ki * error * deltaTime);
  D = kd * (error - lastError) / deltaTime;
  
  /*Compute PID Output*/
  output = P+I+D;

  if (output > outMax) output = outMax;
  else if (output < outMin) output = outMin;

  firingAngle = map(output, 0, 255, 8333, 0); // Map the output to a firing angle in microseconds (for 60Hz AC)
  //  16.67ms / 2 = 8.33ms = 8333us
  
  if (firingAngle > 8333) firingAngle = 8333;
  if (firingAngle < 0) firingAngle = 0;

  /*Remember some variables for next time*/
  lastError = error;
  lastTime = ActTime;
} 

int leerEncoder() { //Navigating the menu with the rotary encoder

  int clk = digitalRead(OUTA);

  if (clk != ultimoCLK) {

    ultimoCLK = clk;
    if (digitalRead(OUTB) != clk)
      return 1;
    else
      return -1;
  }
  ultimoCLK = clk;
  return 0;
}

void Ramp_SetPoint(){

  movingSetPoint = ((millis()-runTime)*m/1000.0) + StartPoint; // Increment the moving setpoint based on the slope every second
  
}

void Triac_Control() {

  // Desactivar interrupciones brevemente para evitar rebotes
  noInterrupts();
  
  // Encender el MOC3021 inmediatamente o con un pequeño delay de seguridad
  // (El MOC3021 es de disparo aleatorio, pero para control de fase, 
  // queremos disparar en el ángulo correcto, no aleatorio. 
  // Nota: El MOC3021 es "Random Phase", el MOC3023 es "Zero Cross". 
  // Para control de fase, el MOC3021 está bien si lo disparas manualmente).
  
  // Esperar el ángulo calculado por el PID
  delayMicroseconds(output);
  
  // Disparar el TRIAC
  digitalWrite(TRIAC_OUT, HIGH);
  delayMicroseconds(10); // Pulso de 10us es suficiente para el MOC3021
  digitalWrite(TRIAC_OUT, LOW);
  
  interrupts();
}

void updateDisplay() {
  
  if (currentMode == 0) {
    lcd.setCursor(0, 7); //1st column, 2nd row
    lcd.print("Ramp  ");
    lcd.setCursor(0, 1); //1st column, 2nd row

    if (isBlinking) {
      lcd.print("Soak< ");
    } else {
      lcd.print("     "); // Espacios vacíos
    }
  } 
  else 
  {
    lcd.setCursor(0, 1); //1st column, 2nd row
    lcd.print("Soak  ");
    lcd.setCursor(0, 7); //1st column, 2nd row

    if (isBlinking) {
      lcd.print("Ramp< ");
    } else {
      lcd.print("     "); // Espacios vacíos
    }
  }

}

void TemperatureDisplay(int temperature) {
      lcd.setCursor(0, 1);
      lcd.print("Temp C: ");

      lcd.setCursor(8, 1);
      lcd.print(temperature, 1); // Display the temperature with one decimal place
}

void SelectTimeDisplay(int desiredTime) {
      lcd.setCursor(0, 1);
      lcd.print("T HH:MM: ");

      int hours = 0; // Reset hours to 0 before calculation
      int minutes = desiredTime; // Reset minutes to 0 before calculation
      lcd.setCursor(9, 1);

      if (desiredTime >60) {
        hours = desiredTime / 60;
        minutes = desiredTime % 60;
      }
        lcd.print(hours);
        lcd.print(":");
        lcd.print(minutes);
}

void RemainingTime(int ActualTime, int TotalTime) {
      lcd.setCursor(0, 0);
      lcd.print("TIME");

      int hours = 0; // Reset hours to 0 before calculation
      int minutes = TotalTime; // Reset minutes to 0 before calculation
      lcd.setCursor(0, 8);

      if (TotalTime >60) {
        hours = TotalTime / 60;
        minutes = TotalTime % 60;
      }
        lcd.print(hours);
        lcd.print(":");
        lcd.print(minutes);

      int hours2 = 0; // Reset hours to 0 before calculation
      int minutes2 = ActualTime; // Reset minutes to 0 before calculation
      lcd.setCursor(0, 8);

      if (ActualTime >60) {
        hours2 = ActualTime / 60;
        minutes2 = ActualTime % 60;
      }
        lcd.print(hours2);
        lcd.print(":");
        lcd.print(minutes2);
}

bool DebounceButton() {
bool ButtonPressed = false; // Flag to indicate if the button was pressed

if(!digitalRead(Select_Button) && prevSB==false) { // If the select button is pressed
    delay(20);
    if(!digitalRead(Select_Button)) { // Check again to confirm the button press
      prevSB = true; // Update the previous state of the select button
      ButtonPressed = true; // Set the button pressed flag
    }
    else
    prevSB = !digitalRead(Select_Button); // Update the previous state of the select button
  }
  else
    prevSB = !digitalRead(Select_Button); // Update the previous state of the select button

  return ButtonPressed; // Return the button pressed flag
  }

  float readTemperature(){
      double tempC = thermocouple.readCelsius();

      if (isnan(tempC)) {
        Serial.println("Error: Intentando de Nuevo");
        tempC = thermocouple.readCelsius();
      }

      if (isnan(tempC)) {
        Serial.println("Error: Tal vez desconectado");
        tempC = -1.0;
      }

      return tempC;
    }

void setup() {
  Serial.begin(9600);
  delay(500);

  lcd.init(); // initialize the lcd 
  lcd.backlight();
  lcd.clear();

  delay(100);

  pinMode(Select_Button, INPUT_PULLUP);
  pinMode(EncButton, INPUT_PULLUP);
  pinMode(OUTA, INPUT);
  pinMode(OUTB, INPUT);
  pinMode(TRIAC_OUT, OUTPUT);
  pinMode(AC_IN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(AC_IN), Triac_Control, CHANGE);


  Serial.println("Baking Time!");
  lcd.setCursor(0, 0); //First column, first row
  lcd.print("Baking Time!"); //Test lcd.write("Text") or lcd.print("Text") to see which works best
  delay(2000);
  noInterrupts();
  dispTime=millis();
}

void loop() {
  Serial.println("Select a Mode: ");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select a Mode: ");
  updateDisplay();

  while(modeSelected == false) {

  cambio=leerEncoder();
  currentMode = (currentMode + cambio + 2) % 2; // Ensure currentMode stays within 0 and 1

  if(millis() - dispTime >= 500) {
    isBlinking = !isBlinking; // Toggle the blinking state
    updateDisplay(); // Update the display based
    dispTime = millis(); // Reset the display timer
  }

  modeSelected=DebounceButton(); // Check for button press to select mode


  } //end Mode Selection Loop

  delay(100);

 switch(currentMode) {
    case 0:
      Serial.println("Soak Mode Selected");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Soak Mode Selected");
      TemperatureDisplay(temperature);

      while(1)
      {
        cambio=leerEncoder();
        temperature += cambio; // Adjust the temperature based on encoder input
        if(temperature < 24) temperature = 24; // Prevent negative temperature

        if(millis() - dispTime >= 500) {
        TemperatureDisplay(temperature);
        dispTime = millis(); // Reset the display timer
        }

        if(DebounceButton()); // Check for button press to exit the loop
        break;
      }
      delay(100);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ramp Time Selection");
      SelectTimeDisplay(minutes_ramp);

      while(1){
        cambio=leerEncoder();
        minutes_ramp += cambio; // Adjust the minutes based on encoder input
        if(minutes_ramp < 0) minutes_ramp = 0; // Prevent negative time

        if(millis() - dispTime >= 150) {
        SelectTimeDisplay(minutes_ramp);
        dispTime = millis(); // Reset the display timer
        }

        if(DebounceButton()); // Check for button press to exit the loop
        break;
      }

      delay(100);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Baking Time: ");
      SelectTimeDisplay(minutes_stay);

      while(1){
        cambio=leerEncoder();
        minutes_stay += cambio; // Adjust the minutes based on encoder input
        if(minutes_stay < 0) minutes_stay = 0; // Prevent negative time

        if(millis() - dispTime >= 150) {
        SelectTimeDisplay(minutes_stay);
        dispTime = millis(); // Reset the display timer
        }

        if(DebounceButton()); // Check for button press to exit the loop
        break;
      }



      actualTemperature=readTemperature(); // Read the actual temperature from the sensor
      StartPoint=actualTemperature; // Set the starting point for the ramp
      m=(temperature-actualTemperature)/(minutes_ramp*60); // Calculate the difference between desired and actual temperature


      

      runTime=millis();
      Ramp_SetPoint();
      PID(millis(), movingSetPoint); // Call PID for variables initialization
      dispTime=millis(); // Reset the display timer
      interrupts();

      while((millis() - runTime) < (unsigned long)(minutes_ramp * 60000)) {

        if((millis() - dispTime) >= 700) {
          dispTime = millis(); // Reset the display timer
          Ramp_SetPoint();
          RemainingTime(((millis() - runTime)/60000), minutes_ramp);
        }

        actualTemperature=readTemperature(); // Read the actual temperature from the sensor
        PID(millis(), movingSetPoint);
        delay(100); // Delay to avoid overwhelming the loop
        
      }
      
      delay(1000); // Wait for a second before starting the soak period

      //Ramp period completed, now enter the soak period
      Serial.println("Soak Period Starting");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ramp Complete");
      lcd.setCursor(0, 1);
      lcd.print("Soak Period Starting");
      delay(1500);
      lcd.clear();

      runTime=millis(); // Reset the run time for the soak period

      while((millis() - runTime) < (unsigned long)(minutes_stay * 60000)) {

        if((millis() - dispTime) >= 700) {
          dispTime = millis(); // Reset the display timer
          RemainingTime(((millis() - runTime)/60000), minutes_stay);
        }

        actualTemperature=readTemperature(); // Read the actual temperature from the sensor
        PID(millis(), temperature);
        delay(100); // Delay to avoid overwhelming the loop
        
      }

      delay(1000);
      Serial.println("Finished!");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Finished!");
      lcd.setCursor(0, 1);
      lcd.print("Enjoy :)");

      break;

    case 1:
      Serial.println("Ramp Mode Selected");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ramp Mode Selected");
      break;
  }

}



