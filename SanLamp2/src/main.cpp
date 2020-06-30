#include <Arduino.h>
#include <FastLED.h>
#include <PubSubClient.h>
//#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

//Настроечные переменные
#define BRIGHTNESS 40 // стандартная маскимальная яркость (0-255)
#define CURRENT_LIMIT 2000 // лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит
#define WIDTH 16 // ширина матрицы
#define HEIGHT 16 // высота матрицы
#define COLOR_ORDER GRB // порядок цветов на ленте. Если цвет отображается некорректно - меняйте. Начать можно с RGB
#define MATRIX_TYPE 0 // тип матрицы: 0 - зигзаг, 1 - параллельная
#define CONNECTION_ANGLE 0 // угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#define STRIP_DIRECTION 0 // направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз
#define LED_PIN D4 // пин ленты
#define BTN_PIN D2
#define MODE_AMOUNT 18
#define NUM_LEDS WIDTH *HEIGHT
#define SEGMENTS 1 // диодов в одном "пикселе" (для создания матрицы из кусков ленты)
CRGB leds[NUM_LEDS];

//Utility Functions
// **************** НАСТРОЙКА МАТРИЦЫ ****************
#if (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 0)
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y

#elif (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 1)
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y x

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 0)
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 3)
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y x

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 2)
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 3)
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y (WIDTH - x - 1)

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 2)
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y y

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 1)
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y (WIDTH - x - 1)

#else
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y
#pragma message "Wrong matrix parameters! Set to default"

#endif

// получить номер пикселя в ленте по координатам
uint16_t getPixelNumber(int8_t x, int8_t y) {
if ((THIS_Y % 2 == 0) || MATRIX_TYPE) { // если чётная строка
return (THIS_Y * _WIDTH + THIS_X);
} else { // если нечётная строка
return (THIS_Y * _WIDTH + _WIDTH - THIS_X - 1);
}

}

// функция получения цвета пикселя по его номеру

uint32_t getPixColor(int thisSegm) {

int thisPixel = thisSegm * SEGMENTS;

if (thisPixel < 0 || thisPixel > NUM_LEDS - 1) return 0;

return (((uint32_t)leds[thisPixel].r << 16) | ((long)leds[thisPixel].g << 8 ) | (long)leds[thisPixel].b);

}

// функция получения цвета пикселя в матрице по его координатам

uint32_t getPixColorXY(int8_t x, int8_t y) {

return getPixColor(getPixelNumber(x, y));

}

// залить все

void fillAll(CRGB color) {
for (int i = 0; i < NUM_LEDS; i++) {
leds[i] = color;
}
}

// функция отрисовки точки по координатам X Y

void drawPixelXY(int8_t x, int8_t y, CRGB color) {
  if (x < 0 || x > WIDTH - 1 || y < 0 || y > HEIGHT - 1) return;
  int thisPixel = getPixelNumber(x, y) * SEGMENTS;
  for (byte i = 0; i < SEGMENTS; i++) {
    leds[thisPixel + i] = color;
  }
}

// ================================= ЭФФЕКТЫ ====================================

struct {
byte brightness = 50;
byte speed = 30;
byte scale = 40;
} modes[MODE_AMOUNT];
int8_t currentMode = 1;
boolean loadingFlag = true;
boolean ONflag = true;
boolean ONflagOld = true;

unsigned char matrixValue[8][16];
unsigned char line[WIDTH];

//Utility

void fadePixel(byte i, byte j, byte step) { // новый фейдер

int pixelNum = getPixelNumber(i, j);

if (getPixColor(pixelNum) == 0) return;

if (leds[pixelNum].r >= 30 ||

leds[pixelNum].g >= 30 ||

leds[pixelNum].b >= 30) {

leds[pixelNum].fadeToBlackBy(step);

} else {

leds[pixelNum] = 0;

}

}

void fader(byte step) {

for (byte i = 0; i < WIDTH; i++) {

for (byte j = 0; j < HEIGHT; j++) {

fadePixel(i, j, step);

}

}

}

void generateLine() {

for (uint8_t x = 0; x < WIDTH; x++) {

line[x] = random(64, 255);

}

}

