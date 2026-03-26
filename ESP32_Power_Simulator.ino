#include <SPI.h>
#include <TFT_eSPI.h>       
#include <Wire.h>
#include <U8g2lib.h>        
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>
#include <WebServer.h>

// ==========================================
// 1. الإعدادات الأساسية
// ==========================================
const char* ssid = "Aa";
const char* password = "12345678";
String BOTtoken = "8539020845:AAG6fGfIBRHmXjQYrYNqE2N-gdhHB92J7mY";
String CHAT_ID = "5495272064"; 

const int BUZZER_PIN = 26; 

// ==========================================
// 2. تعريف الشاشات والاتصال
// ==========================================
TFT_eSPI tft = TFT_eSPI(); 
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 22, 21, U8X8_PIN_NONE);

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
WebServer server(80);

// ==========================================
// 3. المتغيرات وحالات النظام
// ==========================================
enum RectifierMode { SINGLE_HALF, SINGLE_FULL, THREE_HALF, THREE_FULL };
enum ViewMode { VIEW_VIN, VIEW_VOUT, VIEW_I_IN, VIEW_I_OUT, VIEW_ALL };
enum LoadType { LOAD_RESISTIVE, LOAD_INDUCTIVE };

RectifierMode currentRect = THREE_FULL; 
ViewMode currentView = VIEW_ALL;
LoadType currentLoad = LOAD_RESISTIVE; 
bool isPaused = false;

float Vm = 311.12; 
int t_offset = 0;
unsigned long lastBotCheck = 0;

// ==========================================
// 4. دوال واجهة الويب (Web Server Functions)
// ==========================================
void handleRoot() {
  String h = "<!DOCTYPE html><html dir='rtl'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Power Simulator IP Panel</title>";
  h += "<style>body{background:#1a1a1a;color:#fff;font-family:sans-serif;text-align:center;padding:10px;} .card{background:#2a2a2a;padding:15px;border-radius:10px;margin-bottom:15px;border:1px solid #444;}";
  h += ".btn{padding:12px;margin:5px;border:none;border-radius:5px;cursor:pointer;font-weight:bold;color:white;width:130px;font-size:14px;}";
  h += ".blue{background:#007bff;} .orange{background:#fd7e14;} .red{background:#dc3545;} .green{background:#28a745;}";
  h += "canvas{background:#000;border:1px solid #0f0;width:100%;max-width:500px;border-radius:5px;}</style></head><body>";
  
  h += "<h1>⚡ لوحة التحكم اللاسلكية</h1>";
  h += "<div class='card'><h3>الحالة: <span id='st' style='color:#0f0;'>يعمل</span> | Vdc: <span id='vdc' style='color:#ff0;'>0</span> V</h3></div>";

  h += "<div class='card'><h4>الرسم الحي للموجة (Vout)</h4><canvas id='wCan' height='100'></canvas></div>";

  h += "<div class='card'><h4>تغيير نوع الدائرة</h4>";
  h += "<button class='btn blue' onclick=\"fetch('/set?m=0')\">سنجل نصف</button>";
  h += "<button class='btn blue' onclick=\"fetch('/set?m=1')\">سنجل كامل</button>";
  h += "<button class='btn blue' onclick=\"fetch('/set?m=2')\">ثري نصف</button>";
  h += "<button class='btn blue' onclick=\"fetch('/set?m=3')\">ثري كامل</button></div>";

  h += "<div class='card'><h4>التحكم بالمحاكاة</h4>";
  h += "<button class='btn green' onclick=\"fetch('/set?p=0')\">استمرار ▶️</button>";
  h += "<button class='btn red' onclick=\"fetch('/set?p=1')\">توقف ⏸️</button></div>";

  h += "<script>let mode=3; let t=0; const wC=document.getElementById('wCan').getContext('2d');";
  h += "function draw(){ wC.clearRect(0,0,300,100); wC.beginPath(); wC.strokeStyle='#ff0'; wC.lineWidth=2;";
  h += "for(let x=0;x<300;x++){ let y=90; let wt=(x+t)*0.1; let v1=50*Math.sin(wt); let v2=50*Math.sin(wt-2.094); let v3=50*Math.sin(wt-4.188);";
  h += "if(mode==0) y=90-Math.max(0,v1); else if(mode==1) y=90-Math.abs(v1); else if(mode==2) y=90-Math.max(v1,Math.max(v2,v3)); else y=90-Math.max(Math.abs(v1-v2),Math.max(Math.abs(v1-v3),Math.abs(v2-v3)));";
  h += "wC.lineTo(x,y);} wC.stroke(); t+=2; requestAnimationFrame(draw);} draw();";
  h += "setInterval(()=>{fetch('/data').then(r=>r.json()).then(d=>{mode=d.m; document.getElementById('st').innerText=d.p?'متوقف ⏸️':'يعمل ▶️'; document.getElementById('vdc').innerText=d.v.toFixed(1);});},1000);</script>";
  
  h += "</body></html>";
  server.send(200, "text/html", h);
}

