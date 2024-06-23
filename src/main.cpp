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
#include <driver/i2s.h>
#include <vector>

// Definir pines digitales utilizados
#define SD_CS         15
#define SPI_MOSI      12
#define SPI_MISO      13
#define SPI_SCK       14
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26
#define I2C_SDA       21
#define I2C_SCL       22
#define BUTTON_PLAY   4
#define BUTTON_PREV   16
#define BUTTON_NEXT   2

// Declarar objeto de pantalla
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Declaración de objetos de audio y variables globales
AudioFileSourceSD *audioFile;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *audioOutput;
const int Nfiles=15;  //Variable para número de archivos permitidos
String playlist[Nfiles]; // Array para almacenar nombres de archivos MP3
int fileCount = 0; // Contador de archivos en la playlist
int currentIndex = 0; // Índice del archivo actualmente seleccionado
bool isPlaying = false; // Variable para verificar si se está reproduciendo una canción o no
#define DEBOUNCE_DELAY 200 //Debounce botones
std::vector<String> filenames;

// Estados de los botones
bool prevButtonStateRaw = false;
bool playButtonStateRaw = false;
bool nextButtonStateRaw = false;

bool prevButtonState = false;
bool playButtonState = false;
bool nextButtonState = false;

bool prevButtonLastState = false;
bool playButtonLastState = false;
bool nextButtonLastState = false;

unsigned long lastDebounceTimePrev = 0;
unsigned long lastDebounceTimePlay = 0;
unsigned long lastDebounceTimeNext = 0;

// Declaración de funciones
void playMP3(const char *filename);
void listFiles();
void playNext();
void displaySongInfo(const char *filename);
void readButtons();
void displayCurrentSelection();

void setup() {
  Serial.begin(115200);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);

  // Inicializar la tarjeta SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar la tarjeta SD.");
    return;
  }
  Serial.println("Tarjeta SD inicializada correctamente.");

  // Inicializar la comunicación I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("Comunicación I2C inicializada correctamente.");

  // Inicializar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Error en la inicialización de la pantalla OLED."));
    for (;;);
  }
  Serial.println("Pantalla OLED inicializada correctamente.");

  // Configurar pines para audio
  audioOutput = new AudioOutputI2S();
  audioOutput->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioOutput->SetGain(0.125);
  Serial.println("Pines de audio configurados correctamente.");

  // Configurar pines de botones
  pinMode(BUTTON_PLAY, INPUT_PULLUP);
  pinMode(BUTTON_PREV, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  Serial.println("Pines de botones configurados correctamente.");

  // Listar los archivos disponibles en la carpeta "playlist"
  listFiles();
  Serial.println("Archivos en la playlist listados correctamente.");

  // Mostrar la primera canción en la lista
  displayCurrentSelection();
  Serial.println("Canción inicial mostrada correctamente.");
}

void loop() {
  // Leer el estado de los botones
  readButtons();

  // Verificar si la reproducción está en curso
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      // La canción actual ha terminado, reproducir la siguiente automáticamente
      playNext();
      Serial.println("Canción siguiente reproducida automáticamente.");
    }
  }
}


void playMP3(const char *filename) {
  if (isPlaying && mp3) {
    mp3->stop();
    isPlaying = false;
    Serial.println("Reproducción detenida.");
  }

  Serial.print("Reproduciendo archivo MP3: ");
  Serial.println(filename);

  audioFile = new AudioFileSourceSD(filename);
  mp3 = new AudioGeneratorMP3();
  if (mp3->begin(audioFile, audioOutput)) {
    isPlaying = true;
    displaySongInfo(filename);
    Serial.println("Reproducción iniciada correctamente.");
  } else {
    Serial.println("Error al iniciar la reproducción del archivo MP3.");
    delete mp3;
    delete audioFile;
    mp3 = nullptr;
    audioFile = nullptr;
  }
}

void displaySongInfo(const char *filename) {
  Serial.print("Mostrando información para: ");
  Serial.println(filename);

  String fileStr = String(filename);
  fileStr.replace("/playlist/", ""); // Eliminar la ruta
  fileStr.replace(".mp3", ""); // Eliminar la extensión .mp3

  int separatorIndex = fileStr.indexOf('_');
  String songName, artistName;

  if (separatorIndex != -1) {
    songName = fileStr.substring(0, separatorIndex);
    artistName = fileStr.substring(separatorIndex + 1);
  } else {
    songName = fileStr; // Si no hay un guion bajo, usar todo el nombre del archivo como nombre de la canción
    artistName = "Desconocido"; // Artista desconocido
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(songName);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(artistName);

  display.display();
  Serial.println("Display updated.");
}

void listFiles() {
  filenames.clear(); // Limpiar vector de nombres de archivo
  
  File dir = SD.open("/playlist");
  if (!dir) {
    Serial.println("Error al abrir la carpeta de la playlist.");
    return;
  }

  Serial.println("Archivos disponibles en la playlist:");
  fileCount = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    filenames.push_back(String("/playlist/") + entry.name());
  }
  dir.close();

  // Ordenar los nombres de archivo según sea necesario
  // Aquí puedes implementar tu lógica de ordenamiento personalizada
  // Por ejemplo, ordenar alfabéticamente o por fecha de modificación
  // En este ejemplo, se ordena alfabéticamente:
  std::sort(filenames.begin(), filenames.end());

  // Copiar los nombres ordenados al array playlist
  fileCount = filenames.size();
  for (int i = 0; i < fileCount; ++i) {
    playlist[i] = filenames[i];
    Serial.println(playlist[i]);
  }
  
  Serial.println("Archivos ordenados y listados correctamente.");
}

