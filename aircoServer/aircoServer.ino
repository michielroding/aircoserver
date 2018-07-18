#define RECV_PIN 14 // D5
#define SEND_PIN 15 // D6

#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

#include <ESP8266mDNS.h>

#include <IRrecv.h>
#include <IRsend.h>

#include <Carrier.h>

#include <ArduinoJson.h>

char p_buffer[9000];
#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer)

ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81, "", "airco");

IRrecv irrecv(RECV_PIN);
IRsend irsend(SEND_PIN);

decode_results results;

Carrier carrier(MODE_auto, FAN_3, AIRFLOW_dir_1, 25, STATE_on);

void setup() {
  Serial.begin(115200);

  irrecv.enableIRIn();
  irsend.begin();

  WiFiManager wifiManager;
  wifiManager.autoConnect("AirCo", "yachtfocus");

  Serial.print("Airco Server IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("aircoserver")) {
    Serial.println("MDNS responder started, access the webserver at http://aircoserver.local/");
  }

  webServer.on("/", handleWebServerRoot);
  webServer.on("/state", handleWebServerState);
  webServer.on("/fan.svg", handleWebServerSvg);
  webServer.on("/js.min.js", handleWebServerJs);
  webServer.on("/style.min.css", handleWebServerCss);
  webServer.onNotFound(handleWebServerNotFound);
  webServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  if (irrecv.decode(&results)) {
    decodeIrSignal(&results);
    String json = getStateAsJson();
    webSocket.broadcastTXT(json);
    irrecv.resume();
  }
  webServer.handleClient();
  webSocket.loop();
}

void decodeIrSignal(decode_results *results) {
  for (int i = 1; i < results->rawlen; i++) {
    unsigned long duration = results->rawbuf[i] * RAWTICK;

    if (duration > CODE_low + 100) {
      carrier.codes[i - 1] = CODE_high;
    } else {
      carrier.codes[i - 1] = CODE_low;
    }
  }

  carrier.restoreFromCodes();
}

