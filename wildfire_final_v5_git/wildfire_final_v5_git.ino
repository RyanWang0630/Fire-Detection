//   1. Using Roboflow detection API
//   2. Model: (Your project)
//   3. AI Fire Detection → Send Email
//   4. Email Cooldown Duration
//   5. HTML Email + JPEG Frame + Google Maps
// =========================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP_Mail_Client.h>
#include <base64.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// =========================================================================
// 【User Info】
// =========================================================================
#define WIFI_SSID       "WiFi_SSID"
#define WIFI_PASSWORD   "WiFi_Password"

// --- Roboflow Direct Detection API ---
#define ROBOFLOW_API_URL   "https://serverless.roboflow.com"
#define ROBOFLOW_API_KEY   "API-KEY"
#define ROBOFLOW_MODEL     "Model-URL"          // Project ID
#define ROBOFLOW_VERSION   "Version"              // Version of Project
#define FIRE_CONFIDENCE    0.5             // Lower % for testing. 0.9 for real world simulation

// --- Gmail SMTP ---
#define SMTP_HOST       "smtp.gmail.com"
#define SMTP_PORT       465
#define AUTHOR_EMAIL    "sender's gmail"
#define AUTHOR_PASSWORD "google's 16-digit passkey"
#define RECIPIENT_EMAIL "recipient's email"

// --- GPS ---
#define GPS_LATITUDE    "25.0851911" //GPS location for trial
#define GPS_LONGITUDE   "121.5466115"
// --- PMS5003 ---
#define PM25_THRESHOLD  6                    // 6 for testing, 300 for real world simulation
#define PMS_RX_PIN      14                   // PIN position
#define PMS_TX_PIN      15                   // PIN position

// --- Interval ---
const unsigned long CHECK_INTERVAL = 15000;

// --- Email Cooldown ---
const unsigned long EMAIL_COOLDOWN = 60000;   // 1 email/60 seconds

// =========================================================================
// 【AI Thinker ESP32-CAM】
// =========================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =========================================================================
// Global Variable
// =========================================================================
unsigned long lastCheckTime = 0;
unsigned long lastEmailTime = 0;
SMTPSession smtp;
int global_pm25 = -1;

bool initCamera();
int  readPMS5003();
float detectFireByRoboflow(camera_fb_t * fb);
void sendAlertEmail(camera_fb_t * fb, int pm25_val, float confidence);
bool connectWiFi();

// =========================================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n===============================================");
  Serial.println("ESP32-CAM Fire Detection");
  Serial.println("===============================================");
  Serial.println("Using AI to detect fire + email\n");
  
  Serial.println("[STEP 1] Initializing...");
  if (!initCamera()) {
    Serial.println("Failed");
    while(true) delay(1000);
  }
  Serial.println("Ready..");
  
  Serial.println("[STEP 2] PMS5003 Initializing...");
  Serial1.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN);
  Serial.println("PMS5003 Ready..");
  
  Serial.println("[STEP 3] Connecting WiFi...");
  if (connectWiFi()) {
    Serial.printf("WiFi Connected，IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi failed to Connect");
  }
  
  Serial.println("\n System Ready, Detecting...\n");
  Serial.printf("Setting：PM2.5 Threshold = %d, Confidence = %.0f%%\n", 
                PM25_THRESHOLD, FIRE_CONFIDENCE * 100);
  Serial.printf("     Interval = %lu 秒, Email Cooldown = %lu Second\n\n", 
                CHECK_INTERVAL / 1000, EMAIL_COOLDOWN / 1000);
}

