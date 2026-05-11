#ifndef WEB_SERVER_SETUP_H
#define WEB_SERVER_SETUP_H

#include <WiFi.h>
#include <WebServer.h>
#include "config_manager.h"

WebServer server(80);
extern DeviceConfig myConfig; 

// หน้า HTML UI ที่ปรับปรุงใหม่ตามความต้องการทั้ง 8 ข้อ
void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1' charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: sans-serif; background: #f0f2f5; padding: 20px; }";
  html += ".card { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h2 { color: #1a73e8; text-align: center; border-bottom: 2px solid #e8f0fe; padding-bottom: 10px; }";
  html += "h3 { font-size: 1.1em; color: #5f6368; margin-top: 20px; border-left: 4px solid #1a73e8; padding-left: 10px; }";
  html += "label { display: block; margin-top: 10px; font-weight: bold; font-size: 0.9em; }";
  html += "input, select { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #dadce0; border-radius: 4px; box-sizing: border-box; }";
  html += ".btn { width: 100%; padding: 15px; background: #1a73e8; color: white; border: none; border-radius: 4px; font-weight: bold; margin-top: 30px; cursor: pointer; }";
  html += ".note { font-size: 0.8em; color: #70757a; margin-top: 5px; }";
  html += "</style>";
  html += "<script>function toggleStatic(){ var isDhcp = document.getElementById('is_dhcp').value == '1'; document.getElementById('static_fields').style.display = isDhcp ? 'none' : 'block'; }</script>";
  html += "</head><body onload='toggleStatic()'>";
  
  html += "<div class='card'><h2>PWA Scanner Setup</h2>";
  html += "<form action='/save' method='POST'>";
  
  // 1. Network Settings
  html += "<h3>1. Network Settings (LAN)</h3>";
  html += "<label>IP Assignment</label>";
  html += "<select name='is_dhcp' id='is_dhcp' onchange='toggleStatic()'>";
  html += "<option value='1'" + String(myConfig.is_dhcp ? " selected" : "") + ">DHCP (อัตโนมัติ)</option>";
  html += "<option value='0'" + String(!myConfig.is_dhcp ? " selected" : "") + ">Static IP (ระบุเอง)</option>";
  html += "</select>";
  
  html += "<div id='static_fields'>";
  html += "<label>Static IP</label><input type='text' name='ip' value='" + myConfig.static_ip + "'>";
  html += "<label>Subnet Mask</label><input type='text' name='sn' value='" + myConfig.subnet + "'>";
  html += "<label>Gateway</label><input type='text' name='gw' value='" + myConfig.gateway + "'>";
  html += "</div>";

  // 2. MQTT Broker
  html += "<h3>2. MQTT Broker</h3>";
  html += "<label>Broker Host/IP</label><input type='text' name='m_host' value='" + myConfig.mqtt_host + "'>";
  html += "<label>MQTT User</label><input type='text' name='m_user' value='" + myConfig.mqtt_user + "'>";
  html += "<label>MQTT Password</label><input type='password' name='m_pass' value='" + myConfig.mqtt_pass + "'>";

  // 3. Identity & Topics
  html += "<h3>3. Identity & Topics</h3>";
  html += "<label>Site Name</label><input type='text' name='site' value='" + myConfig.site_name + "'>";
  html += "<label>Base Topic (ส่วนแรก)</label><input type='text' name='t_base' value='" + myConfig.base_topic + "'>";
  html += "<p class='note'>*ระบบจะต่อท้ายด้วย IP ให้อัตโนมัติ</p>";
  html += "<label>Whitelist Topic</label><input type='text' name='t_white' value='" + myConfig.whitelist_topic + "'>";
  html += "<label>Alert Topic</label><input type='text' name='t_alert' value='" + myConfig.alert_topic + "'>";

  // 4. Timing
  html += "<h3>4. Timing (Seconds)</h3>";
  html += "<label>MQTT Send Interval (วินาที)</label><input type='number' name='m_int' value='" + String(myConfig.mqtt_interval) + "'>";
  html += "<label>IP Scan Interval (วินาที)</label><input type='number' name='s_int' value='" + String(myConfig.scan_interval) + "'>";

  html += "<button type='submit' class='btn'>SAVE & RESTART SYSTEM</button>";
  html += "</form></div></body></html>";
  
  server.send(200, "text/html", html);
}

// ฟังก์ชันประมวลผลการบันทึกค่า
void handleSave() {
  if (server.hasArg("is_dhcp")) myConfig.is_dhcp = (server.arg("is_dhcp") == "1");
  if (server.hasArg("ip"))      myConfig.static_ip = server.arg("ip");
  if (server.hasArg("sn"))      myConfig.subnet = server.arg("sn");
  if (server.hasArg("gw"))      myConfig.gateway = server.arg("gw");
  
  if (server.hasArg("m_host"))  myConfig.mqtt_host = server.arg("m_host");
  if (server.hasArg("m_user"))  myConfig.mqtt_user = server.arg("m_user");
  if (server.hasArg("m_pass"))  myConfig.mqtt_pass = server.arg("m_pass");
  
  if (server.hasArg("site"))    myConfig.site_name = server.arg("site");
  if (server.hasArg("t_base"))  myConfig.base_topic = server.arg("t_base");
  if (server.hasArg("t_white")) myConfig.whitelist_topic = server.arg("t_white");
  if (server.hasArg("t_alert")) myConfig.alert_topic = server.arg("t_alert");
  
  if (server.hasArg("m_int"))   myConfig.mqtt_interval = server.arg("m_int").toInt();
  if (server.hasArg("s_int"))   myConfig.scan_interval = server.arg("s_int").toInt();

  saveConfig(myConfig); // บันทึกลง Flash ผ่าน Step 1

  String s = "<html><meta charset='UTF-8'><body><center><h2>บันทึกสำเร็จ! บอร์ดกำลังรีสตาร์ท...</h2><p>กรุณารอสักครู่แล้วตรวจสอบในระบบ MQTT</p></center></body></html>";
  server.send(200, "text/html", s);
  
  delay(2000);
  ESP.restart();
}

void startWebConfig() {
  IPAddress apIP(192, 168, 1, 254);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("PWA_Scanner_Setup", "admin1234"); 

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Web Server Ready at 192.168.1.254");
}

void handleWebClient() {
  server.handleClient();
}

#endif