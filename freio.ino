#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// === PINOS ===
const int L_PWM = 7;
const int R_PWM = 8;
const int PWM = 11;  // Ligar os pinos R_ENA e L_ENA no D11 do arduino
const int ledStatus = 13;
const int botaoPin = 2;             // Botão (INT0)
const int sensorPortaPin = 3;       // Porta aberta = HIGH
const int sensorPeFreioPin = 4;       // Cinto desatado = HIGH
const int sensorCarroParadoPin = 3; // Carro parado = sinal pulsante

const int correntePin1 = A0; // Carro parado = sinal pulsante

const int correntePin2 = A1; // Carro parado = sinal pulsante

// === TEMPOS ===
const int TEMPO_50 = 1000;   // ms
const int TEMPO_100 = 4000; // ms
const int UM_SEGUNDO = 1000; //ms

// === EEPROM ===
const int EEPROM_ADDR = 0;

// === ESTADOS ===
bool freiado = false;
bool acaoImediata = false;
bool bloqueadoAcionamentoAutomatico = false;
bool esperandoSoltar = false;
unsigned long tempoPressionado = 0;

void setup() {
  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(PWM, OUTPUT);
  
  pinMode(ledStatus, OUTPUT);
  pinMode(botaoPin, INPUT_PULLUP);
  pinMode(sensorPortaPin, INPUT_PULLUP);
  pinMode(sensorPeFreioPin, INPUT_PULLUP);
  pinMode(sensorCarroParadoPin, INPUT_PULLUP);

  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(botaoPin), wakeUp, LOW);

  freiado = EEPROM.read(EEPROM_ADDR) == 1;

  
}

void aguardar(float tempoAgurde, bool tempoIndeterminado= false) {
  

    Serial.print("aguarde  por:");
    Serial.println(tempoAgurde);
    int tempoInicio = millis();
    // Se ultrapassar o tempoAguarde ou botão foi acionado
    while (((millis() - tempoInicio) <= tempoAgurde)  ||  (tempoIndeterminado && digitalRead(botaoPin) == LOW )){
        float corrente  = lerCorrente(correntePin1,correntePin2);
    }
}

void loop() {
  verificarAcionamentoManual();
  verificarAcionamentoAutomatico();
}

// === ATIVA FREIO E ENTRA EM SLEEP ===
void freiar(int intensidade, bool tempoIndeterminado = false) {
  //if (freiado) return;

  Serial.print("Ativando freio com intensidade ");
  Serial.println(intensidade);

  iniciar(freiado);
  aguardar(intensidade == 100 ? TEMPO_100 : TEMPO_50,tempoIndeterminado);

  freiado = true;
  digitalWrite(ledStatus, HIGH);
  EEPROM.write(EEPROM_ADDR, 1);
  parar();
}

// === LEITURA DO BOTÃO COM CLIQUE CURTO/LONGO ===
void verificarAcionamentoManual() {
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;

  if (peFreio & digitalRead(botaoPin) == LOW && !acaoImediata) {
     bloqueadoAcionamentoAutomatico = false;
     tempoPressionado = millis();

    if (!freiado)
        acaoImediata = true;
       // Freia e entra em Stand By
       freiar(50);
      
    }
// Se manter pressionado o botão, acorda imediatamente
  if (peFreio && acaoImediata) {
    // Se clicou no botão e segurou por mais de 1 sedungo
    if (digitalRead(botaoPin) == LOW && millis() - tempoPressionado >= UM_SEGUNDO){
        //Freia até que solte o botão - parametro TRUE.
        // Depois entra em Stand By
        freiar(100, true);
    }
    if (digitalRead(botaoPin) == HIGH ){
    // Quando soltar o botão 
      if (freiado){
        desativarFreio();
        bloqueadoAcionamentoAutomatico = true;
        acaoImediata = false;
      }

    }
    
  }
}
// === AÇÃO AUTOMÁTICA DE SEGURANÇA ===
void verificarAcionamentoAutomatico() {
  bool portaAberta = digitalRead(sensorPortaPin) == LOW;
  bool carroParado = detectarCarroParado();

  if (portaAberta && carroParado && !freiado && !bloqueadoAcionamentoAutomatico) {
    Serial.println("Ação automática de Segurança: Ativando freio!");
    freiar(50);
  }
}

// === DETECÇÃO DE SINAL PULSANTE (CARRO PARADO) ===
bool detectarCarroParado() {
  unsigned long start = millis();
  int pulsoCount = 0;

  while (millis() - start < 100) {
    if (digitalRead(sensorCarroParadoPin) == LOW) {
      pulsoCount++;
      while (digitalRead(sensorCarroParadoPin) == LOW); // Aguarda cair
    }
  }

  return pulsoCount > 2; // Pelo menos 2 pulsos em 100ms
}

// === DESATIVA FREIO ===
void desativarFreio() {
  if (!freiado) return;

  iniciar(freiado);
  Serial.println("Desativando freio...");

  aguardar(TEMPO_100);
  freiado = false;
  digitalWrite(ledStatus, LOW);
  EEPROM.write(EEPROM_ADDR, 0);
  parar();
}

float leituraMediaAnologica(int pinAnalogico){

  long soma=0;
   for(int i=1;i<=10;i++){
       soma = soma + analogRead(pinAnalogico);
    }
  return soma/10;

}

float lerCorrente(int pinCorrente1,int pinCorrente2){

  float tensao = 0;
  float tensaoA = leituraMediaAnologica(pinCorrente1) * (5 /1023.0); // 5V máxima voltagem do pino que corresponde a 1023.0
  float tensaoB = leituraMediaAnologica(pinCorrente2) * (5 /1023.0); // 5V máxima voltagem do pino que corresponde a 1023.0
  
   tensao = max(tensaoA,tensaoB);
   return tensao / 0.1;

}

void iniciar(bool sentido){
  
    digitalWrite(L_PWM, sentido);
    digitalWrite(R_PWM, !sentido);
    analogWrite(PWM, 255); 
}

void parar(){
    analogWrite(PWM, 0); 
    entrarSleep(); 

}
// === MODO SLEEP PROFUNDO ===
void entrarSleep() {
  Serial.println("Entrando em modo sleep...");

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  noInterrupts();
  EIFR = bit(INTF0); // Limpa flag INT0
  interrupts();

  sleep_cpu();
  // Ao acordar, volta de onde parou
  sleep_disable();
  Serial.println("Acordou do modo sleep.");
}

// === INTERRUPÇÃO DE ACORDAR ===
void wakeUp() {
  // Nada a fazer — apenas acorda
}