String getStateAsJson() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["state"] = carrier.getState() == STATE_on ? "on" : "off";

  switch (carrier.getMode()) {
    case MODE_auto:
      root["mode"] = "auto";
      break;
    case MODE_cold:
      root["mode"] = "cold";
      break;
    case MODE_warm:
      root["mode"] = "warm";
      break;
    case MODE_wind:
      root["mode"] = "wind";
      break;
    case MODE_rain:
      root["mode"] = "rain";
      break;
  }

  root["temperature"] = carrier.getTemperature();
  root["fan"] = carrier.getFan();

  switch (carrier.getAirFlow()) {
    case AIRFLOW_dir_1:
      root["airflow"] = "dir_1";
      break;
    case AIRFLOW_dir_2:
      root["airflow"] = "dir_2";
      break;
    case AIRFLOW_dir_3:
      root["airflow"] = "dir_3";
      break;
    case AIRFLOW_dir_4:
      root["airflow"] = "dir_4";
      break;
    case AIRFLOW_dir_5:
      root["airflow"] = "dir_5";
      break;
    case AIRFLOW_dir_6:
      root["airflow"] = "dir_6";
      break;
    case AIRFLOW_change:
      root["airflow"] = "change";
      break;
    case AIRFLOW_open:
      root["airflow"] = "open";
      break;
  }

  String response;
  root.prettyPrintTo(response);

  return response;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  String json;
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        json = getStateAsJson();
        webSocket.sendTXT(num, json);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);

      {
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(payload);

        if (!root.success()) {
          Serial.println("parseObject() failed");
        } else {
          const char* state = root["state"];
          const char* mode = root["mode"];
          int temperature = root["temperature"];
          int fan = root["fan"];
          const char* airflow = root["airflow"];

          Serial.print("State: ");
          Serial.print(state);
          Serial.print(" Mode: ");
          Serial.print(mode);
          Serial.print(" Temperature: ");
          Serial.print(temperature);
          Serial.print(" Fan: ");
          Serial.print(fan);
          Serial.print(" Airflow: ");
          Serial.println(airflow);

          if (strcmp(state,  "on") == 0)
            carrier.setState(STATE_on);
          else if (strcmp(state, "off") == 0)
            carrier.setState(STATE_off);

          if (strcmp(mode, "auto") == 0)
            carrier.setMode(MODE_auto);
          else if (strcmp(mode, "cold") == 0)
            carrier.setMode(MODE_cold);
          else if (strcmp(mode, "warm") == 0)
            carrier.setMode(MODE_warm);
          else if (strcmp(mode, "wind") == 0)
            carrier.setMode(MODE_wind);
          else if (strcmp(mode, "rain") == 0)
            carrier.setMode(MODE_rain);

          carrier.setTemperature(temperature);

          if (strcmp(airflow, "dir_1") == 0)
            carrier.setAirFlow(AIRFLOW_dir_1);
          else if (strcmp(airflow, "dir_2") == 0)
            carrier.setAirFlow(AIRFLOW_dir_2);
          else if (strcmp(airflow, "dir_3") == 0)
            carrier.setAirFlow(AIRFLOW_dir_3);
          else if (strcmp(airflow, "dir_4") == 0)
            carrier.setAirFlow(AIRFLOW_dir_4);
          else if (strcmp(airflow, "dir_5") == 0)
            carrier.setAirFlow(AIRFLOW_dir_5);
          else if (strcmp(airflow, "dir_6") == 0)
            carrier.setAirFlow(AIRFLOW_dir_6);
          else if (strcmp(airflow, "change") == 0)
            carrier.setAirFlow(AIRFLOW_change);
          else if (strcmp(airflow, "open") == 0)
            carrier.setAirFlow(AIRFLOW_open);

          if (fan == 1)
            carrier.setFan(FAN_1);
          if (fan == 2)
            carrier.setFan(FAN_2);
          if (fan == 3)
            carrier.setFan(FAN_3);
          if (fan == 4)
            carrier.setFan(FAN_4);

          irsend.sendRaw(carrier.codes, CARRIER_BUFFER_SIZE, 38);
          irsend.sendRaw(carrier.codes, CARRIER_BUFFER_SIZE, 38);
          irsend.sendRaw(carrier.codes, CARRIER_BUFFER_SIZE, 38);
        }
      }
      json = getStateAsJson();
      webSocket.broadcastTXT(json);;
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\n", num, length);
      hexdump(payload, length);

      // send message to client
      // webSocket.sendBIN(num, payload, length);
      break;
  }
}

void handleWebServerRoot() {
  webServer.send(
    200,
    "text/html",
    P("<!DOCTYPE html><html><head> <title>Airco Server</title> <link href=\"https://fonts.googleapis.com/css?family=Nova+Mono\" rel=\"stylesheet\"> <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.1.0/css/all.css\" integrity=\"sha384-lKuwvrZot6UHsBSfcMvOkWwlCMgc0TaWr+30HWe3a4ltaBwTZhyTEggF5tJv8tbt\" crossorigin=\"anonymous\"> <link rel=\"stylesheet\" href=\"style.min.css\"></head><body><div id=\"remote\"> <div id=\"mode\"> <i class=\"fas fa-asterisk\" data-mode=\"wind\"></i> <i class=\"fas fa-sun\" data-mode=\"warm\"></i> <span class=\"auto\" data-mode=\"auto\">A</span> <i class=\"fas fa-snowflake\" data-mode=\"cold\"></i> <i class=\"fas fa-tint\" data-mode=\"rain\"></i> </div><div id=\"temperature\"> <input type=\"number\" id=\"degrees\" value=\"\" min=\"17\" max=\"32\" step=\"1\"><span class=\"unit\">&#176;C</span> </div><div id=\"fan-speed\"><img src=\"fan.svg\" width=\"20\"><span class=\"ring_1 ring\"></span><span class=\"ring_2 ring\"></span><span class=\"ring_3 ring\"></span></div><div id=\"state\"> <i class=\"fas fa-power-off\"></i> </div><div id=\"airflow\" class=\"pulse\"> <div class=\"core active\"></div><div class=\"helper\"></div><div class=\"boost active\"></div><div class=\"dir_1 dir\"></div><div class=\"dir_2 dir\"></div><div class=\"dir_3 dir\"></div><div class=\"dir_4 dir\"></div><div class=\"dir_5 dir\"></div><div class=\"dir_6 dir\"></div></div></div><script type=\"text/javascript\" src=\"js.min.js\"></script></body></html>")
  );
}

