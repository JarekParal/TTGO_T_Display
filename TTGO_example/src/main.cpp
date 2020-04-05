/*
  Simple sketch for sending data to the TMEP.cz and showing measured values on TTGO Display.

  Created by JarekParal (github.com/jarekparal)
  Code for TMEP communication was copy from page https://wiki.tmep.cz/doku.php?id=zarizeni:esp8266  
  which was created by mikrom (http://www.mikrom.cz)
  Code for TTGO Display was copy from https://github.com/Xinyuan-LilyGO/TTGO-T-Display.

  License: MIT
*/

#include <Arduino.h>

#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include <Button2.h>
#include "esp_adc_cal.h"
#include "bmp.h"

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN 0x10
#endif

#define ADC_EN 14
#define ADC_PIN 34
#define BUTTON_1 35
#define BUTTON_2 0

// // BME280 defines
// #define I2C_BME_SDA 18
// #define I2C_BME_SCL 19
// #define I2C_BME280_ADDR0 0x76
// #define I2C_BME280_ADDR1 0x77

// #include <Wire.h>
// #include <Adafruit_Sensor.h>
// #include <Adafruit_BME280.h>

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

char buff[512];
int vref = 1100;
int btnCick = false;

#include <DallasTemperature.h> // DS18B20 library
#include <OneWire.h> // OneWire communication library for DS18B20

// Define settings
// const char ssid[]     = "---ssid---"; // WiFi SSID
// const char pass[]     = "---password---"; // WiFi password
// const char domain[]   = "---domain---";  // domain.tmep.cz
// const char guid1[]     = "---guid---"; // name of the sensor page
// const char guid2[]     = "humV"; // hack for second sensor

#include "credential.h"

const byte sleepInMinutes = 1; // How often send data to the server. In minutes
const byte oneWireBusPin = 13; // Pin where is DS18B20 connected

// Create Temperature object "dallasSensors"
OneWire oneWire(oneWireBusPin); // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature dallasSensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.

void wifi_setup() {
    // Connect to the WiFi
    Serial.print(F("Connecting to "));
    Serial.println(ssid);
    tft.print(F("Connecting to "));
    tft.println(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(F("."));
        tft.print(F("."));
    }
    Serial.println();
    tft.println();
    Serial.println(F("WiFi connected"));
    tft.println(F("WiFi connected"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    tft.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    Serial.println();
    tft.println();
}

void temperature_setup() {
    Serial.println("temperature_setup");
    dallasSensors.begin(); // Initialize the DallasTemperature DS18B20 class (not strictly necessary with the client class, but good practice).
}

void temperature_measure() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);

    dallasSensors.requestTemperatures(); // Send the command to get temperatures. request to all devices on the bus
    float temp0 = dallasSensors.getTempCByIndex(0); // Read temperature in "t" variable
    float temp1 = dallasSensors.getTempCByIndex(1);
    if (temp0 == -127.00 || temp1 == -127.00) { // If you have connected it wrong, Dallas read this temperature! :)
        Serial.println(F("Temp error!"));
        tft.println(F("Temp error!"));
        return;
    }

    // Connect to the HOST and send data via GET method
    WiFiClient client; // Use WiFiClient class to create TCP connections

    char host[50]; // Joining two chars is little bit difficult. Make new array, 50 bytes long
    strcpy(host, domain); // Copy /domain/ in to the /host/
    strcat(host, ".tmep.cz"); // Add ".tmep.cz" at the end of the /host/. /host/ is now "/domain/.tmep.cz"

    Serial.print(F("Connecting to "));
    Serial.println(host);
    if (!client.connect(host, 80)) {
        // If you didn't get a connection to the server
        Serial.println(F("Connection failed"));
        tft.println(F("Connection failed"));
        return;
    }
    Serial.println(F("Client connected"));

    // Make an url. We need: /?guid=t
    String url = "/?";
    url += guid0;
    url += "=";
    url += temp0;
    url += "&";
    url += guid1;
    url += "=";
    url += temp1;
    Serial.print(F("Requesting URL: "));
    Serial.println(url);
    tft.print(F("Requesting URL: "));
    tft.println(url);

    // Make a HTTP GETrequest.
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");

    // Workaroud for timeout
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(F(">>> Client Timeout !"));
            tft.println(F(">>> Client Timeout !"));
            client.stop();
            return;
        }
    }

    Serial.println();
}

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms) {
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

bool TFT_BACKLIGHT_toggle = TFT_BACKLIGHT_ON;

void button_init() {
    btn1.setLongClickHandler([](Button2& b) {
        btnCick = false;
        int r = digitalRead(TFT_BL);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Press again to wake up", tft.width() / 2, tft.height() / 2);
        espDelay(6000);
        digitalWrite(TFT_BL, !r);

        tft.writecommand(TFT_DISPOFF);
        tft.writecommand(TFT_SLPIN);
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_deep_sleep_start();
    });
    btn1.setPressedHandler([](Button2& b) {
        //Serial.println("Detect Voltage..");
        if (TFT_BACKLIGHT_toggle) {
            TFT_BACKLIGHT_toggle = false;
        } else {
            TFT_BACKLIGHT_toggle = true;
        }
        digitalWrite(TFT_BL, TFT_BACKLIGHT_toggle);
        //btnCick = true;
    });

    btn2.setPressedHandler([](Button2& b) {
        btnCick = false;
        Serial.println("btn press wifi scan");
    });
}

void button_loop() {
    btn1.loop();
    btn2.loop();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Start");
    tft.init();

    button_init();

    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV\n", adc_chars.vref);
        vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.println("Default Vref: 1100mV");
    }

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.println("Start...");

    wifi_setup();
    temperature_setup();
}

void loop() {
    button_loop();
    temperature_measure();

    // Wait for another round
    delay(sleepInMinutes * 60000);
}
