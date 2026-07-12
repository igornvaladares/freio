//#include <Wire.h>
#include <EEPROM.h>
//#include <math.h>
#include <SoftwareSerial.h>

// Constantes de compilação (não ocupam RAM, não são variáveis globais)
//#define MPU_ADDR 0x68
//#define ADDR_OFFSET 2    // Float usa 4 bytes (endereços 2, 3, 4 e 5)
//#define ADDR_MAGIC 6     // Byte de controle (endereço 6)
//#define MAGIC_BYTE 0xA5  // Valor para validar se já foi calibrado
//#define LIMITE_LADEIRA 5.0 // Graus de inclinação para considerar ladeira

#define PIN_NAO_USADO 9

// === PINOS ===
const int botaoPin = 2;             // Botão (INT0)
const int sensorPeFreioPin = 4;    
const int sensorPortaPin = 8;       
const int ledStatus = 10;
const int beepStatus = 7;

SoftwareSerial receptorStatusCarro(A4,PIN_NAO_USADO); // RX A4;

const int PWM = 11;  // Ligar os pinos R_ENA e L_ENA no D11 do arduino
const int L_PWM = 12;
const int R_PWM = 13;
const int L_IS = A0;
const int R_IS = A1;
const float ganho = 8500.0;
const float resistor = 966.0; //(1kb) 
const int sinalPainelStatus = 9;

// === TEMPOS ===
const int TEMPO_MAXIMO_ACIONAMENTO = 2000;   // ms Click Curto
const int TEMPO_DESTRAVA = 750;   // ms Click Curto

const int CORRENTE_50 = 6;   // ms Click Curto
const int CORRENTE_100 = 12;  // ms Click Longo

const int QUARTO_SEGUNDO = 250; //ms
long ultimoPiscar = 0;
long ultimoBeep = 0;
bool beepando =false;


// === EEPROM ===
const uint8_t EEPROM_ADDR_FREIADO = 0;
const uint8_t EEPROM_ADDR_LIMITE_CORRENTE = 1;
// === ESTADOS ===
bool SENTIDO_FREIANDO = true;

bool freiado = false;
bool correnteNoLimite =false;

struct StatusCarro {
  uint8_t atual;
  uint8_t anterior;
};

uint8_t PARADO_EM_NEUTRO = 1;
uint8_t PARADO_ENGRENADO = 2;
uint8_t EM_MOVIMENTO = 3; // a partir de 3km/h
uint8_t ENGRENADO_PISANDO_ACELERADOR =4;

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
  pinMode(beepStatus, OUTPUT);
  pinMode(sinalPainelStatus, OUTPUT);
  pinMode(botaoPin, INPUT_PULLUP);
  pinMode(L_IS, INPUT);
  pinMode(R_IS, INPUT);

  Serial.begin(115200);
  receptorStatusCarro.begin(9600);
  Serial.println("INICIADO.");
  
  freiado = EEPROM.read(EEPROM_ADDR_FREIADO) > 0;
  correnteNoLimite= EEPROM.read(EEPROM_ADDR_LIMITE_CORRENTE) > 0;
 
  digitalWrite(ledStatus, freiado);
  digitalWrite(sinalPainelStatus, !freiado);

}


void loop() {
 // Serial.print("A0:");
 // Serial.println(analogRead(L_IS));
 // Serial.print("A1:");
 // Serial.println(analogRead(R_IS));
  
  StatusCarro statusCarro = detectarStatusCarro();
  
  verificarAcionamentoManual(statusCarro);
  verificarAcionamentoAutomatico(statusCarro);
  
  gerenciarNotificacao(statusCarro);

} 
// === CONDICÕES PARA BEEPAR ===
void gerenciarNotificacao(StatusCarro statusCarro){
  if (freiado){
    if (statusCarro.atual == ENGRENADO_PISANDO_ACELERADOR || statusCarro.atual == EM_MOVIMENTO ){
         piscarLed();
         beeparERROR();
    }
  }else 
      if (statusCarro.atual == PARADO_EM_NEUTRO){
          piscarLed();
          beeparWARN();
       }else limparNotificacoes(); 
}
// === APAGAR LED E BEET ===

