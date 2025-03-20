#define TITLE "BME680 Display"
#define VERSION "1.0"

#include <Arduino.h>
#include <bsec2.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_ST7789.h>

#define TFT_CS A3
#define TFT_DC D24
#define TFT_RST D25
#define TFT_SDA MOSI
#define TFT_SCL SCK
#define TFT_WD 240
#define TFT_HT 320
#define TFT_ROT 3

#define REPORT_PERIOD 1000

#define IAQ_MAX = 500
#define IAQ_DANGER = 200
#define IAQ_WARNING = 100
#define IAD_SAFE = 50
float iaqLevels[] = { 50, 100, 150, 200, 250, 350, 65535 };
uint8_t numIaqLevels = ARRAY_LEN(iaqLevels);
String iaqText[] = {
  "Excellent",
  "Good",
  "Lightly Polluted",
  "Moderately Polluted",
  "Heavily Polluted",
  "Severely Polluted",
  "Extremely Polluted",
};

void bsecCheckStatus();
void bsecDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec);
void reportToSerial();
void updateCanvas();
void drawDisplay();

Bsec2 envSensor;
bsecSensor sensorList[] = {
  BSEC_OUTPUT_IAQ,
  BSEC_OUTPUT_RAW_TEMPERATURE,
  BSEC_OUTPUT_RAW_PRESSURE,
  BSEC_OUTPUT_RAW_HUMIDITY,
  BSEC_OUTPUT_RAW_GAS,
  BSEC_OUTPUT_STABILIZATION_STATUS,
  BSEC_OUTPUT_RUN_IN_STATUS,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  BSEC_OUTPUT_STATIC_IAQ,
  BSEC_OUTPUT_CO2_EQUIVALENT,
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
  BSEC_OUTPUT_GAS_PERCENTAGE,
  BSEC_OUTPUT_COMPENSATED_GAS
};

struct state {
  int timestamp;
  float iaq;
  int iaqAccuracy;
  float temperatureRaw;
  float pressureRaw;
  float humidtyRaw;
  float gasRaw;
  float stabilizationstatus;
  float runInStatus;
  float temperature;
  float humidity;
  float staticIaq;
  float equivCo2;
  float equivVoc;
  float gasPercentage;
  float gas;
  String airQuality;
};
state sensor_data;

Adafruit_NeoPixel led(1, D16, NEO_GRB);

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_SDA, TFT_SCL, TFT_RST);
GFXcanvas1 canvas(TFT_HT, TFT_WD);

void setup(void)
{
  Serial.begin(115200);
  Wire.begin();
  // while(!Serial) delay(10);
  
  // BME680 Environment Sensor
  !envSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
  envSensor.setTemperatureOffset(TEMP_OFFSET_ULP);
  envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_ULP);
  envSensor.attachCallback(bsecDataCallback);

  // Neopixel LED
  led.begin();
  led.setBrightness(255);
  led.setPixelColor(0, 0, 0, 0);
  led.show();

  // TFT Display
  display.init(TFT_WD, TFT_HT);
  display.setRotation(TFT_ROT);
  display.fillScreen(ST77XX_BLACK);
  canvas.setTextWrap(false);

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(3);
  display.println(TITLE);
  display.setTextSize(2);
  display.printf("Version %s\n\r", VERSION);
  display.printf(
    "BSEC %d.%d.%d.%d\n\r", 
    envSensor.version.major,
    envSensor.version.minor,
    envSensor.version.major_bugfix,
    envSensor.version.minor_bugfix
  );
  delay(2000);
  display.fillScreen(0);
  
  updateCanvas();
  drawDisplay();
}

void updateCanvas() {
  canvas.fillScreen(0);
  canvas.setTextColor(ST77XX_WHITE);
  canvas.setCursor(0, 20);
  if (!sensor_data.stabilizationstatus) {
    canvas.setTextSize(3);
    canvas.println("Stabilizing...");
    canvas.setTextSize(2);
    canvas.println("This may take a while...");
    canvas.println();
  }
  else if (!sensor_data.runInStatus) {
    canvas.setTextSize(2);
    canvas.println("Run-In in Progress...");
    canvas.println("This may take a while...");
    canvas.println();
  }
  else {
    canvas.setTextSize(2);
    canvas.println("Air Quality: " + String(sensor_data.airQuality));
    canvas.println();
    canvas.println("Timestamp: " + String(sensor_data.timestamp));
    canvas.println("IAQ: " + String(sensor_data.iaq, 0) + " (" + String(sensor_data.iaqAccuracy) + ")");
    canvas.println("Temperature: " + String(sensor_data.temperature, 1) + " C");
    canvas.println("Humidity: " + String(sensor_data.humidity, 1) + " %");
    canvas.println("Pressure: " + String(sensor_data.pressureRaw, 1) + " Pa");
    canvas.println("IAQ: " + String(sensor_data.staticIaq, 0));
    canvas.println("CO2: " + String(sensor_data.equivCo2, 1) + "ppm");
    canvas.println("VOC: " + String(sensor_data.equivVoc, 1) + "ppm");
    canvas.println("Gas: " + String(sensor_data.gas) + "Ohm | " + String(sensor_data.gasPercentage, 1) + " %");
    canvas.println();
  }

  if (envSensor.status < BSEC_OK)
    canvas.println("BSEC Error: " + String(envSensor.status));
  else if (envSensor.status > BSEC_OK)
    canvas.println("BSEC Warn: " + String(envSensor.status));
  if (envSensor.sensor.status < BME68X_OK)
    canvas.println("BME68X Error: " + String(envSensor.sensor.status));
  else if (envSensor.sensor.status > BME68X_OK)
    canvas.println("BME68X Warn: " + String(envSensor.sensor.status));
}

