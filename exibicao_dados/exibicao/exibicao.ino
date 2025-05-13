// Pinos do Arduino Nano (ATmega328P)
#define DS_PIN    PC1 // A1
#define STC_PIN   PC2 // A2
#define SHC_PIN   PC3 // A3

/**
 * @param ddr Ponteiro para registrador DDRx (direção)
 * @param port Ponteiro para registrador PORTx (saída)
 * @param bit Posição do bit no registrador (ex: PD2 → 2)
 */
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
  PORTC &= ~(1 << STC_PIN);

  // Envia os bits (MSB primeiro)
  for (int8_t i = 7; i >= 0; i--) {
    // Clock LOW
    PORTC &= ~(1 << SHC_PIN);

    // Define DS
    if (valor & (1 << i))
      PORTC |= (1 << DS_PIN);
    else
      PORTC &= ~(1 << DS_PIN);

    // Clock HIGH (dados são lidos na borda de subida)
    PORTC |= (1 << SHC_PIN);
  }

  // Latch HIGH (armazena os dados nos pinos Q)
  PORTC |= (1 << STC_PIN);
}

void setup() {
  // Configura os pinos A1, A2 e A3 como saída
  DDRC |= (1 << DS_PIN) | (1 << STC_PIN) | (1 << SHC_PIN);
  setupPWM();               // Inicializa o PWM no pino D3
  defineVelocidadeMotor(128); // 50% da velocidade máxima
}

void setupPWM() {
  DDRD |= (1 << PD5); // Configura PD5 (D5) como saída

  // Configura Timer0 para Fast PWM
  // - WGM01 e WGM00 = 1 (Fast PWM)
  // - COM0B1 = 1, COM0B0 = 0 → Clear OC0B on compare match (não inverte)
  // - Prescaler = 64 (CS01 = 1, CS00 = 1)
  TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
  TCCR0B = (1 << CS01) | (1 << CS00);
}

// Define o valor de PWM no pino D3 (0–255)
void defineVelocidadeMotor(uint8_t velocidade) {
  OCR0B = velocidade; // Define duty cycle (0–255)
}


void loop() {
  // Exemplo: acender o número "3" no display de 7 segmentos
  uint8_t numero = 3;
  uint8_t segmentos = mapa7Segmentos(numero);
  enviaPara595(segmentos);
  acendeLED(&DDRB, &PORTB, PB5);
  acionaBuzzer(&DDRD, &PORTD, PD2);
  //desligaBuzzer(&PORTD, PD2);
  delay(1000);
}