// =========================================================================
void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastCheckTime >= CHECK_INTERVAL || lastCheckTime == 0) {
    lastCheckTime = currentTime;
    Serial.println("\n--- Next detection.. ---");
    
    // Reading PM2.5
    int pm25_value = -1;
    for (int i = 0; i < 15; i++) {
      pm25_value = readPMS5003();
      if (pm25_value != -1) break;
      delay(200);
    }
    
    if (pm25_value == -1) {
      Serial.println("Failed to read");
      return;
    }
    
    global_pm25 = pm25_value;
    Serial.printf("PM2.5 = %d ug/m3\n", global_pm25);
    
    if (global_pm25 >= PM25_THRESHOLD) {
      Serial.println("Threshold Exceeded！Activating AI detection...");
      
      // Capture
      camera_fb_t * fb = esp_camera_fb_get();
      if (fb) esp_camera_fb_return(fb);
      fb = esp_camera_fb_get();
      
      if (!fb) {
        Serial.println("Capture failed");
        return;
      }
      Serial.printf("Captured: %d bytes\n", fb->len);
      
      // Check WiFi status
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        if (!connectWiFi()) {
          Serial.println("WiFi failed, proceeding");
          esp_camera_fb_return(fb);
          return;
        }
      }
      
      // Roboflow AI detection
      Serial.println("Uploading Roboflow...");
      float confidence = detectFireByRoboflow(fb);
      
      Serial.printf("Confidence: %.2f%%\n", confidence * 100);
      
      // Email Conditional Statement
      if (confidence >= FIRE_CONFIDENCE) {
        // Email cooldown check
        if (lastEmailTime > 0 && (currentTime - lastEmailTime) < EMAIL_COOLDOWN) {
          unsigned long remain = (EMAIL_COOLDOWN - (currentTime - lastEmailTime)) / 1000;
          Serial.printf("Fire detected, but email is in cooldown，剩 %lu 秒\n", remain);
        } else {
          Serial.println("Confirmed fire, send email!");
          sendAlertEmail(fb, global_pm25, confidence);
          lastEmailTime = millis();
        }
      } else {
        Serial.println("Not fire, could be smoke");
      }
      
      esp_camera_fb_return(fb);
    } else {
      Serial.println("Normal");
    }
  }
}

// =========================================================================
// Roboflow AI Detection
// =========================================================================
float detectFireByRoboflow(camera_fb_t * fb) {
  WiFiClientSecure * client = new WiFiClientSecure;
  if (!client) {
    Serial.println("WiFiClientSecure Failed");
    return 0.0;
  }
  
  client->setInsecure();
  
  HTTPClient https;
  
  // URL: https://serverless.roboflow.com/firer/4?api_key=xxx
  String url = String(ROBOFLOW_API_URL) + "/" + 
               ROBOFLOW_MODEL + "/" + ROBOFLOW_VERSION + 
               "?api_key=" + ROBOFLOW_API_KEY +
               "&confidence=" + String((int)(FIRE_CONFIDENCE * 100));
  
  Serial.printf("   URL: %s\n", url.c_str());
  
  if (!https.begin(*client, url)) {
    Serial.println("HTTPS begin Failed");
    delete client;
    return 0.0;
  }
  
  // detection API form-urlencoded
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Base64 Encode
  Serial.println("   Base64 Encoding...");
  String encoded = base64::encode(fb->buf, fb->len);
  Serial.printf("   Base64 Length: %d\n", encoded.length());
  
  Serial.println("   Uploading(5-15 seconds)...");
  int httpCode = https.POST(encoded);
  
  float max_confidence = 0.0;
  
  if (httpCode == HTTP_CODE_OK) {
    String response = https.getString();
    Serial.printf("   Received（%d bytes）\n", response.length());
    Serial.println("   Responding:");
    Serial.println(response.substring(0, 500));
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      // detection API Response format：
      // {"predictions":[{"class":"fire","confidence":0.85,...}],...}
      JsonArray predictions = doc["predictions"];
      Serial.printf("   found prediction size\n", predictions.size());
      
      for (JsonObject pred : predictions) {
        const char* class_name = pred["class"] | "";
        float conf = pred["confidence"] | 0.0;
        Serial.printf("   → %s (%.2f%%)\n", class_name, conf * 100);
        
        // Checking flame detection
        if (strcmp(class_name, "fire") == 0 || 
            strcmp(class_name, "Fire") == 0 ||
            strcmp(class_name, "flame") == 0 ||
            strcmp(class_name, "FIRE") == 0) {
          if (conf > max_confidence) max_confidence = conf;
        }
      }
    } else {
      Serial.printf("JSON Failed: %s\n", error.c_str());
    }
  } else {
    Serial.printf("HTTP Failed: %d\n", httpCode);
    if (httpCode > 0) {
      String errBody = https.getString();
      Serial.println("Error:");
      Serial.println(errBody.substring(0, 500));
    }
  }
  
  https.end();
  delete client;
  return max_confidence;
}

