#include <SPI.h>
#include <Ethernet.h>


#define ETH_RST   5
#define ETH_CS    10
#define ETH_SCLK  12
#define ETH_MISO  13
#define ETH_MOSI  11


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 123);

EthernetServer server(5000);

void setup() {
  Serial.begin(115200);


  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(200);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  // Khởi tạo SPI với chân custom
  SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI, ETH_CS);
  Ethernet.init(ETH_CS);

  // Khởi tạo Ethernet
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Không có DHCP, dùng IP tĩnh");
    Ethernet.begin(mac, ip);
  }

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  server.begin();
}

void loop() {
  EthernetClient client = server.available();
  if (client) 
  {
    Serial.println("Có client kết nối");
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n' && currentLineIsBlank) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<h1>Hello from ESP32-S3 + W5500!</h1>");
          break;
        }
        if (c == '\n') currentLineIsBlank = true;
        else if (c != '\r') currentLineIsBlank = false;
      }
    }
    delay(1);
    client.stop();
    Serial.println("Ngắt client");
  }
}

