#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

LiquidCrystal_I2C LCD(0x27, 16, 2); // Dirección I2C 0x27, 16 columnas y 2 filas

const int TRIG = 13; // Se ha cambiado a D13 según el nuevo diagrama
const int ECHO = 12; // Se ha cambiado a D12 según el nuevo diagrama
int DURACION;
float DISTANCIA;
float GASTO_POR_METRO = 0.2;
float TANQUE = 25000;

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* openaiApiKey = "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";

void postToAPI(float distance) {
  DynamicJsonDocument doc(1024);

  // Construir el contenido
  String content;
  content.reserve(200);
  content += "The current distance measured is ";
  content += distance;
  content += " meters. Based on this, please provide relevant information.";

  // Poblar el documento JSON
  doc["model"] = "gpt-3.5-turbo";
  JsonArray messages = doc.createNestedArray("messages");

  JsonObject message1 = messages.createNestedObject();
  message1["role"] = "system";
  message1["content"] = "You are providing information about the distance measured by a sensor.";

  JsonObject message2 = messages.createNestedObject();
  message2["role"] = "user";
  message2["content"] = content;

  // Serializar JSON a String
  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.println(jsonPayload);

  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");
  http.addHeader("Authorization", "Bearer " + String(openaiApiKey));
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(jsonPayload);
  String payload = http.getString();
  Serial.println("HTTP Response: " + payload);

  if (httpCode > 0) {
    DynamicJsonDocument doc2(1024);
    deserializeJson(doc2, payload);
    String answer = doc2["choices"][0]["message"]["content"].as<String>();

    // Imprimir la pregunta y respuesta en la consola serie
    Serial.println("P: The current distance measured is " + String(distance) + " meters. Provide relevant information.");
    Serial.println("R: " + answer);

    // Mostrar la respuesta en el LCD
    displayOnLCD(answer);
  } else {
    Serial.print("Error en la solicitud HTTP, código de estado: ");
    Serial.println(httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("debug...");
    delay(500);
  }

  LCD.begin(16, 2);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  LCD.print("Combustible:");

  // Lógica para la solicitud HTTP que se ejecutará solo una vez
  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");
  http.addHeader("Authorization", "Bearer " + String(openaiApiKey));
  http.addHeader("Content-Type", "application/json");

  String body = "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": \"user\", \"content\": \"Initial message.\"}]}";

  int httpCode = http.POST(body);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);

    Serial.print("Initial Answer: ");
    Serial.println(doc["choices"][0]["message"]["content"].as<String>());
    displayOnLCD(doc["choices"][0]["message"]["content"].as<String>());
  } else {
    Serial.println("Error on Initial HTTP request");
  }

  http.end();
}

void loop() {
  LCD.setCursor(0, 1);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  DURACION = pulseIn(ECHO, HIGH);

  DISTANCIA = (DURACION / 2) / 29.1;
  float gasto_combustible = DISTANCIA * GASTO_POR_METRO;
  TANQUE = TANQUE - gasto_combustible;

  LCD.print(TANQUE, 2);
  LCD.print(" m^3");

  // Llama a la función para enviar la distancia al modelo
  postToAPI(DISTANCIA);

  delay(200);
}

void displayOnLCD(const String &text) {
  LCD.clear();

  // Mostrar texto en la pantalla LCD
  if (text.length() <= 16) {
    LCD.setCursor(0, 0);
    LCD.print(text);
  } else if (text.length() <= 32) {
    LCD.setCursor(0, 0);
    LCD.print(text.substring(0, 16));
    LCD.setCursor(0, 1);
    LCD.print(text.substring(16));
  } else {
    LCD.setCursor(0, 0);
    LCD.print(text.substring(0, 16));
    LCD.setCursor(0, 1);
    LCD.print(text.substring(16, 32));
  }
}