void limparNotificacoes(){
  noTone(beepStatus);
  digitalWrite(ledStatus, 0);
}
// === ATIVA FREIO ===
void freiarAteFim(int intensidade) {
 if (correnteNoLimite) return;
  //Serial.print("Ativando freio com intensidade ");
  //Serial.println(intensidade);
  digitalWrite(sinalPainelStatus, LOW);
  ligarMotor(SENTIDO_FREIANDO);
  
  
  if (intensidade == 100)
    aguardarOuSoltarBotao(TEMPO_MAXIMO_ACIONAMENTO, CORRENTE_100); // eenquanto tiver apertando, o freio é acionado
  else
    aguardar(TEMPO_MAXIMO_ACIONAMENTO,CORRENTE_50);

  
  finalizarFreiar(intensidade);
  
}


// === ATIVA FREIO  ===
void iniciarFreiar() {
  if (freiado) return;
  digitalWrite(sinalPainelStatus, LOW);
  //Serial.print("Ativando freio com intesidade : 50 ");
  ligarMotor(SENTIDO_FREIANDO);
  aguardar(TEMPO_MAXIMO_ACIONAMENTO,CORRENTE_50);
 
}

void finalizarFreiar(int intensidade) {

  EEPROM.write(EEPROM_ADDR_FREIADO,intensidade);
  freiado = true;
  parar();
  digitalWrite(ledStatus, true);

}

// === LEITURA DO BOTÃO COM CLIQUE CURTO/LONGO ===
void verificarAcionamentoManual(StatusCarro statusCarro) {
  bool peFreio = digitalRead(sensorPeFreioPin) == LOW;
  if (botaoApertado()){
     long tempoInicio = millis();
    //  Acionar o botao até o tempo "100" ou atingir o limite de corrente
    while (botaoApertado()){
       if ((millis() - tempoInicio) >= QUARTO_SEGUNDO){
            if (peFreio){
               bloqueadoAcionamentoAutomatico = true;
               freiarAteFim(100);
            }
           return;
       }  
    }
    if (freiado){ // FREIO ACIONADO
      if (!correnteNoLimite){ // Evitar destravar quando estiver segurando botao
          if (peFreio){ // Se estiver no N ou em movimento, com freio acionado, necessário pisar no freio e clicar no botão
               bloqueadoAcionamentoAutomatico = true;
               desativarFreio();
           }
      }
    }else{// FREIO NAO ACIONADO
      if (statusCarro.atual!= EM_MOVIMENTO && peFreio){ // Só aciona freio 50% se estiver PARADO e com pé no  freio. 
        bloqueadoAcionamentoAutomatico = true;
        iniciarFreiar(); 
        if (botaoNaoApertado())
            finalizarFreiar(50);
       }
    }
   
  }else if (correnteNoLimite){ // BOTAO SOLTO 
          correnteNoLimite = !correnteNoLimite;
          EEPROM.write(EEPROM_ADDR_LIMITE_CORRENTE,correnteNoLimite);
         }
  
}
// === AÇÃO AUTOMÁTICA DE SEGURANÇA ===
void verificarAcionamentoAutomatico(StatusCarro statusCarro) { 
  bool apertandoFreio = digitalRead(sensorPeFreioPin) == LOW;
  bool portaAberta = digitalRead(sensorPortaPin) == LOW;
  bool portaFechada = !portaAberta;
  bool naofreiado = !freiado;
  bool naoApertandoFreio = !apertandoFreio;
  bool pisarAcelerador = (statusCarro.atual == ENGRENADO_PISANDO_ACELERADOR && portaFechada);
  bool engatarMarchaParado = statusCarro.atual == PARADO_ENGRENADO && portaFechada && freiado && apertandoFreio && !bloqueadoAcionamentoAutomatico;
  bool abrirPortaInesperadamenteParado = portaAberta && naoApertandoFreio && statusCarro.atual == PARADO_ENGRENADO && naofreiado && !bloqueadoAcionamentoAutomatico;
  bool colocarEmNeutroParado = statusCarro.atual == PARADO_EM_NEUTRO && naofreiado && apertandoFreio && !bloqueadoAcionamentoAutomatico;
  bool autoHold =  statusCarro.atual == PARADO_ENGRENADO && statusCarro.anterior == EM_MOVIMENTO && pisouEsoltouFreio(apertandoFreio) && naofreiado && !bloqueadoAcionamentoAutomatico;
 
  if (abrirPortaInesperadamenteParado  || colocarEmNeutroParado || autoHold) {
     freiarAteFim(50);
  }

  if (pisarAcelerador || engatarMarchaParado ) {
     desativarFreio();
     if (pisarAcelerador){
       bloqueadoAcionamentoAutomatico=false;
     }
  }

// Permitir que o modo automatico volte a funcionar
  if (statusCarro.atual == EM_MOVIMENTO &&  portaFechada && !freiado){
     bloqueadoAcionamentoAutomatico = false;
  }
  
}
bool pisouEsoltouFreio(bool freioPressionadoAgora ) {
  static bool freioEstaPisado = false;
  static unsigned long instanteUltimaPisada = 0;
  static  unsigned long agora;
  if (freioPressionadoAgora) {
    // Obtém o tempo atual em milissegundos
    agora = millis();
    return ((agora - instanteUltimaPisada) <= 250);
    
  }else instanteUltimaPisada = agora;       // Salva o momento que soltou da pisada
  
}

