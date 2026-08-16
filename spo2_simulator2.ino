#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <qrcode_espi.h>
#include "logo_ums_140_rgb888_draw.h"
#include <Wire.h>
//#include <pgm.h>

#define DHTPIN 22
#define DHTTYPE DHT22
#define BUZZER_PIN 33 
#define Abang 25 
#define Infra 26 

float ppgPhase = 0.0;

uint16_t UMS_ORANGE;
#define MED_GREEN 0x07E0
#define MED_RED 0xF800
#define MED_DARK 0x18E3
#define MED_SOFT 0xEF7D

uint16_t calData[5] = { 484, 3422, 331, 3403, 1 };


TFT_eSPI tft = TFT_eSPI();
DHT dht(DHTPIN, DHTTYPE);

unsigned long previousMillis = 0;
const long interval = 10;
const int waveDuration = 1000;

int jumlahe = 0;

float phase = 0.0;
const int RED_DC = 20;
const int IR_DC  = 100;
float IR_AC = 45.0;
float redAC = 6.0;

unsigned long lastPPGUpdate = 0;

unsigned long lastPPG = 0;
unsigned long lastLEDUpdate = 0;


// Titik referensi lainnya
const float RATIO_100 = 0.20; 
const float RATIO_95  = 0.60;
const float RATIO_90  = 0.90;
const float RATIO_85  = 0.80;
const float RATIO_80  = 1.00;
const float RATIO_70  = 1.20;

float interpolateRatio(
    float spo2,
    float s1,
    float r1,
    float s2,
    float r2){
    float x = (spo2 - s1) / (s2 - s1);
    return r1 + x * (r2 - r1);
}

const char* ssid = "noviaaa";
const char* password = "12345678";
String scriptURL =
"https://script.google.com/macros/s/AKfycbx_V1JNTR52H295jHwpPVIkVN_PyZ6T23KEFNuAxjJQWBDo5biG7t8ILxaUp7hymzyc/exec";
String qrLink = "";
int bpmList[] = {30, 60, 120, 180, 240};
int spo2List[] = {
    70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,
    90,91,92,93,94,95,96,97,98,99,100
    };

int bpmIndex = 1;    
int spo2Index = 11;   
int bpmInput = 60;
int spo2Input = 81;

int bpmSet = bpmList[bpmIndex];
int spo2Set = spo2List[spo2Index];

float suhuAwal = 0;
float suhuAkhir = 0;
int humAwal = 0;
int humAkhir = 0;

bool popUp = false;

uint16_t x, y;

#define ECG_X      35
#define ECG_Y      105
#define ECG_W      250
#define ECG_H      60

uint8_t ecgBuffer[ECG_W];

unsigned long lastECG = 0;
const int ecgFPS = 60;

bool beatActive = false;
int beatPos = -1;

unsigned long lastBeat = 0;
bool beatState = false;
bool runBlink = false;
unsigned long lastBlink = 0;

enum Page {
  MENU,
  SUHU_AWAL,
  KALIBRASI,
  SUHU_AKHIR,
  RUNN,
  INPUTAN,
  QR
};

Page page = MENU;

float getSpO2Ratio(int spo2) { 
     if (spo2 <= 70)
        return RATIO_70;


    if (spo2 <= 80)
        return interpolateRatio(
            spo2,
            70, RATIO_70,
            80, RATIO_80
        );


    if (spo2 <= 85)
        return interpolateRatio(
            spo2,
            80, RATIO_80,
            85, RATIO_85
        );


    if (spo2 <= 90)
        return interpolateRatio(
            spo2,
            85, RATIO_85,
            90, RATIO_90
        );


    if (spo2 <= 95)
        return interpolateRatio(
            spo2,
            90, RATIO_90,
            95, RATIO_95
        );

   
    if (spo2 <= 100)
        return interpolateRatio(
            spo2,
            95, RATIO_95,
            100, RATIO_100
        );

    return RATIO_100;
        }

