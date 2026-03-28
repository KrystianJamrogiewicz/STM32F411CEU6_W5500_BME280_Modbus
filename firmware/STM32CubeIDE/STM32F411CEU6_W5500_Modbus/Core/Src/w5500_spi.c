#include "w5500_spi.h"
#include "wizchip_conf.h"

extern SPI_HandleTypeDef hspi1; // Zakładamy użycie SPI1

// ==========================================
// FUNKCJE POMOCNICZE CS (Chip Select)
// ==========================================
void W5500_Select(void) {
    // UWAGA: Upewnij się, że to poprawny port i pin dla Twojego sygnału CS!
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

void W5500_Unselect(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

// ==========================================
// FUNKCJE POMOCNICZE SPI (Odczyt/Zapis)
// ==========================================
void W5500_WriteByte(uint8_t byte) {
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

uint8_t W5500_ReadByte(void) {
    uint8_t rx = 0;
    uint8_t tx = 0xFF; // Wysłanie 'pustych' danych, aby wymusić zegar SPI i odebrać bajt
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

// ==========================================
// GŁÓWNA INICJALIZACJA SPRZĘTU I SIECI
// ==========================================
void W5500_Hardware_Init(void) {


	HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(10); // Czekamy 10ms (Reset W5500 aktywowany stanem niskim)
	HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(50); // Dajemy mu 50ms na "ogarnięcie się" przed konfiguracją SPI


    // 1. Rejestracja naszych funkcji SPI w bibliotece WIZnet
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);

    // 2. Inicjalizacja wewnętrznej pamięci buforów W5500 (po 2KB na gniazdo)
    uint8_t rx_tx_buff_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2};
    wizchip_init(rx_tx_buff_sizes, rx_tx_buff_sizes);

    // 3. Konfiguracja sieci (IP, MAC, Bramka)
    wiz_NetInfo netInfo = {
        .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Dowolny unikalny MAC
        .ip   = {192, 168, 1, 4},                    // <--- ADRES IP TWOJEGO STM32
        .sn   = {255, 255, 255, 0},                   // Maska podsieci
        .gw   = {192, 168, 1, 1},                     // Adres routera
        .dns  = {8, 8, 8, 8},                         // Serwer DNS (np. Google)
        .dhcp = NETINFO_STATIC                        // Ustawiamy sztywne IP
    };
    wizchip_setnetinfo(&netInfo);
}