StatusCarro detectarStatusCarro() {
  static StatusCarro statusCarro = {0, 0};
  if (receptorStatusCarro.available()){
       statusCarro.anterior = statusCarro.atual;
       statusCarro.atual = receptorStatusCarro.read();
      //Serial.print("Status CArro:");
      //Serial.println(statusCarro);
  
    }
  return statusCarro;
}

// === DESATIVA FREIO ===
void desativarFreio() {
  if (!freiado) return;
  ligarMotor(!SENTIDO_FREIANDO);
  //Serial.println("Desativando freio...");
  //aguardar(EEPROM.read(EEPROM_ADDR_FREIADO) == 100 ? TEMPO_100: TEMPO_50);
  aguardar(TEMPO_DESTRAVA, CORRENTE_100);

  freiado = false;
  digitalWrite(ledStatus, LOW);
  digitalWrite(sinalPainelStatus, HIGH);
  EEPROM.write(EEPROM_ADDR_FREIADO,0);
  
  parar();
}
// === VARIAS LEITURAS ANALOGICAS PARA SUAVISAR ===
float leituraMediaAnologica(int pinAnalogico){
  long soma=0;
   for(int i=1;i<=10;i++){
       soma = soma + analogRead(pinAnalogico);
     //  delay(5);
    }
  return soma/10;
}

// === INICAR MOVIMENTO ===
void ligarMotor(bool sentido){
  
    digitalWrite(L_PWM, !sentido);
    digitalWrite(R_PWM, sentido);
    analogWrite(PWM, 255); 
}
// === PARAR MOVIMENTO ===
void parar(){
    analogWrite(PWM, 0); 
}
bool botaoApertado(){
  return digitalRead(botaoPin) == LOW;   
}

bool botaoNaoApertado(){
  return digitalRead(botaoPin) == HIGH;   
}
bool isAtingiuCorrenteLimite(float tempoAgurde, int limiteCorrente){
  float tensaoA = leituraMediaAnologica(L_IS) / 1023.0 * 5; // 5V máxima voltagem do pino que corresponde a 1023.0
  float tensaoB = leituraMediaAnologica(R_IS) / 1023.0 * 5; // 5V máxima voltagem do pino que corresponde a 1023.0
  float tensao = max(tensaoA,tensaoB);
  float corrente  = (tensao /resistor)* 8500.0; // 8500 ganho do chip  - BTS7960 reduz o valor em um fator de 8500 para enviar o sinal ao IS
  
  //return corrente>12;
  Serial.print("COrrente:");
  Serial.println(corrente);
  correnteNoLimite = tempoAgurde >100 && corrente>limiteCorrente;
  if (correnteNoLimite){
    EEPROM.write(EEPROM_ADDR_LIMITE_CORRENTE,correnteNoLimite);
  }
  return correnteNoLimite;
  
}

// === AGURDAR ===   
void aguardar(float tempoAgurde, int limiteCorrente) {
    long tempoInicio = millis();
    //Serial.print("aguarde  por:");
    //Serial.println(tempoAgurde);
    // Se ultrapassar o tempoAguarde
    while (((millis() - tempoInicio) <= tempoAgurde)){
        piscarLed();
        if (isAtingiuCorrenteLimite(millis() - tempoInicio,limiteCorrente)){
             break;
         }
        
    }
      
}

