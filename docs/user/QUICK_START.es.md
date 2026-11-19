# Inicio rápido (15 minutos)

Desde el desembalaje hasta el primer arranque. Los detalles están en
[USER_GUIDE.md](USER_GUIDE.md); la instalación de la PWA, en
[PWA_INSTALL.md](PWA_INSTALL.md).

> La versión en ruso es la de referencia. Si hay diferencias, sigue el texto ruso.
> Las etiquetas de la interfaz están en ruso por defecto («Старт», «Стоп»,
> «Система → API-ключ»); los términos en español son traducciones.

## 1. Qué necesitas

- Un dispositivo Automatika montado con firmware V0.9.6.
- Un teléfono u ordenador portátil con Wi-Fi de 2,4 GHz.
- Una toma de 220 V con toma de tierra y diferencial (RCD).
- Agua de refrigeración y líquido para destilar (al menos el 15 % del volumen del hervidor).

## 2. Conexión

1. Conecta la refrigeración (entrada/salida de agua).
2. Llena el hervidor con líquido, sin bajar del nivel mínimo.
3. Cierra la tapa y comprueba que el E-STOP **no** está pulsado.
4. Enciende la alimentación. La pantalla debe mostrar la pantalla de bienvenida y después la principal.

> No enciendas el calentamiento sin el hervidor lleno y sin agua de refrigeración circulando.

## 3. Conexión a la Wi-Fi

En el primer arranque (o tras restablecer la configuración) el dispositivo abre un
punto de acceso abierto **`Automatika-Setup`**.

1. En el teléfono, abre la Wi-Fi y conéctate a `Automatika-Setup`.
2. Abre la aplicación **ESP SoftAP Prov** (Espressif, Android/iPhone) o
   `esp_prov.py` de ESP-IDF.
3. Dispositivo: `Automatika-Setup`, transporte `softap`.
4. Introduce el SSID y la contraseña de tu red de **2,4 GHz**.
5. Espera el mensaje de éxito. El dispositivo reinicia la Wi-Fi y se une a tu red.

El punto de acceso no tiene contraseña (aprovisionamiento con seguridad nivel 0). No
dejes el dispositivo en modo de configuración en un lugar concurrido más tiempo del necesario.

## 4. Abrir la interfaz web

1. En el navegador abre `http://automatika.local`
   (si mDNS no funciona, usa la IP del dispositivo de la lista de clientes del router).
2. Ve a **Sistema → Clave API**.
3. Pulsa **«Generar»**. La clave se crea y se guarda en este navegador. Es una operación
   única: mientras no exista clave, el dispositivo acepta el bootstrap; después, solo la clave actual.
4. Vuelve a **Trabajo → Panel**. El estado debe cambiar a `Online`.

> El firmware no tiene clave por defecto. Si alguien creó una clave antes que tú,
> restablece la configuración (ver SERVICE_MANUAL.md) y repite el proceso.

## 5. Instalar la PWA (opcional)

1. Abre `https://automatika.local`.
2. Acepta la advertencia del certificado autofirmado (o instala `cert.pem` como
   certificado de confianza — [PWA_INSTALL.md](PWA_INSTALL.md)).
3. Chrome (Android): botón **«Instalar»** en la cabecera o en el menú →
   «Instalar aplicación».
4. Safari (iPhone): «Compartir» → «Añadir a pantalla de inicio».

Tras la instalación, la aplicación se abre sin barra de direcciones, funciona como
capa offline y tiene accesos directos a «Panel», «Registro» y «Ajustes».

## 6. Primer arranque de un proceso

1. Comprueba las lecturas: el hervidor y la columna deben estar cerca de la temperatura
   ambiente, y el caudal de refrigeración por encima de 0,5 l/min.
2. Ajusta la potencia manualmente (por ejemplo 30–40 %) y comprueba que la temperatura sube.
3. Para el modo automático, configura un perfil y el PID (ver
   [USER_GUIDE.md](USER_GUIDE.md), sección «Perfiles y PID»).
4. Inicio: pulsa **«Inicio»** y luego la tarjeta del modo (rectificación, destilación,
   cerveza, PVK, NBK, destilación al vacío). Parada: **«Parada»**.
5. Sigue el registro: pestaña **«Registro»** (necesita la clave API).

## 7. Si algo sale mal

- No calienta — [TROUBLESHOOTING.md](../service/TROUBLESHOOTING.md), §2.1.
- Sin Wi-Fi — §2.5.
- La PWA no se instala — §2.6 y [PWA_INSTALL.md](PWA_INSTALL.md).
- Alarma / E-STOP — corta la alimentación de inmediato y consulta
  [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md).

## 8. Qué sigue

- [USER_GUIDE.md](USER_GUIDE.md) — trabajo diario.
- [FAQ.md](FAQ.md) — preguntas frecuentes.
- [video_links.md](video_links.md) — temas de los vídeos tutoriales (sin enlaces todavía).
- [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md) — mantenimiento.
