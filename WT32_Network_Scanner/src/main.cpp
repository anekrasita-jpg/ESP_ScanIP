#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config_manager.h"
#include "web_server_setup.h"
#include "mqtt_handler.h"
#include "network_scanner.h"
#include "whitelist_manager.h" // ต้อง include ไฟล์จัดการ Whitelist ด้วย

// กำหนดเวลา Timeout สำหรับ Watchdog (30 วินาที)
#define WDT_TIMEOUT 30 

DeviceConfig myConfig;

void setup() {
  // 1. เริ่มต้น Serial สำหรับ Debug
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- PWA NETWORK SCANNER STARTING ---");

  // 2. ตั้งค่า Watchdog Timer
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL); 

  // 3. โหลดค่า Config และ Whitelist จาก Flash Memory
  myConfig = loadConfig();
  loadWhitelist(); 

  // 4. เริ่มต้นระบบ Wi-Fi AP สำหรับตั้งค่าหน้างาน (192.168.1.254)
  startWebConfig();

  // 5. เริ่มต้นระบบ Ethernet (LAN)
  initEthernet();

  Serial.println(">>> System Initialized. Waiting for Network...");
}

void loop() {
  // --- "เตะหมา" (Reset Watchdog) ทุกรอบลูป ---
  esp_task_wdt_reset();

  // รักษาการทำงานของ Web Server (หน้าเว็บตั้งค่า)
  handleWebClient();

  // ตรวจสอบการเชื่อมต่อ Ethernet
  if (ETH.linkUp()) {
    // ตรวจสอบและรักษาการเชื่อมต่อ MQTT
    if (!mqttClient.connected()) {
      long now = millis();
      if (now - lastMqttReconnectAttempt > 5000) {
        lastMqttReconnectAttempt = now;
        reconnectMQTT();
        sendRemoteLog("System Booted. Firmware version 1.0.0 Ready.");
      }
    } else {
      // ทำงานในส่วน MQTT (รับ/ส่งข้อมูล)
      mqttClient.loop();
      
      // --- ระบบ Scan อัตโนมัติตามช่วงเวลา ---
      unsigned long currentMillis = millis();
      // แปลงวินาทีเป็น Milliseconds (เช่น 900 x 1000 = 900,000 ms หรือ 15 นาที)
      if (currentMillis - lastScanTime > (myConfig.scan_interval * 1000)) {
        lastScanTime = currentMillis;
        
        // เริ่มต้นการสแกน และตรวจสอบ Whitelist ภายในฟังก์ชันนี้
        runNetworkScan(); 
      }
    }
  }
}