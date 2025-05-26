#include <Wire.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define BUTTON_PIN 12          // Pino D12 (corresponde ao bit 4 do PORTB)
#define BUTTON_MASK (1 << 4)   // Máscara para o bit 4 do PORTB
#define INTERRUPT_PIN PCINT4   // PCINT4 corresponde ao pino D12

volatile unsigned long millis_timer1 = 0;
volatile unsigned long lastInterruptTime = 0;
volatile bool buttonPressed = false;
int debounceTime = 50; // tempo de debounce em ms
float tempo_ant = 0;

int enviou = 0; 
int parada = 1;
int status = 0; 
int mudou = 0;

// variaveis glovais para controle do motor
int pwm_d5 = 0;
int pwm_d6 = 0;

float temperatura = 0;
float inclinacao = 0;
int presenca = 0;
int nivel = 0;
int statusF;
int v1 = 0;
int v2 = 0;
int blocos = 0;

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
  char* token;
  int campo = 0;
  token = strtok(resp, ";");
    
  while (token != NULL) {
        switch (campo) {
            case 0:
                temperatura = atoi(token);
                if(temperatura < 10 || temperatura > 40){
                    mudou = 1;
                }
                break;
            case 1:
                inclinacao = atoi(token);
                if(inclinacao){
                    mudou = 1;
                }
                break;
            case 2:
                presenca = atoi(token);
                break;
            case 3:
                nivel = atoi(token);
                break;
            case 4:
                statusF = atoi(token);
                break;
            case 5:
                v1 = atof(token);
                break;
            case 6:
                v2 = atof(token);
                break;
            case 7:
                blocos = atoi(token);
                break;
        }

        token = strtok(NULL, ";");
        campo++;
    }
}

bool continua_producao(){
  // Envia a string para o escravo
  char msg;
  msg = 'C'; 

    
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
  USART_send_string("Resposta do escravo continuar: ");
  USART_send_string(resp);
  USART_send_string("\n");

  if( resp[0] == 'C'){
    return true;
  }else{
    return false;
  }
}

bool pedir_parada(){
  // Envia a string para o escravo
  char msg;
  msg = 'P'; 

  Wire.beginTransmission(0x40);
  Wire.write(msg);
  Wire.endTransmission();
  USART_send_string("Parada i2C: \n");

  // Solicita resposta (lê até 32 bytes ou encontrar '\n')
  Wire.requestFrom(0x40, 32);
  String response = "";
  while (Wire.available()) {
    char c = Wire.read();
    if (c == '\n') break;  // Para ao encontrar o terminador
    response += c;
  }
  const char* resp = response.c_str();
  USART_send_string("Resposta do escravo parada: ");
  USART_send_string(resp);
  USART_send_string("\n");
  if( resp[0] == 'P'){
    return true;
  }else{
    return false;
  }
}

void setup_botao(){
  // Configura o pino D12 como entrada com pull-up
  DDRB &= ~BUTTON_MASK;    // Configura como entrada (clear no bit)
  //PORTB |= BUTTON_MASK;    // Ativa pull-up (set no bit)
  
  // Configura a interrupção por mudança de pino (PCINT)
  PCICR |= (1 << PCIE0);    // Habilita o banco de interrupção PCINT0 (pinos D8-D13)
  PCMSK0 |= (1 << PCINT4);  // Habilita interrupção específica para o pino D12 (PCINT4)
}

// Rotina de serviço de interrupção para PCINT0 (pinos D8-D13)
ISR(PCINT0_vect) {
  unsigned long currentTime = millis_timer1;

  // Verifica debounce
  if (currentTime - lastInterruptTime > debounceTime) {
    // Lê o estado atual do botão diretamente do registrador
    bool currentState = (PINB & BUTTON_MASK) ? false : true; // Inverte a lógica pull-up
    
    if (!currentState) {
      if(buttonPressed){
        buttonPressed = false;
        parada = 0;
        USART_send_string("Parada desfeita");
      }else {
        buttonPressed = true;
        parada = 1;
        Serial.println("Parada solicitada");
      }
      
      
    } else {
      //Serial.println("Botão liberado (ISR)");
      //Serial.println(buttonPressed);
    }
  }
  lastInterruptTime = currentTime;
}

ISR(TIMER1_COMPA_vect) {
  millis_timer1++;
}

void controle_parada(){
    if(!buttonPressed && (parada == 0)){
        parada = 1; 
        if(continua_producao()){
            USART_send_string("Parada desfeita com sucesso!\n");
            status = 1;
        }else{
            USART_send_string("Nao foi possivel continuar a producao\n");
        }
    }else if (buttonPressed && (parada == 1)){
        parada = 0;
        if(pedir_parada()){
            USART_send_string("Parada realizada com sucesso!\n");
            status = 0;
        }else{
            USART_send_string("Nao foi possivel parar a producao\n");
        }
        
    }
}

void contador_mili(){
  // Configura Timer1 para CTC (Clear Timer on Compare Match)
  TCCR1A = 0;              // Normal operation
  TCCR1B = 0;

  // Modo CTC: WGM12 = 1
  TCCR1B |= (1 << WGM12);

  // Prescaler = 64
  TCCR1B |= (1 << CS11) | (1 << CS10);

  // Valor de comparação para 1 ms:
  // 16 MHz / 64 = 250.000 ticks por segundo
  // 250.000 ticks / 1000 = 250 ticks por milissegundo
  OCR1A = 250 - 1;

  // Habilita interrupção por comparação
  TIMSK1 |= (1 << OCIE1A);
}


void temp_ant(){
    char msg[500];
    if(millis_timer1 - tempo_ant > 3000){
        if(temperatura < 10 || temperatura > 40 && mudou){
            USART_send_string("TEMPERATURA CRÍTICA!\n");
            mudou = 0;
        }
        else if(inclinacao && mudou){
            USART_send_string("MADEIRA FORA DO EIXO!\n");
            mudou = 0;
        }
        USART_send_string("Status da Fabrica: \n");
        USART_send_string("Temperatura: ");
        send_number(temperatura);
        USART_send_string(" Inclinacao: ");
        send_number(inclinacao);
        USART_send_string(" Presenca: ");
        send_number(presenca);
        sprintf(msg, "\nNível: %d Status: %d V1: %d V2: %d Blocos: %d\n", nivel, statusF, v1, v2, blocos);
        USART_send_string(msg);
        tempo_ant = millis_timer1;
    } 
}

void setup() {
  Wire.begin();  // Inicia como mestre
  tempo_ant = millis_timer1;
  //Serial.begin(9600);
  contador_mili();
  setup_botao();
  USART_init(103);
  USART_send_string("Mestre I2C iniciado.\n");
}

void loop() {
    enviar_fabrica_Velocidade();
    pedir_dados();
    controle_velocidade_motorD5();
    controle_velocidade_motorD6();
    controle_parada();
    temp_ant();
    delay(500);
}