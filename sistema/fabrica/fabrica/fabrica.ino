#include <Wire.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define I2C_ADDRESS 0x40  // Endereço do escravo

#define DS_PIN    PD2 // D2
#define STC_PIN   PD3 // D3
#define SHC_PIN   PD4 // D4

// Variáveis globais para os sensores
int presenca = 0; 
int temperatura = 0; 
int inclinacao = 0; 
int nivel = 0;

String receivedString = "";  // Armazena a string recebida
String responseString = "PONG";  // Resposta padrão

int pw1 = 0; 
int pw2 = 0;

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

void receiveEvent(int numBytes) {
  receivedString = "";
  while (Wire.available()) {
    char c = Wire.read();
    receivedString += c;  // Concatena cada byte recebido
  }
  const char* resp = receivedString.c_str();
  USART_send_string("Recebido: ");
  USART_send_string(resp);
  USART_send_string("\n");

  // Define a resposta baseada no comando
  if (resp[0] == 'P' && resp[1] == '1') {
    responseString = "RECEBIDO:V1";
    pw1 = atoi(&resp[3]); 
    send_number(pw1); 
    USART_send_string("\n");
  } else if(resp[0] == 'P' && resp[1] == '2'){
    responseString = "RECEBIDO:V2";
    pw2 = atoi(&resp[3]); 
    send_number(pw2); 
    USART_send_string("\n");
  }else{
    responseString = "COMANDO INVALIDO";
  }


}

void requestEvent() {
  Wire.write(responseString.c_str());  // Envia a string
  Wire.write('\n');  // Terminador (indica fim da mensagem)
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

void sensor_nivel(){
  int valorAnalogico; 
  valorAnalogico = lerADC(3); // entre 0 a 1023 
  if(valorAnalogico > 823){
    nivel = 2;
  }else if(valorAnalogico < 200){
    nivel = 0;
  }
  else{
    nivel = 1;
  }
}

void acendeLED(volatile uint8_t *ddr, volatile uint8_t *port, uint8_t bit) {
  *ddr |= (1 << bit);   // Configura como saída
  *port |= (1 << bit);  // Define como nível alto (acende LED)
}

void acionaBuzzer(volatile uint8_t *ddr, volatile uint8_t *port, uint8_t bit) {
  *ddr |= (1 << bit);   // Configura como saída
  *port |= (1 << bit);  // Nível alto → buzzer ligado
}

void desligaBuzzer(volatile uint8_t *port, uint8_t bit) {
  *port &= ~(1 << bit); // Nível baixo → buzzer desligado
}

// Mapa dos dígitos (nível baixo acende o segmento em ânodo comum)
uint8_t mapa7Segmentos(uint8_t digito) {
  //       pgfedcba  <- segmentos Q6 a Q0
  const uint8_t tabela[] = {
    0b11000000, // 0
    0b11111001, // 1
    0b10100100, // 2
    0b10110000, // 3
    0b10011001, // 4
    0b10010010, // 5
    0b10000010, // 6
    0b11111000, // 7
    0b10000000, // 8
    0b10010000  // 9
  };
  return tabela[digito % 10];
}

void enviaPara595(uint8_t valor) {
  // Latch LOW
  PORTD &= ~(1 << STC_PIN);

  // Envia os bits (MSB primeiro)
  for (int8_t i = 7; i >= 0; i--) {
    // Clock LOW
    PORTD &= ~(1 << SHC_PIN);

    // Define DS
    if (valor & (1 << i))
      PORTD |= (1 << DS_PIN);
    else
      PORTD &= ~(1 << DS_PIN);

    // Clock HIGH (dados são lidos na borda de subida)
    PORTD |= (1 << SHC_PIN);
  }

  // Latch HIGH (armazena os dados nos pinos Q)
  PORTD |= (1 << STC_PIN);
}

void setupPWM_D5_D6() {
  DDRD |= (1 << PD5) | (1 << PD6); // D5 e D6 como saída

  // Configura Fast PWM para os dois canais A e B (D6 e D5)
  TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
  TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler = 64
}

void definePWM_D5() {
  OCR0B = pw1; // D5
}

void definePWM_D6(uint8_t valor) {

  OCR0A = pw2; // D6
}

void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  USART_send_string("Escravo I2C pronto (com terminador)! \n");
  setupPWM_D5_D6();
}

void loop() {
  sensor_presenca();
  sensor_temperatura();
  sensor_inclinacao();
  sensor_nivel();

  /*
  USART_send_string("Presença: ");
  send_number(presenca);
  USART_send_string("Temperatura: ");
  send_number(temperatura);
  USART_send_string("Inclinação: ");
  send_number(inclinacao);
  USART_send_string("Nivel: ");
  send_number(nivel);
  USART_send_string("\n");
  */
  delay(500);
}