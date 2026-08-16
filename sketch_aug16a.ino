#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <math.h>
#include <DNSServer.h>
#include <Preferences.h>
#define SDA_PIN 8
#define SCL_PIN 9
#define BATTERY_PIN 0
const char* ssid = "TREMOR_MONITOR";
const char* password = "12345678";
WebServer server(80);
Preferences pref;
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);
#define SAMPLES 256
#define SAMPLE_RATE 100.0
#define SAMPLE_INTERVAL_US 10000
#include <arduinoFFT.h>
ArduinoFFT<float> FFT;
float vReal[SAMPLES];
float vImag[SAMPLES];
float filteredHz = 0;
float gainHz = 0.223;
float offsetHz = 0.00;
float hzHistory[5] = {0};
byte hzPtr = 0;
DNSServer dnsServer;
const byte DNS_PORT = 53;
int sampleIndex = 0;
bool bufferFull = false;
unsigned long lastSampleTime = 0;
float xVal = 0;
float yVal = 0;
float zVal = 0;
float magnitude = 0;
float rawHz = 0;
float peakHz = 0;
float displayHz = 0;
float peakPerMinute = 0;
float peakAmp = 0;
float vibrationRMS = 0;
bool dataSaved = false;
String statusTremor = "Normal";
float batteryVoltage = 0;
int batteryPercent = 0;
bool measuring = false;
unsigned long startMeasure = 0;
const unsigned long measureDuration = 10000;
int totalData = 0;
float periodUsed = SAMPLES / SAMPLE_RATE;
float resolution = SAMPLE_RATE / SAMPLES;
float nyquist = SAMPLE_RATE / 2.0;
void readBattery() {
int adc = analogRead(BATTERY_PIN);
float adcVoltage = adc * (3.3 / 4095.0);
batteryVoltage = adcVoltage * 2.0;
batteryPercent = map((int)(batteryVoltage * 100), 300, 420, 0, 100);
if (batteryPercent > 100) batteryPercent = 100;
if (batteryPercent < 0) batteryPercent = 0;
}
int16_t rawX, rawY, rawZ;
void readADXLRaw()
{
Wire.beginTransmission(0x53);
Wire.write(0x32);      
Wire.endTransmission(false);
Wire.requestFrom(0x53, 6);
if (Wire.available() != 6)
{
return;
}
rawX = Wire.read() | (Wire.read() << 8);
rawY = Wire.read() | (Wire.read() << 8);
rawZ = Wire.read() | (Wire.read() << 8);
float mag = sqrt(
(float)rawX * rawX +
(float)rawY * rawY +
(float)rawZ * rawZ);
static float dc = 0;
dc = dc * 0.995 + mag * 0.005;
magnitude = mag - dc;
}
void collectSample()
{
if (micros() - lastSampleTime >= SAMPLE_INTERVAL_US)
{
lastSampleTime += SAMPLE_INTERVAL_US;
readADXLRaw();
vReal[sampleIndex] = magnitude;
vImag[sampleIndex] = 0;
sampleIndex++;
if(sampleIndex >= SAMPLES)
{
bufferFull = true;
for(int i=0;i<SAMPLES/2;i++
{
vReal[i] = vReal[i + SAMPLES/2];
vImag[i] = 0;
            }
            sampleIndex = SAMPLES/2;
        }
    }
}
void computeFourier()
{
    float mean = 0;

    for(int i=0;i<SAMPLES;i++)
        mean += vReal[i];

    mean /= SAMPLES;

    float rms = 0;

    for(int i=0;i<SAMPLES;i++)
    {
        vReal[i] -= mean;
        rms += vReal[i] * vReal[i];
    }
    vibrationRMS = sqrt(rms / SAMPLES);
FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HANN, FFT_FORWARD);
    FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, SAMPLES);
if (vibrationRMS < 0.03)
{
    peakHz = 0;
    peakPerMinute = 0;
    statusTremor = "Normal";
    return;
}
    peakAmp = 0;
    peakHz = 0;
int peakIndex = 0;
for(int i = 1; i < SAMPLES/2 - 1; i++)
{
    float freq = (i * SAMPLE_RATE) / SAMPLES;
    if(freq >= 0.5 && freq <= 50.0)
    {
if (vReal[i] > vReal[i-1] &&
    vReal[i] > vReal[i+1] &&
    vReal[i] > peakAmp)
{
    peakAmp = vReal[i];
    peakIndex = i;
}
    }
}
if (peakIndex == 0)
{
    peakHz = 0;
    peakPerMinute = 0;
    filteredHz = 0;
    statusTremor = "Normal";
    return;
}
float alpha = vReal[peakIndex - 1];
float beta  = vReal[peakIndex];
float gamma = vReal[peakIndex + 1];
float p = 0;
float den = (alpha - 2 * beta + gamma);
if (fabs(den) > 0.00001)
{
    p = 0.5 * (alpha - gamma) / den;
}
rawHz = (peakIndex + p) * SAMPLE_RATE / SAMPLES;
Serial.print("PeakIndex = ");
Serial.print(peakIndex);
Serial.print(" | PeakAmp = ");
Serial.print(peakAmp,5);
Serial.print(" | RawHz = ");
Serial.println(rawHz,3);
Serial.print("RAW FFT = ");
Serial.print(rawHz, 3);
Serial.println(" Hz");
peakHz = rawHz - 2.0;
if (peakHz < 0)
    peakHz = 0;
