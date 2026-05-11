#ifndef WHITELIST_MANAGER_H
#define WHITELIST_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

void sendFeedback(String message);
void sendRemoteLog(String logMsg);

extern Preferences preferences;
String whitelist_string = ""; // รูปแบบ: "MAC1,MAC2,MAC3,"

// โหลด Whitelist จาก Flash ตอนเปิดเครื่อง
void loadWhitelist() {
    preferences.begin("pwa-config", true);
    whitelist_string = preferences.getString("w_list", "");
    preferences.end();
    Serial.println(">>> [Whitelist] Loaded: " + whitelist_string);
}

// บันทึก Whitelist ใหม่ (แบบเขียนทับ)
void saveWhitelist(String new_list) {
    preferences.begin("pwa-config", false);
    preferences.putString("w_list", new_list);
    preferences.end();
    whitelist_string = new_list;
    Serial.println(">>> [Whitelist] New list saved to Flash");
}

// ตรวจสอบว่า MAC นี้อยู่ใน List หรือไม่
bool isWhitelisted(String mac) {
    mac.toUpperCase();
    // ค้นหา MAC ใน String (ตรวจสอบว่ามีอยู่ในรายชื่อไหม)
    return (whitelist_string.indexOf(mac) != -1);
}

// จัดการ JSON ที่ส่งมาจาก MQTT (Update Whitelist)
// จัดการ JSON ที่ส่งมาจาก MQTT (Update Whitelist)
void updateWhitelistFromJson(JsonArray arr) {
    String temp_list = "";
    for (JsonVariant v : arr) {
        String m = v.as<String>();
        m.toUpperCase();
        temp_list += m + ","; 
    }

    // --- ส่วนที่เพิ่มการตรวจสอบ ---
    if (temp_list == whitelist_string) {
        Serial.println(">>> [Whitelist] Data is identical to current list. No update needed.");
        // ส่ง Feedback บอกสักนิดว่าไม่มีการเปลี่ยนแปลง
        sendRemoteLog("Whitelist sync ignored: no changes detected.");
        sendFeedback("Whitelist is already up-to-date. No restart.");
    } else {
        Serial.println(">>> [Whitelist] Data changed! Updating and Restarting...");
        sendRemoteLog("Whitelist changed! Updating flash and restarting...");
        saveWhitelist(temp_list);
        sendFeedback("Whitelist updated and system restarting...");
        
        delay(2000);
        ESP.restart();
    }
}

#endif