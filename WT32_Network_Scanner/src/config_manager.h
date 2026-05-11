#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// โครงสร้างข้อมูลที่ปรับปรุงตามความต้องการทั้ง 8 ข้อ
struct DeviceConfig {
  // 1. Network Settings
  bool is_dhcp;
  String static_ip;
  String subnet;
  String gateway;

  // 2. MQTT Broker Settings
  String mqtt_host;
  String mqtt_user;
  String mqtt_pass;

  // 3. Identity
  String site_name;

  // 4. Topics Setup
  String base_topic;      // ส่วนแรกของ Topic เช่น 5531012/BB/SCAN/IP/
  String whitelist_topic; // รับค่า IP ไปเก็บใน list
  String alert_topic;     // แจ้งเตือนเมื่อเจอ IP แปลกปลอม

  // 5. Timing Settings
  int mqtt_interval;      // ความถี่ส่งข้อมูลไป MQTT (วินาที)
  int scan_interval;      // ความถี่การ Scan IP (วินาที)

  // Admin Setup
  String admin_pass;
};

Preferences preferences;

// ฟังก์ชันบันทึกค่าลง Flash Memory
void saveConfig(DeviceConfig cfg) {
  preferences.begin("pwa-config", false);
  
  preferences.putBool("is_dhcp", cfg.is_dhcp);
  preferences.putString("ip", cfg.static_ip);
  preferences.putString("sn", cfg.subnet);
  preferences.putString("gw", cfg.gateway);
  
  preferences.putString("m_host", cfg.mqtt_host);
  preferences.putString("m_user", cfg.mqtt_user);
  preferences.putString("m_pass", cfg.mqtt_pass);
  
  preferences.putString("site", cfg.site_name);
  preferences.putString("t_base", cfg.base_topic);
  preferences.putString("t_white", cfg.whitelist_topic);
  preferences.putString("t_alert", cfg.alert_topic);
  
  preferences.putInt("m_int", cfg.mqtt_interval);
  preferences.putInt("s_int", cfg.scan_interval);
  preferences.putString("a_pass", cfg.admin_pass);
  
  preferences.end();
  Serial.println(">>> [Config] All settings saved to Flash!");
}

// ฟังก์ชันดึงค่าจาก Flash Memory
DeviceConfig loadConfig() {
  preferences.begin("pwa-config", true);
  DeviceConfig cfg;
  
  // โหลดค่าพร้อมกำหนดค่าเริ่มต้น (Default) กรณีที่ยังไม่เคยเซฟมาก่อน
  cfg.is_dhcp    = preferences.getBool("is_dhcp", true); // เริ่มต้นเป็น DHCP
  cfg.static_ip  = preferences.getString("ip", "192.168.1.50");
  cfg.subnet     = preferences.getString("sn", "255.255.255.0");
  cfg.gateway    = preferences.getString("gw", "192.168.1.1");
  
  cfg.mqtt_host  = preferences.getString("m_host", "192.168.1.100");
  cfg.mqtt_user  = preferences.getString("m_user", "pwa_user");
  cfg.mqtt_pass  = preferences.getString("m_pass", "pwa_pass");
  
  cfg.site_name  = preferences.getString("site", "PWA_Station_01");
  cfg.base_topic = preferences.getString("t_base", "5531012/BB/SCAN/IP/");
  cfg.whitelist_topic = preferences.getString("t_white", "5531012/BB/SCAN/WHITELIST");
  cfg.alert_topic     = preferences.getString("t_alert", "5531012/BB/SCAN/ALARM");
  
  cfg.mqtt_interval = preferences.getInt("m_int", 60);   // ส่งทุก 60 วิ
  cfg.scan_interval = preferences.getInt("s_int", 300);  // สแกนทุก 5 นาที
  cfg.admin_pass    = preferences.getString("a_pass", "admin1234");
  
  preferences.end();
  Serial.println(">>> [Config] Loaded settings from Flash.");
  return cfg;
}

#endif