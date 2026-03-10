#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
#include "lwip/napt.h"
}

AsyncWebServer server(80);

unsigned long previousMillis = 0;
long delay_time = 0;
int ledState = LOW;

class wifi_ext {
public:
    String ssid = "";
    String pass = "";
    String ap   = "";
    String user = "";

    void create_server() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            String network_html = "";
            int n = WiFi.scanComplete();

            if (n == -2) {
                WiFi.scanNetworks(true);
            } else if (n > 0) {
                for (int i = 0; i < n; ++i) {
                    String router = WiFi.SSID(i);
                    network_html += "<input type=\"radio\" name=\"ssid\" value=\"" + router + "\" required><label>" + router + "</label><br>";
                }
                WiFi.scanDelete();
            }

            String html = "<html><head><style>body{font-family:sans-serif;padding:20px;} input{margin-bottom:10px;}</style></head><body>";
            html += "<h1>WiFi Extender (Enterprise Support)</h1>";
            html += "<form action=\"/credentials\">";
            html += "<h3>Select Network:</h3>" + network_html;
            html += "<br><input type=\"text\" name=\"user\" placeholder=\"Username (For Myanmar Net)\"><br>";
            html += "<input type=\"password\" name=\"pass\" placeholder=\"Password\" required><br>";
            html += "<input type=\"text\" name=\"ap\" placeholder=\"New Hotspot Name\" required><br>";
            html += "<input type=\"submit\" value=\"Save and Restart\">";
            html += "</form></body></html>";

            request->send(200, "text/html", html);
        });

        server.on("/credentials", HTTP_GET, [](AsyncWebServerRequest *request) {
            String ssid = request->hasParam("ssid") ? request->getParam("ssid")->value() : "";
            String user = request->hasParam("user") ? request->getParam("user")->value() : "";
            String pass = request->hasParam("pass") ? request->getParam("pass")->value() : "";
            String ap   = request->hasParam("ap")   ? request->getParam("ap")->value()   : "";

            File file = LittleFS.open("/config.txt", "w");
            if (file) {
                file.println(ssid);
                file.println(pass);
                file.println(ap);
                file.println(user);
                file.close();
            }

            request->send(200, "text/plain", "Config Saved. Restarting...");
            delay(2000);
            ESP.restart();
        });
    }

    bool load_credentials() {
        File file = LittleFS.open("/config.txt", "r");
        if (!file) return false;

        ssid = file.readStringUntil('\n');
        pass = file.readStringUntil('\n');
        ap   = file.readStringUntil('\n');
        user = file.readStringUntil('\n');
        file.close();

        ssid.trim();
        pass.trim();
        ap.trim();
        user.trim();

        return ssid.length() > 0;
    }
};

wifi_ext my_wifi;

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    bool hasConfig = my_wifi.load_credentials();

    if (!hasConfig) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Pius_Extender_Setup");
        my_wifi.create_server();
        server.begin();
        delay_time = 1000;
    } else {
        String ssid = my_wifi.ssid;
        String pass = my_wifi.pass;
        String ap   = my_wifi.ap;
        String user = my_wifi.user;

        WiFi.mode(WIFI_AP_STA);

        if (user != "") {
            struct station_config sta_conf;
            wifi_station_get_config(&sta_conf);
            os_memset(sta_conf.ssid, 0, 32);
            os_memcpy(sta_conf.ssid, ssid.c_str(), ssid.length());
            wifi_station_set_config(&sta_conf);

            wifi_station_set_wpa2_enterprise_auth(1);
            wifi_station_set_enterprise_username((uint8 *)user.c_str(), user.length());
            wifi_station_set_enterprise_password((uint8 *)pass.c_str(), pass.length());
            wifi_station_connect();
        } else {
            WiFi.begin(ssid.c_str(), pass.c_str());
        }

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        ip_napt_init(1000, 10);
        ip_napt_enable_no(SOFTAP_IF, 1);

        if (ap == "") ap = "Pius_Extender";
        WiFi.softAP(ap.c_str(), pass.c_str());

        delay_time = 200;
    }
}

void loop() {
    if (millis() - previousMillis >= delay_time) {
        previousMillis = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
}
