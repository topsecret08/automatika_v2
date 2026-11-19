# Schnellstart (15 Minuten)

Vom Auspacken bis zum ersten Start. Details stehen in
[USER_GUIDE.md](USER_GUIDE.md); die Installation der PWA in
[PWA_INSTALL.md](PWA_INSTALL.md).

> Maßgeblich ist die russische Fassung. Bei Abweichungen gilt der russische Text.
> Die Beschriftungen der Oberfläche sind standardmäßig russisch («Старт», «Стоп»,
> «Система → API-ключ»); die deutschen Begriffe hier sind Übersetzungen.

## 1. Was Sie brauchen

- Ein zusammengebautes Gerät Automatika mit Firmware V0.9.5 beta.
- Ein Telefon oder Laptop mit WLAN 2,4 GHz.
- Eine geerdete 220-V-Steckdose mit FI-Schutzschalter (RCD).
- Kühlwasser und Flüssigkeit zur Destillation (mindestens 15 % des Blasenvolumens).

## 2. Anschluss

1. Schließen Sie die Kühlung an (Wasser ein/aus).
2. Füllen Sie die Blase mit Flüssigkeit, nicht unter die Mindestfüllhöhe.
3. Schließen Sie den Deckel und prüfen Sie, dass der E-STOP **nicht** gedrückt ist.
4. Schalten Sie die Stromversorgung ein. Das Display zeigt den Startbildschirm und danach den Hauptbildschirm.

> Schalten Sie die Heizung nicht ohne gefüllte Blase und ohne Kühlwasserdurchfluss ein.

## 3. Verbindung mit dem WLAN

Beim ersten Start (oder nach einem Zurücksetzen) öffnet das Gerät einen offenen
Zugangspunkt **`Automatika-Setup`**.

1. Öffnen Sie auf dem Telefon das WLAN und verbinden Sie sich mit `Automatika-Setup`.
2. Starten Sie die App **ESP SoftAP Prov** (Espressif, Android/iPhone) oder
   `esp_prov.py` aus ESP-IDF.
3. Gerät: `Automatika-Setup`, Transport `softap`.
4. Geben Sie SSID und Passwort Ihres Netzes **2,4 GHz** ein.
5. Warten Sie auf die Erfolgsmeldung. Das Gerät startet das WLAN neu und verbindet sich mit Ihrem Netz.

Der Zugangspunkt hat kein Passwort (Provisioning mit Sicherheitsstufe 0). Lassen Sie
das Gerät nicht länger als nötig im Einrichtungsmodus an einem belebten Ort.

## 4. Weboberfläche öffnen

1. Öffnen Sie im Browser `http://automatika.local`
   (funktioniert mDNS nicht, verwenden Sie die IP-Adresse aus der Client-Liste des Routers).
2. Gehen Sie zu **System → API-Schlüssel**.
3. Drücken Sie **„Erzeugen“**. Der Schlüssel wird erstellt und in diesem Browser gespeichert.
   Das ist ein einmaliger Vorgang: Solange kein Schlüssel existiert, akzeptiert das
   Gerät den Bootstrap; danach nur noch den aktuellen Schlüssel.
4. Kehren Sie zu **Arbeit → Dashboard** zurück. Der Status sollte `Online` sein.

> Die Firmware enthält keinen Standardschlüssel. Hat jemand vor Ihnen einen Schlüssel
> erzeugt, setzen Sie die Einstellungen zurück (siehe SERVICE_MANUAL.md) und wiederholen Sie den Vorgang.

## 5. PWA installieren (optional)

1. Öffnen Sie `https://automatika.local`.
2. Bestätigen Sie die Warnung zum selbstsignierten Zertifikat (oder installieren Sie
   `cert.pem` als vertrauenswürdiges Zertifikat — [PWA_INSTALL.md](PWA_INSTALL.md)).
3. Chrome (Android): Schaltfläche **„Installieren“** in der Kopfzeile oder im Menü →
   „App installieren“.
4. Safari (iPhone): „Teilen“ → „Zum Home-Bildschirm“.

Nach der Installation öffnet sich die App ohne Adressleiste, arbeitet als Offline-Hülle
und hat Verknüpfungen zu „Dashboard“, „Protokoll“ und „Einstellungen“.

## 6. Erster Prozessstart

1. Prüfen Sie die Messwerte: Blase und Kolonne sollten nahe Raumtemperatur liegen,
   der Kühlwasserdurchfluss über 0,5 l/min.
2. Stellen Sie die Leistung manuell ein (z. B. 30–40 %) und prüfen Sie, dass die Temperatur steigt.
3. Für den Automatikbetrieb richten Sie ein Profil und PID ein (siehe
   [USER_GUIDE.md](USER_GUIDE.md), Abschnitt „Profile und PID“).
4. Start: Taste **„Start“**, dann die Kachel des Modus wählen (Rektifikation, Destillation,
   Bierbrauen, PVK, NBK, Vakuumdestillation). Stopp: **„Stopp“**.
5. Verfolgen Sie das Protokoll: Reiter **„Protokoll“** (benötigt den API-Schlüssel).

## 7. Wenn etwas nicht funktioniert

- Heizt nicht — [TROUBLESHOOTING.md](../service/TROUBLESHOOTING.md), §2.1.
- Kein WLAN — §2.5.
- Die PWA lässt sich nicht installieren — §2.6 und [PWA_INSTALL.md](PWA_INSTALL.md).
- Alarm / E-STOP — schalten Sie sofort die Stromversorgung ab, dann siehe
  [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md).

## 8. Und dann?

- [USER_GUIDE.md](USER_GUIDE.md) — täglicher Betrieb.
- [FAQ.md](FAQ.md) — häufige Fragen.
- [video_links.md](video_links.md) — Themen der Videoanleitungen (noch ohne Links).
- [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md) — Wartung.
