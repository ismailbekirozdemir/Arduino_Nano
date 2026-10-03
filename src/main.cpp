#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "user_bitmap.h"

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

// İfade / Duygu Türleri (Moods)
enum EyeMood {
    MOOD_NORMAL,        // Standart modern robot gözü
    MOOD_HAPPY,         // Neşeli hilal gülümsemesi (^ ^)
    MOOD_KAWAII,        // Parıltılı büyük sevimli anime gözü (* *)
    MOOD_SURPRISED,     // Şaşırma / O_O (Kocaman açılmış göz + minik gözbebeği + !)
    MOOD_HUNTER,        // Avcı Gözü (Canthal tilt + keskin kaş + odaklı bakış)
    MOOD_USER_SPECIAL   // Kullanıcının fotoğrafındaki özel göz ve göz kırpma ifadesi (%5 Nadir)
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
        // Sevimli / Gülen Göz: Alt kısmı kavisli hilal şeklinde oyulmuş göz (^ ^)
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
        int16_t cutH = (int16_t)(h * 0.58f);
        int16_t cutY = y + (h - cutH) + 2;
        display.fillRoundRect(x - 2, cutY, w + 4, cutH + 4, r, SSD1306_BLACK);
    } else if (currentMood == MOOD_KAWAII) {
        // Tatlı / Parıltılı Anime Gözü (* *)
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
        int16_t pw = w * 0.62f;
        int16_t ph = h * 0.62f;
        int16_t pr = 4;
        if (pr > ph / 2) pr = ph / 2;
        display.fillRoundRect(eye.cx - pw / 2, eye.cy - ph / 2, pw, ph, pr, SSD1306_BLACK);
        // Sol üst büyük ışıltı
        display.fillCircle(eye.cx - 3, eye.cy - 3, 2, SSD1306_WHITE);
        // Sağ alt küçük ışıltı
        display.drawPixel(eye.cx + 3, eye.cy + 2, SSD1306_WHITE);
        display.drawPixel(eye.cx + 4, eye.cy + 3, SSD1306_WHITE);
        // Yanak allık çizgileri (tatlı kızarma efekti)
        if (y + h + 2 < SCREEN_HEIGHT) {
            display.drawFastHLine(eye.cx - 8, y + h + 2, 4, SSD1306_WHITE);
            display.drawFastHLine(eye.cx + 5, y + h + 2, 4, SSD1306_WHITE);
        }
    } else if (currentMood == MOOD_SURPRISED) {
        // Şaşırma Gözleri (O_O): İri gözler + minik küçülmüş gözbebeği
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
        display.fillCircle(eye.cx, eye.cy, 3, SSD1306_BLACK);
    } else if (currentMood == MOOD_HUNTER) {
        // Avcı Gözü (Hunter Eyes): Keskin canthal tilt + odaklı bakış
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
        if (isLeft) {
            // Sol göz: içe (buruna doğru) eğimli kaş ve üst kapak kesiti
            display.fillTriangle(x, y, x + w, y, x + w, y + 4, SSD1306_BLACK);
            display.drawLine(18, 7, 52, 12, SSD1306_WHITE);
            display.drawLine(18, 8, 52, 13, SSD1306_WHITE);
        } else {
            // Sağ göz: içe (buruna doğru) eğimli kaş ve üst kapak kesiti
            display.fillTriangle(x, y, x + w, y, x, y + 4, SSD1306_BLACK);
            display.drawLine(110, 7, 76, 12, SSD1306_WHITE);
            display.drawLine(110, 8, 76, 13, SSD1306_WHITE);
        }
        // Keskin odaklı gözbebeği
        display.fillCircle(eye.cx, eye.cy + 1, 2, SSD1306_BLACK);
        display.drawPixel(eye.cx - 1, eye.cy, SSD1306_WHITE);
    } else {
        // Standart Modern Robot Gözü (Yumuşak köşeli dikdörtgen)
        display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
    }
}

void render() {
    display.clearDisplay();

    if (currentMood == MOOD_USER_SPECIAL) {
        // Kullanıcının fotoğrafındaki özel göz ve göz kırpma ifadesi
        display.drawBitmap(0, 0, epd_bitmap_user_wink, 128, 32, SSD1306_WHITE);
        // Açık olan gözün gözbebeğinde canlılık parıltısı (twinkle efekti)
        if ((millis() / 250) % 2 == 0) {
            display.drawPixel(35, 15, SSD1306_WHITE);
        }
    } else {
        drawSingleEye(leftEye, true);
        drawSingleEye(rightEye, false);

        // Şaşırma modunda gözlerin ortasında komik ünlem işareti (!)
        if (currentMood == MOOD_SURPRISED) {
            display.drawFastVLine(64, 8, 7, SSD1306_WHITE);
            display.drawPixel(64, 18, SSD1306_WHITE);
        }
    }

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
    STATE_HAPPY,
    STATE_KAWAII,
    STATE_SURPRISED,
    STATE_HUNTER,
    STATE_USER_SPECIAL
};

