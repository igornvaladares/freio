#include <EEPROM.h>
// === PINOS ===
const int L_PWM = 12;
const int R_PWM = 13;
const int PWM = 11;  // Ligar os pinos R_ENA e L_ENA no D11 do arduino
const int sinalPainelStatus = 9;

const int ledStatus = 10;
const int botaoPin = 2;             // Botão (INT0)
const int sensorPortaPin = 8;       
const int sensorPeFreioPin = 4;    
const int sensorCarroParadoPin = 3; // Carro parado = sinal pulsante

const int correntePin1 = A0; // Sensor da pont H
const int correntePin2 = A1; // Sensor da pont H
const int entradaStatusCarro = A4; // Dados vindo do arquino de cambio

// === TEMPOS ===
const int TEMPO_50 = 1000;   // ms
const int TEMPO_100 = 2000;  // ms
const int QUARTO_SEGUNDO = 250; //ms
long ultimoPiscar = 0;

// === EEPROM ===
const int EEPROM_ADDR = 0;

// === ESTADOS ===
//bool freiando = true;
bool SENTIDO_FREIANDO = true;

bool freiado = false;
int PARADO_EM_NEUTRO = 1;
int PARADO_ENGRENADO = 2;
int EM_MOVIMENTO = 3; // a partir de 10km/h

bool bloqueadoAcionamentoAutomatico = false;

void setup() {
  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(PWM, OUTPUT);
  
  pinMode(ledStatus, OUTPUT);
  pinMode(sinalPainelStatus, OUTPUT);

  pinMode(botaoPin, INPUT_PULLUP);
  pinMode(sensorPortaPin, INPUT_PULLUP);
  pinMode(sensorPeFreioPin, INPUT_PULLUP);
  pinMode(sensorCarroParadoPin, INPUT_PULLUP);

  Serial.begin(9600);
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
  int statusCarro = detectarStatusCarro();
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
void verificarAcionamentoManual(int statusCarro) {
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;
  if (peFreio && botaoApertado()){
    long tempoInicio = millis();
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (botaoApertado()){
       if ((millis() - tempoInicio) >= QUARTO_SEGUNDO){
            freiar(100);
            return;
       }  
    }
    if (freiado){
        desativarFreio(true);
    }else{
      if (statusCarro!= EM_MOVIMENTO){ // Bloqueia o click simples com carro em movimento. 
        iniciarFreiar(); 
        if (botaoNaoApertado())
            finalizarFreiar(50);
   
       }
    }
  }
}
// === AÇÃO AUTOMÁTICA DE SEGURANÇA ===
void verificarAcionamentoAutomatico(int statusCarro) { 
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;
  bool portaAberta = digitalRead(sensorPortaPin) == LOW;
  bool portaFechada = digitalRead(sensorPortaPin) == HIGH;
  
  Serial.print("Status CArro:");
  Serial.println(statusCarro);
  
  if (portaAberta && !peFreio && statusCarro== PARADO_ENGRENADO && !freiado && !bloqueadoAcionamentoAutomatico) { // CARRO PARADO EM DRIVE
     Serial.println("Ação automática de Segurança: Porta Aberta, Carro parado em Drive");
     freiar(50);
     // Entra em Sleep e só volta com click no botão
  }
  if (statusCarro == PARADO_EM_NEUTRO && !freiado && peFreio && !bloqueadoAcionamentoAutomatico) { // CARRO PARADO EM N
     Serial.println("Ação automática de Segurança: Carro parado em N!");
     freiar(50);
     // Entra em Sleep e só volta com click no botão
  }
  if (statusCarro == PARADO_ENGRENADO && portaFechada && freiado && peFreio && !bloqueadoAcionamentoAutomatico) { // CARRO PARADO EM DRIVE
     Serial.println("Ação automática de Segurança: Carro parado em Drive");
     desativarFreio(false);
  }


// Permitir que o modo automatico volte a funcionar
  if (statusCarro == EM_MOVIMENTO &&  portaFechada && !freiado)
     bloqueadoAcionamentoAutomatico = false;

}

// === DETECÇÃO DE CARRO PARADO N e CARRO PARADO ENGATADO) ===
signed char detectarStatusCarro() {
    int leituraStatusCarro = leituraMediaAnologica(entradaStatusCarro);
    int voltStatusCarro = map(leituraStatusCarro,0,1023,0,500);
    //Serial.print("Volts:");
    //Serial.println(leituraStatusCarro);
    signed char statusCarro =0;
    switch (voltStatusCarro) {
        case 100  ... 200: // valor exato é 150
          statusCarro = PARADO_EM_NEUTRO; // Carro Parado em N
        break;
        case 201 ... 300: // Valor é 250
          statusCarro = PARADO_ENGRENADO;// Carro Parado engatado
          break;
        case 301 ... 400:// Valor é 350
          statusCarro =EM_MOVIMENTO;// Carro em movimento >= 10km/h
        break;
    }
    return statusCarro;
}

// === DESATIVA FREIO ===
void desativarFreio(bool desativarAcionamentoAutomatico) {
  if (!freiado) return;
  bloqueadoAcionamentoAutomatico = desativarAcionamentoAutomatico;
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
   for(int i=1;i<=20;i++){
       soma = soma + analogRead(pinAnalogico);
    }
  return soma/20;
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
