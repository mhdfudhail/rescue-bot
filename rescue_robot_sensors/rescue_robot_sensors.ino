/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-projects-ebook/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include <ESP32Servo.h>
#include "DHT.h"

// Replace with your network credentials
const char* ssid = "MEC-2.4";
const char* password = "qwerty12345";
int i=0;


float temp=0.0;
float humidity=0.0;
int depth=0.0;
int gas=0.0;
int pulse=0;
unsigned long previousTime = 0;

#define PART_BOUNDARY "123456789000000000000987654321"


#define CAMERA_MODEL_AI_THINKER
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM_B
//#define CAMERA_MODEL_WROVER_KIT

#if defined(CAMERA_MODEL_WROVER_KIT)
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM    21
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27

  #define Y9_GPIO_NUM      35
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      19
  #define Y4_GPIO_NUM      18
  #define Y3_GPIO_NUM       5
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22

#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23

  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_M5STACK_WITHOUT_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23

  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       17
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27

  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22

#elif defined(CAMERA_MODEL_M5STACK_PSRAM_B)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     22
  #define SIOC_GPIO_NUM     23

  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#else
  #error "Camera model not selected"
#endif

#define SERVO_1      14
#define SERVO_2      15
#define SERVO_STEP   5

Servo servo1;
Servo servo2;

