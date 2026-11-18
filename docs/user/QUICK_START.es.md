# Inicio rápido (15 minutos)

De la caja al primer proceso. Guía completa: [USER_GUIDE.md](USER_GUIDE.md);
instalación de la PWA: [PWA_INSTALL.md](PWA_INSTALL.md).

## 1. Qué hace falta

- Dispositivo Automatika con firmware V0.9 beta.
- Teléfono u ordenador con Wi-Fi de 2,4 GHz.
- Toma de 220 V con protección diferencial y puesta a tierra.
- Agua de refrigeración y líquido a destilar (mínimo 15 % del volumen).

## 2. Conexión

1. Conecte la refrigeración (entrada y salida de agua).
2. Llene el cubo por encima del nivel mínimo.
3. Cierre la tapa y compruebe que **E-STOP no esté pulsado**.
4. Encienda: debe aparecer la pantalla principal.

> Nunca active el calentamiento con el cubo vacío o sin refrigeración.

## 3. Wi-Fi

En el primer arranque el equipo crea la red **`Automatika-Setup`**.

1. Conéctese a esa red desde el teléfono.
2. Abra **ESP SoftAP Prov** o `esp_prov.py` de ESP-IDF.
3. Introduzca el SSID y la contraseña de su red de 2,4 GHz.
4. Espere la confirmación y la reconexión.

La red de configuración no tiene contraseña: no la deje activa en lugares
públicos más de lo necesario.

## 4. Interfaz web

1. Abra `http://automatika.local` (o la IP del router).
2. Vaya a **Sistema → Clave API** y pulse **Generar**.
3. La clave se guarda en este navegador; sin ella el equipo solo permite
   la creación inicial.
4. Vuelva a **Trabajo → Panel**: el estado debe ser `Online`.

## 5. PWA (opcional)

1. Abra `https://automatika.local` y acepte el certificado autofirmado.
2. Chrome (Android): **Instalar** en la cabecera.
3. Safari (iPhone): **Compartir → Añadir a pantalla de inicio**.

## 6. Primer proceso

1. Compruebe que el cubo y la columna están a temperatura ambiente y que el
   caudal de refrigeración supera 0,5 l/min.
2. Ajuste la potencia manual (30–40 %) y confirme que la temperatura sube.
3. Para el modo automático configure perfil y PID.
4. El botón **Parar** corta el calentamiento; **Safety** restablece la alarma
   solo si el proceso está detenido, E-STOP liberado y el nivel es correcto.

> Seguridad: E-STOP, relé NC de seguridad y verificación activa del lazo
> complementan la instalación eléctrica, no la sustituyen.
