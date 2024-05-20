# ProyectoMP3
 Este proyecto implementa un reproductor de música MP3 utilizando un ESP32-S3 WROOM-1 y una tarjeta SD para almacenar los archivos de audio. El reproductor puede reproducir archivos MP3 almacenados en una carpeta llamada "playlist" en la tarjeta SD y ofrece funciones básicas como reproducir, pausar, avanzar a la siguiente canción y retroceder a la canción anterior.

## Componentes utilizados
1. Microcontrolador Arduino compatible
2. Tarjeta SD
3. Módulo de audio
4. Pantalla OLED (opcional, solo para propósitos de visualización)

## Librerías necesarias
- SD.h: Para el acceso a la tarjeta SD.
- Audio.h: Para la reproducción de audio.
- Adafruit_GFX.h: Para la manipulación de gráficos en la pantalla OLED (opcional).
- Adafruit_SSD1306.h: Para controlar la pantalla OLED (opcional).

## Funcionamiento básico
El programa busca los archivos MP3 en la carpeta "playlist" en la tarjeta SD al inicio.
Los botones físicos conectados al Arduino (play, next y previous) permiten controlar la reproducción de las canciones.
Cuando se presiona el botón de reproducción, el programa reproduce la siguiente canción en la lista.
Los botones de siguiente y anterior permiten avanzar y retroceder en la lista de reproducción, respectivamente.
Se utiliza una pantalla OLED opcional para mostrar información sobre la canción que se está reproduciendo.
