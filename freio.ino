#include <EEPROM.h>
#include <SoftwareSerial.h>
#define PIN_NAO_USADO 9

// === PINOS ===
const int botaoPin = 2;             // Botão (INT0)
const int sensorPeFreioPin = 4;    
const int sensorPortaPin = 8;       
const int ledStatus = 10;
SoftwareSerial receptorStatusCarro(A4,PIN_NAO_USADO); // RX A4;

const int PWM = 11;  // Ligar os pinos R_ENA e L_ENA no D11 do arduino
const int L_PWM = 12;
const int R_PWM = 13;
const int sinalPainelStatus = 9;

// === TEMPOS ===
const int TEMPO_50 = 1100;   // ms
const int TEMPO_100 = 2000;  // ms
const int QUARTO_SEGUNDO = 250; //ms
long ultimoPiscar = 0;

// === EEPROM ===
const int EEPROM_ADDR = 0;

// === ESTADOS ===
//bool freiando = true;
bool SENTIDO_FREIANDO = true;

bool freiado = false;
uint8_t PARADO_EM_NEUTRO = 1;
uint8_t PARADO_ENGRENADO = 2;
uint8_t EM_MOVIMENTO = 3; // a partir de 10km/h

bool bloqueadoAcionamentoAutomatico = false;

void setup() {
  pinMode(sensorPortaPin, INPUT_PULLUP);
  pinMode(sensorPeFreioPin, INPUT_PULLUP);
  delay(500);
  for(int i=0;i<50;i++){
     digitalRead(sensorPeFreioPin);
     delay(1);
  }
  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(PWM, OUTPUT);
  pinMode(ledStatus, OUTPUT);
  pinMode(sinalPainelStatus, OUTPUT);
  pinMode(botaoPin, INPUT_PULLUP);
 
  //Serial.begin(9600);
  receptorStatusCarro.begin(9600);
  Serial.println("INICIADO.");
  
  freiado = EEPROM.read(EEPROM_ADDR) > 0;
  digitalWrite(ledStatus, freiado);
  digitalWrite(sinalPainelStatus, !freiado);
    
}