void drawBorder(){
tft.fillScreen(TFT_BLACK);
tft.drawRect(0,40,320,200,TFT_LIGHTGREY);
tft.fillRect(0,0,320,36,TFT_NAVY);

tft.drawFastHLine(0,36,320,TFT_RED);
tft.drawFastHLine(0,37,320,TFT_RED);
}

void drawButton(int x, int y, int w, int h, String txt, uint16_t color)
{
    tft.fillRoundRect(x+2,y+2,w,h,8,TFT_LIGHTGREY);
    tft.fillRoundRect(x, y, w, h, 8, color);
    tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
    tft.drawRoundRect(x+1, y+1, w-2, h-2, 7, TFT_DARKGREY);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, color);
    tft.drawString(txt, x+w/2, y+h/2, 2);
}

void drawMenu()
{
drawBorder();
tft.setTextDatum(MC_DATUM);
tft.setTextColor(TFT_WHITE,TFT_NAVY);
tft.drawString("Menu Awal",160,20,4);

drawButton(45,58,230,36,"CEK SUHU AWAL",TFT_BLUE);
drawButton(45,108,230,36,"KALIBRASI",TFT_BLUE);
drawButton(45,158,230,36,"CEK SUHU AKHIR",TFT_BLUE);

}

void drawSuhuAwal()
{
    drawBorder();

    tft.drawRect(35,50,255,130,TFT_LIGHTGREY);

    tft.setTextColor(TFT_WHITE,TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Suhu Awal",160,20,4);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setCursor(100,70,6);
    tft.print(suhuAwal,1);
    tft.setCursor(140,120,6);
    tft.print(humAwal);
    tft.setTextSize(2);
    tft.setCursor(215,70,4);
    tft.print("C");
    tft.setCursor(215,120,4);
    tft.print("%");

    tft.setTextSize(1);

    drawButton(20,195,120,30,"Kembali",TFT_RED);
    drawButton(180,195,120,30,"Refresh",TFT_RED);
}

void updateSuhuAwal(){
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setCursor(100,70,6);
    tft.print(suhuAwal,1);
    tft.setCursor(140,120,6);
    tft.print(humAwal);
}



void drawKalibrasi()
{
    drawBorder();

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,TFT_NAVY);
    tft.drawString("Set Kalibrasi",160,20,4);

    tft.drawRoundRect(20,50,280,120,8,TFT_LIGHTGREY);

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.drawString("BPM",40,70,2);

    tft.drawString("SpO2 (%)",40,120,2);

    drawButton(150,62,32,28,"-",TFT_RED);
    drawButton(245,62,32,28,"+",TFT_GREEN);

    drawButton(150,112,32,28,"-",TFT_RED);
    drawButton(245,112,32,28,"+",TFT_GREEN);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.drawNumber(bpmSet,215,76,4);

    tft.drawNumber(spo2Set,215,126,4);

    drawButton(20,200,110,25,"KEMBALI",TFT_RED);
    drawButton(190,200,110,25,"START",TFT_GREEN);
}

void updateKalibrasi()
{
    tft.fillRect(185,55,55,90,TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);

    tft.drawNumber(bpmSet,215,76,4);
    tft.drawNumber(spo2Set,215,126,4);
}

void drawSuhuAkhir()
{
   drawBorder();

    tft.drawRect(35,50,255,130,TFT_LIGHTGREY);

    tft.setTextColor(TFT_WHITE,TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Suhu Akhir",160,20,4);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setCursor(100,70,6);
    tft.print(suhuAkhir,1);
    tft.setCursor(140,120,6);
    tft.print(humAkhir);
    tft.setTextSize(2);
    tft.setCursor(215,70,4);
    tft.print("C");
    tft.setCursor(215,120,4);
    tft.print("%");

    tft.setTextSize(1);

    drawButton(20,195,120,30,"Kembali",TFT_RED);
    drawButton(180,195,120,30,"Refresh",TFT_RED);
}

void updateSuhuAkhir(){
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setCursor(100,70,6);
    tft.print(suhuAkhir,1);
    tft.setCursor(140,120,6);
    tft.print(humAkhir);
}

void drawRunning()
{
    drawBorder();

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,TFT_NAVY);
    tft.drawString("KALIBRASI",160,20,4);

    drawButton(20,195,95,30,"Kembali",TFT_RED);
    drawButton(200,195,95,30,"INPUT",TFT_GREEN);

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.drawString("BPM  :",30,60,2);

    tft.drawString("SpO2 :",30,80,2);

    tft.drawNumber(bpmSet,110,55,4);

    tft.drawNumber(spo2Set,110,75,4);

    tft.drawString("%",170,75,4);
}


