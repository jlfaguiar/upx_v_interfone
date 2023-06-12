#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include <vector>
#include <UniversalTelegramBot.h>
#include <string>
#include <SPIFFS.h>
#include <ArduinoJson.h>

const char* ssid = "UPX V - Interfone"; //WiFi SSID
const char* password = "492C9m4+"; //WiFi Password

#define BOT_TOKEN "6289994300:AAFa6jrMKxsZK0WEuz2WG4t-32LfzhWI9RQ"
#define ARQUIVO_LISTA "/chat_ids_file.json"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
std::vector<String> CHAT_IDS; 

void startCameraServer();
int interval = 6000;  //DELAY

const int ledPin = 4;
const int devModeIn = 16;
const int devModeOut = 15;

bool initializedBot = false;

void salvarLista() {
  if (!SPIFFS.begin(true)) {
    salvarLista();
    return;
  }
  File arquivo = SPIFFS.open(ARQUIVO_LISTA, "w");

  if(!arquivo){
    salvarLista();
    return;
  }

  DynamicJsonDocument doc(512);
  JsonArray array = doc.to<JsonArray>();

  for (const auto& chatId: CHAT_IDS) {
    array.add(chatId);
  }

  if (serializeJson(doc, arquivo) == 0) {
    salvarLista();
  }

  arquivo.close();
  SPIFFS.end();
}

void carregarLista() {

  if (!SPIFFS.begin(true)) {
    carregarLista();
    return;
  }

  if (SPIFFS.exists(ARQUIVO_LISTA)){
    
    File arquivo = SPIFFS.open(ARQUIVO_LISTA, "r");
    if(!arquivo) {
      return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError erro = deserializeJson(doc, arquivo);

    if(erro) {
      arquivo.close();
      return;
    }

    CHAT_IDS.clear();
    JsonArray array = doc.as<JsonArray>();

    for (const auto& chatId: array) {
      CHAT_IDS.push_back(chatId.as<String>());
    }  

    arquivo.close();
  }

  SPIFFS.end();
}

void enviarFoto(const String& chatId) {
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    ESP.restart();
    return;
  }  
  
  if (secured_client.connect("api.telegram.org", 443)) {
    String head = String("--UPX V - Interfone\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n") + chatId + String("\r\n--UPX V - Interfone\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"visitante.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n");
    String tail = "\r\n--UPX V - Interfone--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    secured_client.println(String("POST /bot") + BOT_TOKEN + String("/sendPhoto HTTP/1.1"));
    secured_client.println("Host: api.telegram.org");
    secured_client.println("Content-Length: " + String(totalLen));
    secured_client.println("Content-Type: multipart/form-data; boundary=UPX V - Interfone");
    secured_client.println();
    secured_client.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        secured_client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        secured_client.write(fbBuf, remainder);
      }
    }  
    
    secured_client.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      delay(100);      
      while (secured_client.available()) {
        char c = secured_client.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    secured_client.stop();
  }
  
  return;
}

void setup() {
  
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(devModeIn, INPUT);
  pinMode(devModeOut, OUTPUT);
  carregarLista();
  
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  digitalWrite(devModeOut, HIGH);
}

void loop() {

  if (!digitalRead(devModeIn)) {
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
  
    int numUpdates = bot.getUpdates(bot.last_message_received + 1);

    while (numUpdates) {
      String chatId = String(bot.messages[0].chat_id);
      String text = bot.messages[0].text;

      if (text == "/cadastrar") {
        CHAT_IDS.push_back(chatId);
        salvarLista();
        bot.sendMessage(chatId, "ChatID cadastrado com sucesso! Você será notificado quando houver um visitante");
      } else if (text == "/limpar") {
        CHAT_IDS.clear();
        salvarLista();
        bot.sendMessage(chatId, "A lista de ChatID's foi limpa com sucesso!");
      } else if (text == "/fotografar") {
        bot.sendMessage(chatId, "Estou enviando uma foto da sua câmera!");
        enviarFoto(chatId);
       } else if (text == "/help") {
        bot.sendMessage(chatId, "Os comandos disponíveis são:");
        bot.sendMessage(chatId, "COMANDOS DE CADASTRAMENTO E ADMINISTRAÇÃO");
        bot.sendMessage(chatId, "/help - Abrir o menu de ajuda");
        bot.sendMessage(chatId, "/cadastrar - Cadastra o ChatId da conta telegram para recebimento de alertas de visitas");
        bot.sendMessage(chatId, "/limpar - Limpa a lista de ChatId's cadastrados no interfone");
        bot.sendMessage(chatId, "/fotografar - Tira uma foto da sua câmera");
        bot.sendMessage(chatId, "COMANDOS DE CONTROLE DE ENTRADAS E SAÍDAS");
        bot.sendMessage(chatId, "0 - Fechar o portão - Somente modo não dev");
        bot.sendMessage(chatId, "1 - Abrir o portão - Somente modo não dev");
        bot.sendMessage(chatId, "2 - Abrir por 5 segundos e fechar em seguida - Somente modo não dev");
       } else {
        bot.sendMessage(chatId, "Comando desconhecido. Digite /help para ver os comandos disponíveis!");
        }

 
      numUpdates--;

    }
  }

  Serial.println("detect");
  extern bool detection();
  bool detected = detection();

  for (const auto& chatId: CHAT_IDS){
    if (detected) {
      Serial.println("Enviando para " + String(chatId));
      bot.sendMessage(chatId, "Há alguem em frente a sua porta! Você deseja abri-la?\nEscolha uma das opções:\n2 - Abrir por 5 segundos e fechar em seguida\n1 - Abrir\n0 - Fecha-la");
      enviarFoto(chatId);
    } 
  }

  delay(1000);
}
