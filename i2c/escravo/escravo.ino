#include <Wire.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define I2C_ADDRESS 0x40  // Endereço do escravo


// Implementação da USART
void USART_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_send(char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

void USART_send_string(const char* str) {
    while (*str) {
        USART_send(*str++);
    }
}

void send_hex(uint8_t num) {
    uint8_t nibble;
    
    // Nibble superior
    nibble = (num >> 4) & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
    
    // Nibble inferior
    nibble = num & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
}



String receivedString = "";  // Armazena a string recebida
String responseString = "PONG";  // Resposta padrão

void receiveEvent(int numBytes) {
  receivedString = "";
  while (Wire.available()) {
    char c = Wire.read();
    receivedString += c;  // Concatena cada byte recebido
  }
  const char* resp = receivedString.c_str();
  USART_send_string("Recebido: ");
  USART_send_string(resp);

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
  USART_send_string("Escravo I2C pronto (com terminador)! \n");
}

void loop() {}