void handleData() {
  float Vdc = 0;
  if (currentRect == SINGLE_HALF) Vdc = Vm / 3.14159; 
  else if (currentRect == SINGLE_FULL) Vdc = (2 * Vm) / 3.14159; 
  else if (currentRect == THREE_HALF) Vdc = 0.827 * Vm;
  else if (currentRect == THREE_FULL) Vdc = 1.654 * Vm;
  
  String json = "{\"m\":" + String((int)currentRect) + ",\"v\":" + String(Vdc) + ",\"p\":" + String(isPaused?1:0) + "}";
  server.send(200, "application/json", json);
}

void handleUpdate() {
  if (server.hasArg("m")) currentRect = (RectifierMode)server.arg("m").toInt();
  if (server.hasArg("p")) isPaused = (server.arg("p") == "1");
  
  tone(BUZZER_PIN, 2500, 80); delay(100); tone(BUZZER_PIN, 2500, 80); 
  electricalTransition(); 
  updateOLEDInfo(); 
  drawCircuitDiagram();
  
  server.send(200, "text/plain", "OK");
}

// ==========================================
// 5. الدوال المساعدة للرسم (TFT & OLED)
// ==========================================
void drawDiode(int x, int y, uint16_t color) {
  tft.fillTriangle(x, y - 5, x, y + 5, x + 10, y, color); 
  tft.drawFastVLine(x + 10, y - 5, 11, color);           
}

void updateOLEDInfo() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  int x_off = 8; 

  String modeStr = "";
  float Vdc = 0;

  if (currentRect == SINGLE_HALF) {
    modeStr = "1-PH HALF"; Vdc = Vm / 3.14159; 
  } else if (currentRect == SINGLE_FULL) {
    modeStr = "1-PH FULL"; Vdc = (2 * Vm) / 3.14159; 
  } else if (currentRect == THREE_HALF) {
    modeStr = "3-PH HALF"; Vdc = 0.827 * Vm;
  } else if (currentRect == THREE_FULL) {
    modeStr = "3-PH FULL"; Vdc = 1.654 * Vm;
  }

  u8g2.drawStr(x_off, 12, ("SYS: " + modeStr).c_str());
  u8g2.drawHLine(x_off, 15, 110);
  
  char buf[30];
  sprintf(buf, "Load Type: %s", (currentLoad == LOAD_RESISTIVE) ? "Resistive" : "Inductive");
  u8g2.drawStr(x_off, 28, buf);
  
  sprintf(buf, "Peak Vm : %.1f V", Vm);  u8g2.drawStr(x_off, 42, buf);
  sprintf(buf, "V_dc Out: %.1f V", Vdc); u8g2.drawStr(x_off, 54, buf);
  
  if(isPaused) u8g2.drawStr(x_off, 64, "STATUS: [PAUSED]");
  else u8g2.drawStr(x_off, 64, "STATUS: [LIVE]");
  
  u8g2.sendBuffer();
}

