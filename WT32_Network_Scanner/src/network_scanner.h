#ifndef NETWORK_SCANNER_H
#define NETWORK_SCANNER_H

#include <ETH.h>
#include <ESP32Ping.h>
#include <esp_task_wdt.h>
#include <lwip/etharp.h> // สำหรับจัดการตาราง ARP
#include "mqtt_handler.h"
#include "config_manager.h"
#include "whitelist_manager.h"

extern DeviceConfig myConfig;
unsigned long lastScanTime = 0;

/**
 * ฟังก์ชันดึง MAC Address จากตาราง ARP (Address Resolution Protocol)
 * โดยจะค้นหาคู่ IP-MAC ที่ระบบบันทึกไว้หลังจากมีการรับ-ส่งข้อมูล (เช่น หลัง Ping)
 */
String getMacFromIP(IPAddress ip) {
    struct eth_addr *eth_ret;
    const ip4_addr_t *ip_ret;
    
    // แปลง IPAddress (Arduino) เป็น ip4_addr_t (LwIP) แบบปลอดภัย
    ip4_addr_t target_addr;
    target_addr.addr = (uint32_t)ip;

    // ค้นหาในตาราง ARP ของระบบเน็ตเวิร์ก
    if (etharp_find_addr(NULL, &target_addr, &eth_ret, &ip_ret) >= 0) {
        char macStr[18];
        // ใช้ sprintf เพื่อจัดรูปแบบเลขฐาน 16 ให้เป็น XX:XX:XX:XX:XX:XX
        sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                eth_ret->addr[0], eth_ret->addr[1], eth_ret->addr[2],
                eth_ret->addr[3], eth_ret->addr[4], eth_ret->addr[5]);
        return String(macStr);
    }
    return "00:00:00:00:00:00"; // ส่งค่ากลับเป็น 0 หากไม่พบในตาราง
}

/**
 * ฟังก์ชันหลักในการวนสแกน IP ทั้งวง (Class C)
 */
void runNetworkScan() {
    Serial.println("\n>>> [Scanner] Starting network scan...");
    sendRemoteLog("Starting network scan..."); // ส่ง Log บอกว่าเริ่มสแกนแล้ว
    
    // ตรวจสอบว่า Ethernet เชื่อมต่ออยู่หรือไม่ก่อนเริ่ม
    if (!ETH.linkUp()) {
        Serial.println(">>> [Scanner] Aborted: Ethernet not connected.");
        sendRemoteLog("Error: Ethernet not connected. Aborting scan.");
        return;
    }

    IPAddress localIP = ETH.localIP();
    // ดึงเฉพาะ 3 ชุดแรกของ IP (เช่น 192.168.1.)
    String baseIP = String(localIP[0]) + "." + String(localIP[1]) + "." + String(localIP[2]) + ".";
    
    int foundCount = 0;

    // วนลูปตั้งแต่ .1 ถึง .254
    for (int i = 1; i < 255; i++) {
        // 1. เตะหมา (Watchdog Reset) ทุกครั้งที่ขยับ IP เพื่อไม่ให้บอร์ดรีสตาร์ท
        esp_task_wdt_reset();

        String targetStr = baseIP + String(i);
        IPAddress targetIP;
        targetIP.fromString(targetStr);

        // ข้าม IP ของตัวเองเพื่อประหยัดเวลา
        //if (targetIP == localIP) continue;

        // 2. ส่ง Ping 1 ครั้งเพื่อเช็คสถานะและกระตุ้น ARP Table
        if (Ping.ping(targetIP, 1)) {
            delay(10); 
            String macAddr = getMacFromIP(targetIP);
            macAddr.toUpperCase();

            StaticJsonDocument<256> alertDoc;
            alertDoc["ip"] = targetStr;
            alertDoc["mac"] = macAddr;
            alertDoc["site"] = myConfig.site_name;

            // ตรวจสอบ Whitelist
            if (isWhitelisted(macAddr)) {
                // --- กรณี: เจอใน List (ปกติ) ---
                alertDoc["status"] = "OK";
                alertDoc["name"] = "Registered-Device";
        
                char buffer[256];
                serializeJson(alertDoc, buffer);
        
                // ส่งไป Topic ปกติ (แยกตาม IP)
                int lastDot = targetStr.lastIndexOf('.');
                String fullTopic = myConfig.base_topic + targetStr.substring(lastDot + 1);
                mqttClient.publish(fullTopic.c_str(), buffer);
            } 
            else {
                // --- กรณี: ไม่เจอใน List (คนแปลกหน้า!) ---
                alertDoc["status"] = "UNKNOWN / INTRUDER!";
                alertDoc["name"] = "Unknown-Device";
        
                char buffer[256];
                serializeJson(alertDoc, buffer);
        
                // ส่งไปที่ Alert Topic ที่ตั้งไว้ใน Web Config
                mqttClient.publish(myConfig.alert_topic.c_str(), buffer);
        
                Serial.printf("!!! [ALERT] Unknown Device: %s | MAC: %s\n", targetStr.c_str(), macAddr.c_str());
                sendRemoteLog("ALERT: Intruder detected at " + targetStr + " (" + macAddr + ")");
            }
            foundCount++;
        }

        // 5. ปล่อยให้ Web Server และ MQTT Background Process ทำงานได้
        handleWebClient(); 
        mqttClient.loop();
    }

    Serial.printf(">>> [Scanner] Scan Finished. Total found: %d devices.\n\n", foundCount);
    sendRemoteLog("Scan Finished. Found " + String(foundCount) + " devices.");
}

#endif