
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// 1. LCD SETTINGS
// ==========================================

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// ==========================================
// 2. WIFI SETTINGS
// ==========================================

const char* WIFI_NAME = "prince";
const char* WIFI_PASSWORD = "prince@7419";

IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

bool wifiStarted = false;

// ==========================================
// 3. NOTICE AND SCROLL SETTINGS
// ==========================================

const int MAX_NOTICE_LENGTH = 1000;

String notice = "";

int scrollPosition = 1;

unsigned long lastScroll = 0;
unsigned long endPauseStarted = 0;
unsigned long lastStatusPrint = 0;

const unsigned long SCROLL_INTERVAL_MS = 400;
const unsigned long END_PAUSE_MS = 2500;

bool endPauseActive = false;

// ==========================================
// 4. WEBSITE HTML
// ==========================================

const char webpage[] PROGMEM = R"HTML(

<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">

<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>ESP32 Notice Board</title>

<style>

* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
    font-family: Arial, sans-serif;
}

body {
    min-height: 100vh;
    background: linear-gradient(135deg, #07152f, #123b75);
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 20px;
}

.container {
    width: 100%;
    max-width: 470px;
    background: white;
    padding: 30px;
    border-radius: 18px;
    box-shadow: 0 15px 40px rgba(0,0,0,0.3);
}

h1 {
    text-align: center;
    color: #123b75;
    margin-bottom: 8px;
}

.subtitle {
    text-align: center;
    color: #777;
    font-size: 14px;
    margin-bottom: 25px;
}

label {
    font-weight: bold;
    color: #26364d;
    display: block;
    margin-bottom: 10px;
}

textarea {
    width: 100%;
    height: 130px;
    resize: vertical;
    padding: 14px;
    border: 1px solid #cbd5e1;
    border-radius: 10px;
    font-size: 15px;
    outline: none;
}

textarea:focus {
    border-color: #2563eb;
}

.counter {
    text-align: right;
    color: #777;
    font-size: 12px;
    margin-top: 5px;
}

button {
    width: 100%;
    padding: 14px;
    border: none;
    border-radius: 9px;
    margin-top: 15px;
    font-size: 15px;
    font-weight: bold;
    cursor: pointer;
}

button:disabled {
    opacity: 0.6;
    cursor: not-allowed;
}

.publish {
    background: #1769e0;
    color: white;
}

.clear {
    background: #e8edf5;
    color: #26364d;
}

.preview {
    background: #f1f5fb;
    border-left: 4px solid #1769e0;
    border-radius: 8px;
    padding: 15px;
    margin-top: 22px;
    min-height: 75px;
    overflow-wrap: anywhere;
}

.preview h3 {
    color: #123b75;
    font-size: 14px;
    margin-bottom: 8px;
}

#previewText {
    color: #26364d;
    line-height: 1.5;
}

.status {
    text-align: center;
    margin-top: 15px;
    font-size: 13px;
    min-height: 18px;
    overflow-wrap: anywhere;
}

.footer {
    text-align: center;
    color: #999;
    font-size: 12px;
    margin-top: 20px;
}

</style>

</head>

<body>

<div class="container">

    <h1>NOTICE BOARD</h1>

    <p class="subtitle">
        Internet-Based Digital Notice System
    </p>

    <label for="notice">
        Enter Your Notice
    </label>

    <textarea
        id="notice"
        maxlength="1000"
        placeholder="Write your notice here..."
        oninput="updatePreview()"
    ></textarea>

    <div class="counter">
        <span id="count">0</span>/1000 characters
    </div>

    <button
        class="publish"
        id="publishButton"
        onclick="sendNotice()">
        PUBLISH NOTICE
    </button>

    <button
        class="clear"
        id="clearButton"
        onclick="clearNotice()">
        CLEAR NOTICE
    </button>

    <div class="preview">

        <h3>NOTICE PREVIEW</h3>

        <p id="previewText">
            Your notice will appear here...
        </p>

    </div>

    <div class="status" id="status">
        Checking ESP32 connection...
    </div>

    <div class="footer">
        Powered by ESP32
    </div>

</div>

<script>

const noticeBox = document.getElementById("notice");
const previewText = document.getElementById("previewText");
const statusText = document.getElementById("status");

const publishButton = document.getElementById("publishButton");
const clearButton = document.getElementById("clearButton");

// UPDATE PREVIEW

function updatePreview() {

    previewText.textContent =
        noticeBox.value || "Your notice will appear here...";

    document.getElementById("count").textContent =
        noticeBox.value.length;
}

// CHECK ESP32 CONNECTION