void drawCircuitDiagram() {
  tft.fillRect(0, 0, 320, 85, TFT_LIGHTGREY); 
  tft.setTextColor(TFT_BLACK);
  
  int y_top = 30;    
  int y_input = 45;  
  int y_bottom = 60; 
  int x_off_load = 180; 

  String loadText = (currentLoad == LOAD_RESISTIVE) ? "R" : "R+L";

  if (currentRect == SINGLE_HALF) {
    tft.drawString("1-Phase Half-Wave Rectifier", 40, 5, 2);
    tft.fillCircle(30, 45, 6, TFT_BLUE);
    tft.drawString("~", 27, 40, 1);
    tft.drawFastVLine(30, 25, 20, TFT_BLACK);
    tft.drawFastHLine(30, 25, 40, TFT_BLACK);
    drawDiode(70, 25, TFT_RED); 
    tft.drawFastHLine(80, 25, 100, TFT_BLACK); 
    tft.drawFastVLine(180, 25, 10, TFT_BLACK);
    tft.drawFastHLine(30, 65, 150, TFT_BLACK); 
    tft.drawFastVLine(30, 51, 14, TFT_BLACK);
    tft.drawFastVLine(180, 55, 10, TFT_BLACK);
    tft.fillRect(170, 35, 20, 20, TFT_WHITE);
    tft.drawRect(170, 35, 20, 20, TFT_BLACK);
    tft.drawString(loadText, 172, 38, 2);
  } 
  else if (currentRect == SINGLE_FULL) {
    tft.drawString("1-Phase Full-Wave Bridge", 40, 5, 2);
    int x_start = 80; 
    for (int i = 0; i < 2; i++) {
        int x_pos = x_start + (i * 40); 
        drawDiode(x_pos, y_top, TFT_RED); 
        tft.drawFastVLine(x_pos + 5, y_input, (y_input - y_top) + 5, TFT_BLACK); 
        drawDiode(x_pos, y_bottom, TFT_RED); 
        tft.drawFastVLine(x_pos + 5, y_bottom, (y_input - y_top) + 5, TFT_BLACK); 
    }
    tft.fillCircle(30, y_input, 6, TFT_BLUE);
    tft.drawString("~", 27, y_input - 5, 1);
    tft.drawLine(36, y_input, x_start + 5, y_input, TFT_BLUE); 
    tft.drawFastVLine(30, y_input + 6, 15, TFT_BLUE);
    tft.drawFastHLine(30, y_input + 21, 95, TFT_BLUE);
    tft.drawFastVLine(125, y_input + 5, 16, TFT_BLUE); 
    tft.drawFastHLine(x_start + 5, y_top, 40, TFT_BLACK); 
    tft.drawFastHLine(x_start + 5, y_bottom + 5, 40, TFT_BLACK); 
    tft.drawFastHLine(x_start + 45, y_top, (x_off_load - (x_start + 45)), TFT_BLACK); 
    tft.drawFastHLine(x_start + 45, y_bottom + 5, (x_off_load - (x_start + 45)), TFT_BLACK); 
    tft.fillRect(x_off_load, y_top - 10, 30, (y_bottom - y_top) + 30, TFT_BLACK); 
    tft.fillRect(x_off_load + 2, y_top - 8, 26, (y_bottom - y_top) + 26, TFT_WHITE); 
    tft.drawString(loadText, x_off_load + 4, y_input - 5, 2);
    tft.drawString("+ DC -", 130, y_input - 5, 2);
  }
  else if (currentRect == THREE_HALF) {
    tft.drawString("3-Phase Half-Wave Rectifier", 40, 5, 2);
    for (int i = 0; i < 3; i++) {
      int y_pos = 25 + (i * 20);
      tft.fillCircle(20, y_pos, 4, (i==0)?TFT_BLUE : (i==1)?TFT_GREEN : TFT_MAGENTA);
      tft.drawFastHLine(24, y_pos, 36, TFT_BLACK); 
      drawDiode(60, y_pos, TFT_RED); 
      tft.drawFastHLine(70, y_pos, 40, TFT_BLACK); 
    }
    tft.drawFastVLine(110, 25, 40, TFT_BLACK); 
    tft.drawFastHLine(110, 45, 30, TFT_BLACK); 
    tft.drawString(loadText, 145, 38, 2);
  } 
  else if (currentRect == THREE_FULL) {
    tft.drawString("3-Phase Full-Bridge Rectifier", 40, 5, 2);
    int x_start = 60; 
    for (int i = 0; i < 3; i++) {
        int x_pos = x_start + (i * 40); 
        drawDiode(x_pos, y_top, TFT_RED); 
        tft.drawFastVLine(x_pos + 5, y_input, (y_input - y_top) + 5, TFT_BLACK); 
        drawDiode(x_pos, y_bottom, TFT_RED); 
        tft.drawFastVLine(x_pos + 5, y_bottom, (y_input - y_top) + 5, TFT_BLACK); 
        tft.fillCircle(20, y_input + (i * 10), 3, (i==0)?TFT_BLUE : (i==1)?TFT_GREEN : TFT_MAGENTA); 
        tft.drawLine(23, y_input + (i * 10), x_pos + 5, y_input, (i==0)?TFT_BLUE : (i==1)?TFT_GREEN : TFT_MAGENTA);
    }
    tft.drawFastHLine(x_start + 5, y_top, (3 * 40), TFT_BLACK); 
    tft.drawFastHLine(x_start + 5, y_bottom + 5, (3 * 40), TFT_BLACK); 
    tft.drawFastHLine(x_start + (3 * 40) + 5, y_top, (x_off_load - (x_start + (3 * 40) + 5)), TFT_BLACK); 
    tft.drawFastHLine(x_start + (3 * 40) + 5, y_bottom + 5, (x_off_load - (x_start + (3 * 40) + 5)), TFT_BLACK); 
    tft.fillRect(x_off_load, y_top - 10, 30, (y_bottom - y_top) + 30, TFT_BLACK); 
    tft.fillRect(x_off_load + 2, y_top - 8, 26, (y_bottom - y_top) + 26, TFT_WHITE); 
    tft.drawString(loadText, x_off_load + 4, y_input - 5, 2);
    tft.drawString("+ DC -", 160, y_input - 5, 2); 
  }
}

