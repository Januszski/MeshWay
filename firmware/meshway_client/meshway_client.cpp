#include "meshway_client.h"
#include "meshway.h"
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "MeshWay_AP";
const char* password = "12345678";

WebServer server(80);

String webMessage = "";
char data_payload[BUFFER_SIZE];

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>MeshWay Wi-Fi</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #f2f2f2; margin: 0; padding: 0; }
    .container { max-width: 400px; margin: 50px auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }
    h1 { color: #333; margin-bottom: 20px; }
    input[type=text] { width: 80%; padding: 10px; margin-bottom: 20px; border-radius: 5px; border: 1px solid #ccc; }
    button { padding: 10px 20px; background-color: #007BFF; color: white; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; }
    button:hover { background-color: #0056b3; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Welcome to MeshWay</h1>
    <form action="/send" method="POST">
      <input type="text" name="message" placeholder="Type your message here">
      <br>
      <button type="submit">SEND</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSend() {
if (server.hasArg("message")) {
    webMessage = server.arg("message"); 
    Serial.print("Received message from web: ");
    Serial.println(webMessage);

    sendLoRaMessage(webMessage);
}
  server.sendHeader("Location", "/");
  server.send(303);
}

void meshwayClientInit() {
  Serial.println("Starting MeshWay Client Web Server...");
  WiFi.softAP(ssid, password);
  Serial.print("AP SSID: "); Serial.println(ssid);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/send", HTTP_POST, handleSend);

  server.begin();
  Serial.println("Web server started!");
}

void meshwayClientLoop() {
  server.handleClient();
}
