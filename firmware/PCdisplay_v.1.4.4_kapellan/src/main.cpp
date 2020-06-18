/*
  Блок электроники для крутого моддинга вашего ПК, возможности:
  - Вывод основных параметров железа на внешний LCD дисплей
  - Температура: CPU, GPU, материнская плата, самый горячий HDD
  - Уровень загрузки: CPU, GPU, RAM, видеопамять
  - Температура с внешних датчиков (DS18B20)
  - Текущий уровень скорости внешних вентиляторов
  - Управление большим количеством 12 вольтовых 2, 3, 4 проводных вентиляторов
  - Автоматическое управление скоростью пропорционально температуре
  - Ручное управление скоростью из интерфейса программы
  - Управление RGB светодиодной лентой
  - Управление цветом пропорционально температуре (синий - зелёный - жёлтый - красный)
  - Ручное управление цветом из интерфейса программы

  Программа HardwareMonitorPlus  https://github.com/AlexGyver/PCdisplay
  - Запустить OpenHardwareMonitor.exe
  - Options/Serial/Run - запуск соединения с Ардуиной
  - Options/Serial/Config - настройка параметров работы
   - PORT address - адрес порта, куда подключена Ардуина
   - TEMP source - источник показаний температуры (процессор, видеокарта, максимум проц+видео, датчик 1, датчик 2)
   - FAN min, FAN max - минимальные и максимальные обороты вентиляторов, в %
   - TEMP min, TEMP max - минимальная и максимальная температура, в градусах Цельсия
   - Manual FAN - ручное управление скоростью вентилятора в %
   - Manual COLOR - ручное управление цветом ленты
   - LED brightness - управление яркостью ленты
   - CHART interval - интервал обновления графиков

  Что идёт в порт: 0-CPU temp, 1-GPU temp, 2-mother temp, 3-max HDD temp, 4-CPU load, 5-GPU load, 6-RAM use, 7-GPU memory use, 8-maxFAN, 
  9-minFAN, 10-maxTEMP, 11-minTEMP, 12-manualFAN, 13-manualCOLOR, 14-fanCtrl, 15-colorCtrl, 16-brightCtrl, 17-LOGinterval, 18-tempSource, 19-AltCPU temp
*/
// ------------------------ НАСТРОЙКИ ----------------------------
#define DRIVER_VERSION 1    // 0 - маркировка драйвера кончается на 4АТ, 1 - на 4Т
#define CPU_TEMP_SENSOR 1     // 0 или 1, выбрать перебором тот датчик, с которым температура процессора будет ближе к реальной
#define COLOR_ALGORITM 1    // 0 или 1 - разные алгоритмы изменения цвета (строка 222)
#define ERROR_DUTY 50       // скорость вентиляторов при потере связи
#define ERROR_TEMP 1        // 1 - показывать температуру при потере связи. 0 - нет
#define ERROR_BRIGHTNESS 50 // яркость LED при потере связи
// ------------------------ НАСТРОЙКИ ----------------------------

// ----------------------- ПИНЫ ---------------------------
#define FAN_PIN 9           // на мосфет вентиляторов
#define R_PIN 5             // на мосфет ленты, красный
#define G_PIN 3             // на мосфет ленты, зелёный
#define B_PIN 6             // на мосфет ленты, синий
#define POWER_BOOST 4       // флаг подключения внешнего питания
#define CURRENT_LIMIT 2000  // лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит

// ----------------------- ПИНЫ ---------------------------

// -------------------- БИБЛИОТЕКИ ---------------------
#include <Arduino.h>
#include <string.h>             // библиотека расширенной работы со строками
#include <Wire.h>               // библиотека для соединения
#include <TimerOne.h>           // библиотека таймера
// -------------------- БИБЛИОТЕКИ ---------------------

// настройки пределов скорости и температуры по умолчанию (на случай отсутствия связи)
byte speedMIN = 25, speedMAX = 100, tempMIN = 50, tempMAX = 70;

#if (CPU_TEMP_SENSOR)
int CPUtemp = 19;
#else
int CPUtemp = 0;
#endif

#define printByte(args)  write(args);

char inData[82];       // массив входных значений (СИМВОЛЫ)
int PCdata[20];        // массив численных значений показаний с компьютера
byte blocks, halfs;
byte index = 0;
String string_convert;
unsigned long timeout/*, uptime_timer*/;
boolean lightState, timeOut_flag = 1;
int duty, LEDcolor;
int k, b, R, G, B, Rf, Gf, Bf;
byte mainTemp;
byte lines[] = {4, 5, 7, 6}; // Lines - CPU, GPU, GPUmem, RAMuse
String perc;
unsigned long sec, mins, hrs;

