#include "stDHT.h"
DHT sens(DHT22, 2); // Указать датчик DHT11, DHT21, DHT22

void setup() 
{
  Serial.begin(115200);
  pinMode(2, INPUT);
  digitalWrite(2, HIGH);
  pinMode(3, INPUT);
  digitalWrite(3, HIGH);
}

void loop() 
{
  int t = sens.readTemperature(); // чтение датчика на пине 2
  int h = sens.readHumidity();    // чтение датчика на пине 2
  delay(2000); 

  Serial.print("Hum: ");
  Serial.print(h);
  Serial.print(" %");
  Serial.print("Temp: ");
  Serial.print(t);
  Serial.println(" C ");
}