void handleWebServerSvg() {
  webServer.send(
    200,
    "image/svg+xml",
    P("<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE svg PUBLIC '-//W3C//DTD SVG 1.1//EN'  'http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd'><svg enable-background=\"new 0 0 1000 1000\" version=\"1.1\" viewBox=\"0 0 1000 1000\" xml:space=\"preserve\" xmlns=\"http://www.w3.org/2000/svg\"><metadata> Svg Vector Icons : http://www.onlinewebfonts.com/icon </metadata><g transform=\"translate(0 511) scale(.1 -.1)\"><path d=\"m4517.6 3503v-1504.7l-119.5-41.4c-66.6-20.7-160.8-57.4-209-82.7l-85-43.6-156.2 124.1c-408.9 326.2-843.1 491.6-1350.8 514.6-1134.8 55.1-2125-677.7-2414.4-1780.4-48.2-186.1-59.7-294-68.9-640.9l-13.8-420.4h3007.1l82.7-211.3 82.7-209-110.3-135.5c-213.6-255-388.2-604.2-475.5-951.1-29.9-117.2-39-255-39-537.5 0-459.5 39-636.3 227.4-1024.6 234.3-482.4 595-850 1068.2-1088.9 349.2-176.9 588.1-229.7 1121-245.8l418.1-13.8v3007.1l209.1 82.7 211.3 82.7 140.1-114.9c305.5-248.1 668.5-415.8 1063.6-491.6 271.1-50.5 723.6-34.5 987.8 34.5 854.6 222.9 1500.1 868.4 1722.9 1722.9 48.2 183.8 59.7 291.8 68.9 640.9l13.8 418.1h-3011.6l-41.3 121.7c-20.7 64.3-57.4 158.5-82.7 204.5l-41.3 87.3 108 130.9c330.8 408.9 503.1 850 528.4 1364.6 48.2 987.8-487 1872.2-1387.5 2288-312.4 144.7-560.5 193-1033.8 206.8l-420.4 13.8v-1507.1zm1111.8 383.7c356.1-174.6 634-519.2 737.4-916.6 43.7-174.6 43.7-491.6 0-666.2-36.7-144.7-165.4-420.4-238.9-514.6l-41.4-55.1-124.1 71.2c-68.9 36.8-204.4 96.5-300.9 130.9l-179.2 64.3v971.7c0 535.3 6.9 974 13.8 974 7 0.1 66.7-27.5 133.3-59.6zm-2823.3-2409.8c144.7-36.8 420.4-165.4 514.6-238.9l55.1-43.6-68.9-124.1c-39-68.9-98.8-204.4-133.2-300.9l-64.3-176.9h-971.7c-535.2 0-974 6.9-974 13.8s36.8 82.7 80.4 167.7c183.8 349.2 498.5 595 895.9 703 156.1 43.5 498.4 41.2 666.1-0.1zm2437.4-411.2c431.9-114.9 744.3-514.6 744.3-955.7 0-535.3-452.6-987.8-987.8-987.8s-987.8 452.5-987.8 987.8c0 436.5 307.8 836.2 735.1 955.7 140.1 39 353.8 39 496.2 0zm3581.4-1488.7c-34.5-117.1-170-312.4-321.6-468.6-179.2-179.2-395.1-303.2-643.2-365.3-176.9-45.9-507.7-43.6-686.9 2.3-137.8 34.5-388.2 153.9-491.6 234.3l-55.1 41.4 91.9 183.8c50.5 98.8 108 236.6 130.9 303.2l39.1 119.5h1950.3l-13.8-50.6zm-4725.4-1187.6c96.5-48.3 232-105.7 298.6-126.3l119.5-41.4v-1950.3l-50.5 13.8c-117.1 34.5-312.4 170-466.3 321.6-268.8 268.8-402 595-402 987.8 0 220.5 29.9 365.2 112.6 551.3 50.5 114.9 181.5 333.1 199.9 333.1 6.7 0 89.4-39.1 188.2-89.6z\"/></g></svg>")
  );
}

