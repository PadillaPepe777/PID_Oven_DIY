#include <Arduino.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define Select_Button 2
#define EncButton 3
#define OUTA 4  //CLK
#define OUTB 5  //DT

#define TRIAC_OUT 6
#define AC_IN 7

LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize the LCD with the I2C address and dimensions

unsigned long lastTime;
double input, output, setpoint;
float kp=1.0, ki=0.0, kd=0.0;
double error, lastError, P, I, D;

float outMin = 0, outMax = 255;

volatile bool ultimoCLK = true;
int cambio = 0;


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
  pinMode(AC_IN, INPUT);

  Serial.println("Baking Time!");
  lcd.setCursor(0, 0); //First column, first row
  lcd.print("Baking Time!"); //Test lcd.write("Text") or lcd.print("Text") to see which works best


}

void loop() {
  //Instructions for the PID loop
}

void PID(unsigned long ActTime) {
float deltaTime = (ActTime - lastTime);
  error = setpoint - input;
  P = kp * error;
  I += (ki * error * deltaTime);
  D = kd * (error - lastError) / deltaTime;
  
  /*Compute PID Output*/
  output = P+I+D;

  if (output > outMax) output = outMax;
  else if (output < outMin) output = outMin;

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

void SetPoint(){

}