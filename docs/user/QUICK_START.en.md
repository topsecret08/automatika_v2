# Quick start (15 minutes)

From unpacking to the first run. Details are in
[USER_GUIDE.md](USER_GUIDE.md); PWA installation is in
[PWA_INSTALL.md](PWA_INSTALL.md).

> The Russian version of this guide is the reference. If the two differ, follow
> the Russian text. Button labels in the device interface are Russian by default
> («Старт», «Стоп», «Система → API-ключ»); the English words here are translations.

## 1. What you need

- An assembled Automatika device with firmware V0.9.5 beta.
- A phone or laptop with 2.4 GHz Wi-Fi.
- A grounded 220 V socket with an RCD (residual current device).
- Cooling water and liquid for distillation (at least 15 % of the still volume).

## 2. Connection

1. Connect the cooling (water in/out).
2. Fill the still with liquid, not below the minimum level.
3. Close the lid and check that E-STOP is **not** pressed.
4. Switch the power on. The screen should show the splash screen and then the main screen.

> Do not switch the heater on without a filled still and cooling water flowing.

## 3. Connecting to Wi-Fi

On the first start (or after a settings reset) the device opens an open access point
**`Automatika-Setup`**.

1. On your phone, open Wi-Fi and connect to `Automatika-Setup`.
2. Run the **ESP SoftAP Prov** app (Espressif, Android/iPhone) or
   `esp_prov.py` from ESP-IDF.
3. Device: `Automatika-Setup`, transport `softap`.
4. Enter the SSID and password of your **2.4 GHz** network.
5. Wait for the success message. The device restarts Wi-Fi and joins your network.

The access point has no password (provisioning with security level 0). Do not leave
the device in setup mode in a crowded place longer than necessary.

## 4. Open the web interface

1. In a browser open `http://automatika.local`
   (if mDNS does not work, use the device IP from your router's client list).
2. Go to **System → API key**.
3. Press **"Generate"**. The key is created and saved in this browser. This is a
   one-time operation: until a key exists, the device accepts the bootstrap; after
   that only the current key is accepted.
4. Go back to **Work → Dashboard**. The status should become `Online`.

> The firmware has no default key. If someone created a key before you, reset the
> settings (see SERVICE_MANUAL.md) and repeat.

## 5. Install the PWA (optional)

1. Open `https://automatika.local`.
2. Accept the self-signed certificate warning (or install `cert.pem` as a trusted
   certificate — [PWA_INSTALL.md](PWA_INSTALL.md)).
3. Chrome (Android): the **"Install"** button in the header or the menu →
   "Install app".
4. Safari (iPhone): "Share" → "Add to Home Screen".

After installation the app opens without an address bar, works as an offline shell,
and has shortcuts to "Dashboard", "Log" and "Settings".

## 6. First run of a process

1. Check the readings: the still and column should be close to room temperature, and
   the cooling flow should be above 0.5 l/min.
2. Set the power manually (for example 30–40 %) and check that the temperature rises.
3. For the automatic mode, set up a profile and PID (see
   [USER_GUIDE.md](USER_GUIDE.md), section "Profiles and PID").
4. Start: press **"Start"**, then choose the mode tile (rectification, distillation,
   beer, PVK, NBK, vacuum distillation). Stop: **"Stop"**.
5. Watch the log: the **"Log"** tab (needs the API key).

## 7. If something goes wrong

- Does not heat — [TROUBLESHOOTING.md](../service/TROUBLESHOOTING.md), §2.1.
- No Wi-Fi — §2.5.
- The PWA does not install — §2.6 and [PWA_INSTALL.md](PWA_INSTALL.md).
- Alarm / E-STOP — switch the power off at once, then see
  [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md).

## 8. What next

- [USER_GUIDE.md](USER_GUIDE.md) — daily work.
- [FAQ.md](FAQ.md) — frequent questions.
- [video_links.md](video_links.md) — video tutorial topics (no links yet).
- [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md) — maintenance.
