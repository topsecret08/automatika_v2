# Schnellstart (15 Minuten)

Vom Auspacken bis zum ersten Prozess. Ausführlich: [USER_GUIDE.md](USER_GUIDE.md);
PWA-Installation: [PWA_INSTALL.md](PWA_INSTALL.md).

## 1. Voraussetzungen

- Automatika mit Firmware V0.8 beta.
- Telefon oder Notebook mit 2,4-GHz-WLAN.
- 220-V-Steckdose mit FI-Schutzschalter und Erdung.
- Kühlwasser und Flüssigkeit zum Destillieren (mindestens 15 % des Volumens).

## 2. Anschluss

1. Kühlung anschließen (Wasserein- und -ausgang).
2. Kessel über die Mindestfüllmenge befüllen.
3. Deckel schließen und prüfen, dass **NOT-AUS nicht gedrückt** ist.
4. Einschalten: Der Hauptbildschirm muss erscheinen.

> Nie ohne Füllung oder ohne Kühlung heizen.

## 3. WLAN

Beim ersten Start öffnet das Gerät das Netz **`Automatika-Setup`**.

1. Mit dem Telefon mit diesem Netz verbinden.
2. **ESP SoftAP Prov** oder `esp_prov.py` aus ESP-IDF starten.
3. SSID und Passwort des 2,4-GHz-Netzes eingeben.
4. Bestätigung und erneute Verbindung abwarten.

Das Einrichtungsnetz ist offen: nicht unnötig lange an öffentlichen Orten
aktiv lassen.

## 4. Weboberfläche

1. `http://automatika.local` öffnen (oder IP-Adresse aus dem Router).
2. **System → API-Schlüssel** öffnen und **Erzeugen** drücken.
3. Der Schlüssel wird in diesem Browser gespeichert; ohne ihn ist nur die
   erstmalige Erzeugung erlaubt.
4. Zurück zu **Arbeit → Dashboard**: Status muss `Online` sein.

## 5. PWA (optional)

1. `https://automatika.local` öffnen und das selbstsignierte Zertifikat
   akzeptieren.
2. Chrome (Android): **Installieren** in der Kopfzeile.
3. Safari (iPhone): **Teilen → Zum Home-Bildschirm**.

## 6. Erster Prozess

1. Kessel und Kolonne müssen Umgebungstemperatur haben, Kühlwasserdurchfluss
   über 0,5 l/min liegen.
2. Manuelle Leistung einstellen (30–40 %) und Temperaturanstieg prüfen.
3. Für den Automatikbetrieb Profil und PID konfigurieren.
4. **Stopp** schaltet die Heizung ab; **Safety** quittiert nur, wenn Prozess
   gestoppt, NOT-AUS gelöst und der Füllstand in Ordnung ist.

> Sicherheit: NOT-AUS, NC-Sicherheitsrelais und aktive Kreisprüfung ergänzen
> die Elektroinstallation, sie ersetzen sie nicht.
