#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// === PINOS ===
const int L_PWM = 12;
const int R_PWM = 13;
const int PWM = 11;  // Ligar os pinos R_ENA e L_ENA no D11 do arduino
const int ledStatus = 10;
const int botaoPin = 2;             // Botão (INT0)
const int sensorPortaPin = 8;       
const int sensorPeFreioPin = 4;    
const int sensorCarroParadoPin = 3; // Carro parado = sinal pulsante

const int correntePin1 = A0; // Sensor da pont H
const int correntePin2 = A1; // Sensor da pont H

// === TEMPOS ===
const int TEMPO_50 = 1000;   // ms
const int TEMPO_100 = 4000; // ms
const int MEIO_SEGUNDO = 200; //ms

// === EEPROM ===
const int EEPROM_ADDR = 0;

// === ESTADOS ===
bool freiando = true;
bool freiado = false;
bool bloqueadoAcionamentoAutomatico = false;
long ultimoPiscar = 0;
int intensidadeUsada; 
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
  Serial.println("INICIADO.");

  attachInterrupt(digitalPinToInterrupt(botaoPin), wakeUp, FALLING);
  
  attachInterrupt(digitalPinToInterrupt(sensorCarroParadoPin), wakeUp, FALLING);
  
  freiado = EEPROM.read(EEPROM_ADDR) > 0;
  digitalWrite(ledStatus, freiado);
  entrarSleep();
  
}

void aguardarOuSoltarBotao(float tempoAgurde) {
    long tempoInicio = millis();
    Serial.print("aguarde  por:");
    Serial.print(tempoAgurde);
    Serial.println(" ou solte botao");
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (((millis() - tempoInicio) <= tempoAgurde)){
        if  (digitalRead(botaoPin)){
            break;
        }   
        piscarLed();
        float corrente  = lerCorrente(correntePin1,correntePin2);
    }
      
}
void piscarLed(){
     bool statusLed = digitalRead(ledStatus);
     if (millis() - ultimoPiscar >=150){
        ultimoPiscar = millis();
        digitalWrite(ledStatus, !statusLed);
     }
  
  }
void aguardar(float tempoAgurde) {
    long tempoInicio = millis();
    Serial.print("aguarde  por:");
    Serial.println(tempoAgurde);
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (((millis() - tempoInicio) <= tempoAgurde)){
        float corrente  = lerCorrente(correntePin1,correntePin2);
        piscarLed();
    }
      
}

void loop() {

  verificarAcionamentoManual();
  verificarAcionamentoAutomatico();

} 

// === ATIVA FREIO E ENTRA EM SLEEP ===
void freiar(int intensidade) {
 // if (freiado) return;

  Serial.print("Ativando freio com intensidade ");
  Serial.println(intensidade);

  iniciar(freiando);

  // Se intensidade for 100% ou 50%
  if (intensidade == 100)
    aguardarOuSoltarBotao(TEMPO_100);
  else
    aguardar(TEMPO_50);
  
  finalizarFreiar(intensidade);
  
}


// === ATIVA FREIO E ENTRA EM SLEEP ===
void iniciarFreiar() {
  if (freiado) return;
  
  Serial.print("Ativando freio com intesidade : 50 ");
  iniciar(freiando);
  aguardar(TEMPO_50);
 
}

void finalizarFreiar(int intensidade) {
  
  freiado = true;
  // Para e entra em Sleep
  parar();
  digitalWrite(ledStatus, true);
  EEPROM.write(EEPROM_ADDR, intensidade);
  entrarSleep(); 

}

// === LEITURA DO BOTÃO COM CLIQUE CURTO/LONGO ===
void verificarAcionamentoManual() {
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;
  peFreio = true;
  if (peFreio && botaoApertado()){
    long tempoInicio = millis();
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (botaoApertado()){
       if ((millis() - tempoInicio) >= MEIO_SEGUNDO){
            freiar(100);
            return;
       }  
    }
    if (freiado){
        desativarFreio();
        bloqueadoAcionamentoAutomatico = true;
    }else{
        iniciarFreiar(); 
        if (botaoNaoApertado())
            finalizarFreiar(50);
   
       }
  }
  /*
  if (peFreio && botaoApertado() && naoApertandoBotao){
     tempoPressionado = millis();
     naoApertandoBotao = false;
  }
  if (peFreio && botaoApertado()){
    long duracao = millis() - tempoPressionado;
    if (freiado){
      desativarFreio();
      bloqueadoAcionamentoAutomatico = true;
    }else{
      if (duracao <= MEIO_SEGUNDO){
        iniciarFreiar(); 
        if (botaoNaoApertado())
          finalizarFreiar();
        else
          freiar(100);
        
       }
   }
  }
  */
}
// === AÇÃO AUTOMÁTICA DE SEGURANÇA ===
void verificarAcionamentoAutomatico() {
 
  bool portaAberta = digitalRead(sensorPortaPin) == LOW;
  bool carroParado = detectarCarroParado();
  if (portaAberta && carroParado && !freiado && !bloqueadoAcionamentoAutomatico) {
     Serial.println("Ação automática de Segurança: Ativando freio!");
     freiar(50);
     // Entra em Sleep e só volta com click no botão
  }
// Permitir que o modo automatico volte a funcionar
  bloqueadoAcionamentoAutomatico = (!portaAberta && !carroParado && !freiado);
  
}

// === DETECÇÃO DE SINAL PULSANTE (CARRO PARADO) ===
bool detectarCarroParado() {
  unsigned long start = millis();
  bool ultimoEstado = HIGH;
  int pulsoCount = 0;

  while (millis() - start < 100) {
    bool estadoAtual = digitalRead(sensorCarroParadoPin);
    if (ultimoEstado == HIGH && estadoAtual == LOW) {
      pulsoCount++; // Contagem só na transição 1 -> 0
    } 
     ultimoEstado = estadoAtual;
    }
    return pulsoCount < 2; // Pelo menos 2 pulsos em 100ms
}

// === DESATIVA FREIO ===
void desativarFreio() {
  if (!freiado) return;

  iniciar(!freiando);
  Serial.println("Desativando freio...");

  aguardar(EEPROM.read(EEPROM_ADDR) == 100 ? TEMPO_100: TEMPO_50);
  freiado = false;

  digitalWrite(ledStatus, LOW);
  
  EEPROM.write(EEPROM_ADDR,0);
  
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
  
    digitalWrite(L_PWM, !sentido);
    digitalWrite(R_PWM, sentido);
    analogWrite(PWM, 255); 
}

void parar(){
    analogWrite(PWM, 0); 
}
bool botaoApertado(){
  return digitalRead(botaoPin) == LOW;   
}

bool botaoNaoApertado(){
  return digitalRead(botaoPin) == HIGH;   
}

// === MODO SLEEP PROFUNDO ===
void entrarSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  noInterrupts();
  EIFR = bit(INTF0); // Limpa flag INT0
  interrupts();

  sleep_cpu();
  // Ao acordar, volta de onde parou
  sleep_disable();
  //Serial.println("Acordou do modo sleep.");
}

// === INTERRUPÇÃO DE ACORDAR ===
void wakeUp() {
  // Nada a fazer — apenas acorda
}