if (peakAmp < 0.005)
{
    displayHz = 0;
    peakPerMinute = 0;
    statusTremor = "Normal";
    return;
}
displayHz = peakHz;
filteredHz = filteredHz * 0.5 + peakHz * 0.5;
displayHz = filteredHz;
peakPerMinute = displayHz * 60;
if (displayHz > 12.0)
{
    statusTremor = "Tremor";
}
else
{
    statusTremor = "Normal";
}
}
String htmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FFT Tremor Monitor</title>
  <style>
body{
    margin:0;
    font-family:Arial,sans-serif;
    background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);
    color:white;
    text-align:center;
    overflow-x:hidden;
}
.box{
    width:90%;
    max-width:430px;
    margin:25px auto;
    padding:25px;
    background:rgba(255,255,255,0.08);
    backdrop-filter:blur(15px);
    border-radius:20px;
    box-shadow:0 0 30px rgba(0,0,0,.4);
}
h1{
font-size:30px;
margin-bottom:20px;
letter-spacing:2px;
}
.status{
padding:20px;
font-size:28px;
font-weight:bold;
border-radius:18px;
background:#2e7d32;
box-shadow:0 0 20px rgba(0,255,100,.5);
animation:pulse 2s infinite;
}
@keyframes pulse{
0%{transform:scale(1);}
50%{transform:scale(1.03);}
100%{transform:scale(1);}
}
.btn{
width:220px;
padding:16px;
margin:12px;
font-size:20px;
font-weight:bold;
border:none;
border-radius:50px;
cursor:pointer;
transition:.3s;
}
.start{
background:#00c853;
color:white;
}
.start:hover{
transform:scale(1.05);
background:#00e676;
}
.view{
background:#1976d2;
color:white;
}
.view:hover{
transform:scale(1.05);
background:#42a5f5;
}
.card{
margin-top:15px;
padding:15px;
background:rgba(255,255,255,.08);
border-radius:15px;
font-size:20px;
}
.progress{
width:100%;
height:18px;
background:#333;
border-radius:20px;
overflow:hidden;
margin-top:8px;
}
.bar{
height:100%;
width:50%;
background:linear-gradient(90deg,#00e676,#00c853);
transition:.5s;
}
    .battery {
      margin-top: 10px;
      font-size: 15px;
      color: #ddd;
    }
  </style>
</head>
<body>
  <div class="box">
<h1>TREMOR MONITOR</h1>
<p style="opacity:.8">
Analisis Tremor Menggunakan FFT
</p>
    <div class="status" id="statusBox">Normal</div>
<div class="card">
Battery
<div class="progress">
<div class="bar" id="bar"></div>
</div>
<div id="battery">0%</div>
</div>
<div class="card">
Frekuensi Dominan
<h2 id="hz">0.00 Hz</h2>
</div>
<div class="card">
Tremor / Menit
<h2 id="permin">0</h2>
</div>
<div class="card">
RMS Getaran
<h2 id="rms">0.000</h2>
</div>
  </div>
<button onclick="startMeasure()" class="btn start">
START
</button>
<div id="measureStatus"
style="margin-top:15px;
font-size:18px;
font-weight:bold;
color:#00e676;">
Siap melakukan pengukuran
</div>
<button onclick="viewData()" class="btn view">
VIEW DATA
</button>
<script>
let fetching = false;
function updateData(){
    if(fetching) return;
    fetching = true;
    fetch("/data")
    .then(r=>r.json())
    .then(data=>{
        document.getElementById("battery").innerHTML =
            data.battery + "%";
        document.getElementById("bar").style.width =
            data.battery + "%";

        document.getElementById("hz").innerHTML =
            data.hz + " Hz";
        document.getElementById("permin").innerHTML =
            data.permin + " /min";
        document.getElementById("rms").innerHTML =
            data.rms;
        let box = document.getElementById("statusBox");
        if(data.status=="Normal")
            box.innerHTML="Normal";
        else if(data.status=="Tremor Ringan")
            box.innerHTML="Tremor Ringan";
        else if(data.status=="Tremor Berat")
            box.innerHTML="Tremor Berat";
        else
            box.innerHTML= data.status;
        if(data.status=="Normal"){
       box.style.background="#2e7d32";
            box.style.boxShadow="0 0 25px #00ff66";
        }
        else if(data.status=="Tremor Ringan"){
            box.style.background="#f57c00";
            box.style.boxShadow="0 0 25px orange";
        }
        else if(data.status=="Tremor Berat"){
            box.style.background="#c62828";
            box.style.boxShadow="0 0 25px red";
        }
        else{
            box.style.background="#546e7a";
            box.style.boxShadow="0 0 25px #607d8b";
        }
if(data.saved){
    document.getElementById("measureStatus").innerHTML =
    "Pengukuran selesai, data berhasil disimpan.";
    alert("Pengukuran selesai!\nData berhasil disimpan.");
    fetch("/resetSaved");
}
        fetching = false;
    })
    .catch(()=>{
        fetching = false;
    });
}
updateData();
setInterval(updateData,400);
function startMeasure(){
    document.getElementById("measureStatus").innerHTML =
    "Pengukuran sedang berlangsung...";
    fetch("/start")
    .then(r=>r.text())
    .then(()=>{
        alert("Pengukuran dimulai selama 1 menit");
    });
}
function viewData(){
    location.href="/historyPage";
}
</script>
</body>
</html>
)rawliteral";
return html;
}
String historyPage(){
String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>History</title>
<style>
body{
    margin:0;
    background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);
    font-family:Arial;
    color:white;
    text-align:center;
}
table{
    width:95%;
    margin:25px auto;
    background:rgba(255,255,255,.08);
    backdrop-filter:blur(15px);
    border-radius:15px;
    overflow:hidden;
    border-collapse:collapse;
}
td,th{
border:1px solid white;
padding:8px;
}
button{
margin:15px;
padding:14px 25px;
background:#1976d2;
color:white;
border:none;
border-radius:30px;
font-size:18px;
cursor:pointer;
transition:.3s;
}
button:hover{
transform:scale(1.05);
background:#42a5f5;
}
</style>
</head>
<body>
<h2>Riwayat Pengukuran</h2>
<table id="tbl"></table>
<button onclick="hapusTerpilih()">
Hapus Data Terpilih
</button>
<br>
<button onclick="location.href='/'">
Kembali
</button>
<script>
fetch("/history")
.then(r=>r.json())
.then(data=>{
let t=document.getElementById("tbl");
t.innerHTML=`
<tr>
<th>Pilih</th>
<th>No</th>
<th>Data</th>
</tr>`;
for(let i=0;i<data.length;i++){
t.innerHTML+=`
<tr>
<td><input type="checkbox" class="cek" value="${i}"></td>
<td>${i+1}</td>
<td>${data[i]}</td>
</tr>`;
}	
});
async function hapusTerpilih(){
let list=[];
document.querySelectorAll(".cek").forEach(c=>{
if(c.checked)
list.push(c.value);
});
if(list.length==0){
alert("Pilih data terlebih dahulu");
return;
}
fetch("/delete",{
method:"POST",
headers:{
"Content-Type":"application/json"
},
body:JSON.stringify(list)
})
.then(r=>r.text())
.then(()=>{
alert("Data berhasil dihapus");
location.reload();
});
}
</script>
</body>
</html>
)rawliteral";
return html;
}
void resetSaved()
{
    dataSaved = false;
    server.send(200,"text/plain","OK");
}
void saveResult()
{
    String data =
    "Frekuensi : " + String(displayHz,2) + " Hz | " +
    "Tremor : " + String(peakPerMinute,1) + " /min | " +
    "RMS : " + String(vibrationRMS,3) + " | " +
    statusTremor;
    int index = 0;
    while(pref.isKey(("data"+String(index)).c_str()))
    {
        index++;
    }
    pref.putString(("data"+String(index)).c_str(), data);
    totalData = index + 1;
    pref.putInt("jumlah", totalData);
    dataSaved = true;
}
void historyData(){
String json="[";
for(int i=0;i<totalData;i++){
json+="\"";
json+=pref.getString(
("data"+String(i)).c_str(),
""
);
json+="\"";
if(i<totalData-1)
json+=",";
}
json+="]";
server.send(200,"application/json",json);
}
void clearData() {
    for (int i = 0; i < totalData; i++) {
        pref.remove(("data" + String(i)).c_str());
    }
    totalData = 0;
    pref.putInt("jumlah", 0);
    pref.end();                       
    pref.begin("tremor", false);      
    server.send(200, "text/plain", "OK");
    Serial.println("Semua data berhasil dihapus");
}
void deleteSelected()
{
    String body = server.arg("plain");
    bool hapus[200] = {false};
    int idx = 0;
    while (true)
    {
        int p = body.indexOf(',', idx);
        String s;
        if (p == -1)
            s = body.substring(idx);
        else
            s = body.substring(idx, p);
        s.replace("[", "");
        s.replace("]", "");
        s.replace("\"", "");
        if (s.length())
            hapus[s.toInt()] = true;
        if (p == -1)
            break;
        idx = p + 1;
    }
    String temp[200];
    int baru = 0;
    for (int i = 0; i < totalData; i++)
    {
        if (!hapus[i])
        {
            temp[baru] = pref.getString(("data" + String(i)).c_str(), "");
            baru++;
        }
    }
    for (int i = 0; i < totalData; i++)
    {
        pref.remove(("data" + String(i)).c_str());
    }
    for (int i = 0; i < baru; i++)
    {
        pref.putString(("data" + String(i)).c_str(), temp[i]);
    }
    totalData = baru;
    pref.putInt("jumlah", totalData);
    server.send(200, "text/plain", "OK");
}
void startMeasureWeb() {
  if(measuring){
    server.send(200,"text/plain","BUSY");
    return;
  }
  measuring = true;
  startMeasure = millis();
  server.send(200,"text/plain","OK");
}
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}
void handleHistoryPage(){
server.send(
200,
"text/html",
historyPage()
);
}
void handleData() {
  server.sendHeader("Cache-Control","no-cache");
  String json="{";
  json += "\"battery\":" + String(batteryPercent) + ",";
  json += "\"status\":\"" + statusTremor + "\",";
  json += "\"hz\":" + String(displayHz,2) + ",";
  json += "\"permin\":" + String(peakPerMinute,1) + ",";
  json += "\"rms\":" + String(vibrationRMS,3);
  json += ",\"saved\":";
json += (dataSaved ? "true" : "false");
  json += "}";
  server.send(200,"application/json",json);
}
void setup() {
  Serial.begin(115200);
  lastSampleTime = micros();
  pref.begin("tremor",false);
totalData =
pref.getInt("jumlah",0);
  delay(1000);
  analogReadResolution(12);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println();
  Serial.println("Cek ADXL345...");
Wire.beginTransmission(0x53);
if (Wire.endTransmission() != 0)
{
    Serial.println("ADXL345 tidak ditemukan");
    while(1);
}
Wire.beginTransmission(0x53);
if (Wire.endTransmission() != 0)
{
    Serial.println("ADXL345 tidak ditemukan");
    while (1)
    {
        delay(1000);
    }
}
Serial.println("ADXL345 terdeteksi");
Wire.beginTransmission(0x53);
Wire.write(0x2D);
Wire.write(0x08);
Wire.endTransmission();
Wire.beginTransmission(0x53);
Wire.write(0x31);
Wire.write(0x08);
Wire.endTransmission();
  Serial.println("ADXL345 terdeteksi");
Wire.beginTransmission(0x53);
Wire.write(0x2C);
Wire.write(0x0A);      
Wire.endTransmission();
for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = 0;
    vImag[i] = 0;
}
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println();
  Serial.println("WiFi ESP32 aktif");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
