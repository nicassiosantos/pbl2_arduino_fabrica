#include <Arduino.h>
#include <avr/io.h> 
#include <util/delay.h> 

int presenca = 0; 
int temperatura = 0; 
int inclinacao = 0; 


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

uint16_t lerADC(int canal) {
    ADMUX = (1 << REFS0) | (canal & 0x07); // AVCC como VREF + canal.
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Habilita ADC + prescaler 128.
    ADCSRA |= (1 << ADSC); // Inicia conversão.
    while (!(ADCSRA & (1 << ADIF))); // Aguarda fim da conversão.
    ADCSRA |= (1 << ADIF); // Limpa a flag.
    return ADC; // Lê ADCL e ADCH (macro definida no avr/io.h).
}


void sensor_presenca(){
  int valorAnalogico; 
  valorAnalogico = lerADC(0); // entre 0 a 1023 
  if(valorAnalogico < 40){
    presenca = 1;
  }else{
    presenca = 0;
  }
}

void sensor_temperatura(){
  int valorAnalogico; 
  valorAnalogico = lerADC(1); // entre 0 a 1023

  // Converte o valor para tensão (0V a 5V)
  float tensao = (valorAnalogico * 5.0) / 1023.0;

  // Converte tensão para temperatura (LM35: 10mV/°C)
  temperatura = tensao * 100;
}

void sensor_inclinacao(){
  int valorAnalogico; 
  valorAnalogico = lerADC(2); // entre 0 a 1023 
  if(valorAnalogico < 500){
    inclinacao = 1;
  }else{
    inclinacao = 0;
  }
}

void setup() {
  
}

void loop() {
  sensor_presenca();
  sensor_temperatura();
  sensor_inclinacao();
  USART_init(103); // Baud rate 9600 com F_CPU 16MHz 

  USART_send_string("Presença: ");
  send_number(presenca);
  USART_send_string(" Temperatura: ");
  send_number(temperatura);
  USART_send_string(" Inclinacao: ");
  send_number(inclinacao);
  USART_send_string(" \r\n");

  delay(1000);
}
