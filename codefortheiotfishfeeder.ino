#include "thingProperties.h"
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define the blower and machine relay pins
int blower = 4;
int machine = 5;

// Define the DHT sensor type and pin
#define DHTPIN 8    // Pin where the DHT22 is connected
#define DHTTYPE DHT22   // DHT 22 (AM2302)

// Define the ultrasonic sensor pins and variables
const int trigPin = 6; // Trigger pin of the ultrasonic sensor
const int echoPin = 7; // Echo pin of the ultrasonic sensor
long duration;
int distance; // Variable to store distance
int foodPercentage; // Variable to store the percentage of food left
const int maxDistance = 49; // Maximum distance when the container is empty
const int minDistance = 2; // Minimum distance when the container is full

// Define the LED pins
const int greenLED = 9;
const int yellowLED = 10;
const int redLED = 11;

// Define the buzzer pin
const int buzzer = 12;

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Initialize the LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change the address 0x27 to match your module

unsigned long previousMillis = 0;
unsigned long displayPreviousMillis = 0;
const long interval = 2000; // Interval for sensor readings (2 seconds)
const long displayInterval = 5000; // Interval for switching display (5 seconds)
bool showTempHumidity = true;

void setup() {
  // Initialize serial and wait for port to open
  Serial.begin(9600);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500);

  // Set the blower and machine pins as output
  pinMode(blower, OUTPUT);
  pinMode(machine, OUTPUT);

  // Initialize the LED and buzzer pins
  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Initialize DHT sensor
  dht.begin();

  // Initialize the LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Initialize the ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // Synchronize initial blower and machine states with cloud switches
  if (blower_status) {
    digitalWrite(blower, HIGH);
  } else {
    digitalWrite(blower, LOW);
  }

  if (machine_status) {
    digitalWrite(machine, HIGH);
  } else {
    digitalWrite(machine, LOW);
  }

  // Initialize LEDs based on cloud variables
  updateLEDs();
}

void loop() {
  ArduinoCloud.update();

  // Get the current time
  unsigned long currentMillis = millis();

  // Read the DHT22 sensor and ultrasonic sensor at the specified interval
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read temperature and humidity
    humiditypin = dht.readHumidity();
    temperaturepin = dht.readTemperature();

    // Check if any reads failed and exit early (to try again)
    if (isnan(humiditypin) || isnan(temperaturepin)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperaturepin);
      Serial.print(" °C ");
      Serial.print("Humidity: ");
      Serial.print(humiditypin);
      Serial.println(" %");
    }

    // Read the ultrasonic sensor
    readUltrasonicSensor();
  }

  // Switch display content every 5 seconds
  if (currentMillis - displayPreviousMillis >= displayInterval) {
    displayPreviousMillis = currentMillis;
    showTempHumidity = !showTempHumidity;
  }

  if (showTempHumidity) {
    // Display temperature and humidity
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperaturepin);
    lcd.print(" C");
    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humiditypin);
    lcd.print(" %");
  } else {
    // Display food level
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Food Left: ");
    lcd.print(foodPercentage);
    lcd.print(" %");
  }

  // Check the status of all schedules and update blower accordingly
  bool anyScheduleActive = schedule1.isActive() || schedule2.isActive() || schedule3.isActive();

  Serial.print("Schedule1 is active: ");
  Serial.println(schedule1.isActive());
  Serial.print("Schedule2 is active: ");
  Serial.println(schedule2.isActive());
  Serial.print("Schedule3 is active: ");
  Serial.println(schedule3.isActive());

  if (anyScheduleActive) {
    digitalWrite(blower, HIGH);
    blower_status = true;
  } else {
    digitalWrite(blower, LOW);
    blower_status = false;
  }

  // Update machine relay based on machine_status
  if (machine_status) {
    digitalWrite(machine, HIGH);
  } else {
    digitalWrite(machine, LOW);
  }

  // Control LEDs and buzzer based on food level
  if (foodPercentage > 70) {
    // Food is full
    greenLED_status = true;
    yellowLED_status = false;
    redLED_status = false;
    noTone(buzzer);
  } else if (foodPercentage > 30) {
    // Food is in the middle
    greenLED_status = false;
    yellowLED_status = true;
    redLED_status = false;
    noTone(buzzer);
  } else if (foodPercentage <= 30) {
    // Food is low or empty
    greenLED_status = false;
    yellowLED_status = false;
    redLED_status = true;
    tone(buzzer, 1000); // Buzzer beeps at 1000 Hz
  }

  // Update cloud variables
  updateLEDs();
}

void readUltrasonicSensor() {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin, and calculate the distance
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  // Map the distance to a percentage
  foodPercentage = map(distance, minDistance, maxDistance, 100, 0);

  // Constrain the value between 0 and 100
  foodPercentage = constrain(foodPercentage, 0, 100);

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm, Food Left: ");
  Serial.print(foodPercentage);
  Serial.println(" %");

  // Update the cloud variable
  foodlevel = foodPercentage;
}

void updateLEDs() {
  // Update LEDs based on cloud variables
  digitalWrite(greenLED, greenLED_status ? HIGH : LOW);
  digitalWrite(yellowLED, yellowLED_status ? HIGH : LOW);
  digitalWrite(redLED, redLED_status ? HIGH : LOW);
}

void onBlowerStatusChange() {
  Serial.print("blower status changed: ");
  Serial.println(blower_status);
  if (blower_status) {
    digitalWrite(blower, HIGH);
  } else {
    digitalWrite(blower, LOW);
  }
}

void onMachineStatusChange() {
  Serial.print("machine status changed: ");
  Serial.println(machine_status);
  if (machine_status) {
    digitalWrite(machine, HIGH);
  } else {
    digitalWrite(machine, LOW);
  }
}

void onSchedule1Change() {
  Serial.println("Schedule1 changed.");
}

void onSchedule2Change() {
  Serial.println("Schedule2 changed.");
}

void onSchedule3Change() {
  Serial.println("Schedule3 changed.");
}

void onHumiditypinChange() {
  Serial.print("Humiditypin changed: ");
  Serial.println(humiditypin);
}

void onTemperaturepinChange() {
  Serial.print("Temperaturepin changed: ");
  Serial.println(temperaturepin);
}

void onFoodlevelChange() {
  Serial.print("Foodlevel changed: ");
  Serial.println(foodlevel);
}

void onGreenLEDStatusChange() {
  Serial.print("Green LED status changed: ");
  Serial.println(greenLED_status);
  updateLEDs();
}

void onRedLEDStatusChange() {
  Serial.print("Red LED status changed: ");
  Serial.println(redLED_status);
  updateLEDs();
}

void onYellowLEDStatusChange() {
  Serial.print("Yellow LED status changed: ");
  Serial.println(yellowLED_status);
  updateLEDs();
}
