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
  int iaq_accuracy;
  float raw_temp;
  float raw_pressure;
  float raw_humidity;
  float raw_gas;
  float stabilization_status;
  float run_in_status;
  float temperature;
  float humidity;
  float static_iaq;
  float co2_equiv;
  float breath_voc_equiv;
  float gas_percentage;
  float gas;
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
  canvas.setCursor(5, 30); // Pos. is BASE LINE when using fonts!
  display.print(TITLE);
  display.print(" ");
  display.setTextSize(2);
  display.print(VERSION);
  display.setTextSize(3);
  display.println();
  display.setTextSize(2);
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
  canvas.setTextSize(2);
  canvas.setCursor(5, 20); // Pos. is BASE LINE when using fonts!
  canvas.println("Timestamp: " + String(sensor_data.timestamp));
  canvas.println("IAQ: " + String(sensor_data.iaq, 0) + " (" + String(sensor_data.iaq_accuracy) + ")");
  canvas.println("Temperature: " + String(sensor_data.temperature, 1) + " C");
  canvas.println("Humidity: " + String(sensor_data.humidity, 1) + " %");
  canvas.println("IAQ: " + String(sensor_data.static_iaq, 0));
  canvas.println("CO2: " + String(sensor_data.co2_equiv));
  canvas.println("VOC: " + String(sensor_data.breath_voc_equiv));
  canvas.println("Gas Percentage: " + String(sensor_data.gas_percentage) + " %");
  canvas.println("Gas: " + String(sensor_data.gas));
  canvas.println("Stabilized: " + String(sensor_data.stabilization_status, 0));
  canvas.println("Run-in Status: " + String(sensor_data.run_in_status, 0));
  canvas.println();

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
  Serial.println("Timestamp: " + String(sensor_data.timestamp));
  Serial.println("IAQ: " + String(sensor_data.iaq) + " (" + String(sensor_data.iaq_accuracy) + ")");
  Serial.println("Raw Temperature: " + String(sensor_data.raw_temp) + " C");
  Serial.println("Raw Pressure: " + String(sensor_data.raw_pressure) + " Pa");
  Serial.println("Raw Humidity: " + String(sensor_data.raw_humidity) + " %");
  Serial.println("Raw Gas: " + String(sensor_data.raw_gas));
  Serial.println("Stabilization Status: " + String(sensor_data.stabilization_status));
  Serial.println("Run-in Status: " + String(sensor_data.run_in_status));
  Serial.println("Temperature: " + String(sensor_data.temperature) + " C");
  Serial.println("Humidity: " + String(sensor_data.humidity) + " %");
  Serial.println("Static IAQ: " + String(sensor_data.static_iaq));
  Serial.println("CO2 Equivalent: " + String(sensor_data.co2_equiv));
  Serial.println("Breath VOC Equivalent: " + String(sensor_data.breath_voc_equiv));
  Serial.println("Gas Percentage: " + String(sensor_data.gas_percentage) + " %");
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
      sensor_data.iaq_accuracy = (int)output.accuracy;
      break;
    case BSEC_OUTPUT_RAW_TEMPERATURE:
      sensor_data.raw_temp = output.signal;
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      sensor_data.raw_pressure = output.signal;
      break;
    case BSEC_OUTPUT_RAW_HUMIDITY:
      sensor_data.raw_humidity = output.signal;
      break;
    case BSEC_OUTPUT_RAW_GAS:
      sensor_data.raw_gas = output.signal;
      break;
    case BSEC_OUTPUT_STABILIZATION_STATUS:
      sensor_data.stabilization_status = output.signal;
      break;
    case BSEC_OUTPUT_RUN_IN_STATUS:
      sensor_data.run_in_status = output.signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
      sensor_data.temperature = output.signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
      sensor_data.humidity = output.signal;
      break;
    case BSEC_OUTPUT_STATIC_IAQ:
      sensor_data.static_iaq = output.signal;
      break;
    case BSEC_OUTPUT_CO2_EQUIVALENT:
      sensor_data.co2_equiv = output.signal;
      break;
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
      sensor_data.breath_voc_equiv = output.signal;
      break;
    case BSEC_OUTPUT_GAS_PERCENTAGE:
      sensor_data.gas_percentage = output.signal;
      break;
    case BSEC_OUTPUT_COMPENSATED_GAS:
      sensor_data.gas = output.signal;
      break;
    default:
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