void drawSimulation() {
  if (isPaused) return;
  
  tft.fillRect(0, 85, 320, 155, TFT_BLACK); 
  tft.drawFastHLine(0, 160, 320, TFT_DARKGREY); 

  float Vdc = 0;
  if (currentRect == SINGLE_HALF) Vdc = Vm / 3.14159; 
  else if (currentRect == SINGLE_FULL) Vdc = (2 * Vm) / 3.14159; 
  else if (currentRect == THREE_HALF) Vdc = 0.827 * Vm;
  else if (currentRect == THREE_FULL) Vdc = 1.654 * Vm;

  for (int x = 0; x < 320; x++) {
    float wt = (x + t_offset) * 0.05;
    float Van = Vm * sin(wt);
    float Vbn = Vm * sin(wt - 2.094); 
    float Vcn = Vm * sin(wt - 4.188); 
    float Vout = 0;
    
    if (currentRect == SINGLE_HALF) Vout = std::max(0.0f, Van); 
    else if (currentRect == SINGLE_FULL) Vout = std::abs(Van); 
    else if (currentRect == THREE_HALF) Vout = std::max({Van, Vbn, Vcn});
    else if (currentRect == THREE_FULL) Vout = std::max({Van-Vbn, Van-Vcn, Vbn-Vcn, Vbn-Van, Vcn-Van, Vcn-Vbn});
    
    if (currentView == VIEW_VIN || currentView == VIEW_ALL) {
      tft.drawPixel(x, 160 - (Van/6), TFT_BLUE);
      if (currentRect == THREE_HALF || currentRect == THREE_FULL) {
        tft.drawPixel(x, 160 - (Vbn/6), TFT_GREEN);
        tft.drawPixel(x, 160 - (Vcn/6), TFT_MAGENTA);
      }
    }
    
    if (currentView == VIEW_VOUT || currentView == VIEW_ALL) {
      tft.drawPixel(x, 160 - (Vout/6), TFT_YELLOW);
    }
    
    float I_out = (currentLoad == LOAD_RESISTIVE) ? (Vout / 6) : (Vdc / 6);

    float I_in = 0;
    if (currentRect == SINGLE_HALF) {
        if (Van > 0) I_in = I_out;
    } else if (currentRect == SINGLE_FULL) {
        if (Van > 0) I_in = I_out;
        else I_in = -I_out;
    } else if (currentRect == THREE_HALF) {
        if (Vout == Van) I_in = I_out;
    } else if (currentRect == THREE_FULL) {
        if (Vout == Van - Vbn || Vout == Van - Vcn) I_in = I_out;
        else if (Vout == Vbn - Van || Vout == Vcn - Van) I_in = -I_out;
    }

    if (currentView == VIEW_I_IN || currentView == VIEW_ALL) {
      tft.drawPixel(x, 160 - I_in, TFT_CYAN); 
    }

    if (currentView == VIEW_I_OUT || currentView == VIEW_ALL) {
      tft.drawPixel(x, 160 - I_out, TFT_RED); 
    }
  }
}

