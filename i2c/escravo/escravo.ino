#include <Wire.h>

#define I2C_ADDRESS 0x40  // Endereço do escravo

String receivedString = "";  // Armazena a string recebida
String responseString = "PONG";  // Resposta padrão

void receiveEvent(int numBytes) {
  receivedString = "";
  while (Wire.available()) {
    char c = Wire.read();
    receivedString += c;  // Concatena cada byte recebido
  }
  Serial.print("Recebido: ");
  Serial.println(receivedString);

  // Define a resposta baseada no comando
  if (receivedString == "PING") {
    responseString = "PONG";
  } else {
    responseString = "COMANDO INVALIDO";
  }
}

void requestEvent() {
  Wire.write(responseString.c_str());  // Envia a string
  Wire.write('\n');  // Terminador (indica fim da mensagem)
}

void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println("Escravo I2C pronto (com terminador)!");
}

void loop() {}