int servo1Pos = 0;
int servo2Pos = 0;

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Robot Control Dashboard</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f0f0f0;
        }
        .dashboard {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        .panel {
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .video-panel {
            width: 100%;
        }
        .video-container {
            width: 100%;
            aspect-ratio: 16/9;
            background: #000;
            border-radius: 5px;
            overflow: hidden;
        }
        #videoFeed {
            width: 100%;
            height: 100%;
            object-fit: cover;
        }
        .video-controls {
            margin-top: 10px;
            display: flex;
            gap: 10px;
        }
        .servo-controls {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 20px;
        }
        .servo-visual {
            width: 120px;
            height: 120px;
            margin: 10px auto;
            position: relative;
        }
        .servo-arm {
            width: 60px;
            height: 6px;
            background: #2196F3;
            position: absolute;
            top: 57px;
            left: 60px;
            transform-origin: 0 50%;
            transition: transform 0.3s ease;
        }
        .servo-base {
            width: 40px;
            height: 40px;
            background: #424242;
            border-radius: 50%;
            position: absolute;
            top: 40px;
            left: 40px;
        }
        .control-group {
            margin: 10px 0;
        }
        .value-display {
            font-size: 20px;
            text-align: center;
            margin: 5px 0;
        }
        input[type="range"] {
            width: 100%;
            margin: 10px 0;
        }
        input[type="text"] {
            flex-grow: 1;
            padding: 8px;
            border: 1px solid #ccc;
            border-radius: 4px;
        }
        button {
            background: #2196F3;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
        }
        button:hover {
            background: #1976D2;
        }
        .presets {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-top: 10px;
        }
        .sensor-panel {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 20px;
        }
        .sensor-card {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
        }
        .sensor-value {
            font-size: 1.8em;
            font-weight: bold;
            color: #2196F3;
        }
        .sensor-label {
            color: #666;
            margin-top: 5px;
        }
        @media (max-width: 768px) {
            .servo-controls {
                grid-template-columns: 1fr;
            }
            .sensor-panel {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="dashboard">
        <!-- Video Feed -->
        <div class="panel video-panel">
            <h2 style="text-align: center;">Video Feed</h2>
            <div class="video-container">
                <img id="photo" src="" alt="Video Feed">
            </div>
            <div class="video-controls">
                <input type="text" id="streamUrl" placeholder="Enter stream URL (e.g., http://camera-ip:port/stream)">
                <button onclick="connectStream()">Connect</button>
            </div>
        </div>

        <!-- Servo Controls -->
        <div class="panel">
            <div class="servo-controls">
                <!-- Servo 1 -->
                <div>
                    <h3 style="text-align: center;">Servo 1 Control</h3>
                    <div class="servo-visual">
                        <div class="servo-base"></div>
                        <div class="servo-arm" id="servo1Arm"></div>
                    </div>
                    <div class="control-group">
                        <div class="value-display">
                            Position: <span id="angle1Display">90</span>°
                        </div>
                        <input type="range" id="servo1Slider" min="0" max="180" value="90" step="1">
                    </div>
                    <div class="presets">
                        <button onclick="setPosition(1, 0)">0°</button>
                        <button onclick="setPosition(1, 90)">90°</button>
                        <button onclick="setPosition(1, 180)">180°</button>
                    </div>
                </div>

                <!-- Servo 2 -->
                <div>
                    <h3 style="text-align: center;">Servo 2 Control</h3>
                    <div class="servo-visual">
                        <div class="servo-base"></div>
                        <div class="servo-arm" id="servo2Arm"></div>
                    </div>
                    <div class="control-group">
                        <div class="value-display">
                            Position: <span id="angle2Display">90</span>°
                        </div>
                        <input type="range" id="servo2Slider" min="0" max="180" value="90" step="1">
                    </div>
                    <div class="presets">
                        <button onclick="setPosition(2, 0)">0°</button>
                        <button onclick="setPosition(2, 90)">90°</button>
                        <button onclick="setPosition(2, 180)">180°</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- Sensor Readings -->
        <div class="panel">
            <div class="sensor-panel">
                <div class="sensor-card">
                    <div class="sensor-value" id="depthValue">0.0</div>
                    <div class="sensor-label">Depth (cm)</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-value" id="tempValue">25.0</div>
                    <div class="sensor-label">Temperature (°C)</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-value" id="humidityValue">45</div>
                    <div class="sensor-label">Humidity (%)</div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-value" id="gasValue">45</div>
                    <div class="sensor-label">Gas </div>
                </div>
                <div class="sensor-card">
                    <div class="sensor-value" id="pulseValue">45</div>
                    <div class="sensor-label">Pulse </div>
                </div>
                
            </div>
        </div>
    </div>

    <script>
        // Servo control
        const servo1Slider = document.getElementById('servo1Slider');
        const servo2Slider = document.getElementById('servo2Slider');
        const angle1Display = document.getElementById('angle1Display');
        const angle2Display = document.getElementById('angle2Display');
        const servo1Arm = document.getElementById('servo1Arm');
        const servo2Arm = document.getElementById('servo2Arm');

        function updateServoPosition(servoNum, angle) {
            const display = servoNum === 1 ? angle1Display : angle2Display;
            const arm = servoNum === 1 ? servo1Arm : servo2Arm;

            display.textContent = angle;
            arm.style.transform = `rotate(${angle}deg)`;
            // Here you would send the angle to your servo via API
            SendServoPos1(servoNum,angle);
            console.log(`Sending angle to servo ${servoNum}: ${angle}°`);
        }

        function setPosition(servoNum, angle) {
            const slider = servoNum === 1 ? servo1Slider : servo2Slider;
            slider.value = angle;
            updateServoPosition(servoNum, angle);
        }

        // Video stream
        function connectStream() {
            const url = document.getElementById('streamUrl').value.trim();
            if (url) {
                document.getElementById('videoFeed').src = url;
            } else {
                alert('Please enter a valid stream URL');
            }
        }

 function SendServoPos1(servo_num, pos) {
    let num = servo_num.toString();
    let data = pos.toString();
     var xhr = new XMLHttpRequest();
     xhr.open("GET", "/servo?num="+ num +"&"+ "data=" + data, true);
     xhr.send();
 }
   function fetchSensorData() {
    fetch('/sensor1?data=')
        .then(response => response.json())
        .then(data => {
            document.getElementById('depthValue').textContent = data.sensor_value.toFixed(2);
        })
        .catch(err => {
            console.error('Error fetching sensor data:', err);
        });
        fetch('/sensor2')
        .then(response => response.json())
        .then(data => {
            document.getElementById('tempValue').textContent = data.sensor_value.toFixed(2);
        })
        .catch(err => {
            console.error('Error fetching sensor data:', err);
        });   
        fetch('/sensor3')
        .then(response => response.json())
        .then(data => {
            document.getElementById('humidityValue').textContent = data.sensor_value.toFixed(2);
        })
        .catch(err => {
            console.error('Error fetching sensor data:', err);
        });
        fetch('/sensor4')
        .then(response => response.json())
        .then(data => {
            document.getElementById('gasValue').textContent = data.sensor_value.toFixed(2);
        })
        .catch(err => {
            console.error('Error fetching sensor data:', err);
        });
        fetch('/sensor5')
        .then(response => response.json())
        .then(data => {
            document.getElementById('pulseValue').textContent = data.sensor_value.toFixed(2);
        })
        .catch(err => {
            console.error('Error fetching sensor data:', err);
        });
    }

        // Event listeners
        servo1Slider.addEventListener('input', (e) => updateServoPosition(1, e.target.value));
        servo2Slider.addEventListener('input', (e) => updateServoPosition(2, e.target.value));

        // Initialize positions
        updateServoPosition(1, 90);
        updateServoPosition(2, 90);

        // Simulate sensor updates every 2 seconds
        setInterval(fetchSensorData, 2000);

        window.onload = document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
    </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

  static esp_err_t sensor_handler1(httpd_req_t *req) {
    char response[32];
    float sensorValue = depth;
    snprintf(response, sizeof(response), "{\"sensor_value\": %.2f}", sensorValue);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
 static esp_err_t sensor_handler2(httpd_req_t *req) {
    char response[32];
    float sensorValue = temp;
    snprintf(response, sizeof(response), "{\"sensor_value\": %.2f}", sensorValue);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
static esp_err_t sensor_handler3(httpd_req_t *req) {
    char response[32];
    float sensorValue = humidity;
    snprintf(response, sizeof(response), "{\"sensor_value\": %.2f}", sensorValue);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
static esp_err_t sensor_handler4(httpd_req_t *req) {
    char response[32];
    float sensorValue = gas;
    snprintf(response, sizeof(response), "{\"sensor_value\": %.2f}", sensorValue);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
static esp_err_t sensor_handler5(httpd_req_t *req) {
    char response[32];
    float sensorValue = pulse;
    snprintf(response, sizeof(response), "{\"sensor_value\": %.2f}", sensorValue);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
static esp_err_t servo_handler(httpd_req_t *req) {
    char* buf;
    size_t buf_len;
    char num[32] = {0};   // Buffer for "num" parameter
    char data[32] = {0};  // Buffer for "data" parameter

    // Get the length of the query string
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        // Get the query string
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            // Extract "num" parameter
            if (httpd_query_key_value(buf, "num", num, sizeof(num)) == ESP_OK) {
                // Extract "data" parameter
                if (httpd_query_key_value(buf, "data", data, sizeof(data)) == ESP_OK) {
                    // Successfully retrieved both parameters
                   
                    
                    // Add your logic here to process "num" and "data"
                    if(atoi(num)==1){
                      servo1.write(atoi(data));
                       Serial.print("Servo Num: ");
                    Serial.print(atoi(num));
                    Serial.print("position");
                    Serial.println(atoi(data));
                    }
                    else{
                      servo2.write(atoi(data));
                       Serial.print("Servo Num: ");
                    Serial.print(atoi(num));
                    Serial.print("position");
                    Serial.println(atoi(data));
                    }
                    
                    

                    free(buf);
                    httpd_resp_sendstr(req, "Parameters received successfully.");
                    return ESP_OK;
                } else {
                    free(buf);
                    httpd_resp_send_404(req);
                    return ESP_FAIL; // "data" parameter not found
                }
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL; // "num" parameter not found
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL; // Failed to parse query string
        }
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL; // No query string found
    }
}


void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  // httpd_uri_t cmd_uri = {
  //   .uri       = "/action",
  //   .method    = HTTP_GET,
  //   .handler   = cmd_handler,
  //   .user_ctx  = NULL
  // };
httpd_uri_t servo1_uri = {
    .uri       = "/servo",
    .method    = HTTP_GET,
    .handler   = servo_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

//SENSOR DATA
  httpd_uri_t depth_sensor_uri = {
    .uri       = "/sensor1",
    .method    = HTTP_GET,
    .handler   = sensor_handler1,
    .user_ctx  = NULL
    };

  httpd_uri_t temp_sensor_uri = {
    .uri       = "/sensor2",
    .method    = HTTP_GET,
    .handler   = sensor_handler2,
    .user_ctx  = NULL
    };

  httpd_uri_t humidity_sensor_uri = {
    .uri       = "/sensor3",
    .method    = HTTP_GET,
    .handler   = sensor_handler3,
    .user_ctx  = NULL
    };
    
  httpd_uri_t gas_sensor_uri = {
    .uri       = "/sensor4",
    .method    = HTTP_GET,
    .handler   = sensor_handler4,
    .user_ctx  = NULL
    };
  httpd_uri_t pulse_sensor_uri = {
    .uri       = "/sensor5",
    .method    = HTTP_GET,
    .handler   = sensor_handler5,
    .user_ctx  = NULL
    };



  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    // httpd_register_uri_handler(camera_httpd, &cmd_uri);
    httpd_register_uri_handler(camera_httpd, &servo1_uri);
    httpd_register_uri_handler(camera_httpd, &depth_sensor_uri);
    httpd_register_uri_handler(camera_httpd, &temp_sensor_uri);
    httpd_register_uri_handler(camera_httpd, &humidity_sensor_uri);
    httpd_register_uri_handler(camera_httpd, &gas_sensor_uri);
    httpd_register_uri_handler(camera_httpd, &pulse_sensor_uri);

  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  servo1.setPeriodHertz(50);    // standard 50 hz servo
  servo2.setPeriodHertz(50);    // standard 50 hz servo

  servo1.attach(SERVO_1, 500, 2400);
  servo2.attach(SERVO_2, 500, 2400);

  servo1.write(servo1Pos);
  servo2.write(servo2Pos);

  Serial.begin(115200);
  Serial.setDebugOutput(false);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());

  // Start streaming web server
  startCameraServer();
  
  // pinMode(2, INPUT);
  pinMode(12,INPUT);
  // adcAttachPin(12);
  dht.begin();
  
}

void loop() {

  unsigned long currentTime = millis();
  if ( currentTime - previousTime >= 2000) {
    // read dht
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    Serial.println(analogRead(12));

    temp = dht.readTemperature();
    humidity = dht.readHumidity();
    depth = 12;
    gas = 40;
    pulse = 70;

    previousTime = currentTime;
  }
  

// if (Serial.available() > 0) {
//     String data = Serial.readStringUntil('\n');
//     // Find the positions of commas
//     int firstComma = data.indexOf(',');
//     int secondComma = data.indexOf(',', firstComma + 1);
//     int thirdComma = data.indexOf(',', secondComma + 1);
//     int fourthComma = data.indexOf(',', thirdComma + 1);
    
//     // Extract and convert the values
//     temp = data.substring(0, firstComma).toFloat();
//     humidity = data.substring(firstComma + 1, secondComma).toFloat();
//     depth = data.substring(secondComma + 1, thirdComma).toInt();
//     gas = data.substring(thirdComma + 1, fourthComma).toInt();
//     pulse = data.substring(fourthComma + 1).toInt();
//   }


  


}