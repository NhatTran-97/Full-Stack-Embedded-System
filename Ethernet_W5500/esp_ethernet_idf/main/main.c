#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

static const char *TAG = "eth_tcp_w5500";

// --- Pins (ESP32-S3 PLC Mini) ---
#define PIN_ETH_RST 5
#define PIN_ETH_CS 10
#define PIN_ETH_SCLK 12
#define PIN_ETH_MISO 13
#define PIN_ETH_MOSI 11
#define PIN_ETH_INT -1 // không nối INT → dùng polling

// --- Static IP (đổi theo mạng của bạn) ---
#define STATIC_IP_A 192
#define STATIC_IP_B 168
#define STATIC_IP_C 1
#define STATIC_IP_D 123
#define GW_A 192
#define GW_B 168
#define GW_C 1
#define GW_D 1
#define MASK_A 255
#define MASK_B 255
#define MASK_C 255
#define MASK_D 0

#define TCP_PORT 5000

// =============== TCP SERVER ===============
static void tcp_server_task(void *arg)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listenfd < 0)
    {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(listenfd);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listenfd, 2) != 0)
    {
        ESP_LOGE(TAG, "listen() failed: %d", errno);
        close(listenfd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (1)
    {
        struct sockaddr_in6 source_addr;
        uint32_t addr_len = sizeof(source_addr);
        int sock = accept(listenfd, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGW(TAG, "accept() failed: %d", errno);
            continue;
        }

        // cấu hình keepalive
        int ka = 1, idle = 10, intvl = 5, cnt = 3;
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));

        const char *banner = "ESP32-S3 W5500 TCP Server Ready\n";
        send(sock, banner, strlen(banner), 0);

        char buf[512];
        int n;
        while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
        {
            buf[n] = 0;
            ESP_LOGI(TAG, "RX(%d): %s", n, buf);
            send(sock, buf, n, 0); // echo lại
        }
        ESP_LOGI(TAG, "Client disconnected (n=%d, errno=%d)", n, errno);
        shutdown(sock, 0);
        close(sock);
    }
}

// =============== ETH EVENTS ===============
static void eth_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id)
    {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up → start TCP server");
        xTaskCreate(tcp_server_task, "tcp_server", 8192, NULL, 5, NULL);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

// =============== MAIN ===============
void app_main(void)
{
    // Reset W5500 cứng
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_ETH_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io);
    gpio_set_level(PIN_ETH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_ETH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));

    // SPI2 bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_ETH_MOSI,
        .miso_io_num = PIN_ETH_MISO,
        .sclk_io_num = PIN_ETH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // SPI device cho W5500
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 26 * 1000 * 1000,
        .spics_io_num = PIN_ETH_CS,
        .queue_size = 20,
        .command_bits = 16,
        .address_bits = 8,
        .dummy_bits = 0,
    };

    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_cfg.int_gpio_num = PIN_ETH_INT; // -1: không dùng INT
    w5500_cfg.poll_period_ms = 10;        // bắt buộc khi không có INT

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num = PIN_ETH_RST;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth_handle));

    // Gắn driver vào netif
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // STATIC IP trước khi start
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, STATIC_IP_A, STATIC_IP_B, STATIC_IP_C, STATIC_IP_D);
    IP4_ADDR(&ip_info.gw, GW_A, GW_B, GW_C, GW_D);
    IP4_ADDR(&ip_info.netmask, MASK_A, MASK_B, MASK_C, MASK_D);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &ip_info));
    ESP_LOGI(TAG, "Static IP set to: " IPSTR, IP2STR(&ip_info.ip));

    // MAC cho W5500
    uint8_t mac_addr[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    // Đọc lại để kiểm tra
    uint8_t mac_read[6];
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_read));
    ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_read[0], mac_read[1], mac_read[2],
             mac_read[3], mac_read[4], mac_read[5]);

    // Start Ethernet
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}