State state = STATE_IDLE;
unsigned long timer = 0;
unsigned long interval = 2500;
float currentSpeed = 2.5f;

void triggerNextAction() {
    int roll = random(0, 100);

    if (roll < 18) {
        // [18%] Standart Doğal Göz Kırpma (Normal Blink)
        state = STATE_BLINK_CLOSE;
        targetLeft.h = 2.0f;
        targetLeft.r = 1.0f;
        targetRight.h = 2.0f;
        targetRight.r = 1.0f;
        currentSpeed = 4.8f;
        timer = millis();
    } else if (roll < 26) {
        // [8%] Çift Göz Kırpma (Double Blink)
        state = STATE_DOUBLE_BLINK_1_CLOSE;
        targetLeft.h = 2.0f;
        targetLeft.r = 1.0f;
        targetRight.h = 2.0f;
        targetRight.r = 1.0f;
        currentSpeed = 5.0f;
        timer = millis();
    } else if (roll < 33) {
        // [7%] Robot Tek Göz Kırpma (Cute Robot Wink)
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
    } else if (roll < 45) {
        // [12%] Sağa veya Sola Bakınma (Look Left / Right)
        state = STATE_LOOK;
        float dirX = (random(0, 2) == 0) ? -14.0f : 14.0f;
        setNormalTarget(dirX, 0.0f);
        currentSpeed = 2.0f;
        timer = millis();
        interval = random(1200, 2200);
    } else if (roll < 53) {
        // [8%] Yukarı veya Aşağı Bakınma (Look Up / Down)
        state = STATE_LOOK;
        float dirY = (random(0, 2) == 0) ? -4.0f : 4.0f;
        setNormalTarget(0.0f, dirY);
        currentSpeed = 1.8f;
        timer = millis();
        interval = random(1000, 1800);
    } else if (roll < 65) {
        // [12%] Mutlu / Gülen Robot İfadesi (Happy Hilal Gözler ^ ^)
        state = STATE_HAPPY;
        currentMood = MOOD_HAPPY;
        setTargetEyes(
            LEFT_EYE_CENTER_X, EYE_CENTER_Y, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_RADIUS,
            RIGHT_EYE_CENTER_X, EYE_CENTER_Y, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_RADIUS
        );
        currentSpeed = 2.5f;
        timer = millis();
        interval = random(1600, 2500);
    } else if (roll < 76) {
        // [11%] Tatlı / Parıltılı Anime Gözler (Kawaii Sparkle * *)
        state = STATE_KAWAII;
        currentMood = MOOD_KAWAII;
        setTargetEyes(
            LEFT_EYE_CENTER_X, EYE_CENTER_Y, 30.0f, 26.0f, 7.0f,
            RIGHT_EYE_CENTER_X, EYE_CENTER_Y, 30.0f, 26.0f, 7.0f
        );
        currentSpeed = 2.0f;
        timer = millis();
        interval = random(1800, 2800);
    } else if (roll < 86) {
        // [10%] Şaşırma Gözleri (Surprised O_O !)
        state = STATE_SURPRISED;
        currentMood = MOOD_SURPRISED;
        setTargetEyes(
            LEFT_EYE_CENTER_X, EYE_CENTER_Y, 26.0f, 28.0f, 8.0f,
            RIGHT_EYE_CENTER_X, EYE_CENTER_Y, 26.0f, 28.0f, 8.0f
        );
        currentSpeed = 4.2f;
        timer = millis();
        interval = random(1400, 2200);
    } else if (roll < 95) {
        // [9%] Avcı Gözü (Hunter Eyes / Canthal Tilt)
        state = STATE_HUNTER;
        currentMood = MOOD_HUNTER;
        setTargetEyes(
            LEFT_EYE_CENTER_X, EYE_CENTER_Y + 1.0f, 34.0f, 13.0f, 2.0f,
            RIGHT_EYE_CENTER_X, EYE_CENTER_Y + 1.0f, 34.0f, 13.0f, 2.0f
        );
        currentSpeed = 2.2f;
        timer = millis();
        interval = random(2000, 3200);
    } else {
        // [5%] KULLANICININ ÖZEL GÖZÜ (Nadir Easter Egg - Fotoğraftaki İfade ve Göz Kırpma)
        state = STATE_USER_SPECIAL;
        currentMood = MOOD_USER_SPECIAL;
        timer = millis();
        interval = random(2600, 3600);
        Serial.println(F("[NADIR EMOTE] Kullanicinin ozel goz ifadesi tetiklendi!"));
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
    interval = random(2000, 3500);
    currentSpeed = 2.5f;
    Serial.println(F("[BILGI] Gelismis animasyon motoru aktif."));
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
                interval = random(1800, 4000);
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
                interval = random(1800, 4000);
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
                interval = random(1800, 4000);
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
        case STATE_KAWAII:
        case STATE_SURPRISED:
        case STATE_HUNTER:
        case STATE_USER_SPECIAL:
            if (now - timer >= interval) {
                setNormalTarget();
                currentSpeed = 2.5f;
                state = STATE_IDLE;
                timer = now;
                interval = random(1600, 3500);
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