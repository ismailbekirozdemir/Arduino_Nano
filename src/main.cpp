#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------------------------------------------------------------
// Ekran Yapılandırması (0.91" 128x32 I2C OLED)
// -------------------------------------------------------------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------------------------------------------
// Göz Boyutları ve Varsayılan Konumları (128x32 Çözünürlük İçin)
// -------------------------------------------------------------
const float DEFAULT_WIDTH   = 28.0f;
const float DEFAULT_HEIGHT  = 22.0f;
const float DEFAULT_RADIUS  = 5.0f;

const float LEFT_EYE_CENTER_X  = 36.0f;
const float RIGHT_EYE_CENTER_X = 92.0f;
const float EYE_CENTER_Y       = 16.0f;

// İfade Türleri
enum EyeMood {
    MOOD_NORMAL,
    MOOD_HAPPY,
    MOOD_CURIOUS
};

EyeMood currentMood = MOOD_NORMAL;

// Göz Nitelikleri Yapısı (Merkez tabanlı koordinat sistemi)
struct Eye {
    float cx;  // Merkez X
    float cy;  // Merkez Y
    float w;   // Genişlik
    float h;   // Yükseklik
    float r;   // Köşe yuvarlama yarıçapı
};

Eye leftEye;
Eye rightEye;

Eye targetLeft;
Eye targetRight;

// -------------------------------------------------------------
// Matematik & İnterpolasyon Yardımcısı (Yumuşak Geçiş)
// -------------------------------------------------------------
float approach(float current, float target, float speed) {
    if (current < target) {
        current += speed;
        if (current > target) current = target;
    } else if (current > target) {
        current -= speed;
        if (current < target) current = target;
    }
    return current;
}

void setTargetEyes(float lcx, float lcy, float lw, float lh, float lr,
                   float rcx, float rcy, float rw, float rh, float rr) {
    targetLeft.cx = lcx;
    targetLeft.cy = lcy;
    targetLeft.w  = lw;
    targetLeft.h  = lh;
    targetLeft.r  = lr;

    targetRight.cx = rcx;
    targetRight.cy = rcy;
    targetRight.w  = rw;
    targetRight.h  = rh;
    targetRight.r  = rr;
}

void setNormalTarget(float offsetX = 0.0f, float offsetY = 0.0f) {
    currentMood = MOOD_NORMAL;
    setTargetEyes(
        LEFT_EYE_CENTER_X + offsetX, EYE_CENTER_Y + offsetY, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_RADIUS,
        RIGHT_EYE_CENTER_X + offsetX, EYE_CENTER_Y + offsetY, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_RADIUS
    );
}

// -------------------------------------------------------------
// Çizim Fonksiyonları
// -------------------------------------------------------------
void drawSingleEye(const Eye& eye, bool isLeft) {
    int16_t w = round(eye.w);
    int16_t h = round(eye.h);
    int16_t x = round(eye.cx - (eye.w / 2.0f));
    int16_t y = round(eye.cy - (eye.h / 2.0f));
    int16_t r = round(eye.r);

    if (w <= 0 || h <= 0) return;
    if (r > h / 2) r = h / 2;
    if (r > w / 2) r = w / 2;
    if (r < 0) r = 0;

    if (currentMood == MOOD_HAPPY) {
        // Sevimli / Gülen Göz: Alt kısmı kavisli hilal şeklinde kesilmiş göz
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
        int16_t cutH = (int16_t)(h * 0.58f);
        int16_t cutY = y + (h - cutH) + 2;
        display.fillRoundRect(x - 2, cutY, w + 4, cutH + 4, r, SSD1306_BLACK);
    } else {
        // Standart Modern Robot Gözü (Yumuşak köşeli dikdörtgen)
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
    }
}

void render() {
    display.clearDisplay();
    drawSingleEye(leftEye, true);
    drawSingleEye(rightEye, false);
    display.display();
}

void updateEyes(float speed) {
    leftEye.cx = approach(leftEye.cx, targetLeft.cx, speed);
    leftEye.cy = approach(leftEye.cy, targetLeft.cy, speed);
    leftEye.w  = approach(leftEye.w,  targetLeft.w,  speed);
    leftEye.h  = approach(leftEye.h,  targetLeft.h,  speed);
    leftEye.r  = approach(leftEye.r,  targetLeft.r,  speed);

    rightEye.cx = approach(rightEye.cx, targetRight.cx, speed);
    rightEye.cy = approach(rightEye.cy, targetRight.cy, speed);
    rightEye.w  = approach(rightEye.w,  targetRight.w,  speed);
    rightEye.h  = approach(rightEye.h,  targetRight.h,  speed);
    rightEye.r  = approach(rightEye.r,  targetRight.r,  speed);
}

