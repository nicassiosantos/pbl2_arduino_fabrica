#include <avr/io.h>
#include <avr/interrupt.h>

#define SLAVE_ADDRESS 0x40
#define F_CPU 16000000UL

// Protótipos das funções USART
void USART_init(unsigned int ubrr);
void USART_send(char data);
void USART_send_string(const char* str);
void send_hex(uint8_t num);

// Variáveis globais para o timer
volatile uint32_t timer0_millis = 0;

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
            uint8_t data = TWDR;
            USART_send_string("Dado recebido: 0x");
            send_hex(data);
            USART_send_string("\r\n");
            
            // Controle do LED (PB5 - pino 13 no Arduino)
            if(data == 0x55) {
                PORTB |= (1 << PB5);
                USART_send_string("LED LIGADO\r\n");
            } else if(data == 0xAA) {
                PORTB &= ~(1 << PB5);
                USART_send_string("LED DESLIGADO\r\n");
            }
            
            TWCR |= (1 << TWEA);
            break;
        }
            
        case 0xA0:  // TW_SR_STOP: STOP recebido
            USART_send_string("STOP recebido - comunicacao concluida\r\n");
            TWCR |= (1 << TWEA);
            break;
            
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

void send_hex(uint8_t num) {
    uint8_t nibble;
    
    // Nibble superior
    nibble = (num >> 4) & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
    
    // Nibble inferior
    nibble = num & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
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