server.on("/resetSaved", resetSaved);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/start", startMeasureWeb);
server.on("/history", historyData);
server.on("/historyPage",handleHistoryPage);
server.on("/clear", clearData);
server.on("/delete", HTTP_POST, deleteSelected);
  server.onNotFound(handleRoot);
  server.begin();
  Serial.println("Web server dimulai");
}
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  collectSample();
  if (bufferFull) {
    readBattery();
    computeFourier();
    bufferFull = false;
Serial.print("RAW FFT : ");
Serial.print(rawHz, 3);
Serial.print(" Hz | Peak : ");
Serial.print(peakHz, 3);
Serial.print(" Hz | Display : ");
Serial.print(displayHz, 1);
Serial.print(" Hz | Display : ");
Serial.print(displayHz, 3);
Serial.print(" Hz | RMS : ");
Serial.print(vibrationRMS, 3);
Serial.print(" | PeakAmp : ");
Serial.print(peakAmp, 3);
Serial.print(" | Battery : ");
Serial.print(batteryPercent);
Serial.println("%");
    Serial.print(" Hz | ");
    Serial.print(peakPerMinute, 1);
    Serial.print(" /min | RMS: ");
    Serial.print(vibrationRMS, 3);
    Serial.print(" | Status: ");
    Serial.print(statusTremor);
    Serial.print(" | Battery: ");
    Serial.print(batteryPercent);
    Serial.println("%");
    }
if(measuring){
    if(millis()-startMeasure>=measureDuration){
        measuring=false;
        saveResult();
    }
}
}