void shiftUp() {

for (uint8_t y = HEIGHT - 1; y > 0; y--) {

for (uint8_t x = 0; x < WIDTH; x++) {

uint8_t newX = x;

if (x > 15) newX = x - 15;

if (y > 7) continue;

matrixValue[y][newX] = matrixValue[y - 1][newX];

}

}

for (uint8_t x = 0; x < WIDTH; x++) {

uint8_t newX = x;

if (x > 15) newX = x - 15;

matrixValue[0][newX] = line[newX];

}

}

// draw a frame, interpolating between 2 "key frames"

// @param pcnt percentage of interpolation

// --------------------------------- конфетти ------------------------------------

void sparklesRoutine() {

for (byte i = 0; i < modes[currentMode].scale; i++) {

byte x = random(0, WIDTH);

byte y = random(0, HEIGHT);

if (getPixColorXY(x, y) == 0)

leds[getPixelNumber(x, y)] = CHSV(random(0, 255), 255, 255);

}

fader(70);

}

// функция плавного угасания цвета для всех пикселей

// -------------------------------------- огонь ---------------------------------------------

// эффект "огонь"

#define SPARKLES 1 // вылетающие угольки вкл выкл

int pcnt = 0;

byte hue;

//these values are substracetd from the generated values to give a shape to the animation

const unsigned char valueMask[8][16] PROGMEM = {

{32 , 0 , 0 , 0 , 0 , 0 , 0 , 32 , 32 , 0 , 0 , 0 , 0 , 0 , 0 , 32 },

{64 , 0 , 0 , 0 , 0 , 0 , 0 , 64 , 64 , 0 , 0 , 0 , 0 , 0 , 0 , 64 },

{96 , 32 , 0 , 0 , 0 , 0 , 32 , 96 , 96 , 32 , 0 , 0 , 0 , 0 , 32 , 96 },

{128, 64 , 32 , 0 , 0 , 32 , 64 , 128, 128, 64 , 32 , 0 , 0 , 32 , 64 , 128},

{160, 96 , 64 , 32 , 32 , 64 , 96 , 160, 160, 96 , 64 , 32 , 32 , 64 , 96 , 160},

{192, 128, 96 , 64 , 64 , 96 , 128, 192, 192, 128, 96 , 64 , 64 , 96 , 128, 192},

{255, 160, 128, 96 , 96 , 128, 160, 255, 255, 160, 128, 96 , 96 , 128, 160, 255},

{255, 192, 160, 128, 128, 160, 192, 255, 255, 192, 160, 128, 128, 160, 192, 255}

};

const unsigned char hueMask[8][16] PROGMEM = {

{1 , 11, 19, 25, 25, 22, 11, 1 , 1 , 11, 19, 25, 25, 22, 11, 1 },

{1 , 8 , 13, 19, 25, 19, 8 , 1 , 1 , 8 , 13, 19, 25, 19, 8 , 1 },

{1 , 8 , 13, 16, 19, 16, 8 , 1 , 1 , 8 , 13, 16, 19, 16, 8 , 1 },

{1 , 5 , 11, 13, 13, 13, 5 , 1 , 1 , 5 , 11, 13, 13, 13, 5 , 1 },

{1 , 5 , 11, 11, 11, 11, 5 , 1 , 1 , 5 , 11, 11, 11, 11, 5 , 1 },

{0 , 1 , 5 , 8 , 8 , 5 , 1 , 0 , 0 , 1 , 5 , 8 , 8 , 5 , 1 , 0 },

{0 , 0 , 1 , 5 , 5 , 1 , 0 , 0 , 0 , 0 , 1 , 5 , 5 , 1 , 0 , 0 },

{0 , 0 , 0 , 1 , 1 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 0 , 0 , 0 }

};