void Runninge()
{
    unsigned long interval = 60000UL / bpmSet;
    if(millis() - lastBeat >= interval)
    {
        lastBeat = millis();
        triggerBeat();
    }

    if(millis()-lastBlink>1000)
    {
        lastBlink=millis();

        runBlink=!runBlink;

        tft.fillRect(180,170,120,20,TFT_BLACK);

        if(runBlink)
        {
            tft.setTextColor(TFT_GREEN,TFT_BLACK);
            tft.drawString("RUNNING...",180,170,2);
        }
    }
}

void updateECG()
{
    if(millis()-lastECG<1000/ecgFPS) return;

    lastECG=millis();

    for(int i=0;i<ECG_W-1;i++)
        ecgBuffer[i]=ecgBuffer[i+1];

    int y = ECG_H/2;

    if(beatPos>=0)
    {
        int p = ECG_W-1-beatPos;

        if(p<4)
            y = ECG_H/2;

        else if(p==4)
            y = ECG_H/2-6;

        else if(p==5)
            y = ECG_H/2+8;

        else if(p==6)
            y = 4; 

        else if(p==7)
            y = ECG_H-5;

        else if(p==8)
            y = ECG_H/2-4;

        else if(p<14)
            y = ECG_H/2;

        beatPos--;

        if(beatPos<0)
            beatPos=-1;
    }

    ecgBuffer[ECG_W-1]=y;
}

void drawECG()
{
    tft.fillRect(ECG_X,ECG_Y,ECG_W,ECG_H,TFT_BLACK);

    tft.drawRect(ECG_X,ECG_Y,ECG_W,ECG_H,TFT_DARKGREY);

    for(int i=1;i<ECG_W;i++)
    {
        tft.drawLine(
            ECG_X+i-1,
            ECG_Y+ecgBuffer[i-1],
            ECG_X+i,
            ECG_Y+ecgBuffer[i],
            TFT_RED);
    }
}

void initECG()
{
    for(int i=0;i<ECG_W;i++)
        ecgBuffer[i]=ECG_H/2;
}

void triggerBeat()
{
    beatPos = ECG_W - 1;
    phase = PI / 2.0;
}


float ppgWave() { 
   float p = ppgPhase;
 if (ppgPhase < 0.5) return ppgPhase / 0.5; 
  else return 1.0 - ((ppgPhase - 0.5) / 0.5);
}

void updateSimulator()
{
 unsigned long now = micros();

    if (lastPPGUpdate == 0)
    {
        lastPPGUpdate = now;
        return;
    }

    unsigned long dt = now - lastPPGUpdate;
    lastPPGUpdate = now;

    float frequency = bpmSet / 60.0;

    ppgPhase += frequency * ((float)dt / 1000000.0);

    while (ppgPhase >= 1.0)
        ppgPhase -= 1.0;

    float wave = ppgWave();
    float targetRatio = getSpO2Ratio(spo2Set);

    float currentIRAC  = IR_AC;
    float currentREDAC = targetRatio * (currentIRAC / (float)IR_DC) * RED_DC;

    int redPWM = RED_DC + (int)(redAC * wave);
    int irPWM  = IR_DC  + (int)(currentIRAC  * wave);

    redPWM = constrain(redPWM, 0, 255);
    irPWM  = constrain(irPWM,  0, 255);

    analogWrite(Abang, redPWM);
    analogWrite(Infra,  irPWM);

Serial.print("IR = ");
Serial.print(irPWM);
Serial.print(", RED = ");
Serial.println(redPWM);
}

