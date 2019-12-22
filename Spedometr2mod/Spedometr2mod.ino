#include <SSD1306.h>  // Библиотека для работы с дисплеем
#include "UbloxGPS.h" // Библиотека для работы с GPS
#include "font.h"     // Шрифт Orbitron_Light_26, говно шрифт, не моноширинный

// Инициализируем дисплей 128х64 подключенный к пинам D2 и D1
SSD1306  display(0x3c, 4, 5); 

bool deBug = false;          // Отладка на порту
int ScreenUpdateTime = 2000; // Время обновления экрана, мс (чем чаще - тем больше погрешность измерений)

int  loops = 0;         // для отладки
char gpsSpeed[3];       // Буфер для строки с скоростью
int  gpsSpeedKm = 0;    // Скорость в км/ч
int  gpsSpeedKmMAX = 0; // Максимальная скорость
bool start = false;     // Старт замера
long startMillis = 0;   // Начало отсчета
long currentMillis = 0; // Текущее время
float meteringTime = 0; // Время замера
long NumSatellites = 0; // Количество спутников
char gpsTime[9];        // Время
unsigned long lastScreenUpdate = 0; // Последнее обновление экрана
int ScreenUpdateTimeOSV = 400; // Время обновления экрана когда GPS в поиске
char symbols[5] = "|/-\\";
int symbolnow = 0;

// Cтруктура результатов
struct Metering
{
  float accel30;
  float accel60;
  float accel100;
  bool met30;
  bool met60;
  bool met100;
};
Metering metering;

void setup() {
  delay(1500);                    // Без этой задержки плата GPS подвешивает Wemos
  //serial.begin(57600);            // Скорость обмена с GPS, при 115200 мой чип работает не стабильно

  serial.begin(115200, SWSERIAL_8N1,
        D7, D8, false,
        256, 0);
        
  Serial.begin(115200);           // Вывод в порт для дебага, 115200 в оригинале

  gpsSetup();                     // Настройка модуля Ublox
  metering = {30.0, 60.0, 100.0, true, true, true }; // Тут будем хранить результаты

  // Вывод приветствия
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
//  display.drawString(64, 1, "ardulogic.ru | thid.ru");
  display.drawString(64, 1, "drive2.ru / users / DeHb");
  display.setFont(ArialMT_Plain_16);
  display.drawString(65, 15, "Spedometr2mod");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 36, "Screen update "+String(ScreenUpdateTime)+"ms");
  display.drawString(64, 52, "GPS 16,67Hz");
  display.display();
  delay(5000);
  
}
void loop() {
  currentMillis = millis(); // текущее время в миллисекундах
  int msgType = processGPS();
  if (msgType == MT_NAV_PVT) {

    NumSatellites = ubxMessage.navPvt.numSV;
    gpsSpeedKm = ubxMessage.navPvt.gSpeed * 0.0036; // Переводим в км/ч

    if (deBug) {
     loops++;
     if ( loops > 120 ) { loops = 0; }
     gpsSpeedKm = loops; 
    }

    if (gpsSpeedKm > gpsSpeedKmMAX) { gpsSpeedKmMAX = gpsSpeedKm; }
    // Если движемся
    if (gpsSpeedKm > 0) {

      // Если это был старт
      if (!start) {
        start = true;
        startMillis = millis();
        metering.met30 = false;
        metering.met60 = false;
        metering.met100 = false;
      } 
      if (!metering.met30 && !metering.met60 && !metering.met100 && gpsSpeedKm > 29) { clearResult(); } // обнуление результатов на экране при достижении скорости
     
      meteringTime = (float)(currentMillis - startMillis) / 1000; // Время замера

      // Заношу результаты замера 
      if (!metering.met30 && gpsSpeedKm >= 30) {
        metering.accel30 = meteringTime; // Разгон до 30км/ч
        metering.met30 = true; }
      if (!metering.met60 && gpsSpeedKm >= 60) {
        metering.accel60 = meteringTime; // Разгон до 60км/ч
        metering.met60 = true; }
      if (!metering.met100 && gpsSpeedKm >= 100) {
        metering.accel100 = meteringTime; // Разгон до 100км/ч
        metering.met100 = true; }
    } else if (start && 0 == gpsSpeedKm) { // Если остановились
      start = false; }

   if (deBug) {
    Serial.print("#SV: ");      Serial.print(ubxMessage.navPvt.numSV);
    Serial.print(" fixType: "); Serial.print(ubxMessage.navPvt.fixType);
    Serial.print(" Date:");     Serial.print(ubxMessage.navPvt.year); Serial.print("/"); Serial.print(ubxMessage.navPvt.month); Serial.print("/"); Serial.print(ubxMessage.navPvt.day); Serial.print(" "); Serial.print(ubxMessage.navPvt.hour); Serial.print(":"); Serial.print(ubxMessage.navPvt.minute); Serial.print(":"); Serial.print(ubxMessage.navPvt.second);
    Serial.print(" lat/lon: "); Serial.print(ubxMessage.navPvt.lat / 10000000.0f); Serial.print(","); Serial.print(ubxMessage.navPvt.lon / 10000000.0f);
    Serial.print(" gSpeed: ");  Serial.print(ubxMessage.navPvt.gSpeed / 1000.0f);
    Serial.print(" heading: "); Serial.print(ubxMessage.navPvt.heading / 100000.0f);
    Serial.print(" hAcc: ");    Serial.print(ubxMessage.navPvt.hAcc / 1000.0f);
    Serial.println(); 
   } // debug
  } // if (msgType == MT_NAV_PVT)

  int ScreenUpdateTimeNow = ScreenUpdateTime;
  if ( NumSatellites < 2 ) { ScreenUpdateTimeNow = ScreenUpdateTimeOSV; }
  if (currentMillis - lastScreenUpdate > ScreenUpdateTimeNow) {
    updateDisplay();
    lastScreenUpdate = currentMillis;
  }

}
void clearResult() {
 metering.accel30 = 0.0;
 metering.accel60 = 0.0;
 metering.accel100 = 0.0; 
 gpsSpeedKmMAX = 0;
}