void drawFrame(int pcnt) {

int nextv;

//each row interpolates with the one before it

for (unsigned char y = HEIGHT - 1; y > 0; y--) {

for (unsigned char x = 0; x < WIDTH; x++) {

uint8_t newX = x;

if (x > 15) newX = x - 15;

if (y < 8) {

nextv =

(((100.0 - pcnt) * matrixValue[y][newX]

+ pcnt * matrixValue[y - 1][newX]) / 100.0)

- pgm_read_byte(&(valueMask[y][newX]));

CRGB color = CHSV(

modes[currentMode].scale * 1 + pgm_read_byte(&(hueMask[y][newX])), // H

255, // S

(uint8_t)max(0, nextv) // V

);

leds[getPixelNumber(x, y)] = color;

} else if (y == 8 && SPARKLES) {

if (random(0, 20) == 0 && getPixColorXY(x, y - 1) != 0) drawPixelXY(x, y, getPixColorXY(x, y - 1));

else drawPixelXY(x, y, 0);

} else if (SPARKLES) {

// старая версия для яркости

if (getPixColorXY(x, y - 1) > 0)

drawPixelXY(x, y, getPixColorXY(x, y - 1));

else drawPixelXY(x, y, 0);

}

}

}

//first row interpolates with the "next" line
for (unsigned char x = 0; x < WIDTH; x++) {
uint8_t newX = x;
if (x > 15) newX = x - 15;
CRGB color = CHSV(
modes[currentMode].scale * 1 + pgm_read_byte(&(hueMask[0][newX])), // H
255, // S
(uint8_t)(((100.0 - pcnt) * matrixValue[0][newX] + pcnt * line[newX]) / 100.0) // V
);
leds[getPixelNumber(newX, 0)] = color;
}
}

//these are the hues for the fire,

//should be between 0 (red) to about 25 (yellow)

void fireRoutine() {

if (loadingFlag) {
loadingFlag = false;
//FastLED.clear();
generateLine();
}
if (pcnt >= 100) {
shiftUp();
generateLine();
pcnt = 0;
}

drawFrame(pcnt);

pcnt += 30;

}

// Randomly generate the next line (matrix row)

// ---------------------------------------- радуга ------------------------------------------

void rainbowVertical() {
  hue += 2;
  for (byte j = 0; j < HEIGHT; j++) {
    CHSV thisColor = CHSV((byte)(hue + j * modes[currentMode].scale), 255, 255);
    for (byte i = 0; i < WIDTH; i++)
      drawPixelXY(i, j, thisColor);
  }
}
void rainbowHorizontal() {
  hue += 2;
  for (byte i = 0; i < WIDTH; i++) {
    CHSV thisColor = CHSV((byte)(hue + i * modes[currentMode].scale), 255, 255);
    for (byte j = 0; j < HEIGHT; j++)
      drawPixelXY(i, j, thisColor);   //leds[getPixelNumber(i, j)] = thisColor;
  }
}

// ---------------------------------------- ЦВЕТА ------------------------------------------

void colorsRoutine() {

hue += modes[currentMode].scale;

for (int i = 0; i < NUM_LEDS; i++) {

leds[i] = CHSV(hue, 255, 255);

}

}

// --------------------------------- ЦВЕТ ------------------------------------

void colorRoutine() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(modes[currentMode].scale * 1, 255, 255);
  }
}

// ------------------------------ снегопад 2.0 --------------------------------

void snowRoutine() {
  // сдвигаем всё вниз
  for (byte x = 0; x < WIDTH; x++) {
    for (byte y = 0; y < HEIGHT - 1; y++) {
      drawPixelXY(x, y, getPixColorXY(x, y + 1));
    }
  }

  for (byte x = 0; x < WIDTH; x++) {
    // заполняем случайно верхнюю строку
    // а также не даём двум блокам по вертикали вместе быть
    if (getPixColorXY(x, HEIGHT - 2) == 0 && (random(0, modes[currentMode].scale) == 0))
      drawPixelXY(x, HEIGHT - 1, 0xE0FFFF - 0x101010 * random(0, 4));
    else
      drawPixelXY(x, HEIGHT - 1, 0x000000);
  }
}

// ------------------------------ МАТРИЦА ------------------------------