// ==========================================
// 6. التأثيرات السينمائية
// ==========================================
void electricalTransition() {
  bool previousPauseState = isPaused;
  isPaused = true; 
  for(int i = 0; i < 25; i++) {
    int x1 = random(0, 320);
    int y1 = random(85, 240);
    int x2 = x1 + random(-50, 50);
    int y2 = y1 + random(-50, 50);
    tft.drawLine(x1, y1, x2, y2, TFT_CYAN);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, TFT_WHITE); 
    delay(15); 
  }
  for(int x = 0; x < 320; x += 20) {
    tft.fillRect(x, 85, 20, 155, TFT_BLACK);
    delay(15); 
  }
  tft.drawFastHLine(0, 160, 320, TFT_DARKGREY);
  isPaused = previousPauseState; 
}

// ==========================================
// 7. معالجة أوامر تليجرام
// ==========================================
void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    text.trim(); 
    String chat_id = String(bot.messages[i].chat_id);
    
    Serial.print("==== أمر جديد وصل ==== : [");
    Serial.print(text);
    Serial.println("]");

    // ------ قائمة الأوامر التفصيلية (تم تعديلها لتصبح جداول) ------
    if (text == "الاوامر" || text == "القائمة" || text == "/help" || text == "اوامر") {
      String helpMsg = "📋 *دليل التحكم بنظام المحاكاة*\n";
      helpMsg += "────────────────\n\n";
      
      helpMsg += "🔌 *أولاً: نوع الدائرة*\n";
      helpMsg += "```text\n";
      helpMsg += "| الأمر       | الدائرة المستهدفة   |\n";
      helpMsg += "|-------------|---------------------|\n";
      helpMsg += "| سنجل نصف    | 1-Phase Half-Wave   |\n";
      helpMsg += "| سنجل كامل   | 1-Phase Full-Bridge |\n";
      helpMsg += "| ثري نصف     | 3-Phase Half-Wave   |\n";
      helpMsg += "| ثري كامل    | 3-Phase Full-Bridge |\n";
      helpMsg += "```\n";
      
      helpMsg += "💡 *ثانياً: نوع الحمل*\n";
      helpMsg += "```text\n";
      helpMsg += "| الأمر       | وصف الحمل           |\n";
      helpMsg += "|-------------|---------------------|\n";
      helpMsg += "| مقاومة      | Resistive Load (R)  |\n";
      helpMsg += "| حثي         | Inductive Load (R+L)|\n";
      helpMsg += "```\n";
      
      helpMsg += "📊 *ثالثاً: عرض الموجات*\n";
      helpMsg += "```text\n";
      helpMsg += "| الأمر       | الموجة المطلوبة     |\n";
      helpMsg += "|-------------|---------------------|\n";
      helpMsg += "| جهد الدخل   | Source Voltage (AC) |\n";
      helpMsg += "| جهد الخرج   | Output Voltage (DC) |\n";
      helpMsg += "| تيار الدخل  | Input Current (Is)  |\n";
      helpMsg += "| تيار الخرج  | Load Current (Io)   |\n";
      helpMsg += "| الكل        | عرض جميع المسارات   |\n";
      helpMsg += "```\n";
      
      helpMsg += "⏸️ *رابعاً: تحكم إضافي*\n";
      helpMsg += "```text\n";
      helpMsg += "| الأمر       | الوظيفة             |\n";
      helpMsg += "|-------------|---------------------|\n";
      helpMsg += "| توقف        | إيقاف حركة الموجات  |\n";
      helpMsg += "| استمرار     | استئناف حركة الموجات|\n";
      helpMsg += "```\n";
      
      bot.sendMessage(chat_id, helpMsg, "Markdown");
      continue; 
    }

    // 1. أوامر نوع الدائرة
    if (text == "1" || text == "سنجل نصف" || text == "١") currentRect = SINGLE_HALF;
    else if (text == "2" || text == "سنجل كامل" || text == "٢") currentRect = SINGLE_FULL;
    else if (text == "3" || text == "ثري نصف" || text == "٣") currentRect = THREE_HALF;
    else if (text == "4" || text == "ثري كامل" || text == "٤") currentRect = THREE_FULL;
    
    // 2. أوامر نوع الحمل
    else if (text == "مقاومة") currentLoad = LOAD_RESISTIVE;
    else if (text == "حثي") currentLoad = LOAD_INDUCTIVE;

    // 3. أوامر العرض والموجات
    else if (text == "جهد الدخل") currentView = VIEW_VIN;
    else if (text == "جهد الخرج") currentView = VIEW_VOUT;
    else if (text == "تيار الدخل") currentView = VIEW_I_IN;
    else if (text == "تيار الخرج") currentView = VIEW_I_OUT;
    else if (text == "الكل") currentView = VIEW_ALL;
    
    // 4. أوامر التحكم
    else if (text == "توقف") isPaused = true;
    else if (text == "استمرار") isPaused = false;
    else {
      bot.sendMessage(chat_id, "عذراً، أمر غير معروف. أرسل كلمة (الاوامر) لعرض القائمة.", "");
      continue; 
    }

    tone(BUZZER_PIN, 2500, 80);  
    delay(100);
    tone(BUZZER_PIN, 2500, 80);  

    electricalTransition(); 
    updateOLEDInfo();
    drawCircuitDiagram(); 
    
    bot.sendMessage(chat_id, "تم التنفيذ بنجاح ✅: " + text, "");
  }
}

