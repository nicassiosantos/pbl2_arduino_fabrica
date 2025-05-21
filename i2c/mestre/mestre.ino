#include <Wire.h>

void setup() {
  Wire.begin();        // Inicia o I2C como mestre
  Serial.begin(9600);
}

void loop() {
  // Envia um comando (0x01 ou 0x02) para o escravo
  uint8_t command = 0x02;  // Teste alterando para 0x02
  Wire.beginTransmission(0x40);  // Endereço do escravo
  Wire.write(command);           // Envia o comando
  Wire.endTransmission();

  // Solicita 1 byte de resposta do escravo
  Wire.requestFrom(0x40, 1);
  if (Wire.available()) {
    uint8_t response = Wire.read();
    Serial.print("Resposta do escravo: 0x");
    Serial.println(response, HEX);  // Deve imprimir 0x55 ou 0xAA
  }

  delay(1000);  // Espera 1 segundo antes de repetir
}