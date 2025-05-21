#include <Wire.h>
#include <avr/io.h>
#include <avr/interrupt.h>


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


void setup() {
  Wire.begin();  // Inicia como mestre
  Serial.begin(9600);
  //USART_init(103);
  USART_send_string("Mestre I2C iniciado. Envie 'PING' pelo monitor serial.\n");
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
    const char* resp = response.c_str();
    USART_send_string("Resposta do escravo: ");
    USART_send_string(resp);
  }
}