void waiting(){
tft.drawRoundRect(40,75,240,85,8,TFT_YELLOW);
tft.fillRoundRect(42,77,236,81,8,MED_SOFT);  
tft.setTextDatum(MC_DATUM);
tft.setTextColor(TFT_BLACK,MED_SOFT);
tft.drawString("Mengirim Data..",160,120,4); 
}

void popSuccess(){
tft.drawRoundRect(40,75,240,85,8,TFT_YELLOW);
tft.fillRoundRect(42,77,236,81,8,MED_SOFT);  
tft.fillCircle(160, 92, 10, TFT_GREEN);
tft.setTextDatum(MC_DATUM);
tft.setTextColor(TFT_BLACK,MED_SOFT);
tft.drawString("Data Terkirim",160,130,4);
delay(2000);
drawInput();
}

void popFail(){
tft.drawRoundRect(40,75,240,85,8,TFT_YELLOW);
tft.fillRoundRect(42,77,236,81,8,MED_SOFT);  
tft.fillCircle(160, 92, 10, TFT_RED);
tft.setTextDatum(MC_DATUM);
tft.setTextColor(TFT_BLACK,MED_SOFT);
tft.drawString("Gagal Mengirim",160,130,4);
delay(2000);
drawInput();
}

void drawInput()
{
    drawBorder();

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,TFT_NAVY);
    tft.drawString("Input Oxymeter",160,20,4);

    tft.drawRoundRect(20,50,280,120,8,TFT_LIGHTGREY);

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.drawString("BPM",40,70,2);

    tft.drawString("SpO2 (%)",40,120,2);

    tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.drawString("Pengukuran ",100,170,2);
    tft.drawNumber(jumlahe,180,170,2);
    tft.drawString("/ 6 ",195,170,2);

    drawButton(150,62,32,28,"-",TFT_RED);
    drawButton(245,62,32,28,"+",TFT_GREEN);

    drawButton(150,112,32,28,"-",TFT_RED);
    drawButton(245,112,32,28,"+",TFT_GREEN);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.drawNumber(bpmInput,215,76,4);

    tft.drawNumber(spo2Input,215,126,4);

    drawButton(20,200,110,25,"KEMBALI",TFT_RED);
    drawButton(190,200,110,25,"SAVE",TFT_NAVY);
}

void updateInput()
{
    tft.fillRect(185,55,55,90,TFT_BLACK);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);

    tft.drawNumber(bpmInput,215,76,4);
    tft.drawNumber(spo2Input,215,126,4);
}

int nilainya = 0;


void setup() {
  Serial.begin(115200);
  tft.init();
  UMS_ORANGE = tft.color565(255,100,0);
  tft.setRotation(3);
  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode( Abang, OUTPUT );
  pinMode( Infra, OUTPUT );
  analogWriteFrequency(10000);
analogWrite(Abang, 0);
analogWrite(Infra, 0);

if (SD.begin(15)) {

    Serial.println("SD OK");

    if (!SD.exists("/backup.csv")) {

        File file = SD.open("/backup.csv", FILE_WRITE);

        file.println("BPM_Set,SpO2_Set,BPM_Input,SpO2_Input,SuhuAwal,SuhuAkhir,RHAwal,RHAkhir");

        file.close();
    }
}
else {
    Serial.println("SD ERROR");
}

WiFi.begin(ssid,password);

tft.setTouch(calData);


showLogo();
showInitializing();
tft.setTextSize(1);
drawMenu();

}


void loop() {
touchHandle();

if(page==RUNN)
    {
        Runninge();
        updateECG();
        drawECG();
        updateSimulator();
    }else
    {
        analogWrite(Abang, 0);
        analogWrite(Infra, 0);

    }
}