// ==========================================
// 8. دالة الإعداد
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(BUZZER_PIN, OUTPUT);
  tone(BUZZER_PIN, 2000, 100);
  delay(150);
  tone(BUZZER_PIN, 3000, 200);

  u8g2.begin();
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting WiFi...", 10, 10, 2);

  WiFi.begin(ssid, password);
  client.setInsecure(); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: "); 
  Serial.println(WiFi.localIP()); 
  
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/set", handleUpdate);
  server.begin();

  tft.fillScreen(TFT_BLACK);
  tft.drawString("Syncing Time...", 10, 10, 2);
  
  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    now = time(nullptr);
  }
  Serial.println("Time Synced Successfully!");

  String welcomeMsg = "✅ *مختبر محاكاة إلكترونيات القدرة مستعد!*\n\n";
  welcomeMsg += "أرسل كلمة `الاوامر` لعرض القائمة التفصيلية للتحكم بالدوائر والموجات.";
  
  bot.sendMessage(CHAT_ID, welcomeMsg, "Markdown");
  
  drawCircuitDiagram();
  updateOLEDInfo();
}

// ==========================================
// 9. الحلقة الرئيسية
// ==========================================
void loop() {
  server.handleClient(); 

  if (millis() - lastBotCheck > 2000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages > 0) {
      handleMessages(numNewMessages);
    }
    lastBotCheck = millis();
  }

  if (!isPaused) {
    t_offset += 3; 
    drawSimulation();
  }

  delay(10); 
}
