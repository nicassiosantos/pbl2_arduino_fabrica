#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL
#define F_SCL 100000UL
#define PRESCALER 1
#define TWBR_VALUE ((F_CPU/F_SCL)-16)/(2*PRESCALER)

// Variáveis globais para o timer
volatile uint32_t timer0_millis = 0;

// Configuração USART
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
    char nibble;
    
    nibble = (num >> 4) & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
    
    nibble = num & 0x0F;
    USART_send(nibble > 9 ? (nibble - 10 + 'A') : (nibble + '0'));
}

// Configuração do Timer0 para millis()
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

// Interrupção do Timer0
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

void I2C_Init() {
    TWBR = (uint8_t)TWBR_VALUE;
    TWCR = (1 << TWEN);
    USART_init(103); // 9600 baud para 16MHz
    USART_send_string("Mestre I2C inicializado\r\n");
    delay_ms(1000);
}

bool I2C_Start() {
    PORTC |= (1 << PORTC4) | (1 << PORTC5); 
    USART_send_string("Enviando START condition\r\n");
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    
    uint32_t timeout = millis() + 100;
    while (!(TWCR & (1 << TWINT))) {
        if (millis() > timeout) {
            USART_send_string("Erro: Timeout no START\r\n");
            TWCR = 0;
            return false;
        }
    }
    
    uint8_t status = TWSR & 0xF8;
    if (status != 0x08) {
        USART_send_string("Erro no START. Status: 0x");
        send_hex(status);
        USART_send_string("\r\n");
        return false;
    }
    return true;
}

void I2C_Stop() {
    USART_send_string("Enviando STOP condition\r\n");
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

int main(void) {
    // Inicializações
    timer0_init();
    sei(); // Habilita interrupções globais
    I2C_Init();
    delay_ms(2000);

    while(1) {
        if (!I2C_Start()) {
            delay_ms(1000);
            continue;
        }
        
        uint8_t slave_address = 0x40 << 1;
        USART_send_string("Enviando endereco: 0x");
        send_hex(slave_address >> 1);
        USART_send_string("\r\n");
        
        TWDR = slave_address;
        TWCR = (1 << TWINT) | (1 << TWEN);
        
        uint32_t timeout = millis() + 100;
        while (!(TWCR & (1 << TWINT))) {
            if (millis() > timeout) {
                USART_send_string("Timeout no enderecamento\r\n");
                I2C_Stop();
                delay_ms(1000);
                continue;
            }
        }
        
        uint8_t status = TWSR & 0xF8;
        if (status != 0x18) {
            USART_send_string("Erro no enderecamento. Status: 0x");
            send_hex(status);
            USART_send_string("\r\n");
            I2C_Stop();
            delay_ms(1000);
            continue;
        }
        
        USART_send_string("Comunicacao estabelecida com sucesso!\r\n");
        I2C_Stop();
        delay_ms(2000);
    }
    
    return 0;
}