void touchHandle(){
    if(tft.getTouch(&x,&y))
    {
       /* Serial.print(x);
        Serial.print(" , ");
        Serial.println(y);*/
        switch(page)
        {
            case MENU:

                if(x>45 && x<275 && y>58 && y<94)
                {
                    page=SUHU_AWAL;
                    bacaSuhuAwal();
                    drawSuhuAwal();
                    cekAlarm(suhuAwal,humAwal);
                    delay(200);
                }

                if(x>45 && x<275 && y>108 && y<144)
                {
                    page=KALIBRASI;
                    drawKalibrasi();
                    delay(200);
                    
                }

                if(x>45 && x<275 && y>158 && y<194)
                {
                    page=SUHU_AKHIR;
                    bacaSuhuAkhir();
                    drawSuhuAkhir();
                    cekAlarm(suhuAkhir,humAkhir);
                    delay(200);
                }

            break;

            case SUHU_AWAL:

                if(x>20 && x<150 && y>200 && y<230)
                {
                    page=MENU;
                    drawMenu();
                    noTone(BUZZER_PIN);
                    delay(200);
                }else if(x>190 && x<300 && y>200 && y<230){
                    page=SUHU_AWAL;
                    bacaSuhuAwal();
                    cekAlarm(suhuAwal,humAwal);
                    updateSuhuAwal();
                }

            break;

            case KALIBRASI:

                if(x>20 && x<120 && y>210 && y<230)
                {
                    page=MENU;
                    drawMenu();
                    delay(200);
                }else if(x>150 && x<185 && y>55 && y<90) //bpm -
                {
                    if(bpmIndex>0)
                    {
                        bpmIndex--;
                        if(bpmIndex <= 0) bpmIndex = 0;
                        bpmSet=bpmList[bpmIndex];
                        updateKalibrasi();
                        delay(200);
                    }
                }else if(x>250 && x<280 && y>60 && y<90) //bpm +
                {
                    if(bpmIndex<4)
                    {
                        bpmIndex++;
                        if(bpmIndex >= 4) bpmIndex = 4;
                        bpmSet=bpmList[bpmIndex];
                        updateKalibrasi();
                        delay(200);
                    }
                }else if(x>150 && x<185 && y>110 && y<140) //spo2 -
                {
                    if(spo2Index>0)
                    {
                        spo2Index--;
                        if(spo2Index < 0) spo2Index = 0;
                        spo2Set=spo2List[spo2Index];
                        updateKalibrasi();
                        delay(200);
                    }
                }else if(x>250 && x<280 && y>110 && y<140) //spo2 +
                {
                    if(spo2Index<30)
                    {
                        spo2Index++;
                        if(spo2Index > 30) spo2Index = 30;
                        spo2Set=spo2List[spo2Index];
                        updateKalibrasi();
                        delay(200);
                    }
                }else if(x>200 && x<300 && y>205 && y<230)
                {
                    ppgPhase = 0.0;
                    lastPPGUpdate = micros();

                    lastBeat = millis();
                    lastECG = millis();

                    tone(BUZZER_PIN,2000,80);
                    delay(200);
                    page = RUNN;
                    drawRunning();
                    initECG();
                }

            break;
            case RUNN:
            if(x>15 && x<110 && y>200 && y<230)
                {
                    analogWrite(Abang, 0);
                    analogWrite(Infra, 0);

                    page=KALIBRASI;
                    drawKalibrasi();

                    delay(200);
                }else if(x>206 && x<300 && y>200 && y<230)
                {
                    page = INPUTAN;
                    bpmInput = bpmSet;
                    spo2Input = spo2Set;
                    jumlahe++;
                    drawInput();
                }

            break;

            case INPUTAN:
                if(x>206 && x<300 && y>200 && y<230){
                    jumlahe++;
                    waiting();
                    saveToSpreadsheet();
                    if (jumlahe >= 7) {

                    jumlahe = 0;

                    showQRPage();

                    }

                }else if(x>15 && x<110 && y>200 && y<230){
                    page = MENU;
                    drawMenu();
                    delay(200);
                }else if(x>150 && x<185 && y>55 && y<90) //bpm -
                {
                    
                        bpmInput--;
                        if(bpmInput <= 30) bpmInput = 30;
                        updateInput();
                        delay(200);
                    
                }else if(x>250 && x<280 && y>60 && y<90) //bpm +
                {
                    
                        bpmInput++;
                        if (bpmInput > 240) bpmInput = 240;
                        updateInput();
                        delay(200);
                    
                }else if(x>150 && x<185 && y>110 && y<140) //spo2 -
                {
                    
                        spo2Input--;
                        if (spo2Input < 60) spo2Input = 60;
                        updateInput();
                        delay(200);
                    
                }else if(x>250 && x<280 && y>110 && y<140) //spo2 +
                {
                        spo2Input++;
                        if (spo2Input > 105) spo2Input = 105;
                        updateInput();
                        delay(200);
    
                }

                break;
            
            case QR:
                if(x>10 && x<25 && y>200 && y<225)
                {
                    page=MENU;
                    drawMenu();
                    delay(200);
                }
            break;

            case SUHU_AKHIR:

                if(x>20 && x<150 && y>200 && y<230)
                {
                    page=MENU;
                    drawMenu();
                    noTone(BUZZER_PIN);
                    delay(200);
                }else if(x>190 && x<300 && y>200 && y<230){
                    page=SUHU_AKHIR;
                    bacaSuhuAkhir();
                    cekAlarm(suhuAkhir,humAkhir);
                    updateSuhuAkhir();
                }

            break;
        }
    }
}