void handleWebServerJs() {
  webServer.send(
    200,
    "application/javascript",
    P("var wsServer=\"ws://aircoserver.local:81\";var lastAirflow=null;var airflowIndex=1;var airflowChange;var lastFanSpeed=null;var fanSpeedChange;var fanSpeedIndex=1;var degreesElement=document.querySelector(\"#degrees\");var stateElement=document.querySelector(\"#state > i\");var airflowContainer=document.querySelector(\"#airflow\");var fanspeedContainer=document.querySelector(\"#fan-speed\");var aircoWs=new WebSocket(wsServer,\"airco\");aircoWs.onmessage=function(data){data=JSON.parse(data.data);degreesElement.value=data.temperature;setMode(data.mode);setState(data.state);setFanSpeed(data.fan);setAirflow(data.airflow)};aircoWs.onclose=function(){console.warn(\"WebSocket closed\")};aircoWs.onerror=function(event){console.error(\"WebSocket error observed:\",event)};degreesElement.addEventListener(\"blur\",publishState);document.querySelectorAll(\"#mode > *\").forEach(function(element){element.addEventListener(\"click\",function(){setMode(this.dataset.mode);publishState()})});degreesElement.addEventListener(\"change\",publishState);stateElement.addEventListener(\"click\",function(){setState(stateElement.classList.contains(\"active\")?\"off\":\"on\");publishState()});airflowContainer.addEventListener(\"click\",function(){switch(lastAirflow){case\"open\":setAirflow(\"dir_1\");break;case\"dir_1\":setAirflow(\"dir_2\");break;case\"dir_2\":setAirflow(\"dir_3\");break;case\"dir_3\":setAirflow(\"dir_4\");break;case\"dir_4\":setAirflow(\"dir_5\");break;case\"dir_5\":setAirflow(\"dir_6\");break;case\"dir_6\":setAirflow(\"change\");break;case\"change\":setAirflow(\"open\");break}publishState()});fanspeedContainer.addEventListener(\"click\",function(){if(lastFanSpeed===4){setFanSpeed(1)}else{setFanSpeed(lastFanSpeed+1)}publishState()});function setFanSpeed(fan){if(lastFanSpeed!==fan){if(fan===4){clearInterval(fanSpeedChange);addClass(\"#fan-speed .ring_1\",\"active\");fanSpeedChange=setInterval(function(){if(fanSpeedIndex>=4){fanSpeedIndex=1}removeClass(\"#fan-speed .ring_2, #fan-speed .ring_3\",\"active\");for(var i=2;i<=fanSpeedIndex;i++){addClass(\"#fan-speed .ring_\"+i,\"active\")}fanSpeedIndex++},1e3)}else{clearInterval(fanSpeedChange);removeClass(\"#fan-speed .ring\",\"active\");for(var i=0;i<=fan;i++){addClass(\"#fan-speed .ring_\"+i,\"active\")}}lastFanSpeed=fan}}function setAirflow(airflow){if(lastAirflow!==airflow){if(airflow===\"change\"){clearInterval(airflowChange);airflowChange=setInterval(function(){removeClass(\"#airflow .dir.active\",\"active\");if(airflowIndex>=7){airflowIndex=1}addClass(\"#airflow .dir_\"+airflowIndex,\"active\");airflowIndex++},1e3)}else if(airflow===\"open\"){clearInterval(airflowChange);airflowChange=setInterval(function(){if(airflowIndex>=4){airflowIndex=1}switch(airflowIndex){case 1:removeClass(\"#airflow .boost\",\"active\");removeClass(\"#airflow .dir\",\"active\");break;case 2:addClass(\"#airflow .boost\",\"active\");break;case 3:addClass(\"#airflow .dir\",\"active\");break}airflowIndex++},1e3)}else if(airflow.substr(0,4)===\"dir_\"){clearInterval(airflowChange);removeClass(\"#airflow .dir.active\",\"active\");addClass(\"#airflow .\"+airflow,\"active\")}}lastAirflow=airflow}function setMode(mode){removeClass(\"#mode .active\",\"active\");addClass('#mode > [data-mode=\"'+mode+'\"]',\"active\")}function setState(state){if(state===\"on\"){stateElement.classList.add(\"active\")}else{stateElement.classList.remove(\"active\")}}function publishState(){aircoWs.send(JSON.stringify({state:stateElement.classList.contains(\"active\")?\"on\":\"off\",mode:document.querySelector(\"#mode .active\")?document.querySelector(\"#mode .active\").dataset.mode:\"auto\",temperature:degreesElement.value,fan:lastFanSpeed,airflow:lastAirflow}))}function removeClass(selector,classname){document.querySelectorAll(selector).forEach(function(element){element.classList.remove(classname)})}function addClass(selector,classname){document.querySelectorAll(selector).forEach(function(element){element.classList.add(classname)})}")
  );
}