async function checkConnection() {

    try {

        const response = await fetch("/ping", {
            method: "GET",
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("Server not responding");
        }

        statusText.textContent = "ESP32 Connected";
        statusText.style.color = "green";

    } catch (error) {

        statusText.textContent = "ESP32 connection failed";
        statusText.style.color = "red";
    }
}

// SEND COMPLETE NOTICE AS PLAIN TEXT

async function sendNotice() {

    const message = noticeBox.value.trim();

    if (message.length === 0) {

        statusText.textContent = "Please enter a notice.";
        statusText.style.color = "red";

        return;
    }

    publishButton.disabled = true;

    statusText.textContent = "Sending notice...";
    statusText.style.color = "blue";

    try {

        const response = await fetch("/update", {

            method: "POST",

            headers: {
                "Content-Type": "text/plain;charset=UTF-8"
            },

            body: message,

            cache: "no-store"
        });

        const result = await response.text();

        if (!response.ok) {
            throw new Error(result);
        }

        statusText.textContent = result;
        statusText.style.color = "green";

    } catch (error) {

        statusText.textContent =
            "Publish failed: " + error.message;

        statusText.style.color = "red";

    } finally {

        publishButton.disabled = false;
    }
}

// CLEAR NOTICE

async function clearNotice() {

    clearButton.disabled = true;

    statusText.textContent = "Clearing notice...";
    statusText.style.color = "blue";

    try {

        const response = await fetch("/clear", {
            method: "POST",
            cache: "no-store"
        });

        const result = await response.text();

        if (!response.ok) {
            throw new Error(result);
        }

        noticeBox.value = "";

        updatePreview();

        statusText.textContent = result;
        statusText.style.color = "green";

    } catch (error) {

        statusText.textContent =
            "Clear failed: " + error.message;

        statusText.style.color = "red";

    } finally {

        clearButton.disabled = false;
    }
}

// CHECK CONNECTION WHEN PAGE OPENS

checkConnection();

</script>

</body>
</html>

)HTML";

// ==========================================
// 5. LCD HOME SCREEN
// ==========================================

void showHomeScreen() {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("NOTICE BOARD");

    lcd.setCursor(0, 1);

    if (notice.length() == 0) {
        lcd.print("No Notice Yet");
    } else {
        lcd.print("Notice Received");
    }
}

// ==========================================
// 6. HOME PAGE
// ==========================================

void handleRoot() {

    server.sendHeader("Cache-Control", "no-store");

    server.send_P(
        200,
        "text/html",
        webpage
    );
}

// ==========================================
// 7. CONNECTION TEST
// ==========================================

void handlePing() {

    server.send(
        200,
        "text/plain",
        "ESP32 ONLINE"
    );

    Serial.println("Browser connection verified");
}

// ==========================================
// 8. RECEIVE NOTICE
// ==========================================

void handleUpdate() {

    Serial.println();
    Serial.println("==============================");
    Serial.println("NOTICE REQUEST RECEIVED");

    // Read the raw text body
    String newNotice = server.arg("plain");

    // Compatibility with earlier form-based webpage
    if (newNotice.length() == 0) {
        newNotice = server.arg("notice");
    }

    newNotice.trim();

    Serial.print("Received bytes: ");
    Serial.println(newNotice.length());

    if (newNotice.length() == 0) {

        Serial.println("ERROR: EMPTY NOTICE");

        server.send(
            400,
            "text/plain",
            "Please enter a notice"
        );

        return;
    }

    if (newNotice.length() > MAX_NOTICE_LENGTH) {

        Serial.println("ERROR: NOTICE TOO LONG");

        server.send(
            413,
            "text/plain",
            "Notice exceeds 1000 bytes"
        );

        return;
    }

    // Store complete notice
    notice = newNotice;

    // Start the scrolling animation again
    scrollPosition = 1;

    endPauseActive = false;

    lastScroll = millis();

    Serial.println("NOTICE STORED SUCCESSFULLY");

    Serial.println("COMPLETE NOTICE:");

    Serial.println(notice);

    Serial.println("==============================");

    showHomeScreen();

    server.send(
        200,
        "text/plain",
        "Notice published successfully"
    );
}

// ==========================================
// 9. CLEAR NOTICE
// ==========================================

void handleClear() {

    Serial.println("CLEAR REQUEST RECEIVED");

    notice = "";

    scrollPosition = 1;

    endPauseActive = false;

    showHomeScreen();

    Serial.println("NOTICE CLEARED");

    server.send(
        200,
        "text/plain",
        "Notice cleared successfully"
    );
}

// ==========================================
// 10. CAPTIVE PORTAL FALLBACK
// ==========================================

void handleNotFound() {

    if (server.method() == HTTP_GET) {

        handleRoot();

    } else {

        server.send(
            404,
            "text/plain",
            "Request not found"
        );
    }
}

// ==========================================
// 11. SLOW LCD SCROLLING
// ==========================================