// -------------------------------------------------------------
// Açılış / Uyanma Animasyonu
// -------------------------------------------------------------
void playWakeupAnimation() {
    // 1. Kapalı göz çizgisi (uykuda)
    setTargetEyes(
        LEFT_EYE_CENTER_X, EYE_CENTER_Y, DEFAULT_WIDTH, 2.0f, 1.0f,
        RIGHT_EYE_CENTER_X, EYE_CENTER_Y, DEFAULT_WIDTH, 2.0f, 1.0f
    );
    leftEye = targetLeft;
    rightEye = targetRight;
    render();
    delay(500);

    // 2. Yavaşça aralanma (uykudan uyanış)
    targetLeft.h = 8.0f;
    targetRight.h = 8.0f;
    for (int i = 0; i < 30; i++) {
        updateEyes(0.6f);
        render();
        delay(15);
    }
    delay(200);

    // 3. Hızlı odaklanma kırpması
    targetLeft.h = 2.0f;
    targetRight.h = 2.0f;
    for (int i = 0; i < 15; i++) {
        updateEyes(3.0f);
        render();
        delay(10);
    }

    // 4. Tam uyanış
    setNormalTarget();
    for (int i = 0; i < 35; i++) {
        updateEyes(1.8f);
        render();
        delay(15);
    }
    delay(300);
}

// -------------------------------------------------------------
// Robot Davranış Durum Makinesi (State Machine)
// -------------------------------------------------------------
enum State {
    STATE_IDLE,
    STATE_BLINK_CLOSE,
    STATE_BLINK_OPEN,
    STATE_DOUBLE_BLINK_1_CLOSE,
    STATE_DOUBLE_BLINK_1_OPEN,
    STATE_DOUBLE_BLINK_2_CLOSE,
    STATE_DOUBLE_BLINK_2_OPEN,
    STATE_WINK_CLOSE,
    STATE_WINK_OPEN,
    STATE_LOOK,
    STATE_HAPPY
};

State state = STATE_IDLE;
unsigned long timer = 0;
unsigned long interval = 2500;
float currentSpeed = 2.5f;

void triggerNextAction() {
    int roll = random(0, 100);

    if (roll < 40) {
        // Standart Göz Kırpma (Normal Blink)
        state = STATE_BLINK_CLOSE;
        targetLeft.h = 2.0f;
        targetLeft.r = 1.0f;
        targetRight.h = 2.0f;
        targetRight.r = 1.0f;
        currentSpeed = 4.8f;
        timer = millis();
    } else if (roll < 55) {
        // Çift Göz Kırpma (Double Blink)
        state = STATE_DOUBLE_BLINK_1_CLOSE;
        targetLeft.h = 2.0f;
        targetLeft.r = 1.0f;
        targetRight.h = 2.0f;
        targetRight.r = 1.0f;
        currentSpeed = 5.0f;
        timer = millis();
    } else if (roll < 65) {
        // Tek Göz Kırpma / Göz Kırpışı (Wink - Çapkın/Neşeli)
        state = STATE_WINK_CLOSE;
        bool rightWinks = (random(0, 2) == 0);
        if (rightWinks) {
            targetRight.h = 2.0f;
            targetRight.r = 1.0f;
        } else {
            targetLeft.h = 2.0f;
            targetLeft.r = 1.0f;
        }
        currentSpeed = 4.2f;
        timer = millis();
    } else if (roll < 80) {
        // Sağa veya Sola Bakınma (Look Left / Right)
        state = STATE_LOOK;
        float dirX = (random(0, 2) == 0) ? -14.0f : 14.0f;
        setNormalTarget(dirX, 0.0f);
        currentSpeed = 2.0f;
        timer = millis();
        interval = random(1200, 2200);
    } else if (roll < 90) {
        // Yukarı veya Aşağı Bakınma (Look Up / Down)
        state = STATE_LOOK;
        float dirY = (random(0, 2) == 0) ? -4.0f : 4.0f;
        setNormalTarget(0.0f, dirY);
        currentSpeed = 1.8f;
        timer = millis();
        interval = random(1000, 1800);
    } else {
        // Mutlu / Gülen Robot İfadesi (Happy Mood)
        state = STATE_HAPPY;
        currentMood = MOOD_HAPPY;
        setTargetEyes(
            LEFT_EYE_CENTER_X, EYE_CENTER_Y, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_RADIUS,
            RIGHT_EYE_CENTER_X, EYE_CENTER_Y, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_RADIUS
        );
        currentSpeed = 2.2f;
        timer = millis();
        interval = random(1500, 2500);
    }
}

