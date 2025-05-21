#include <Wire.h>

void setup() {
  Wire.begin();  // Inicia como mestre
  Serial.begin(9600);
  Serial.println("Mestre I2C iniciado. Envie 'PING' pelo monitor serial.");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');  // Lê do monitor serial
    input.trim();  // Remove espaços e newlines extras

    // Envia a string para o escravo
    Wire.beginTransmission(0x40);
    Wire.write(input.c_str());
    Wire.endTransmission();

    // Solicita resposta (lê até 32 bytes ou encontrar '\n')
    Wire.requestFrom(0x40, 32);
    String response = "";
    while (Wire.available()) {
      char c = Wire.read();
      if (c == '\n') break;  // Para ao encontrar o terminador
      response += c;
    }
    Serial.print("Resposta do escravo: ");
    Serial.println(response);
  }
}