#include <avr/io.h>
#include <avr/interrupt.h>

#define SLAVE_ADDRESS 0x40
#define F_CPU 16000000UL

#define SER    PD2 // D2
#define RCLK   PD3 // D3
#define SRCLK   PD4 // D4
// Pinos de controle dos dígitos (comum dos displays)
#define DIG1  PD2
#define DIG2  PD3

#define MAX_BUFFER_SIZE 64
volatile char i2c_buffer[MAX_BUFFER_SIZE];
volatile uint8_t buffer_index = 0;

// Protótipos das funções USART
void USART_init(unsigned int ubrr);
void USART_send(char data);
void USART_send_string(const char* str);
void send_hex(uint8_t num);

// Variáveis globais para o timer
volatile uint32_t timer0_millis = 0;

// Variáveis globais para os sensores
int presenca = 0; 
int temperatura = 0; 
int inclinacao = 0; 
int nivel = 0;

// Inicialização do Timer0 para millis()
void timer0_init() {
    // Modo CTC (Clear Timer on Compare Match)
    TCCR0A = (1 << WGM01);
    // Prescaler de 64 e inicia o timer
    TCCR0B = (1 << CS01) | (1 << CS00);
    // Valor para comparação (1ms com 16MHz e prescaler 64)
    OCR0A = 249;
    // Habilita interrupção por comparação
    TIMSK0 = (1 << OCIE0A);
}

// Rotina de serviço de interrupção do Timer0
ISR(TIMER0_COMPA_vect) {
    timer0_millis++;
}

// Função millis() usando o timer
uint32_t millis() {
    uint32_t m;
    cli();
    m = timer0_millis;
    sei();
    return m;
}

// Função delay usando o timer
void delay_ms(uint32_t ms) {
    uint32_t start = millis();
    while ((millis() - start) < ms);
}

// Inicialização do I2C no modo escravo
void I2C_Init() {
    // Habilita pull-ups para SDA (PC4) e SCL (PC5)
    PORTC |= (1 << PORTC4) | (1 << PORTC5);
    
    // Configura endereço do escravo (7 bits + bit RW)
    TWAR = (SLAVE_ADDRESS << 1);
    
    // Ativa interface I2C, ACK e interrupção
    TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
    
    // Inicializa USART para debug
    USART_init(103); // 9600 baud para 16MHz
    
    // Mensagem de inicialização
    USART_send_string("Escravo I2C inicializado\r\n");
    USART_send_string("Endereco: 0x");
    send_hex(SLAVE_ADDRESS);
    USART_send_string("\r\n");
}

// Rotina de serviço de interrupção do I2C
ISR(TWI_vect) {
    uint8_t status = TWSR & 0xF8;
    
    switch(status) {
        case 0x60:  // TW_SR_SLA_ACK: Endereço reconhecido
            USART_send_string("Endereco reconhecido - modo receptor\r\n");
            TWCR |= (1 << TWEA);
            break;
            
        case 0x80:  // TW_SR_DATA_ACK: Dado recebido
        {
          char data = TWDR;
          USART_send_string("Dado recebido: ");
          send_number(data);
          USART_send_string("\r\n");

          // Armazena no buffer se ainda houver espaço
          if (buffer_index < MAX_BUFFER_SIZE - 1) {
              i2c_buffer[buffer_index++] = data;
          }

          TWCR |= (1 << TWEA); // Continua aceitando dados
          break;
        }
            
        case 0xA0:  // TW_SR_STOP: STOP recebido
        {
          i2c_buffer[buffer_index] = '\0'; // Finaliza a string

          USART_send_string("String recebida: ");
          USART_send_string((char*)i2c_buffer);
          USART_send_string("\r\n");

          buffer_index = 0; // Reinicia índice do buffer
          TWCR |= (1 << TWEA);
          break;
        }
            
        default:
            USART_send_string("Estado desconhecido: 0x");
            send_hex(status);
            USART_send_string("\r\n");
            TWCR |= (1 << TWEA);
            break;
    }
}

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

void send_hex(uint8_t num) {
    uint8_t nibble;
    
    // Nibble superior
    nibble = (num >> 4) & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
    
    // Nibble inferior
    nibble = num & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
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
const uint8_t numeros[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

void shiftOut_74HC595(uint8_t valor) {
    for (int8_t i = 7; i >= 0; i--) {
        // Seta o bit no pino SER
        if (valor & (1 << i))
            PORTB |= (1 << SER);
        else
            PORTB &= ~(1 << SER);

        // Pulso no clock (SRCLK)
        PORTB |= (1 << SRCLK);
        PORTB &= ~(1 << SRCLK);
    }

    // Pulso no latch (RCLK) para atualizar as saídas
    PORTB |= (1 << RCLK);
    PORTB &= ~(1 << RCLK);
}

void setupPWM_D5_D6() {
  DDRD |= (1 << PD5) | (1 << PD6); // D5 e D6 como saída

  // Configura Fast PWM para os dois canais A e B (D6 e D5)
  TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
  TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler = 64
}

void definePWM_D5(uint8_t valor) {
  OCR0B = valor; // D5
}

void definePWM_D6(uint8_t valor) {
  OCR0A = valor; // D6
}

void setup() {
  // Pinos do 74HC595 como saída
  DDRB |= (1 << SER) | (1 << SRCLK) | (1 << RCLK);
  // Pinos dos dígitos como saída
  DDRD |= (1 << DIG1) | (1 << DIG2);
  setupPWM_D5_D6();               // Inicializa o PWM no pino D3
  definePWM_D5(200);
  definePWM_D6(200);
}

int main(void) {
    // Configura pino do LED (PB5) como saída
    DDRB |= (1 << PB5);
    
    // Inicializa timer, I2C e USART
    timer0_init();
    I2C_Init();
    sei(); // Habilita interrupções globais
    
    while(1) {
        // O trabalho principal é feito na interrupção
        // Podemos adicionar outras tarefas aqui se necessário
        delay_ms(100);
    }
    
    return 0;
}