void aguardarOuSoltarBotao(float tempoAgurde) {
    long tempoInicio = millis();
    Serial.print("aguarde  por:");
    Serial.print(tempoAgurde);
    Serial.println(" ou solte botao");
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (((millis() - tempoInicio) <= tempoAgurde)){
        if  (botaoNaoApertado()){
            break;
        }   
        piscarLed();
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
    // Se ultrapassar o tempoAguarde
    while (((millis() - tempoInicio) <= tempoAgurde)){
        piscarLed();
    }
      
}

void loop() {
  uint8_t statusCarro = detectarStatusCarro();
  verificarAcionamentoManual(statusCarro);
  verificarAcionamentoAutomatico(statusCarro);

} 

// === ATIVA FREIO ===
void freiar(int intensidade) {
 
  Serial.print("Ativando freio com intensidade ");
  Serial.println(intensidade);
  digitalWrite(sinalPainelStatus, LOW);
  iniciar(SENTIDO_FREIANDO);
  // Se intensidade for 100% ou 50%
  if (intensidade == 100)
    aguardarOuSoltarBotao(TEMPO_100); // eenquanto tiver apertando, o freio é acionado
  else
    aguardar(TEMPO_50);
  
  finalizarFreiar(intensidade);
  
}


// === ATIVA FREIO  ===
void iniciarFreiar() {
  if (freiado) return;
  digitalWrite(sinalPainelStatus, LOW);
  Serial.print("Ativando freio com intesidade : 50 ");
  iniciar(SENTIDO_FREIANDO);
  aguardar(TEMPO_50);
 
}

void finalizarFreiar(int intensidade) {

  EEPROM.write(EEPROM_ADDR,intensidade);
  freiado = true;
  parar();
  digitalWrite(ledStatus, true);

}

// === LEITURA DO BOTÃO COM CLIQUE CURTO/LONGO ===
void verificarAcionamentoManual(uint8_t statusCarro) {
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;
  if (botaoApertado()){
    long tempoInicio = millis();
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (botaoApertado()){
       if ((millis() - tempoInicio) >= QUARTO_SEGUNDO){
            if (peFreio){
               bloqueadoAcionamentoAutomatico = true;
               freiar(100);
            }
           return;
       }  
    }
    if (freiado){ // FREIO ACIONADO
      if (statusCarro == PARADO_ENGRENADO){ // Se estiver engrenado, com freio acionado, só clicar no botão
           bloqueadoAcionamentoAutomatico = true;
           desativarFreio();
       }else{
            if (peFreio){ // Se estiver no N ou em movimento, com freio acionado, necessário pisar no freio e clicar no botão
              bloqueadoAcionamentoAutomatico = true;
              desativarFreio();
            }
       }
    }else{// FREIO NAO ACIONADO
      if (isParado(statusCarro) && peFreio){ // Só aciona freio 50% se estiver PARADO e com pé no  freio. 
        bloqueadoAcionamentoAutomatico = true;
        iniciarFreiar(); 
        if (botaoNaoApertado())
            finalizarFreiar(50);
       }
    }
  }
}
// === AÇÃO AUTOMÁTICA DE SEGURANÇA ===
void verificarAcionamentoAutomatico(uint8_t statusCarro) { 
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;
  bool portaAberta = digitalRead(sensorPortaPin) == LOW;
  bool portaFechada = digitalRead(sensorPortaPin) == HIGH;
  
 
  if (portaAberta && !peFreio && statusCarro== PARADO_ENGRENADO && !freiado && !bloqueadoAcionamentoAutomatico) { // CARRO PARADO EM DRIVE
     Serial.println("Ação automática de Segurança: Porta Aberta, Carro parado em Drive");
     freiar(50);
     // Entra em Sleep e só volta com click no botão
  }
  if (statusCarro == PARADO_EM_NEUTRO && !freiado && peFreio && !bloqueadoAcionamentoAutomatico) { // CARRO PARADO EM N
     Serial.println("Ação automática de Segurança: Carro parado em N, pé no freio");
     freiar(50);
     // Entra em Sleep e só volta com click no botão
  }
  if (statusCarro == PARADO_ENGRENADO && portaFechada && freiado && peFreio && !bloqueadoAcionamentoAutomatico) { // CARRO PARADO EM DRIVE
     Serial.println("Ação automática de Segurança: Carro parado em Drive, freiado, pé no freio");
     desativarFreio();
  }


// Permitir que o modo automatico volte a funcionar
  if (statusCarro == EM_MOVIMENTO &&  portaFechada && !freiado){
     bloqueadoAcionamentoAutomatico = false;
     Serial.println("Ação automática de Segurança DESBLOQUEADA");
  }
}

// === DETECÇÃO DE CARRO PARADO N e CARRO PARADO ENGATADO) ===
uint8_t detectarStatusCarro() {
  static uint8_t statusCarro =0;
  if (receptorStatusCarro.available()){
      statusCarro = receptorStatusCarro.read();
      Serial.print("Status CArro:");
      Serial.println(statusCarro);
  
    }
  return statusCarro;
}

bool isParado(uint8_t statusCarro){
    return statusCarro != EM_MOVIMENTO; 
}
// === DESATIVA FREIO ===
void desativarFreio() {
  if (!freiado) return;
  
  iniciar(!SENTIDO_FREIANDO);
  Serial.println("Desativando freio...");
  aguardar(EEPROM.read(EEPROM_ADDR) == 100 ? TEMPO_100: TEMPO_50);
  freiado = false;
  digitalWrite(ledStatus, LOW);
  digitalWrite(sinalPainelStatus, HIGH);
  EEPROM.write(EEPROM_ADDR,0);
  
  parar();
}

float leituraMediaAnologica(int pinAnalogico){
  long soma=0;
   for(int i=1;i<=10;i++){
       soma = soma + analogRead(pinAnalogico);
     //  delay(5);
    }
  return soma/10;
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