void matrixRoutine() {

for (byte x = 0; x < WIDTH; x++) {

// заполняем случайно верхнюю строку

uint32_t thisColor = getPixColorXY(x, HEIGHT - 1);

if (thisColor == 0)

drawPixelXY(x, HEIGHT - 1, 0x00FF00 * (random(0, modes[currentMode].scale) == 0));

else if (thisColor < 0x002000)

drawPixelXY(x, HEIGHT - 1, 0);

else

drawPixelXY(x, HEIGHT - 1, thisColor - 0x002000);

}

// сдвигаем всё вниз

for (byte x = 0; x < WIDTH; x++) {

for (byte y = 0; y < HEIGHT - 1; y++) {

drawPixelXY(x, y, getPixColorXY(x, y + 1));

}

}

}

// ----------------------------- СВЕТЛЯКИ ------------------------------

#define LIGHTERS_AM 100
int lightersPos[2][LIGHTERS_AM];
int8_t lightersSpeed[2][LIGHTERS_AM];
CHSV lightersColor[LIGHTERS_AM];
byte loopCounter;
int angle[LIGHTERS_AM];
int speedV[LIGHTERS_AM];
int8_t angleSpeed[LIGHTERS_AM];

void lightersRoutine() {
  if (loadingFlag) {
    loadingFlag = false;
    randomSeed(millis());
    for (byte i = 0; i < LIGHTERS_AM; i++) {
      lightersPos[0][i] = random(0, WIDTH * 10);
      lightersPos[1][i] = random(0, HEIGHT * 10);
      lightersSpeed[0][i] = random(-10, 10);
      lightersSpeed[1][i] = random(-10, 10);
      lightersColor[i] = CHSV(random(0, 255), 255, 255);
    }
  }
  FastLED.clear();
  if (++loopCounter > 20) loopCounter = 0;
  for (byte i = 0; i < modes[currentMode].scale; i++) {
    if (loopCounter == 0) {     // меняем скорость каждые 255 отрисовок
      lightersSpeed[0][i] += random(-3, 4);
      lightersSpeed[1][i] += random(-3, 4);
      lightersSpeed[0][i] = constrain(lightersSpeed[0][i], -20, 20);
      lightersSpeed[1][i] = constrain(lightersSpeed[1][i], -20, 20);
    }

    lightersPos[0][i] += lightersSpeed[0][i];
    lightersPos[1][i] += lightersSpeed[1][i];

    if (lightersPos[0][i] < 0) lightersPos[0][i] = (WIDTH - 1) * 10;
    if (lightersPos[0][i] >= WIDTH * 10) lightersPos[0][i] = 0;

    if (lightersPos[1][i] < 0) {
      lightersPos[1][i] = 0;
      lightersSpeed[1][i] = -lightersSpeed[1][i];
    }
    if (lightersPos[1][i] >= (HEIGHT - 1) * 10) {
      lightersPos[1][i] = (HEIGHT - 1) * 10;
      lightersSpeed[1][i] = -lightersSpeed[1][i];
    }
    drawPixelXY(lightersPos[0][i] / 10, lightersPos[1][i] / 10, lightersColor[i]);
  }
}

// ******************* НАСТРОЙКИ NOISE EFFECTS *****************

// "масштаб" эффектов. Чем меньше, тем крупнее!

#define MADNESS_SCALE 100
#define CLOUD_SCALE 30
#define LAVA_SCALE 50
#define PLASMA_SCALE 30
#define RAINBOW_SCALE 30
#define RAINBOW_S_SCALE 20
#define ZEBRA_SCALE 30
#define FOREST_SCALE 120
#define OCEAN_SCALE 90

// ***************** ДЛЯ РАЗРАБОТЧИКОВ ******************

// The 16 bit version of our coordinates

static uint16_t x;

static uint16_t y;

static uint16_t z;

uint16_t speed = 20; // speed is set dynamically once we've started up

uint16_t scale = 30; // scale is set dynamically once we've started up

// This is the array that we keep our computed noise values in

#define MAX_DIMENSION (max(WIDTH, HEIGHT))

#if (WIDTH > HEIGHT)

uint8_t noise[WIDTH][WIDTH];

#else

uint8_t noise[HEIGHT][HEIGHT];

#endif

CRGBPalette16 currentPalette( PartyColors_p );

uint8_t colorLoop = 1;

uint8_t ihue = 0;

// ******************* СЛУЖЕБНЫЕ *******************

