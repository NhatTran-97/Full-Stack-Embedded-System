
#include <SPI.h>
#include <Ethernet.h>

// Định nghĩa chân W5500
#define ETH_RST   5
#define ETH_CS    10
#define ETH_SCLK  12
#define ETH_MISO  13
#define ETH_MOSI  11

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

EthernetServer server(5000);  // TCP server trên port 5000

void setup() {
  Serial.begin(115200);

  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(200);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  // Khởi tạo SPI
  SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI, ETH_CS);
  Ethernet.init(ETH_CS);


  if (Ethernet.begin(mac) == 0) {
    Serial.println("Không có DHCP, dùng IP tĩnh");
    Ethernet.begin(mac, ip);
  }

  Serial.print("ESP32-S3 IP: ");
  Serial.println(Ethernet.localIP());

  server.begin();
}

void loop()
 {
  EthernetClient client = server.available();
  if (client) {
    Serial.println("Client kết nối!");

    while (client.connected()) {
      if (client.available()) {
        String data = client.readStringUntil('\n');  // đọc chuỗi tới newline
        Serial.print("Nhận từ client: ");
        Serial.println(data);


        client.print("ESP32-S3 nhận được: ");
        client.println(data);
      }
    }

    client.stop();
    Serial.println("Client ngắt kết nối");
  }
}