void setup() {
  Serial.begin(9600);
  Timer1.initialize(40);   // поставить частоту ШИМ 25 кГц (40 микросекунд)
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  digitalWrite(R_PIN, 0);
  digitalWrite(G_PIN, 0);
  digitalWrite(B_PIN, 0);
  delay(2000);               // на 2 секунды
  PCdata[8] = speedMAX;
  PCdata[9] = speedMIN;
  PCdata[10] = tempMAX;
  PCdata[11] = tempMIN;
}
// 8-maxFAN, 9-minFAN, 10-maxTEMP, 11-minTEMP, 12-mnlFAN

void parsing() {
  while (Serial.available() > 0) {
    char aChar = Serial.read();
    if (aChar != 'E') {
      inData[index] = aChar;
      index++;
      inData[index] = '\0';
    } else {
      char *p = inData;
      char *str;
      index = 0;
      String value = "";
      while ((str = strtok_r(p, ";", &p)) != NULL) {
        string_convert = str;
        PCdata[index] = string_convert.toInt();
        index++;
      }
      index = 0;
    }
    if (!timeOut_flag) {                                // если связь была потеряна, но восстановилась
      //if (ERROR_UPTIME) uptime_timer = millis();        // сбросить uptime, если разрешено
    }
    timeout = millis();
    timeOut_flag = 1;
  }
}

void dutyCalculate() {
  if (PCdata[12] == 1)                  // если стоит галочка ManualFAN
    duty = PCdata[14];                  // скважность равна установленной ползунком
  else {                                // если нет
    switch (PCdata[18]) {
      case 0: mainTemp = PCdata[CPUtemp];                   // взять опорную температуру как CPU
        break;
      case 1: mainTemp = PCdata[1];                   // взять опорную температуру как GPU
        break;
      case 2: mainTemp = max(PCdata[CPUtemp], PCdata[1]);   // взять опорную температуру как максимум CPU и GPU
        break;
    }
    duty = map(mainTemp, PCdata[11], PCdata[10], PCdata[9], PCdata[8]);
    duty = constrain(duty, PCdata[9], PCdata[8]);
  }
  if (!timeOut_flag) duty = ERROR_DUTY;               // если пропало соединение, поставить вентиляторы на ERROR_DUTY
}

void timeoutTick() {
  if ((millis() - timeout > 5000) && timeOut_flag) {
    timeOut_flag = 0;
  }
}

void LEDcontrol() {
  if (timeOut_flag) {
    b = PCdata[16];
    if (PCdata[13] == 1)          // если стоит галочка Manual Color
      LEDcolor = PCdata[15];      // цвет равен установленному ползунком
    else {                        // если нет
      LEDcolor = map(mainTemp, PCdata[11], PCdata[10], 0, 1000);
      LEDcolor = constrain(LEDcolor, 0, 1000);
    }
  
    if (COLOR_ALGORITM) {
      // алгоритм цвета 1
      // синий убавляется, зелёный прибавляется
      // зелёный убавляется, красный прибавляется
      if (LEDcolor <= 500) {
        k = map(LEDcolor, 0, 500, 0, 255);
        R = 0;
        G = k;
        B = 255 - k;
      }
      if (LEDcolor > 500) {
        k = map(LEDcolor, 500, 1000, 0, 255);
        R = k;
        G = 255 - k;
        B = 0;
      }
  
    } else {
      // алгоритм цвета 2
      // синий максимум, плавно прибавляется зелёный
      // зелёный максимум, плавно убавляется синий
      // зелёный максимум, плавно прибавляется красный
      // красный максимум, плавно убавляется зелёный
  
      if (LEDcolor <= 250) {
        k = map(LEDcolor, 0, 250, 0, 255);
        R = 0;
        G = k;
        B = 255;
      }
      if (LEDcolor > 250 && LEDcolor <= 500) {
        k = map(LEDcolor, 250, 500, 0, 255);
        R = 0;
        G = 255;
        B = 255 - k;
      }
      if (LEDcolor > 500 && LEDcolor <= 750) {
        k = map(LEDcolor, 500, 750, 0, 255);
        R = k;
        G = 255;
        B = 0;
      }
      if (LEDcolor > 750 && LEDcolor <= 1000) {
        k = map(LEDcolor, 750, 1000, 0, 255);
        R = 255;
        G = 255 - k;
        B = 0;
      }
    }
  } else {
    b = ERROR_BRIGHTNESS;
    R = 255;
    G = 255;
    B = 255;
  }

  Rf = (b * R / 100);
  Gf = (b * G / 100);
  Bf = (b * B / 100);
  analogWrite(R_PIN, Rf);
  analogWrite(G_PIN, Gf);
  analogWrite(B_PIN, Bf);
}

// ------------------------------ ОСНОВНОЙ ЦИКЛ -------------------------------
void loop() {
  parsing();                          // парсим строки с компьютера
  dutyCalculate();                    // посчитать скважность для вентиляторов
  Timer1.pwm(FAN_PIN, duty * 10);     // управлять вентиляторами
  timeoutTick();                      // проверка таймаута
  LEDcontrol();                       // управлять цветом ленты
}
// ------------------------------ ОСНОВНОЙ ЦИКЛ -------------------------------
