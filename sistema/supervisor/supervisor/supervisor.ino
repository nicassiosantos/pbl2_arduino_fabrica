#include <Wire.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// variaveis glovais para controle do motor
int pwm_d5 = 0;
int pwm_d6 = 0;
int enviou = 0; 

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

uint16_t lerADC(int canal) {
    ADMUX = (1 << REFS0) | (canal & 0x07); // AVCC como VREF + canal.
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Habilita ADC + prescaler 128.
    ADCSRA |= (1 << ADSC); // Inicia conversão.
    while (!(ADCSRA & (1 << ADIF))); // Aguarda fim da conversão.
    ADCSRA |= (1 << ADIF); // Limpa a flag.
    return ADC; // Lê ADCL e ADCH (macro definida no avr/io.h).
}

    // poten     pwm
    // 1023      255
    // 255 * poten = 1023 * pwm
    //pwm = (255 * poten) / 1023
void controle_velocidade_motorD5(){
    float valorAnalogico; 
    valorAnalogico = lerADC(0); // entre 0 a 1023
    pwm_d5 = 255 * valorAnalogico / 1023;
    //pwm_d5 = valorAnalogico;
}

void controle_velocidade_motorD6(){
    float valorAnalogico;
    valorAnalogico = lerADC(1); // entre 0 a 1023 
    pwm_d6 = 255 * valorAnalogico / 1023;
} 

void enviar_fabrica_Velocidade(){
  
  // Envia a string para o escravo
  char msg[50]; 
  if(enviou == 0){
    sprintf(msg, "P1:%d", pwm_d5);
    enviou = 1;
  }else if(enviou == 1){
    sprintf(msg, "P2:%d", pwm_d6);
    enviou = 0;
  }
  Wire.beginTransmission(0x40);
  Wire.write(msg);
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
  USART_send_string("\n");
}

void pedir_dados(){
  
  // Envia a string para o escravo
  char msg;
  msg = 'D';   

  Wire.beginTransmission(0x40);
  Wire.write(msg);
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
  USART_send_string("\n");


}

void setup() {
  Wire.begin();  // Inicia como mestre
  //Serial.begin(9600);
  USART_init(103);
  USART_send_string("Mestre I2C iniciado.\n");
}

void loop() {
  enviar_fabrica_Velocidade();
  pedir_dados();
  controle_velocidade_motorD5();
  controle_velocidade_motorD6();
  USART_send_string("PWM D5: ");
  send_number(pwm_d5);
  USART_send_string("PWM D6: ");
  send_number(pwm_d6);  
  USART_send_string("\n");
  delay(700);
}