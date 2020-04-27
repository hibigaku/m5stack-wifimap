#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String key = YOUR_API_KEY;
String host_geolocate = "https://www.googleapis.com/geolocation/v1/geolocate?";
String host_staticmap = "https://maps.googleapis.com/maps/api/staticmap?";
const size_t MAX_AP = 3;   //リスエストする最大AP数
float lat, lng;
int accuracy;
uint8_t buff[320 * 220] = { 0 };

void setup() {
  Serial.begin(115200);
  
  M5.begin(); //M5Stackオブジェクトの初期化
  displayStatus("scan SSID");
  
  delay(100);

  connectWiFi();
}

void displayStatus(String state) 
{
  M5.Lcd.setTextSize(2);
  M5.Lcd.fillScreen(BLACK); //LCDをクリア
  M5.Lcd.setCursor(0, 0);   //カーソル位置設定
  M5.Lcd.println(state);
  delay(500);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);  //STAモード（子機）として使用
  WiFi.disconnect();    //Wi-Fi切断

  WiFi.begin("SSID", "pass");
  while (WiFi.status() != WL_CONNECTED) {
    displayStatus("WiFi Connected.");
    displayStatus("Press button A:");
  }
  delay(1000);
}

void loop() {
  M5.update();
 
  if (M5.BtnA.wasReleased()) {
    clickBtnA();
  }
}

void clickBtnA()
{
  displayStatus("Scanning...");
  String json_request = scanWiFiNetworks();
  displayStatus("Scan finishted.");
  if (json_request.length()) {
    if (getGeolocation(json_request)) {
      char pos[32];
      sprintf(pos, "Lat:%.4f Lng:%.4f", lat, lng);
      displayStatus(pos);
      displayMap();
    } else {
      displayStatus("Failed to obtain location");
    }
  }  
  delay(30000);
}

String scanWiFiNetworks()
{
  String json_request = "";
  
  int n = WiFi.scanNetworks();  //ネットワークをスキャンして数を取得

  if (n == 0) {
    //ネットワークが見つからないとき
    displayStatus("no networks found");
  } else {
    //ネットワークが見つかったとき
    displayStatus(" networks found");
    json_request = serializeJsonRequest(n);
  }
  return json_request;
}

String serializeJsonRequest(int cnt_ap)
{
  Serial.print("AP count="); Serial.println(cnt_ap);
  String json;
  const size_t capacity = JSON_ARRAY_SIZE(MAX_AP) + (MAX_AP)*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 34 + (MAX_AP * 29);
  DynamicJsonDocument doc(capacity);

  doc["considerIp"] = "false";
  JsonArray wifiAccessPoints = doc.createNestedArray("wifiAccessPoints");
  for (int i = 0; i < cnt_ap; i++)  {
    String bssid = WiFi.BSSIDstr(i);
    Serial.print(i); Serial.print(" BSSID="); Serial.println(bssid);
    JsonObject wifiAP = wifiAccessPoints.createNestedObject();
    wifiAP["macAddress"] = WiFi.BSSIDstr(i);
    if (i + 1 == MAX_AP) break;
  }

  serializeJson(doc, json);
  Serial.println("request->"); Serial.println(json);
  return json;
}

bool getGeolocation(String json_request) 
{  
  bool b = false;
  HTTPClient http;
  Serial.println(host_geolocate + "key=" + key);
  http.begin(host_geolocate + "key=" + key);
  int status_code = http.POST(json_request);
  Serial.printf("status_code=%d\r\n", status_code);
  if (status_code == 200) {
    String json_response = http.getString();
    Serial.println("respose->"); Serial.println(json_response);
    
    const size_t capacity = 2*JSON_OBJECT_SIZE(2) + 30;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, json_response);
    lat = doc["location"]["lat"];
    lng = doc["location"]["lng"];
    accuracy = doc["accuracy"];

    b = true;
  }
  http.end();
  return b;
}

void displayMap()
{
  char pos[32];
  sprintf(pos, "%f,%f", lat, lng);
  String query = "center=" + String(pos) 
                        + "&size=320x220&zoom=15&format=jpg-baseline"
                        + "&markers=size:mid|color:red|label:A|" + String(pos)
                        + "&key=" + key;
  String url = host_staticmap + query;
  HTTPClient http;
  http.begin(url);
  int status_code = http.GET();
  if (status_code == 200) {
    int len = http.getSize();
    Serial.printf("[HTTP] size: %d\n", len);
    if (len > 0) {
      WiFiClient * stream = http.getStreamPtr();
      Serial.printf("[HTTP] strm ptr: %x\n", stream);
      // read all data from server
      uint8_t* p = buff;
      int l = len;
      while (http.connected() && (l > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();
        if (size) {
          int s = ((size > sizeof(buff)) ? sizeof(buff) : size);
          int c = stream->readBytes(p, s);
          p += c;
          Serial.printf("[HTTP] read: %d\n", c);
          if (l > 0) {
            l -= c;
          }
        }
      }
    }
    Serial.println("");
    Serial.println("[HTTP] connection closed.");
    M5.Lcd.drawJpg(buff, len, 0, 20);
    
  } else {
    Serial.println("[HTTP] Failed");
  }
  http.end();
}
