#include <Arduino.h>
#include <Arduino_MKRIoTCarrier.h>
#include <FlashStorage.h>
#include <math.h>

struct PersistedEnvironment {
  uint32_t signature;
  float temperatureC;
  float humidity;
};

constexpr uint32_t kPersistSignature = 0xA1B2C3D4;
constexpr float kTemperatureDeltaToPersist = 0.5f;
constexpr float kHumidityDeltaToPersist = 2.5f;
constexpr unsigned long kTelemetryIntervalMs = 2000;
constexpr unsigned long kPersistPauseMs = 30000;
constexpr float kSmoothingFactor = 0.2f;

FlashStorage(persistStore, PersistedEnvironment);

MKRIoTCarrier carrier;
PersistedEnvironment persisted = {};
unsigned long lastTelemetry = 0;
float smoothedTemperature = NAN;
float smoothedHumidity = NAN;
unsigned long skipAutoPersistUntil = 0;

PersistedEnvironment loadPersistedEnvironment() {
  PersistedEnvironment data = persistStore.read();
  if (data.signature != kPersistSignature || !isfinite(data.temperatureC) || !isfinite(data.humidity)) {
    data.signature = 0;
    data.temperatureC = NAN;
    data.humidity = NAN;
  }
  return data;
}

void persistEnvironment(float temperatureC, float humidity) {
  persisted.signature = kPersistSignature;
  persisted.temperatureC = temperatureC;
  persisted.humidity = humidity;
  persistStore.write(persisted);
}

void printPersistedEnvironment() {
  if (persisted.signature != kPersistSignature) {
    Serial.println("No persisted environment data found.");
    return;
  }
  Serial.print("Last stored baseline -> Temperature: ");
  Serial.print(persisted.temperatureC, 2);
  Serial.print(" °C, Humidity: ");
  Serial.print(persisted.humidity, 1);
  Serial.println(" %");
}

void initializeCarrier() {
  CARRIER_CASE = false;
  if (!carrier.begin()) {
    Serial.println("Failed to initialize MKR IoT Carrier!");
    while (true) {
      delay(1000);
    }
  }
  carrier.Env.begin();
  carrier.Buttons.begin();
  carrier.leds.setBrightness(80);
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 2000) {
    delay(10);
  }

  initializeCarrier();

  persisted = loadPersistedEnvironment();
  printPersistedEnvironment();
}

bool shouldPersist(float temperatureC, float humidity) {
  if (persisted.signature != kPersistSignature) {
    return true;
  }

  if (!isfinite(persisted.temperatureC) || !isfinite(persisted.humidity)) {
    return true;
  }

  return fabs(temperatureC - persisted.temperatureC) >= kTemperatureDeltaToPersist ||
         fabs(humidity - persisted.humidity) >= kHumidityDeltaToPersist;
}

void logEnvironment(float temperatureC, float humidity) {
  Serial.print("Current -> Temperature: ");
  Serial.print(temperatureC, 2);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");
}

void updateLed(float temperatureC) {
  const float minTemp = 10.0f;
  const float maxTemp = 35.0f;
  float ratio = (temperatureC - minTemp) / (maxTemp - minTemp);
  ratio = constrain(ratio, 0.0f, 1.0f);

  uint8_t red = static_cast<uint8_t>(ratio * 255);
  uint8_t blue = static_cast<uint8_t>((1.0f - ratio) * 255);

  for (uint8_t i = 0; i < 5; ++i) {
    carrier.leds.setPixelColor(i, carrier.leds.Color(red, 0, blue));
  }
  carrier.leds.show();
}

void updateSmoothing(float temperatureC, float humidity) {
  if (!isfinite(smoothedTemperature)) {
    smoothedTemperature = temperatureC;
  } else {
    smoothedTemperature = (kSmoothingFactor * temperatureC) +
                          ((1.0f - kSmoothingFactor) * smoothedTemperature);
  }

  if (!isfinite(smoothedHumidity)) {
    smoothedHumidity = humidity;
  } else {
    smoothedHumidity = (kSmoothingFactor * humidity) +
                       ((1.0f - kSmoothingFactor) * smoothedHumidity);
  }
}

void serviceTouchControls(float temperatureC, float humidity) {
  carrier.Buttons.update();
  if (carrier.Buttons.onTouchDown(TOUCH0)) {
    Serial.println("Touch pad 0 pressed -> storing new baseline.");
    persistEnvironment(temperatureC, humidity);
    Serial.println("Persisted new baseline environment readings to flash.");
    skipAutoPersistUntil = 0;
  }

  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    Serial.println("Touch pad 4 pressed -> clearing baseline from flash.");
    persisted.signature = 0;
    persisted.temperatureC = NAN;
    persisted.humidity = NAN;
    persistStore.write(persisted);
    skipAutoPersistUntil = millis() + kPersistPauseMs;
    Serial.println("Auto persistence paused for 30 seconds to allow conditions to stabilise.");
  }
}

void loop() {
  unsigned long now = millis();
  if (now - lastTelemetry < kTelemetryIntervalMs) {
    return;
  }
  lastTelemetry = now;

  float temperatureC = carrier.Env.readTemperature();
  float humidity = carrier.Env.readHumidity();

  if (!isfinite(temperatureC) || !isfinite(humidity)) {
    Serial.println("Failed to read environment sensor.");
    return;
  }

  updateSmoothing(temperatureC, humidity);
  logEnvironment(smoothedTemperature, smoothedHumidity);
  updateLed(smoothedTemperature);

  serviceTouchControls(smoothedTemperature, smoothedHumidity);

  if (skipAutoPersistUntil != 0 && millis() < skipAutoPersistUntil) {
    return;
  }
  skipAutoPersistUntil = 0;

  if (shouldPersist(smoothedTemperature, smoothedHumidity)) {
    persistEnvironment(smoothedTemperature, smoothedHumidity);
    Serial.println("Persisted new baseline environment readings to flash.");
  }
}