void drawDisplay() {
  display.drawBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height(), ST77XX_WHITE, ST77XX_BLACK);
}

void reportToSerial() {
  Serial.println("Air Quality: " + String(sensor_data.airQuality));
  Serial.println("Timestamp: " + String(sensor_data.timestamp));
  Serial.println("IAQ: " + String(sensor_data.iaq) + " (" + String(sensor_data.iaqAccuracy) + ")");
  Serial.println("Raw Temperature: " + String(sensor_data.temperatureRaw) + " C");
  Serial.println("Raw Pressure: " + String(sensor_data.pressureRaw) + " Pa");
  Serial.println("Raw Humidity: " + String(sensor_data.humidtyRaw) + " %");
  Serial.println("Raw Gas: " + String(sensor_data.gasRaw));
  Serial.println("Stabilization Status: " + String(sensor_data.stabilizationstatus));
  Serial.println("Run-in Status: " + String(sensor_data.runInStatus));
  Serial.println("Temperature: " + String(sensor_data.temperature) + " C");
  Serial.println("Humidity: " + String(sensor_data.humidity) + " %");
  Serial.println("Static IAQ: " + String(sensor_data.staticIaq));
  Serial.println("CO2 Equivalent: " + String(sensor_data.equivCo2));
  Serial.println("Breath VOC Equivalent: " + String(sensor_data.equivVoc));
  Serial.println("Gas Percentage: " + String(sensor_data.gasPercentage) + " %");
  Serial.println("Gas: " + String(sensor_data.gas));
  Serial.println();
}

unsigned long lastUpdate = 0;
void loop(void)
{
  if (!envSensor.run())
    bsecCheckStatus();

  if ((millis() - lastUpdate) >= REPORT_PERIOD) {
    if(Serial) reportToSerial();

    updateCanvas();
    drawDisplay();
    
    lastUpdate = millis();
  }
}

void bsecDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
  if (!outputs.nOutputs) return;

  sensor_data.timestamp = (int)(outputs.output[0].time_stamp / 1000000LL);
  for (uint8_t i = 0; i < outputs.nOutputs; i++)
  {
    Serial.write(outputs.output[i].sensor_id);
    const bsecData output = outputs.output[i];
    switch (output.sensor_id)
    {
    case BSEC_OUTPUT_IAQ:
      sensor_data.iaq = output.signal;
      sensor_data.iaqAccuracy = (int)output.accuracy;
      break;
    case BSEC_OUTPUT_RAW_TEMPERATURE:
      sensor_data.temperatureRaw = output.signal;
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      sensor_data.pressureRaw = output.signal;
      break;
    case BSEC_OUTPUT_RAW_HUMIDITY:
      sensor_data.humidtyRaw = output.signal;
      break;
    case BSEC_OUTPUT_RAW_GAS:
      sensor_data.gasRaw = output.signal;
      break;
    case BSEC_OUTPUT_STABILIZATION_STATUS:
      sensor_data.stabilizationstatus = output.signal;
      break;
    case BSEC_OUTPUT_RUN_IN_STATUS:
      sensor_data.runInStatus = output.signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
      sensor_data.temperature = output.signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
      sensor_data.humidity = output.signal;
      break;
    case BSEC_OUTPUT_STATIC_IAQ:
      sensor_data.staticIaq = output.signal;
      break;
    case BSEC_OUTPUT_CO2_EQUIVALENT:
      sensor_data.equivCo2 = output.signal;
      break;
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
      sensor_data.equivVoc = output.signal;
      break;
    case BSEC_OUTPUT_GAS_PERCENTAGE:
      sensor_data.gasPercentage = output.signal;
      break;
    case BSEC_OUTPUT_COMPENSATED_GAS:
      sensor_data.gas = output.signal;
      break;
    default:
      break;
    }
  }

  sensor_data.airQuality = iaqText[numIaqLevels + 1];
  for(uint8_t i = 0; i < numIaqLevels; i++){
    if(sensor_data.iaq <= iaqLevels[i]) {
      sensor_data.airQuality = iaqText[i];
      break;
    }
  }
}

void bsecCheckStatus()
{
  if (envSensor.status != BSEC_OK)
    Serial.printf("BSEC %s: %d\n\r", envSensor.status < BSEC_OK ? "Error": "Warn", envSensor.status);
  if (envSensor.sensor.status != BME68X_OK)
    Serial.printf("BME68X %s: %d\n\r", envSensor.sensor.status < BME68X_OK ? "Error": "Warn", envSensor.sensor.status);
}