// =========================================================================
// Email Alarm
// =========================================================================
void sendAlertEmail(camera_fb_t * fb, int pm25_val, float confidence) {
  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  
  SMTP_Message message;
  message.sender.name = "WildFire Detection";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "SOS！ Fire Detected!!";
  message.addRecipient("Fire Department", RECIPIENT_EMAIL);
  
  String htmlMsg = "";
  htmlMsg += "<div style='font-family:Arial;border:3px solid #ff0000;padding:20px;border-radius:8px;'>";
  htmlMsg += "<h2 style='color:#ff0000;'> Fire Alarm</h2>";
  htmlMsg += "<hr style='border:1px solid #ff0000;'>";
  htmlMsg += "<p><b> PM2.5：</b> " + String(pm25_val) + " ug/m3</p>";
  htmlMsg += "<p><b>AI Confidence：</b> <span style='color:#ff0000;font-weight:bold;'>" + String(confidence * 100, 1) + "%</span></p>";
  htmlMsg += "<p><b>GPS：</b> " + String(GPS_LATITUDE) + ", " + String(GPS_LONGITUDE) + "</p>";
  htmlMsg += "<p style='margin-top:20px;'><a href='http://maps.google.com/?q=";
  htmlMsg += String(GPS_LATITUDE) + "," + String(GPS_LONGITUDE);
  htmlMsg += "' style='background:#d9534f;color:#fff;padding:12px 25px;text-decoration:none;border-radius:4px;'>";
  htmlMsg += "Open Google Maps </a></p>";
  htmlMsg += "<p style='color:#999;font-size:12px;margin-top:20px;'>";
  htmlMsg += "Email was automatically sent by the ESP32-CAM Wildfire AI Detection. The attached photo was captured at the moment of the trigger.";
  htmlMsg += "</p>";
  htmlMsg += "</div>";
  message.html.content = htmlMsg.c_str();
  
  SMTP_Attachment att;
  att.descr.filename = "wildfire.jpg";
  att.descr.mime = "image/jpeg";
  att.blob.data = fb->buf;
  att.blob.size = fb->len;
  att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
  message.addAttachment(att);
  
  Serial.println("Connnecting SMTP...");
  if (!smtp.connect(&config)) {
    Serial.println("SMTP Connection failed");
    return;
  }
  
  Serial.println(" Sending..");
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Failed to Send");
    Serial.printf("  Error: %s\n", smtp.errorReason().c_str());
  } else {
    Serial.println("Successfully sent");
  }
}

// =========================================================================
// WiFi
// =========================================================================
bool connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  Serial.println();
  return (WiFi.status() == WL_CONNECTED);
}

// =========================================================================
// Camera Initialization
// =========================================================================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    Serial.println(" PSRAM, VGA 640x480");
  } else {
    config.frame_size   = FRAMESIZE_CIF;
    config.jpeg_quality = 15;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
    Serial.println(" No PSRAM, CIF 400x296");
  }
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("   Camera Error: 0x%x\n", err);
    return false;
  }
  return true;
}

// =========================================================================
// PMS5003
// =========================================================================
int readPMS5003() {
  if (Serial1.available() >= 32) {
    if (Serial1.read() == 0x42) {
      if (Serial1.read() == 0x4D) {
        uint8_t buf[30];
        Serial1.readBytes(buf, 30);
        return (buf[10] << 8) | buf[11];
      }
    }
  }
  return -1;
}