void displayNotice() {

    if (notice.length() == 0) {
        return;
    }

    // Hold the blank screen at the end
    // before repeating the notice.

    if (endPauseActive) {

        if (millis() - endPauseStarted >= END_PAUSE_MS) {

            endPauseActive = false;

            scrollPosition = 1;

            lastScroll = millis();
        }

        return;
    }

    // Control scrolling speed
    if (millis() - lastScroll < SCROLL_INTERVAL_MS) {
        return;
    }

    lastScroll = millis();

    lcd.setCursor(0, 0);
    lcd.print("LATEST NOTICE   ");

    lcd.setCursor(0, 1);

    // Move the complete notice through
    // a 16-character viewing window.

    for (int i = 0; i < LCD_COLUMNS; i++) {

        int noticeIndex = scrollPosition + i - LCD_COLUMNS;

        if (noticeIndex >= 0 &&
            noticeIndex < notice.length()) {

            lcd.print(notice.charAt(noticeIndex));

        } else {

            lcd.print(" ");
        }
    }

    // Continue until the entire message has passed
    if (scrollPosition >= notice.length() + LCD_COLUMNS) {

        endPauseActive = true;

        endPauseStarted = millis();

    } else {

        scrollPosition++;
    }
}

// ==========================================
// 12. START ESP32 WIFI
// ==========================================

bool startWiFi() {

    Serial.println();
    Serial.println("STARTING ESP32 WIFI...");

    lcd.clear();
    lcd.print("Starting WiFi");

    WiFi.mode(WIFI_AP);

    WiFi.setSleep(false);

    bool configResult = WiFi.softAPConfig(
        apIP,
        gateway,
        subnet
    );

    if (!configResult) {

        Serial.println("IP CONFIGURATION FAILED");

        return false;
    }

    bool started = WiFi.softAP(
        WIFI_NAME,
        WIFI_PASSWORD,
        1,
        false,
        4
    );

    if (!started) {

        Serial.println("ACCESS POINT FAILED");

        return false;
    }

    delay(500);

    IPAddress actualIP = WiFi.softAPIP();

    Serial.println();
    Serial.println("==============================");

    Serial.println("ACCESS POINT STARTED");

    Serial.print("WiFi Name: ");
    Serial.println(WIFI_NAME);

    Serial.print("WiFi Password: ");
    Serial.println(WIFI_PASSWORD);

    Serial.print("IP Address: ");
    Serial.println(actualIP);

    Serial.println("==============================");

    // DNS captive portal
    dnsServer.start(
        53,
        "*",
        actualIP
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("WiFi Started");

    lcd.setCursor(0, 1);
    lcd.print(actualIP.toString());

    return true;
}

// ==========================================
// 13. START WEB SERVER
// ==========================================

void startWebServer() {

    server.on("/", HTTP_GET, handleRoot);

    server.on("/ping", HTTP_GET, handlePing);

    server.on("/update", HTTP_POST, handleUpdate);

    server.on("/clear", HTTP_POST, handleClear);

    // Captive portal detection paths
    server.on("/generate_204", HTTP_GET, handleRoot);

    server.on("/hotspot-detect.html", HTTP_GET, handleRoot);

    server.on("/connecttest.txt", HTTP_GET, handleRoot);

    server.on("/ncsi.txt", HTTP_GET, handleRoot);

    server.on("/fwlink", HTTP_GET, handleRoot);

    server.onNotFound(handleNotFound);

    server.begin();

    Serial.println("WEB SERVER STARTED");

    Serial.println("Website: http://192.168.4.1/");

    Serial.println("Test: http://192.168.4.1/ping");
}

// ==========================================
// 14. SETUP
// ==========================================

void setup() {

    Serial.begin(115200);

    delay(1000);

    Wire.begin(21, 22);

    lcd.init();

    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("NOTICE BOARD");

    lcd.setCursor(0, 1);
    lcd.print("Starting...");

    delay(1500);

    wifiStarted = startWiFi();

    if (wifiStarted) {

        startWebServer();

        delay(1000);

        showHomeScreen();

    } else {

        Serial.println("SYSTEM WIFI STARTUP FAILED");

        lcd.clear();
        lcd.print("WiFi Error");
    }
}

// ==========================================
// 15. MAIN LOOP
// ==========================================

void loop() {

    if (wifiStarted) {

        // Process Wi-Fi DNS requests
        dnsServer.processNextRequest();

        // Process website requests
        server.handleClient();

        // Scroll LCD notice
        displayNotice();

        // Print connected device count
        if (millis() - lastStatusPrint >= 5000) {

            lastStatusPrint = millis();

            Serial.print("Connected devices: ");

            Serial.println(
                WiFi.softAPgetStationNum()
            );
        }
    }

    delay(2);
}
