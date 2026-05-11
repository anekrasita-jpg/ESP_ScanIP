#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <ETH.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config_manager.h"
#include "whitelist_manager.h"

// ขาสำหรับ WT32-ETH01
#define ETH_ADDR          1
#define ETH_POWER_PIN    16
#define ETH_MDC_PIN      23
#define ETH_MDIO_PIN     18
#define ETH_TYPE         ETH_PHY_LAN8720
#define ETH_CLK_MODE     ETH_CLOCK_GPIO0_IN

WiFiClient ethClient;
PubSubClient mqttClient(ethClient);
extern DeviceConfig myConfig;

unsigned long lastMqttReconnectAttempt = 0;

// ฟังก์ชันสำหรับส่ง Feedback กลับไปยัง MQTT
void sendFeedback(String message) {
    String statusTopic = myConfig.base_topic + "config/status";
    StaticJsonDocument<200> doc;
    doc["site"] = myConfig.site_name;
    doc["ip"] = ETH.localIP().toString();
    doc["msg"] = message;
    doc["status"] = "success";

    char buffer[200];
    serializeJson(doc, buffer);
    mqttClient.publish(statusTopic.c_str(), buffer);
    Serial.println(">>> [MQTT] Feedback sent: " + message);
}

void sendRemoteLog(String logMsg) {
    // พิมพ์ออก Serial ด้วยเผื่อต่อสายดู
    Serial.println(">>> [LOG] " + logMsg);

    if (mqttClient.connected()) {
        String logTopic = myConfig.base_topic + "config/log";
        StaticJsonDocument<512> doc;
        doc["site"] = myConfig.site_name;
        doc["ip"] = ETH.localIP().toString();
        doc["timestamp"] = millis();
        doc["msg"] = logMsg;

        char buffer[512];
        serializeJson(doc, buffer);
        mqttClient.publish(logTopic.c_str(), buffer);
    }
}

// --- ฟังก์ชัน Callback เมื่อได้รับข้อความจาก MQTT ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf(">>> [MQTT] Message arrived [%s]\n", topic);
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        Serial.println(">>> [MQTT] JSON Parse failed!");
        return;
    }

    String cmd = doc["cmd"] | "";
    bool shouldSave = false;

    // 1. แบบ Sync All (หัวข้อที่ 1 & 2)
    if (cmd == "sync_all") {
        JsonObject data = doc["data"];
        if (!data.isNull()) {
            if (data.containsKey("mqtt_host")) myConfig.mqtt_host = data["mqtt_host"].as<String>();
            if (data.containsKey("is_dhcp"))   myConfig.is_dhcp   = data["is_dhcp"].as<bool>();
            if (data.containsKey("mqtt_interval")) myConfig.mqtt_interval = data["mqtt_interval"].as<int>();
            if (data.containsKey("scan_interval")) myConfig.scan_interval = data["scan_interval"].as<int>();
            shouldSave = true;
            Serial.println(">>> [MQTT] Sync All received");
        }
    } 
    // 2. แบบ Update รายตัว (หัวข้อที่ 3 & 4)
    else if (cmd == "update") {
        String key = doc["key"] | "";
        JsonVariant value = doc["value"];
        
        if (key != "" && !value.isNull()) {
            if (key == "mqtt_host")     myConfig.mqtt_host = value.as<String>();
            else if (key == "mqtt_user") myConfig.mqtt_user = value.as<String>();
            else if (key == "mqtt_pass") myConfig.mqtt_pass = value.as<String>();
            else if (key == "site_name") myConfig.site_name = value.as<String>();
            else if (key == "base_topic") myConfig.base_topic = value.as<String>();
            else if (key == "mqtt_interval") myConfig.mqtt_interval = value.as<int>();
            else if (key == "scan_interval") myConfig.scan_interval = value.as<int>();
            
            shouldSave = true;
            Serial.println(">>> [MQTT] Update key: " + key);
        }
    }
    // เพิ่มในส่วน mqttCallback ของ mqtt_handler.h
    // ... ภายใน if (cmd == "update_whitelist") ...
    else if (cmd == "update_whitelist") {
        JsonArray data = doc["data"].as<JsonArray>();
        if (!data.isNull()) {
            updateWhitelistFromJson(data);
        }
    }

    // บันทึกและรีสตาร์ท (หัวข้อที่ 5)
    if (shouldSave) {
        saveConfig(myConfig);
        sendFeedback("Configuration updated and saved to flash");
        delay(2000);
        Serial.println(">>> [System] Restarting to apply changes...");
        ESP.restart();
    }
}

void initEthernet() {
    if (myConfig.is_dhcp) {
        ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    } else {
        IPAddress local_IP, subnet, gateway, dns;
        local_IP.fromString(myConfig.static_ip);
        subnet.fromString(myConfig.subnet);
        gateway.fromString(myConfig.gateway);
        dns.fromString(myConfig.gateway);
        ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
        ETH.config(local_IP, gateway, subnet, dns);
    }
}

void reconnectMQTT() {
    mqttClient.setServer(myConfig.mqtt_host.c_str(), 1883);
    mqttClient.setCallback(mqttCallback); // ลงทะเบียน Callback
    
    if (!mqttClient.connected()) {
        String clientId = "PWA-Scanner-" + myConfig.site_name;
        if (mqttClient.connect(clientId.c_str(), myConfig.mqtt_user.c_str(), myConfig.mqtt_pass.c_str())) {
            Serial.println(">>> [MQTT] Connected");
            
            // Subscribe หัวข้อส่วนกลาง (all)
            mqttClient.subscribe("55310/REG1/SCAN_IP/config/set/all");
            
            // Subscribe หัวข้อรายตัว (base_topic + config/set)
            String personalTopic = myConfig.base_topic + "config/set";
            mqttClient.subscribe(personalTopic.c_str());
            
            // ส่งรายงานตัวเมื่อต่อสำเร็จ
            sendFeedback("System online and subscribed to config topics");
        }
    }
}

void sendScanData(String targetIp, String mac, String name) {
    StaticJsonDocument<256> doc;
    doc["ip"] = targetIp;
    doc["mac"] = mac;
    doc["name"] = name;
    doc["site"] = myConfig.site_name;

    char buffer[256];
    serializeJson(doc, buffer);

    int lastDot = targetIp.lastIndexOf('.');
    String lastByte = targetIp.substring(lastDot + 1);
    String fullTopic = myConfig.base_topic + lastByte;

    mqttClient.publish(fullTopic.c_str(), buffer);
}

#endif