void updateDisplay() {
  display.clear();
// Рисуем индикатор приёма
  display.drawVerticalLine(1, 12, 2);
  display.drawVerticalLine(0, 12, 2); 
// Херовый приём  
  if (NumSatellites > 3 ) {
  display.drawVerticalLine(4, 10, 4);
  display.drawVerticalLine(3, 10, 4); }
// Нормальный
  if (NumSatellites > 5 ) {
  display.drawVerticalLine(7, 7, 7);
  display.drawVerticalLine(6, 7, 7); }
// Отличный приём спутников
  if (NumSatellites > 8 ) {
  display.drawVerticalLine(10, 4, 10);
  display.drawVerticalLine(9, 4, 10); }
// Разделительная линия
  display.drawHorizontalLine(0, 15, 128);

// Выводим количество спутников 
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(14, 2, (String)NumSatellites+" fix: "+ubxMessage.navPvt.fixType); // вывожу количество пойманных спутников
// Вывод времени
  sprintf(gpsTime, "%02d:%02d:%02d",ubxMessage.navPvt.hour, ubxMessage.navPvt.minute, ubxMessage.navPvt.second);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(128, 2, (String)"UTC "+gpsTime);
// Погрешность измерений
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if ( NumSatellites < 2 ) {
   symbolnow++;
   if (symbolnow > 3 ) { symbolnow = 0; }
   display.drawString(1, 54, String(symbols[symbolnow])+" SEARCH GPS"); }
   else {
    float sAcc = ubxMessage.navPvt.sAcc / 1000.0f;
    if (sAcc > 9) { sAcc = 9; };
    display.drawString(1, 54, "± "+String(sAcc)+"m"); }
// Вывод скорости
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(Orbitron_Light_26);
  sprintf(gpsSpeed, "%03d", gpsSpeedKm);
  display.drawString(44, 20, gpsSpeed);
// Максимальной скорости за заезд
  if ( gpsSpeedKmMAX > 30 ) {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(44, 40, "max "+(String)gpsSpeedKmMAX+" km/h"); }
  // Разгон до 30 км/ч
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(128, 20, (String)metering.accel30);
  // Разгон до 60 км/ч
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(128, 34, (String)metering.accel60);
  // Разгон до 100 км/ч
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(128, 48, (String)metering.accel100);
  display.display();
}
