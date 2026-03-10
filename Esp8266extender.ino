#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#define NAPT 1000
#define NAPT_PORT 10

extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
#include "lwip/napt.h"
}

AsyncWebServer server(80);

unsigned long previousMillis = 0;
long delay_time = 500; 
int ledState = LOW;

class wifi_ext {
public:
    String ssid = "", pass = "", ap = "", user = "";

    void create_server() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            int n = WiFi.scanComplete();
            String network_list = "";

            if (n == -2) {
                WiFi.scanNetworks(true);
                network_list = "<p>Scanning... Please wait 5 seconds and <a href='/'>Refresh</a></p>";
            } else if (n > 0) {
                for (int i = 0; i < n; ++i) {
                    String s = WiFi.SSID(i);
                    network_list += "<div><input type='radio' name='ssid' value='" + s + "' id='s" + String(i) + "' required>";
                    network_list += "<label for='s" + String(i) + "'>" + s + " (" + String(WiFi.RSSI(i)) + "dBm)</label></div>";
                }
                WiFi.scanDelete();
            } else {
                network_list = "<p>No networks found. <a href='/scan'>Scan Again</a></p>";
            }

            String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;padding:20px;line-height:1.6;} input[type=text],input[type=password]{width:100%;padding:10px;margin:10px 0;} .box{border:1px solid #ccc;padding:15px;border-radius:8px;} .btn{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;}</style></head><body>";
            html += "<h2>Pius WiFi Extender</h2><div class='box'><form action='/credentials'>";
            html += "<h3>1. Select WiFi:</h3>" + network_list;
            html += "<h3>2. Myanmar Net Identity (Optional):</h3><input type='text' name='user' placeholder='Identify'>";
            html += "<h3>3. WiFi Password:</h3><input type='password' name='pass' placeholder='Password' required>";
            html += "<h3>4. New AP Name:</h3><input type='text' name='ap' placeholder='Repeater Name'>";
            html += "<button type='submit' class='btn'>Save and Restart</button></form>";
            html += "<br><a href='/scan'><button class='btn' style='background:#6c757d;'>Scan WiFi</button></a></div></body></html>";
            request->send(200, "text/html", html);
        });

        server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
            WiFi.scanNetworks(true);
            request->redirect("/");
        });

        server.on("/credentials", HTTP_GET, [](AsyncWebServerRequest *request) {
            File file = LittleFS.open("/config.txt", "w");
            if (file) {
                file.println(request->arg("ssid"));
                file.println(request->arg("pass"));
                file.println(request->arg("ap"));
                file.println(request->arg("user"));
                file.close();
            }
            request->send(200, "text/plain", "Saved! Device is restarting. Please connect to your new WiFi in 30 seconds.");
            delay(2000);
            ESP.restart();
        });
    }

    bool load_credentials() {
        File file = LittleFS.open("/config.txt", "r");
        if (!file) return false;
        ssid = file.readStringUntil('\n'); ssid.trim();
        pass = file.readStringUntil('\n'); pass.trim();
        ap = file.readStringUntil('\n'); ap.trim();
        user = file.readStringUntil('\n'); user.trim();
        file.close();
        return ssid.length() > 0;
    }
};

wifi_ext my_wifi;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    LittleFS.begin();

    if (!my_wifi.load_credentials()) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("Pius_Extender_Setup");
        my_wifi.create_server();
        server.begin();
        WiFi.scanNetworks(true); // အစကတည်းက scan ထားပေးခြင်း
    } else {
        WiFi.mode(WIFI_AP_STA);
        if (my_wifi.user != "") {
            wifi_station_disconnect();
            struct station_config sta_conf;
            memset(&sta_conf, 0, sizeof(sta_conf));
            memcpy(sta_conf.ssid, my_wifi.ssid.c_str(), my_wifi.ssid.length());
            wifi_station_set_config(&sta_conf);
            wifi_station_set_wpa2_enterprise_auth(1);
            wifi_station_set_enterprise_username((uint8 *)my_wifi.user.c_str(), my_wifi.user.length());
            wifi_station_set_enterprise_password((uint8 *)my_wifi.pass.c_str(), my_wifi.pass.length());
            wifi_station_connect();
        } else {
            WiFi.begin(my_wifi.ssid.c_str(), my_wifi.pass.c_str());
        }

        int count = 0;
        while (WiFi.status() != WL_CONNECTED && count < 30) {
            delay(500); Serial.print("."); count++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            ip_napt_init(NAPT, NAPT_PORT);
            ip_napt_enable_no(SOFTAP_IF, 1);
            int channel = WiFi.channel();
            String ap_name = (my_wifi.ap.length() > 0) ? my_wifi.ap : "Pius_Extender";
            WiFi.softAP(ap_name.c_str(), my_wifi.pass.c_str(), channel);
            delay_time = 200;
        } else {
            LittleFS.remove("/config.txt");
            ESP.restart();
        }
    }
}

void loop() {
    if (millis() - previousMillis >= (unsigned long)delay_time) {
        previousMillis = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
}
