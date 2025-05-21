#include <Wire.h>

#define I2C_ADDRESS 0x40  // Endereço I2C do escravo

void USART_init(unsigned int ubrr) {
    // Configura a taxa de transmissão
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    
    // Habilita transmissão
    UCSR0B = (1 << TXEN0);
    
    // Configura formato: 8 bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_send(char data) {
    while (!(UCSR0A & (1 << UDRE0))); // Espera buffer livre
    UDR0 = data;
}

void USART_send_string(const char* str) {
    while (*str) {
        USART_send(*str++);
    }
}

void send_number(uint16_t num) {
    char buffer[10];
    uint8_t i = 0;

    if (num == 0) {
        USART_send('0');
        return;
    }

    while (num > 0) {
        buffer[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i > 0) {
        USART_send(buffer[--i]);
    }
}

// Variável para armazenar o último comando recebido
volatile uint8_t receivedCommand = 0;

// Função chamada quando o mestre envia dados
void receiveEvent(int numBytes) {
  if (Wire.available()) {
    receivedCommand = Wire.read();  // Lê o comando enviado pelo mestre
    Serial.print("Comando recebido: 0x");
    Serial.println(receivedCommand, HEX);
  }
}

// Função chamada quando o mestre solicita dados
void requestEvent() {
  // Responde com um valor diferente dependendo do comando recebido
  switch (receivedCommand) {
    case 0x01:
      Wire.write(0x55);  // Responde com 0x55 se o comando foi 0x01
      break;
    case 0x02:
      Wire.write(0xAA);  // Responde com 0xAA se o comando foi 0x02
      break;
    default:
      Wire.write(0x00);  // Resposta padrão
      break;
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_ADDRESS);      // Inicia o I2C como escravo com o endereço 0x10
  Wire.onReceive(receiveEvent); // Configura a função de recebimento
  Wire.onRequest(requestEvent); // Configura a função de envio
  Serial.println("Escravo I2C inicializado!");
}

void loop() {
  // Nada é necessário aqui, pois as callbacks tratam a comunicação
}