// -------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println(F("\n===================================="));
    Serial.println(F(" Robot Goz Animasyonu (0.91\" OLED)  "));
    Serial.println(F("===================================="));

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // I2C ekranı başlat (önce 0x3C, bulunamazsa 0x3D denenir)
    bool displayReady = false;
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        displayReady = true;
        Serial.println(F("[BILGI] OLED ekran 0x3C adresinde bulundu."));
    } else if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        displayReady = true;
        Serial.println(F("[BILGI] OLED ekran 0x3D adresinde bulundu."));
    }

    if (!displayReady) {
        Serial.println(F("[HATA] OLED ekran algilanamadi!"));
        Serial.println(F("Lutfen baglantilari kontrol edin:"));
        Serial.println(F("  OLED VCC -> Arduino 5V veya 3.3V"));
        Serial.println(F("  OLED GND -> Arduino GND"));
        Serial.println(F("  OLED SCL -> Arduino A5"));
        Serial.println(F("  OLED SDA -> Arduino A4"));
        while (true) {
            // Hata durumunda dahili LED yanıp söner
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
            digitalWrite(LED_BUILTIN, LOW);
            delay(200);
        }
    }

    // Akıcı animasyon için I2C saat hızını 400kHz Fast-Mode'a çıkar
    Wire.setClock(400000);

    // Rastgelelik için boşta olan A0 analog pininden tohum al
    randomSeed(analogRead(A0));

    display.clearDisplay();
    display.display();

    // Uyanma animasyonunu çalıştır
    playWakeupAnimation();

    // Bekleme (Idle) moduna geç
    state = STATE_IDLE;
    timer = millis();
    interval = random(2000, 4000);
    currentSpeed = 2.5f;
    Serial.println(F("[BILGI] Animasyon dongusu aktif."));
}

void loop() {
    unsigned long now = millis();

    switch (state) {
        case STATE_IDLE:
            if (now - timer >= interval) {
                triggerNextAction();
            }
            break;

        case STATE_BLINK_CLOSE:
            if (leftEye.h <= 2.5f || (now - timer > 120)) {
                state = STATE_BLINK_OPEN;
                setNormalTarget();
                currentSpeed = 4.0f;
                timer = now;
            }
            break;

        case STATE_BLINK_OPEN:
            if (leftEye.h >= (DEFAULT_HEIGHT - 1.0f) || (now - timer > 160)) {
                state = STATE_IDLE;
                timer = now;
                interval = random(2000, 4500);
                currentSpeed = 2.5f;
            }
            break;

        case STATE_DOUBLE_BLINK_1_CLOSE:
            if (leftEye.h <= 2.5f || (now - timer > 100)) {
                state = STATE_DOUBLE_BLINK_1_OPEN;
                targetLeft.h = 10.0f;
                targetRight.h = 10.0f;
                currentSpeed = 4.2f;
                timer = now;
            }
            break;

        case STATE_DOUBLE_BLINK_1_OPEN:
            if (leftEye.h >= 9.0f || (now - timer > 100)) {
                state = STATE_DOUBLE_BLINK_2_CLOSE;
                targetLeft.h = 2.0f;
                targetRight.h = 2.0f;
                currentSpeed = 5.0f;
                timer = now;
            }
            break;

        case STATE_DOUBLE_BLINK_2_CLOSE:
            if (leftEye.h <= 2.5f || (now - timer > 100)) {
                state = STATE_DOUBLE_BLINK_2_OPEN;
                setNormalTarget();
                currentSpeed = 3.8f;
                timer = now;
            }
            break;

        case STATE_DOUBLE_BLINK_2_OPEN:
            if (leftEye.h >= (DEFAULT_HEIGHT - 1.0f) || (now - timer > 180)) {
                state = STATE_IDLE;
                timer = now;
                interval = random(2000, 4500);
                currentSpeed = 2.5f;
            }
            break;

        case STATE_WINK_CLOSE:
            if (leftEye.h <= 2.5f || rightEye.h <= 2.5f || (now - timer > 140)) {
                state = STATE_WINK_OPEN;
                setNormalTarget();
                currentSpeed = 3.6f;
                timer = now;
            }
            break;

        case STATE_WINK_OPEN:
            if ((leftEye.h >= DEFAULT_HEIGHT - 1.0f && rightEye.h >= DEFAULT_HEIGHT - 1.0f) || (now - timer > 180)) {
                state = STATE_IDLE;
                timer = now;
                interval = random(2000, 4500);
                currentSpeed = 2.5f;
            }
            break;

        case STATE_LOOK:
            if (now - timer >= interval) {
                setNormalTarget();
                currentSpeed = 2.5f;
                state = STATE_IDLE;
                timer = now;
                interval = random(1500, 3000);
            }
            break;

        case STATE_HAPPY:
            if (now - timer >= interval) {
                setNormalTarget();
                currentSpeed = 2.5f;
                state = STATE_IDLE;
                timer = now;
                interval = random(2000, 4000);
            }
            break;
    }

    // Pozisyonları hedefe doğru yumuşakça yaklaştır
    updateEyes(currentSpeed);

    // Ekrana çiz
    render();

    // ~40 FPS yenileme hızı
    delay(20);
}