void cekAlarm(float suhu, float hum)
{
    bool alarm = false;

    if (suhu < 20.0 || suhu > 30.0)
        alarm = true;

    if (hum < 35.0 || hum > 73.0)
        alarm = true;

    if (alarm)
    {
        tone(BUZZER_PIN, 3000);
    }
    else
    {
        noTone(BUZZER_PIN);
    }
}


void showQRPage() {
  tft.fillScreen(TFT_WHITE);

  if (qrLink != "") {
    page = QR;
    drawQRCode(qrLink, 0, 0, 1);
    drawButton(10,200,25,25,"<-",TFT_RED);

  } else {

    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    centerText("QR GAGAL DIBUAT PERIKSA WiFi", 120);
    delay(3000);
    tft.fillScreen(TFT_WHITE);
    page = MENU;
    drawMenu();

  }
}

void drawQRCode(String text, int x, int y, int scale) {
  QRcode_eSPI qrcode(&tft);

  qrcode.init();
  qrcode.create(text);
}  

void bacaSuhuAwal(){
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    suhuAwal = t;
    humAwal = h;
}
void bacaSuhuAkhir(){
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    suhuAkhir = t;
    humAkhir = h;
}

void saveToSpreadsheet() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) suhuAkhir = t;
  if (!isnan(h)) humAkhir = h;

  if (suhuAwal == 0) suhuAwal = suhuAkhir;
  if (humAwal == 0) humAwal = humAkhir;
File file = SD.open("/backup.csv", FILE_APPEND);

if(file){

    file.print(bpmSet);
    file.print(",");

    file.print(spo2Set);
    file.print(",");

    file.print(bpmInput);
    file.print(",");

    file.print(spo2Input);
    file.print(",");

    file.print(suhuAwal,1);
    file.print(",");

    file.print(suhuAkhir,1);
    file.print(",");

    file.print(humAwal);
    file.print(",");

    file.println(humAkhir);

    file.close();

    Serial.println("DATA DISIMPAN KE SD");

}
else{

    Serial.println("GAGAL MENULIS SD");

}
String url = scriptURL;
url += "?bpm=" + String(bpmSet);
url += "&spo2=" + String(spo2Set);
url += "&bpmHasil=" + String(bpmInput);
url += "&spo2Hasil=" + String(spo2Input);
url += "&suhuAwal=" + String(suhuAwal, 1);
url += "&suhuAkhir=" + String(suhuAkhir, 1);
url += "&humAwal=" + String(humAwal);
url += "&humAkhir=" + String(humAkhir);
WiFiClientSecure client;
client.setInsecure();

