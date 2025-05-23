#include <Wire.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define I2C_ADDRESS 0x40  // Endereço do escravo
#define BUTTON_PIN 12  // Pino D12 (corresponde ao bit 4 do PORTB)
#define BUTTON_MASK (1 << 4) // Máscara para o bit 4 do PORTB
#define INTERRUPT_PIN PCINT4
#define SER    PD2 // D2 -- DATA (DS)
#define RCLK   PD3 // D3 -- CLOCK (SH_CP)
#define SRCLK   PD4 // D4 -- LATCH (ST_CP)

// Variáveis globais para os sensores
int presenca = 0; 
int temperatura = 0; 
int inclinacao = 0; 
int nivel = 0;
char msg[100];  

int status = 1;
float v1 = 0;
float v2 = 0;
float v1_ant = 0;
float v2_ant = 0;
int blocos = 0;
int control_vel1 = 0;
int control_vel2 = 0;
float tempo_atual = 0;
float tempo_ant1 = 0;
float tempo_ant2 = 0;
float rotacoes1 = 0;
float rotacoes2 = 0;
float centimetros1 = 0;
float centimetros2 = 0; 

volatile unsigned long millis_timer1 = 0;
volatile unsigned long lastInterruptTime = 0;
volatile bool buttonPressed = false;
int debounceTime = 50; // tempo de debounce em ms

String receivedString = "";  // Armazena a string recebida
String responseString = "";  // Resposta padrão

float pw1 = 0; 
float pw2 = 0;

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
  //USART_send_string("Recebido: ");
  //USART_send_string(resp);
  //USART_send_string("\n");

  // Define a resposta baseada no comando
  if (resp[0] == 'P' && resp[1] == '1') {
    responseString = "RECEBIDO:V1";
    pw1 = atoi(&resp[3]); 
    //send_number(pw1); 
    //USART_send_string(" ");
  } else if(resp[0] == 'P' && resp[1] == '2'){
    responseString = "RECEBIDO:V2";
    pw2 = atoi(&resp[3]); 
    //send_number(pw2); 
    //USART_send_string("\n");
  }else if(resp[0] == 'D'){
    responseString = String(temperatura) + ";" + String(inclinacao) + ";" + String(presenca) + ";" + 
    String(nivel) + ";" + String(status) + ";"+ String(v1) + ";"+ String(v2) + ";"+ String(blocos) + ";";
    //USART_send_string(responseString.c_str());
  }else if(resp[0] == 'P'){
    status = 0;
    responseString = "Parou";
  }else if(resp[0] == 'C'){
    status = 1;
    responseString = "Continua";
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

void acendeLED_R() {
  DDRB |= (1 << DDB1);
  PORTB |= (1 << PORTB1);
}

void acendeLED_G() {
  DDRB |= (1 << DDB0);
  PORTB |= (1 << PORTB0);
}

void apagaLED_R() {
  PORTB &= ~(1 << PORTB1); // Desliga o LED no pino D9
}

void apagaLED_G() {
  PORTB &= ~(1 << PORTB0); // Desliga o LED no pino D8
}

void acionaBuzzer() {
  DDRB |= (1 << DDB2);    // Configura D10 (PORTB2) como saída
  PORTB |= (1 << PORTB2); // Nível alto → buzzer ligado
}

void desligaBuzzer() {
  PORTB &= ~(1 << PORTB2); // Nível baixo → buzzer desligado
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

void definePWM_D5() { // vertical 10
  OCR0B = pw1; // D5
  // 255 200
  // pw1   x
  v1 = (pw1 * 5000) / 255;
  if(v1 == v1_ant){
    float temp = tempo_atual - tempo_ant1; 
    rotacoes1 = temp*(v1/60000);
    centimetros1 += rotacoes1/20;
    tempo_ant1 = tempo_atual;
  }else{ 
    float temp = tempo_atual - tempo_ant1; 
    rotacoes1 = temp*(v1_ant/60000);
    centimetros1 += rotacoes1/20;
    v1_ant = v1;
    tempo_ant1 = tempo_atual;
  }
}

void definePWM_D6() { // horizontal 25
  OCR0A = pw2; // D6
  v2 = (pw2 * 5000) / 255;
  if(v2 == v2_ant){
    float temp = tempo_atual - tempo_ant2; 
    rotacoes2 = temp*(v2/60000);
    centimetros2 += rotacoes2/20;
    tempo_ant2 = tempo_atual;
  }else{ 
    float temp = tempo_atual - tempo_ant2; 
    rotacoes2 = temp*(v2_ant/60000);
    centimetros2 += rotacoes2/20;
    v2_ant = v2;
    tempo_ant2 = tempo_atual;
  }
  
}

void contar_bloco(){ 
  if(centimetros1 >= 10 && centimetros2 >= 25){
    blocos +=1;
    centimetros1 = 0; 
    centimetros2 = 0;
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

void setup_botao(){
  // Configura o pino D12 como entrada com pull-up
  DDRB &= ~BUTTON_MASK;    // Configura como entrada (clear no bit)
  //PORTB |= BUTTON_MASK;    // Ativa pull-up (set no bit)
  
  // Configura a interrupção por mudança de pino (PCINT)
  PCICR |= (1 << PCIE0);    // Habilita o banco de interrupção PCINT0 (pinos D8-D13)
  PCMSK0 |= (1 << PCINT4);  // Habilita interrupção específica para o pino D12 (PCINT4)
}

ISR(TIMER1_COMPA_vect) {
  millis_timer1++;
  tempo_atual = millis_timer1;
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
        status = 1;
        USART_send_string("Parada desfeita.");
        Serial.println(status);
      }else {
        buttonPressed = true;
        status = 0;
        Serial.println("Parada realizada com sucesso!.");
        Serial.println(status);
      }
      
      
    } else {
      //Serial.println("Botão liberado (ISR)");
      //Serial.println(buttonPressed);
    }
  }
  lastInterruptTime = currentTime;
}


void setup() {
  //USART_init(103);
  Serial.begin(9600);
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  USART_send_string("Escravo I2C pronto (com terminador)! \n");
  setupPWM_D5_D6();
  contador_mili();
  setup_botao();
  sei();
}

void loop() {
    if(status){
        sensor_presenca();
        sensor_temperatura();
        sensor_inclinacao();
        sensor_nivel();
        definePWM_D5();
        definePWM_D6();
        contar_bloco();
    }
  
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
  delay(200);
}