void handleWebServerCss() {
  webServer.send(
    200,
    "text/css",
    P("#remote{border:3px solid #696969;border-radius:12px;padding:12px}#mode{font-size:40px;color:#a9a9a9}#mode>*,#state>i{cursor:pointer}#mode .active,#state>i.active{color:#000}#mode .auto{font-family:Nova Mono}#temperature,#temperature>#degrees{font-family:Nova Mono;font-size:90px}#temperature{color:#000}#temperature>#degrees{border:0;width:1.3em}#temperature>.unit{color:#a9a9a9}#fan-speed{width:90px;margin-left:120px}#fan-speed .ring,#fan-speed img{position:absolute;top:11px;right:14px}#fan-speed .ring{top:0;right:0;border:3px solid #a9a9a9;border-top:none;border-bottom:none}#fan-speed .ring.active{border-color:#000}#fan-speed .ring_1{top:10px;right:10px;width:22px;height:22px;border-radius:11px}#fan-speed .ring_2{top:5px;right:5px;width:32px;height:32px;border-radius:16px}#fan-speed .ring_3{width:42px;height:42px;border-radius:21px}#airflow,#fan-speed,#state{position:relative}#state{margin-left:85px;width:80px}#state>i{position:absolute;font-size:40px;color:red}#airflow{width:40px;height:40px}#airflow .active{background:#000!important}#airflow>*{position:absolute;top:0;right:0}#airflow .core{width:9px;height:9px;background:0 0;border-radius:0 0 0 9px;z-index:3}#airflow .helper{width:11px;height:11px;background:#fff;border-radius:0 0 0 11px;z-index:2;animation-name:none}#airflow .boost{width:15px;height:15px;background:#a9a9a9;border-radius:0 0 0 15px}#airflow .dir{width:20px;height:2px;background:#a9a9a9;right:18px}#airflow .dir_2{transform:translate(1px,8px) rotate(-18deg)}#airflow .dir_3{transform:translate(4px,16px) rotate(-36deg)}#airflow .dir_4{transform:translate(10px,22px) rotate(-54deg)}#airflow .dir_5{transform:translate(19px,26px) rotate(-72deg)}#airflow .dir_6{transform:translate(27px,27px) rotate(-90deg)}")
  );
}

void handleWebServerState() {
  webServer.send(200, "application/json", getStateAsJson());
}

void handleWebServerNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", message);
}