// === AGUARDAR OU INTERROMPER NO MEIO DO AGUARDE ===
void aguardarOuSoltarBotao(float tempoMaximo, int limiteCorrente) {
    long tempoInicio = millis();
    Serial.print("aguarde  por:");
    Serial.print(tempoMaximo);
    Serial.println(" ou solte botao");
    // Se ultrapassar o tempoAguarde ou botão foi solto
    while (((millis() - tempoInicio) <= tempoMaximo)){
        if  (botaoNaoApertado() || isAtingiuCorrenteLimite((millis() - tempoInicio), limiteCorrente)){
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
void beeparERROR(){
     if (millis() - ultimoBeep >=150){
         ultimoBeep = millis();
        if (beepando) tone(beepStatus,2000); else noTone(beepStatus);
        beepando=!beepando;
     }
  
}

void beeparWARN() {
    static unsigned long ultimoCiclo = 0;
    unsigned long tempoAtual = millis();
    unsigned long tempoNoCiclo = (tempoAtual - ultimoCiclo) % 1000;
    
    // Reinicia o ciclo a cada 1 segundo
    if (tempoAtual - ultimoCiclo >= 1000) {
        ultimoCiclo = tempoAtual;
    }
    
    // Dois bipes de 100ms com 200ms entre eles
    if (tempoNoCiclo < 100) {
        tone(beepStatus, 2000);  // Primeiro beep
    } else if (tempoNoCiclo >= 100 && tempoNoCiclo < 300) {
        noTone(beepStatus);       // Pausa de 300ms
    } else if (tempoNoCiclo >= 300 && tempoNoCiclo < 400) {
        tone(beepStatus, 2000);   // Segundo beep
    } else {
        noTone(beepStatus);       // Silêncio até completar 1 segundo
    }
}
/*
float lerAcelerometro() {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B); // Começa a ler no registrador do acelerômetro
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true); // Pede 6 bytes (X, Y, Z)
    
    int16_t acX = Wire.read() << 8 | Wire.read();
    Wire.read(); Wire.read(); // Pula AcY (não necessário para inclinação frente/trás)
    int16_t acZ = Wire.read() << 8 | Wire.read();
    
    // Calcula e RETORNA o ângulo de Pitch (frente/trás) em graus
    return atan2(acX, acZ) * 180.0 / PI;
}
// 2. MÉTODO DE CALIBRAÇÃO AUTÔNOMA
// -------------------------------------------------------------------
void calibrarAcelerometro() {
    float somaPitch = 0.0;
    
    // Faz 100 leituras para tirar a média e eliminar ruídos
    for (int i = 0; i < 100; i++) {
        somaPitch += lerAcelerometro(); // Soma o valor retornado diretamente
        delay(10); 
    }
    
    float offset = somaPitch / 100.0;
    
    // Salva o valor calculado e a marca de "já calibrado" na EEPROM
    EEPROM.put(ADDR_OFFSET, offset);
    EEPROM.write(ADDR_MAGIC, MAGIC_BYTE);
}
// -------------------------------------------------------------------
// 3. MÉTODO DE INICIALIZAÇÃO DO HARDWARE
// -------------------------------------------------------------------
void inicializarAcelerometro() {
    // Inicia comunicação I2C
    Wire.begin();
    
    // Acorda o MPU6050 (sai do modo sleep)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0);
    Wire.endTransmission(true);

    // Verifica se já existe uma calibração salva. Se não, executa a calibração.
    if (EEPROM.read(ADDR_MAGIC) != MAGIC_BYTE) {
        calibrarAcelerometro();
    }
}

// -------------------------------------------------------------------
// 4. MÉTODO DE VERIFICAÇÃO DA LADEIRA (Lógica de Negócio)
// -------------------------------------------------------------------
bool verificarLadeira() {
    // 'static' mantém o valor entre as chamadas, mas com escopo LOCAL
    static float offsetPitch = -999.0;
    
    // Lê da EEPROM apenas na primeira execução deste método
    if (offsetPitch == -999.0) {
        EEPROM.get(ADDR_OFFSET, offsetPitch);
    }

    // Chama a função e guarda o valor retornado
    float pitchAtual = lerAcelerometro();
    
    // Aplica a calibração subtraindo o offset
    float pitchReal = pitchAtual - offsetPitch;

    // Verifica inclinação (usando fabs para valor absoluto de float)
    return (fabs(pitchReal) > LIMITE_LADEIRA);
}
*/
