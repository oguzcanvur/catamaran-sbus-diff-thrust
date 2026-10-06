# STM32G431RB Katamaran Tekne Kontrolü

RadioLink AT9S Pro + R9DS (S.BUS) → NUCLEO-G431RB → 2× çift yönlü ESC.
Sağ joystick ile arcade (tek kol) diferansiyel sürüş.

## Dosya yapısı

| Dosya | Görev |
|---|---|
| `Core/Inc/boat_config.h` | **Tüm ayarlar** (deadband, TURN_SENSITIVITY, kanal/motor ters çevirme, timeout) |
| `Core/Src/sbus.c` | S.BUS çözücü: USART1 + DMA + Idle Line, 25 bayt paket, failsafe/timeout |
| `Core/Src/catamaran_mixer.c` | Deadband + diferansiyel itiş mikseri + clamp |
| `Core/Src/motor_pwm.c` | TIM2 CH1/CH2 50 Hz, 1 µs çözünürlük ESC çıkışı |
| `Core/Src/main.c` | Saat (170 MHz), çevre birimi init, ana döngü, durum LED'i |
| `Core/Src/stm32g4xx_it.c` | Kesmeler; HardFault'ta motorlar nötre çekilir |
| `Drivers/` | STM32Cube_FW_G4_V1.6.3 HAL + CMSIS (kopyalandı) |
| `Makefile` | Komut satırı derlemesi |

## Pinler

| Sinyal | Pin | Ayar |
|---|---|---|
| S.BUS (R9DS) | PA10 – USART1_RX (AF7) | 100000 baud, 8E2, **RX Invert aktif**, DMA1_Ch1 |
| Sol ESC | PA0 – TIM2_CH1 (AF1) | PSC 169, ARR 19999 → 50 Hz, 1 µs |
| Sağ ESC | PA1 – TIM2_CH2 (AF1) | 〃 |
| Durum LED | PA5 – LD2 | Hızlı yanıp sönme: arming · yavaş: sinyal yok/failsafe · sabit: OK |
| GND | — | Nucleo, R9DS ve iki ESC sinyal GND'si ortak |

> ESC'lerin BEC (+5V) çıkışlarından **yalnızca birini** R9DS'i beslemek için kullanın, iki BEC'i paralel bağlamayın.

## Derleme

**STM32CubeIDE:** *File → Import → C/C++ → Existing Code as Makefile Project* → bu klasörü seçin,
toolchain olarak *MCU ARM GCC* seçin → Build. Hata ayıklama için *Debug As → STM32 C/C++ Application*.

**Komut satırı** (CubeIDE'nin make ve gcc'si PATH'teyse):
```bash
make -j8
```
Çıktı: `build/stm_motor_kontrol.elf / .hex / .bin` (STM32CubeProgrammer ile ya da Nucleo sürücüsüne `.bin` kopyalayarak yüklenebilir).

## Güvenlik davranışı

1. Açılışta **3 s** boyunca her iki ESC'ye 1500 µs gönderilir (ESC arming).
2. **100 ms** geçerli S.BUS paketi gelmezse veya alıcı failsafe bayrağı set ise → iki motor da 1500 µs (dur).
3. Bozuk paketler (yanlış header/footer, uzunluk ≠ 25, parity/frame hatası) atılır, alım otomatik yeniden başlar.
4. HardFault / Error_Handler durumunda PWM register'ları doğrudan 1500 µs'ye çekilir.
5. Kumandada da R9DS failsafe ayarını CH1/CH2 = orta konum olarak ayarlayın (yedek güvenlik).

## İlk çalıştırma kontrol listesi (pervanesiz!)

1. Pervaneleri sökün, ESC'leri çift yönlü (bidirectional/3D) moda alın, gerekirse 1000/1500/2000 µs ile kalibre edin.
2. Debugger'da `rc.raw[0]`, `rc.raw[1]` değerlerini izleyin. Kol uçlarında 172/1811'den farklıysa
   `SBUS_RAW_MIN/CENTER/MAX` değerlerini düzeltin (veya kumandada End Point ayarlayın).
3. Kol ileri → iki motor ileri dönmeli. Ters ise `THROTTLE_REVERSE 1`.
4. Kol sağa → sol motor ileri, sağ motor geri (tekne sağa döner). Ters ise `STEERING_REVERSE 1`.
5. Tek bir motor ters dönüyorsa ilgili `LEFT/RIGHT_MOTOR_REVERSE 1` (veya motor fazlarından ikisini değiştirin).

## Mikser örnekleri (TURN_SENSITIVITY = 0.85)

| Gaz (CH2) | Dümen (CH1) | Sol | Sağ |
|---|---|---|---|
| 1500 | 1500 | 1500 | 1500 |
| 1530 | 1460 | 1466 | 1534 (gaz deadband'de, dümen değil) |
| 2000 | 1500 | 2000 | 2000 |
| 1500 | 2000 | 1925 | 1075 (yerinde sağa dönüş) |
| 2000 | 2000 | 2000 | 1575 |
| 1750 | 1250 | 1538 | 1962 |