void fillNoiseLED() {

uint8_t dataSmoothing = 0;

if ( speed < 50) {

dataSmoothing = 200 - (speed * 4);

}

for (int i = 0; i < MAX_DIMENSION; i++) {

int ioffset = scale * i;

for (int j = 0; j < MAX_DIMENSION; j++) {

int joffset = scale * j;

uint8_t data = inoise8(x + ioffset, y + joffset, z);

data = qsub8(data, 16);

data = qadd8(data, scale8(data, 39));

if ( dataSmoothing ) {

uint8_t olddata = noise[i][j];

uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);

data = newdata;

}

noise[i][j] = data;

}

}

z += speed;

// apply slow drift to X and Y, just for visual variation.

x += speed / 8;

y -= speed / 16;

for (int i = 0; i < WIDTH; i++) {

for (int j = 0; j < HEIGHT; j++) {

uint8_t index = noise[j][i];

uint8_t bri = noise[i][j];

// if this palette is a 'loop', add a slowly-changing base value

if ( colorLoop) {

index += ihue;

}

// brighten up, as the color palette itself often contains the

// light/dark dynamic range desired

if ( bri > 127 ) {

bri = 255;

} else {

bri = dim8_raw( bri * 2);

}

CRGB color = ColorFromPalette( currentPalette, index, bri);

drawPixelXY(i, j, color); //leds[getPixelNumber(i, j)] = color;

}

}

ihue += 1;

}

void fillnoise8() {

for (int i = 0; i < MAX_DIMENSION; i++) {

int ioffset = scale * i;

for (int j = 0; j < MAX_DIMENSION; j++) {

int joffset = scale * j;

noise[i][j] = inoise8(x + ioffset, y + joffset, z);

}

}

z += speed;

}

void madnessNoise() {

if (loadingFlag) {

loadingFlag = false;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

}

fillnoise8();

for (int i = 0; i < WIDTH; i++) {

for (int j = 0; j < HEIGHT; j++) {

CRGB thisColor = CHSV(noise[j][i], 255, noise[i][j]);

drawPixelXY(i, j, thisColor); //leds[getPixelNumber(i, j)] = CHSV(noise[j][i], 255, noise[i][j]);

}

}

ihue += 1;

}

void rainbowNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = RainbowColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 1;

}

fillNoiseLED();

}

void rainbowStripeNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = RainbowStripeColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 1;

}

fillNoiseLED();

}

void zebraNoise() {

if (loadingFlag) {

loadingFlag = false;

// 'black out' all 16 palette entries...

fill_solid( currentPalette, 16, CRGB::Black);

// and set every fourth one to white.

currentPalette[0] = CRGB::White;

currentPalette[4] = CRGB::White;

currentPalette[8] = CRGB::White;

currentPalette[12] = CRGB::White;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 1;

}

fillNoiseLED();

}

void forestNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = ForestColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 0;

}

fillNoiseLED();

}

void oceanNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = OceanColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 0;

}

fillNoiseLED();

}

void plasmaNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = PartyColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 1;

}

fillNoiseLED();

}

void cloudNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = CloudColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 0;

}

fillNoiseLED();

}

void lavaNoise() {

if (loadingFlag) {

loadingFlag = false;

currentPalette = LavaColors_p;

scale = modes[currentMode].scale;

speed = modes[currentMode].speed;

colorLoop = 0;

}

fillNoiseLED();

}

//=====================effectTicker=========================

uint32_t effTimer;

void effectsTick() {

if (ONflag && millis() - effTimer >= modes[currentMode].speed ) {
effTimer = millis();
switch (currentMode) {
case 1: colorRoutine();
break;
case 2: fireRoutine();
break;
case 3: snowRoutine();
break;
case 4: matrixRoutine();
break;
case 5: madnessNoise();
break;
case 6: cloudNoise();
break;
case 7: lavaNoise();
break;
case 8: plasmaNoise();
break;
case 9: rainbowNoise();
break;
case 10: rainbowStripeNoise();
break;
case 11: zebraNoise();
break;
case 12: forestNoise();
break;
case 13: oceanNoise();
break;
case 14: colorRoutine();
break;
case 15: rainbowHorizontal();
break;
case 16: sparklesRoutine();
break;
case 17: lightersRoutine();
break;
case 18: rainbowVertical();
break;
}
FastLED.show();
}
}

