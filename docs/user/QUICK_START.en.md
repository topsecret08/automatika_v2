# Quick start (15 minutes)

From unpacking to the first run. Full guide: [USER_GUIDE.md](USER_GUIDE.md);
PWA installation: [PWA_INSTALL.md](PWA_INSTALL.md).

## 1. What you need

- Automatika device with firmware V0.8 beta.
- Phone or laptop on 2.4 GHz Wi-Fi.
- 220 V outlet with earth leakage protection and grounding.
- Cooling water and at least 15 % of the cube volume filled.

## 2. Hook-up

1. Connect cooling water inlet and outlet.
2. Fill the cube above the minimum level.
3. Close the lid and make sure **E-STOP is not pressed**.
4. Power on: the main screen should appear.

> Never heat an empty cube or run without cooling.

## 3. Wi-Fi

On first boot the device starts the **`Automatika-Setup`** access point.

1. Join it from your phone.
2. Open **ESP SoftAP Prov** or `esp_prov.py` from ESP-IDF.
3. Enter your 2.4 GHz SSID and password.
4. Wait for confirmation and reconnection.

The setup AP has no password: do not leave it active in public places longer
than needed.

## 4. Web UI

1. Open `http://automatika.local` (or the IP shown by your router).
2. Go to **System → API key** and press **Generate**.
3. The key is stored in this browser; until it exists the device allows only
   the initial bootstrap.
4. Return to **Work → Dashboard**; the status must show `Online`.

## 5. PWA (optional)

1. Open `https://automatika.local` and accept the self-signed certificate.
2. Chrome (Android): **Install** in the header.
3. Safari (iPhone): **Share → Add to Home Screen**.

## 6. First process

1. Confirm the cube and column are at room temperature and cooling flow is
   above 0.5 l/min.
2. Set manual power (30–40 %) and check that temperature rises.
3. For automatic mode configure a profile and PID.
4. **Stop** cuts heating; **Safety** only resets when the process is stopped,
   E-STOP is released and the level is valid.

> Safety: the E-STOP, the NC safety relay and active loop verification
> complement the electrical installation; they do not replace it.
