#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "DHT.h"
#include <Wire.h>
#include <Adafruit_MLX90614.h>

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
#include "SH1106Wire.h"

//Pinos do NodeMCU
// SDA => D1/d3
// SCL => D2/d5 
//SENSOR IR scl d3/sda d4
// Inicializa o display Oled

SH1106Wire  display(0x3c, D1, D2);

#define DEBUG
#define INTERVALO_ENVIO       5000
#define DHTPIN D4   
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define DEBUG



//Variáveis do sensor de bpm
const int pulsePin = A0;
int UpperThreshold = 518;
int LowerThreshold = 490;
int reading = 0;
float BPM = 0;
bool IgnoreReading = false;
bool FirstPulseDetected = false;
unsigned long FirstPulseTime = 0;
unsigned long SecondPulseTime = 0;
unsigned long PulseInterval = 0;



//-------------------------------------------------------


//informações da rede WIFI
const char* ssid = "Bruno 2G";                 //SSID da rede WIFI
const char* password =  "11235813";    //senha da rede wifi

const char* mqttServer = "tailor.cloudmqtt.com";   //server
const char* mqttUser = "uffdqqwl";              //user
const char* mqttPassword = "7RJqyFBtPtQN";      //password
const int mqttPort = 13520;                     //port

const char* mqttTopicSub ="casa/L1";            //tópico que sera assinado

float temperatura;
float umidade;

 int ultimoEnvioMQTT = 0;
WiFiClient espClient;
PubSubClient client(espClient);
 float tempObjeto = 0;
void setup() {
 
  Serial.begin(115200);
  mlx.begin(); 
  

   
   display.init();
  display.flipScreenVertically();
   ProgressBar();

  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef DEBUG
    Serial.println("Conectando ao WiFi..");
    #endif
  }
  #ifdef DEBUG
  Serial.println("Conectado na rede WiFi");
  #endif
 
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
 
  while (!client.connected()) {
    #ifdef DEBUG
    Serial.println("Conectando ao Broker MQTT...");
    #endif
 
    if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
      #ifdef DEBUG
      Serial.println("Conectado");  
      #endif
 
    } else {
      #ifdef DEBUG 
      Serial.print("falha estado  ");
      Serial.print(client.state());
      #endif
      delay(2000);
 
    }
  }

  //subscreve no tópico
  client.subscribe(mqttTopicSub);
 dht.begin();
 
//Inicializa sensor de temperatura infravermelho

}
 
void callback(char* topic, byte* payload, unsigned int length) {

  //armazena msg recebida em uma sring
  payload[length] = '\0';
  String strMSG = String((char*)payload);

  #ifdef DEBUG
  Serial.print("Mensagem chegou do tópico: ");
  Serial.println(topic);
  Serial.print("Mensagem:");
  Serial.print(strMSG);
  Serial.println();
  Serial.println("-----------------------");
  #endif

 
}

//função pra reconectar ao servido MQTT
void reconect() {
  //Enquanto estiver desconectado
  while (!client.connected()) {
    #ifdef DEBUG
    Serial.print("Tentando conectar ao servidor MQTT");
    #endif
     
    bool conectado = strlen(mqttUser) > 0 ?
                     client.connect("ESP8266Client", mqttUser, mqttPassword) :
                     client.connect("ESP8266Client");

    if(conectado) {
      #ifdef DEBUG
      Serial.println("Conectado!");
      #endif
      //subscreve no tópico
      client.subscribe(mqttTopicSub, 1); //nivel de qualidade: QoS 1
    } else {
      #ifdef DEBUG
      Serial.println("Falha durante a conexão.Code: ");
      Serial.println( String(client.state()).c_str());
      Serial.println("Tentando novamente em 10 s");
      #endif
      //Aguarda 10 segundos 
      delay(10000);
    }
  }
}



void enviaTIR(){
  char MsgTIRMQTT[10];


   Serial.print("tObject = "); Serial.print(mlx.readObjectTempC()); Serial.println("*C");
  tempObjeto = (mlx.readObjectTempC());

   sprintf(MsgTIRMQTT,"%.2f",tempObjeto);
    client.publish("casa/tir", MsgTIRMQTT);

  Serial.println();
  delay(500);
  }
 
void loop() {

telainicial();
  if (!client.connected()) {
    reconect();
  }
  
   //envia a cada X segundos
  if ((millis() - ultimoEnvioMQTT) > INTERVALO_ENVIO)
  {
      enviaDHT();
   
      // enviaTIR();
      
      ultimoEnvioMQTT = millis();
      //envia chave
      client.publish("casa/id", "uffdqqwl");
        
  
  }
//Essa função precisa ser enviada o maximo de vezes possivel
  batimentos();



  client.loop();
}

//função para leitura do DHT11
void enviaDHT(){

  char MsgUmidadeMQTT[10];
  char MsgTemperaturaMQTT[10];
  
   umidade = dht.readHumidity();
   temperatura = dht.readTemperature();
  

  if (isnan(temperatura) || isnan(umidade)) 
  {
    #ifdef DEBUG
    Serial.println("Falha na leitura do dht11...");
    #endif
  } 
  else 
  {
   /* #ifdef DEBUG
    Serial.print("Umidade: ");
    Serial.print(umidade);
    Serial.print(" \n"); //quebra de linha
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");
    #endif*/

    sprintf(MsgUmidadeMQTT,"%.2f",umidade);
    client.publish("casa/umidade", MsgUmidadeMQTT);
    sprintf(MsgTemperaturaMQTT,"%.2f",temperatura);
    client.publish("casa/temperatura", MsgTemperaturaMQTT);
  }
}

  
  
 
void batimentos(){

  char MsgBPMMQTT[10];

  
  reading = analogRead(pulsePin);
  // Heart beat leading edge detected.
      if(reading > UpperThreshold && IgnoreReading == false){
        if(FirstPulseDetected == false){
          FirstPulseTime = millis();
          FirstPulseDetected = true;
        }
        else{
          SecondPulseTime = millis();
          PulseInterval = SecondPulseTime - FirstPulseTime;
          FirstPulseTime = SecondPulseTime;
        }
        IgnoreReading = true;
      }

      // Heart beat trailing edge detected.
      if(reading < LowerThreshold){
        IgnoreReading = false;
      }  

      BPM = (1.0/PulseInterval) * 60.0 * 1000;

     /* Serial.print(reading);
      Serial.print("\t");
      Serial.print(PulseInterval);
      Serial.print("\t");
      Serial.print(BPM);
      Serial.println(" BPM");*/

      
      sprintf(MsgBPMMQTT,"%.0f",BPM);
        client.publish("casa/bpm", MsgBPMMQTT);
      Serial.flush();
      

      // Please don't use delay() - this is just for testing purposes.
      delay(50);  
    
  }

  void telainicial()
{
  //Apaga o display
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //Seleciona a fonte
  display.setFont(ArialMT_Plain_16);
  display.drawString(63, 10, String(temperatura)+" ºC");
  display.drawString(63, 26, String(BPM)+" bpm");
  display.drawString(63, 45, String(umidade)+" %");
  display.display();
}


void ProgressBar()
{
  for (int counter = 0; counter <= 100; counter++)
  {
    display.clear();
    //Desenha a barra de progresso
    display.drawProgressBar(0, 32, 120, 10, counter);
    //Atualiza a porcentagem completa
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 15, String(counter) + "%");
    display.display();
    delay(10);
  }
}