void playNext() {
  if (fileCount > 0) {
    currentIndex = (currentIndex + 1) % fileCount;

    // Verificar si currentIndex sigue siendo válido
    if (currentIndex < fileCount) {
      playMP3(playlist[currentIndex].c_str());
      Serial.print("Reproducción siguiente canción: ");
      Serial.println(playlist[currentIndex]);
    } 
    else {
      // currentIndex está fuera de los límites de la lista, volver al primer archivo
      currentIndex = 0;
      Serial.println("Volviendo al inicio de la lista.");
      playMP3(playlist[currentIndex].c_str());
      Serial.print("Reproduciendo siguiente canción: ");
      Serial.println(playlist[currentIndex]);
    } 
  }
  else {
    Serial.println("No hay archivos en la lista.");
    // Aquí podrías mostrar un mensaje en la pantalla OLED
    } 
  
}

void readButtons() {
  // Leer estado actual de los botones
  prevButtonStateRaw = digitalRead(BUTTON_PREV);
  playButtonStateRaw = digitalRead(BUTTON_PLAY);
  nextButtonStateRaw = digitalRead(BUTTON_NEXT);

  // Obtener el tiempo actual
  unsigned long currentTime = millis();

  // Botón PREV
  if (prevButtonStateRaw != prevButtonLastState) {
    lastDebounceTimePrev = currentTime;
  }
  if ((currentTime - lastDebounceTimePrev) > DEBOUNCE_DELAY) {
    if (prevButtonStateRaw != prevButtonState) {
      prevButtonState = prevButtonStateRaw;
      if (prevButtonState == LOW) {
        // Acción al detectar flanco descendente
        currentIndex = (currentIndex - 1 + fileCount) % fileCount;
        Serial.println("Valor current index");
        Serial.println(currentIndex);
        if (isPlaying) {
          playMP3(playlist[currentIndex].c_str());
        } else {
          displayCurrentSelection();
        }
        Serial.println("Canción anterior seleccionada correctamente.");
      }
    }
  }
  prevButtonLastState = prevButtonStateRaw;

  // Botón PLAY
  if (playButtonStateRaw != playButtonLastState) {
    lastDebounceTimePlay = currentTime;
  }
  if ((currentTime - lastDebounceTimePlay) > DEBOUNCE_DELAY) {
    if (playButtonStateRaw != playButtonState) {
      playButtonState = playButtonStateRaw;
      if (playButtonState == LOW) {
        // Acción al detectar flanco descendente
        if (!isPlaying) {
          playMP3(playlist[currentIndex].c_str());
          Serial.println("Canción reproducida correctamente.");
        } else {
          // Pausar o detener la reproducción si está en curso
          // Aquí puedes implementar una lógica adicional si se desea.
          mp3->stop();
          displayCurrentSelection(); // Actualizar la pantalla
          Serial.println("Reproducción detenida.");
          isPlaying = false;
        }
        }
      }
    }
  playButtonLastState = playButtonStateRaw;


  // Botón NEXT
  if (nextButtonStateRaw != nextButtonLastState) {
    lastDebounceTimeNext = currentTime;
  }
  if ((currentTime - lastDebounceTimeNext) > DEBOUNCE_DELAY) {
    if (nextButtonStateRaw != nextButtonState) {
      nextButtonState = nextButtonStateRaw;
      if (nextButtonState == LOW) {
        // Acción al detectar flanco descendente
        currentIndex = (currentIndex + 1) % fileCount;
        Serial.println("Valor current index");
        Serial.println(currentIndex);
        if (isPlaying) {
          playMP3(playlist[currentIndex].c_str());
        } else {
          displayCurrentSelection();
        }
        Serial.println("Canción siguiente seleccionada correctamente.");
      }
    }
  }
  nextButtonLastState = nextButtonStateRaw;
}



void displayCurrentSelection(){
  if (fileCount > 0) {
    String fileStr = playlist[currentIndex];
    fileStr.replace("/playlist/", ""); // Eliminar la ruta
    fileStr.replace(".mp3", ""); // Eliminar la extensión .mp3

  int separatorIndex = fileStr.indexOf('_');
  String songName, artistName;

  if (separatorIndex != -1) {
    songName = fileStr.substring(0, separatorIndex);
    artistName = fileStr.substring(separatorIndex + 1);
  } else {
    songName = fileStr; // Si no hay un guion bajo, usar todo el nombre del archivo como nombre de la canción
    artistName = "Desconocido"; // Artista desconocido
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(songName);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(artistName);

  display.display();
  }
}