//MQTT setup
const char* ssid = "Sansemilyan";
const char* password = "89237073589";
const char* mqtt_server = "farmer.cloudmqtt.com";
uint16_t mqtt_port = 13091; // Порт для подключения к серверу MQTT
const char *mqtt_user = "upyhxiux";
const char *mqtt_pass = "vxkWzVyVL7E6";

WiFiClient espClient;
PubSubClient client(espClient);
//unsigned long lastMsg = 0;
//#define MSG_BUFFER_SIZE (50)
//char msg[MSG_BUFFER_SIZE];

byte* lastMsg ;
int value = 0;
StaticJsonDocument<200> doc;
void setup_wifi() {
delay(10);

// We start by connecting to a WiFi network
Serial.println();
Serial.print("Connecting to ");
Serial.println(ssid);
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
randomSeed(micros());
Serial.println("");
Serial.println("WiFi connected");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());
}

void changePower() {
  if (ONflag) {
    effectsTick();
    for (int i = 0; i < modes[currentMode].brightness; i += 8) {
      FastLED.setBrightness(i);
      delay(1);
      FastLED.show();
    }
    FastLED.setBrightness(modes[currentMode].brightness);
    delay(2);
    FastLED.show();
  } else {
    effectsTick();
    for (int i = modes[currentMode].brightness; i > 8; i -= 8) {
      FastLED.setBrightness(i);
      delay(1);
      FastLED.show();
    }
    FastLED.clear();
    delay(2);
    FastLED.show();
  }
}

void parseJSON(){
// Deserialize the JSON document
DeserializationError error = deserializeJson(doc, lastMsg);
if (error) {
Serial.print(F("deserializeJson() failed: "));
Serial.println(error.c_str());
return;
}
ONflag = doc["Power"];
if (ONflagOld!=ONflag)
  changePower();
currentMode = doc["Mode"];
modes[currentMode].speed= doc["Speed"];
modes[currentMode].scale= doc["Scale"];
modes[currentMode].brightness= doc["Brightness"];

FastLED.setBrightness(modes[currentMode].brightness);

Serial.print("ONflag");
Serial.println(ONflag);
Serial.print("ONflagOld");
Serial.println(ONflagOld);

ONflagOld = ONflag;

/*
Serial.print("ONflag");
Serial.println(ONflag);
Serial.print("currentMode");
Serial.println(currentMode);
Serial.print("modes[currentMode].speed");
Serial.println(modes[currentMode].speed);
Serial.print("modes[currentMode].scale");
Serial.println(modes[currentMode].scale);
Serial.print("modes[currentMode].brightness");
Serial.println(modes[currentMode].brightness);*/

}
void callback(char* topic, byte* payload, unsigned int length) {
Serial.print("Message arrived [");
Serial.print(topic);
Serial.print("] ");
for (int i = 0; i < length; i++) {
Serial.print((char)payload[i]);
}
Serial.println();
lastMsg = payload;
parseJSON();
}
void reconnect() {

// Loop until we're reconnected
while (!client.connected()) {
Serial.print("Attempting MQTT connection...");
// Attempt to connect
if ( client.connect("mqtt_user",mqtt_user, mqtt_pass)) {
Serial.println("connected");
// Once connected, publish an announcement...
client.publish("GLamp", "hello world");
// ... and resubscribe
client.subscribe("GLamp");
} else {
Serial.print("failed, rc=");
Serial.print(client.state());
Serial.println(" try again in 5 seconds");
// Wait 5 seconds before retrying
delay(5000);
}
}
}

void setup() {
//pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output
Serial.begin(9600);
setup_wifi();
client.setServer(mqtt_server, mqtt_port);
client.setCallback(callback);
FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)/*.setCorrection( TypicalLEDStrip )*/;
FastLED.setBrightness(BRIGHTNESS);
if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
FastLED.show();

}

void loop() {

if (!client.connected()) {
reconnect();
}
client.loop();
effectsTick();
}