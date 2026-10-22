#include <Arduino.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h> //SDA A4, SCL A5

#define Select_Button 2
#define EncButton 3
#define OUTA 4  //CLK
#define OUTB 5  //DT

#define TRIAC_OUT 6
#define AC_IN 7

LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize the LCD with the I2C address and dimensions

bool isBlinking = false; // Variable to control blinking state
int currentMode = 0; // 0 for Soak, 1 for Ramp
unsigned long dispTime = 0; // Variable to track the last time the display was updated
bool modeSelected = false; // Variable to track if a mode has been selected
bool prevSB = true; // Variable to track the previous state of the select button

unsigned long lastTime;
double input, output, Setpoint;
float kp=1.0, ki=0.0, kd=0.0;
double error, lastError, P, I, D;

float outMin = 0, outMax = 255;
int temperature = 25; // Initial temperature value for testing

volatile bool ultimoCLK = true;
int cambio = 0;

int SetPoint_mS=1000; //How long for setpoint to move in mS ramp mode
float movingSetPoint=0; //Variable to hold the moving setpoint in ramp mode


void PID(unsigned long ActTime, float NowSetPoint) {
float deltaTime = (ActTime - lastTime);
  error = NowSetPoint - input;
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

void Ramp_SetPoint(){
  //Implement a ramp function to gradually change the setpoint over time
}

void Triac_Control() {
  //Control the triac based on the PID output and the AC Zero crossing detection
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
  delay(2000);
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
        TemperatureDisplay(temperature);

        if(DebounceButton()); // Check for button press to exit the loop
        break;
      }


        

      break;
    case 1:
      Serial.println("Ramp Mode Selected");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ramp Mode Selected");
      break;
  }

}