HTTPClient http;
http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
url.replace(" ", "");
Serial.println(url);
http.begin(client, url);
Serial.println("========== URL ==========");
Serial.println(url);
Serial.println("=========================");
Serial.println();
Serial.println("===== DATA KALIBRASI =====");

Serial.print("BPM SET      : ");
Serial.println(bpmSet);

Serial.print("SpO2 SET     : ");
Serial.println(spo2Set);

Serial.print("BPM INPUT    : ");
Serial.println(bpmInput);

Serial.print("SpO2 INPUT   : ");
Serial.println(spo2Input);

Serial.print("SUHU AWAL    : ");
Serial.println(suhuAwal, 1);

Serial.print("SUHU AKHIR   : ");
Serial.println(suhuAkhir, 1);

Serial.print("RH AWAL      : ");
Serial.println(humAwal);

Serial.print("RH AKHIR     : ");
Serial.println(humAkhir);

Serial.println("================================");
Serial.println(url);
Serial.println("================================");
Serial.println("REQUEST:");
Serial.println(url);
int httpCode = http.GET();

Serial.print("HTTP Code : ");
Serial.println(httpCode);

if (httpCode < 0) {
  Serial.print("Error : ");
  Serial.println(http.errorToString(httpCode));
}

Serial.println(http.getString());
if (httpCode == 200) {

    Serial.println("UPLOAD BERHASIL");

    qrLink = "https://docs.google.com/spreadsheets/d/18z9m0P9GDKcLRRUqQgAG69c1Md-s__HEbU14Naq7E8c/edit";
    popSuccess();
}
else {

    Serial.println("UPLOAD GAGAL");
    popFail();
    qrLink = "";

}

http.end();



Serial.print("Pengukuran ke-");
Serial.println(jumlahe);
}

void centerText(String text, int y) {
  int tw = tft.textWidth(text);
  tft.setCursor((320 - tw) / 2, y);
  tft.print(text);
}

void showLogo() {
  tft.fillScreen(TFT_WHITE);
drawLogoUMS(tft, 90, 10);
  String nama = "Novia Surya Sukawati";
  String nim = "01202205032";

  delay(700);
  tft.setTextSize(2);

  for (int y = 235; y >= 165; y--) {
    tft.fillRect(0, 150, 320, 90, TFT_WHITE);

    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    centerText(nama, y);

    tft.drawLine(90, y + 27, 230, y + 27, UMS_ORANGE);

    tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
    centerText(nim, y + 40);

    delay(30);
  }

  tft.fillRect(0, 150, 320, 90, TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  centerText(nama, 165);

  tft.drawLine(90, 192, 230, 192, UMS_ORANGE);

  tft.setTextColor(TFT_DARKGREY, TFT_WHITE);
  centerText(nim, 205);

  delay(500);
}

void showInitializing() {
  tft.fillScreen(TFT_WHITE);

  tft.setTextColor(UMS_ORANGE, TFT_WHITE);
  tft.setTextSize(3);
  centerText("Initializing", 70);

  tft.drawRoundRect(50, 130, 220, 18, 8, UMS_ORANGE);

  for (int i = 0; i <= 216; i += 2) {
    tft.fillRoundRect(52, 132, i, 14, 6, UMS_ORANGE);

    tft.fillRect(0, 170, 320, 35, TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextColor(UMS_ORANGE, TFT_WHITE);

    if (i < 70) centerText("Initializing.", 180);
    else if (i < 140) centerText("Initializing..", 180);
    else centerText("Initializing...", 180);

    delay(30);
  }

  delay(300);
}
