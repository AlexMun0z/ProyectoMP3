#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <Audio.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceSD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>  

// Definir pines digitales utilizados
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26
#define PLAY_BUTTON   14
#define NEXT_BUTTON   15
#define PREV_BUTTON   16

// Declarar objeto de pantalla
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Declaración de objetos de audio y variables globales
AudioFileSourceSD *audioFile;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *audioOutput;
File dir; // Variable global para la carpeta de la playlist
File entry; // Variable para la canción que se va a reproducir
bool isPlaying = false; // Variable para verificar si se esta reproduciendo una canción o no
uint32_t lastPosition = 0; // Variable para almacenar la última posición de reproducción

// Declaración de funciones
void playMP3(const char *filename);
void listFiles();

void setup() {
  Serial.begin(115200);

  // Inicializar la tarjeta SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar la tarjeta SD.");
    return;
  }

  // Inicializar la pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Configurar pines para audio
  audioOutput = new AudioOutputI2S(I2S_BCLK, I2S_LRC, I2S_DOUT);

  // Configurar los pines de los botones como entrada con resistencia pull-up
  pinMode(PLAY_BUTTON, INPUT);
  pinMode(NEXT_BUTTON, INPUT);
  pinMode(PREV_BUTTON, INPUT);

  // Listar los archivos disponibles en la carpeta "playlist"
  listFiles();
}

void loop() {
  // Verificar si se presiona el botón de reproducción
  if (digitalRead(PLAY_BUTTON) == HIGH) {
    // Si no se está reproduciendo nada, reproducir la siguiente canción en la lista
    if (!isPlaying) {
      dir = SD.open("/playlist"); // Abrir la carpeta "playlist"
      if (!dir) {
        Serial.println("Error al abrir la carpeta de la playlist.");
        return;
      }
      entry = dir.openNextFile(); // Abrir el siguiente archivo en la lista
      if (!entry) {
        Serial.println("No hay archivos en la lista de reproducción.");
        dir.close();
        return;
      }
      playMP3(entry.name()); // Reproducir el archivo MP3
      entry.close(); // Cerrar el archivo
      dir.close(); // Cerrar la carpeta
    }
  }

  // Verificar si se presiona el botón de siguiente
  if (digitalRead(NEXT_BUTTON) == HIGH) {
    // Si no se está reproduciendo nada, pasar a la siguiente canción en la lista
    if (!isPlaying) {
      dir = SD.open("/playlist"); // Abrir la carpeta "playlist"
      if (!dir) {
        Serial.println("Error al abrir la carpeta de la playlist.");
      }
      entry = dir.openNextFile(); // Abrir el siguiente archivo en la lista
      if (!entry) {
        // Si no hay más archivos, volver al principio de la lista
        dir.rewindDirectory();
        entry = dir.openNextFile(); // Abrir el primer archivo en la lista
      }
      if (entry) {
        playMP3(entry.name()); // Reproducir el archivo MP3
        entry.close(); // Cerrar el archivo
      }
      dir.close(); // Cerrar la carpeta
    }
  }

  // Verificar si se presiona el botón de anterior
  if (digitalRead(PREV_BUTTON) == HIGH) {
    // Si no se está reproduciendo nada, retroceder a la canción anterior en la lista
    if (!isPlaying) {
      dir = SD.open("/playlist"); // Abrir la carpeta "playlist"
      if (!dir) {
        Serial.println("Error al abrir la carpeta de la playlist.");
        return;
      }
      File prevEntry;
      while (true) {
        entry = dir.openNextFile(); // Abrir el siguiente archivo en la lista
        if (!entry) {
          // Si no hay más archivos, volver al principio de la lista
          dir.rewindDirectory();
          entry = dir.openNextFile(); // Abrir el primer archivo en la lista
        }
        if (entry && entry != prevEntry) {
          prevEntry = entry;
        } else {
          break; // Salir del bucle si se encontró la canción anterior
        }
      }
      if (prevEntry) {
        playMP3(prevEntry.name()); // Reproducir el archivo MP3
        prevEntry.close(); // Cerrar el archivo
      }
      dir.close(); // Cerrar la carpeta
    }
  }
}

// Función para reproducir un archivo MP3
void playMP3(const char *filename) {
  // Abrir el archivo MP3
  audioFile = new AudioFileSourceSD(filename);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(audioFile, audioOutput);
  isPlaying = true; // Marcar como reproducción en curso

  // Obtener el nombre del artista del nombre del archivo MP3
  String artistName = ""; // Variable para almacenar el nombre del artista
  int artistIndex = String(filename).lastIndexOf('_'); // Buscar el índice del último guión bajo
  if (artistIndex != -1) {
    artistName = String(filename).substring(artistIndex + 1); // Extraer el nombre del artista del nombre del archivo
    artistName.replace(".mp3", ""); // Eliminar la extensión ".mp3" del nombre del artista
  }
  
  // Actualizar la pantalla con el nombre del archivo MP3 y el nombre del artista
  display.clearDisplay();
  display.setTextSize(1);      
  display.setTextColor(SSD1306_WHITE); 
  display.setCursor(0,0);
  display.println("Reproduciendo:");
  display.setTextSize(2);      
  display.setCursor(0,10);
  display.println(filename);
  display.setCursor(0,30); // Ajustar la posición del cursor para mostrar el nombre del artista
  display.println(artistName);
  display.display();

  // Reproducir el archivo MP3
  while (mp3->isRunning()) {
    if (!isPlaying) {
      break;
    }
    if (mp3->loop()) {
      break; // Salir del bucle si la reproducción termina
    }
  }

  // Finalizar la reproducción y liberar recursos
  delete mp3;
  delete audioFile;
}

// Función para listar los archivos disponibles en la carpeta de la lista de reproducción
void listFiles() {
  dir = SD.open("/playlist"); // Abrir la carpeta "playlist"
  if (!dir) {
    Serial.println("Error al abrir la carpeta de la playlist.");
    return;
  }

  Serial.println("Archivos disponibles en la playlist:");
  while (true) {
    entry = dir.openNextFile(); // Abrir el siguiente archivo en la lista
    if (!entry) {
      break; // Salir del bucle si no hay más archivos
    }
    Serial.println(entry.name()); // Imprimir el nombre del archivo
    entry.close(); // Cerrar el archivo
  }
  dir.close(); // Cerrar la carpeta

  // Abrir y seleccionar el primer archivo en la lista de reproducción
  dir = SD.open("/playlist");
  if (dir) {
    entry = dir.openNextFile(); // Abrir el primer archivo en la lista
    if (entry) {
      Serial.println("Archivo seleccionado: " + String(entry.name()));
      entry.close(); // Cerrar el archivo
    }
    dir.close(); // Cerrar la carpeta
  }
}
