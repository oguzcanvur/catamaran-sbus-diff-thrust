# STM32G431RB Katamaran Tekne Kontrolü

RadioLink AT9S Pro + R9DS (S.BUS) → NUCLEO-G431RB → 2× çift yönlü ESC.
Sağ joystick ile arcade (tek kol) diferansiyel sürüş, CH10 arm anahtarı,
PC'de canlı telemetri / kalibrasyon / motor nötr ayarı arayüzü.

## Dosya yapısı

| Dosya | Görev |
|---|---|
| `Core/Inc/boat_config.h` | **Tüm ayarlar** (deadband, TURN_SENSITIVITY, kanal/motor ters çevirme, arm anahtarı, timeout'lar) |
| `Core/Inc/rc_calibration.h` | CH1/CH2 ham S.BUS min/merkez/max (arayüzden yazılır) |
| `Core/Inc/motor_trim.h` | Motor nötr **varsayılanları** (asıl değerler flash'ta) |
| `Core/Src/sbus.c` | S.BUS çözücü: USART1 + DMA + Idle Line, 25 bayt paket, kanal bazlı kalibrasyon, failsafe/timeout |
| `Core/Src/catamaran_mixer.c` | Deadband + diferansiyel itiş mikseri + clamp |
| `Core/Src/arm_switch.c` | CH10 arm anahtarı durum makinesi ve güvenlik kilitleri |
| `Core/Src/motor_pwm.c` | TIM2 CH1/CH2 50 Hz, 1 µs çözünürlük ESC çıkışı, motor bazlı nötr |
| `Core/Src/settings.c` | Motor nötrlerinin flash'ta (son 2 KB sayfa) kalıcı saklanması |
| `Core/Src/telemetry.c` | ST-LINK sanal COM üzerinden telemetri çıktısı ve PC komutları |
| `Core/Src/main.c` | Saat (170 MHz), çevre birimi init, ana döngü, durum LED'i |
| `Core/Src/stm32g4xx_it.c` | Kesmeler; HardFault'ta motorlar nötre çekilir |
| `tools/telemetry_gui.ps1` | **PC arayüzü** (Windows, ek kurulum gerekmez) |
| `tools/serial_monitor.ps1` | Terminalde düz metin telemetri izleyici |
| `Drivers/` | STM32Cube_FW_G4_V1.6.3 HAL + CMSIS |
| `Makefile` | Komut satırı derlemesi |

## Pinler

| Sinyal | Pin | Ayar |
|---|---|---|
| S.BUS (R9DS CH9/SBUS portu) | PA10 (Arduino D2) – USART1_RX | 100000 baud, 8E2, **RX Invert aktif**, DMA1_Ch1 |
| Sol ESC sinyal | PA0 (Arduino A0) – TIM2_CH1 | PSC 169, ARR 19999 → 50 Hz, 1 µs |
| Sağ ESC sinyal | PA1 (Arduino A1) – TIM2_CH2 | 〃 |
| Telemetri / komut | PA2/PA3 – LPUART1 | ST-LINK sanal COM portu, 115200 8N1 (USB kablosu üzerinden) |
| Durum LED | PA5 – LD2 | Sabit: ARMED · saniyede bir çakma: DISARM · yavaş: sinyal yok · hızlı: başlatma |
| GND | — | Nucleo, R9DS ve iki ESC sinyal GND'si ortak |

> ESC'lerin BEC (+5V) çıkışlarından **yalnızca birini** kullanın (teknede Nucleo **E5V** + R9DS),
> diğer ESC'nin kırmızı kablosunu bağlamayın. Masada test ederken Nucleo USB'den beslenir.

## Kanallar

| Kanal | Görev |
|---|---|
| CH1 (sağ kol yatay) | Dümen |
| CH2 (sağ kol dikey) | Gaz (ileri/geri) |
| CH10 | **Arm anahtarı** (>1600 µs ARM, <1400 µs DISARM) |

Kumandada model tipi ACRO, wing/tail Normal, CH1/CH2/CH10 için **mix kapalı** olmalı; karıştırmayı kart yapar.

## Derleme ve yükleme

**STM32CubeIDE:** *File → Import → C/C++ → Existing Code as Makefile Project* → bu klasörü seçin,
toolchain olarak *MCU ARM GCC* seçin → Build. Hata ayıklama için *Debug As → STM32 C/C++ Application*.

**Komut satırı** (CubeIDE'nin make ve arm-none-eabi-gcc'si PATH'teyse):
```bash
make -j8
```
Çıktı: `build/stm_motor_kontrol.elf / .hex / .bin`. STM32CubeProgrammer ile ya da Nucleo sürücüsüne
`.bin` kopyalayarak yüklenebilir. Yazılım güncellemesi flash'taki motor nötr ayarlarını silmez.

## PC arayüzü

```bash
powershell -ExecutionPolicy Bypass -File tools\telemetry_gui.ps1
```
ST-LINK COM portunu otomatik bulur. COM portunu aynı anda tek program kullanabilir.

- **Canlı görünüm:** durum (BASLAT / DISARM / ARM? / ARMED / KAYIP / TEST), sağ kol konumu, iki motorun
  µs değeri ve yönü, CH10, paket/hata sayaçları.
- **Kalibrasyon (sağ panel):** sağ kolun uçlarını ve merkezini ölçer, `Core/Inc/rc_calibration.h` dosyasını
  yazar. Ardından yeniden derleyip yükleyin.
- **Motor nötr testi (alt panel):** test modunda ESC'lere doğrudan 1350–1650 µs gönderilir, motorun
  durduğu aralık bulunur. **"Karta yükle (kalıcı)"** nötrleri kartın flash'ına yazar; derleme gerekmez.

### Seri komutlar (115200 8N1, satır sonu `\n`)

| Komut | Etki |
|---|---|
| `T <sol> <sağ>` | Test modu: ESC'lere doğrudan µs yaz (1350–1650). 300 ms yenilenmezse kapanır. |
| `X` | Test modundan çık |
| `N <sol> <sağ>` | Motor nötrlerini uygula ve flash'a kaydet |

## Güvenlik davranışı

1. Açılışta **3 s** boyunca her iki ESC'ye nötr gönderilir (ESC başlatma).
2. **CH10 kapalıyken** motorlar kilitli. Arm için: anahtar açılıştan sonra en az bir kez kapalı görülmüş
   olmalı **ve** sağ kol ortada olmalı. Aksi halde durum `ARM?` olarak kalır.
3. **100 ms** geçerli S.BUS paketi gelmezse veya alıcı failsafe bayrağı set ise → iki motor da nötr.
   Kesinti **1 s**'yi aşarsa otomatik disarm (anahtarı kapatıp açmak gerekir).
4. PC test modu 300 ms komut gelmezse kendiliğinden kapanır; test aralığı 1350–1650 µs ile sınırlı.
5. Bozuk paketler (yanlış header/footer, uzunluk ≠ 25, parity/frame hatası) atılır, alım otomatik yeniden başlar.
6. HardFault / Error_Handler durumunda PWM register'ları doğrudan 1500 µs'ye çekilir.
7. R9DS failsafe ayarında CH1/CH2 orta, **CH10 kapalı** olarak ayarlayın (yedek güvenlik).

## İlk çalıştırma kontrol listesi (pervanesiz!)

1. Pervaneleri sökün, ESC'leri çift yönlü (bidirectional/3D) moda alın.
2. Arayüzü açın, kumandayı açın. Kol bırakıkken CH1/CH2 ≈ 1500 olmalı; değilse **kalibrasyon** yapın.
3. Motorlar nötrde kesik kesik dönüyorsa **motor nötr testi** ile durma noktasını bulup karta yükleyin.
4. CH10'u açın (ARMED). Kol ileri → iki motor ileri dönmeli. Ters ise `THROTTLE_REVERSE 1`.
5. Kol sağa → sol motor ileri, sağ motor geri (tekne sağa döner). Ters ise `STEERING_REVERSE 1`.
6. Tek bir motor ters dönüyorsa ilgili `LEFT/RIGHT_MOTOR_REVERSE 1` (veya motor fazlarından ikisini değiştirin).

## Mikser örnekleri (TURN_SENSITIVITY = 0.85, nötr 1500)

| Gaz (CH2) | Dümen (CH1) | Sol | Sağ |
|---|---|---|---|
| 1500 | 1500 | 1500 | 1500 |
| 1530 | 1460 | 1466 | 1534 (gaz deadband'de, dümen değil) |
| 2000 | 1500 | 2000 | 2000 |
| 1500 | 2000 | 1925 | 1075 (yerinde sağa dönüş) |
| 2000 | 2000 | 2000 | 1575 |
| 1750 | 1250 | 1538 | 1962 |

Motor nötrü 1500'den farklıysa (örn. 1487) çıkışlar o motor için aynı miktar kaydırılır.
