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

// Definir pines digitales utilizados
#define SD_CS         10 //pin fisico  9
#define SPI_MOSI      11 //pin fisico 10
#define SPI_MISO      13 //pin fisico 11
#define SPI_SCK       12 //pin fisico 12
#define I2S_DOUT      36 //pin fisico 37
#define I2S_BCLK      37 //pin fisico 38
#define I2S_LRC       38 //pin fisico 39
#define I2C_SDA       8  //pin fisico 18
#define I2C_SCL       9  //pin fisico 46
#define PLAY_BUTTON   5  //pin físico 4
#define NEXT_BUTTON   6  //pin físico 5
#define PREV_BUTTON   7  //pin físico 6

// Declarar objeto de pantalla
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Declaración de objetos de audio y variables globales
AudioFileSourceSD *audioFile;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *audioOutput;
String playlist[20]; // Array para almacenar nombres de archivos MP3
File dir; // Variable global para la carpeta de la playlist
File entry; // Variable para la canción que se va a reproducir
int fileCount = 0; // Contador de archivos en la playlist
int currentIndex = -1; // Índice del archivo actual
bool isPlaying = false; // Variable para verificar si se está reproduciendo una canción o no



// Declaración de funciones
void playMP3(const char *filename);
void listFiles();
void playNext();
void playPrevious();
void displaySongInfo(const char *filename);

void setup() {
  Serial.begin(115200);
  // Inicializar la tarjeta SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar la tarjeta SD.");
    return;
  }

  // Inicializar la comunicación I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  // Inicializar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Error en la inicialización de la pantalla OLED."));
    for (;;);
  }

  // Configurar pines para audio
  audioOutput = new AudioOutputI2S();
   
  i2s_pin_config_t i2sPins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_config_t i2sConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2sPins);

  // Configurar los pines de los botones como entrada con resistencia pull-up
  pinMode(PLAY_BUTTON, INPUT_PULLUP);
  pinMode(NEXT_BUTTON, INPUT_PULLUP);
  pinMode(PREV_BUTTON, INPUT_PULLUP);

  // Listar los archivos disponibles en la carpeta "playlist"
  listFiles();
}

void loop() {
  static bool playButtonState = HIGH; // Estado anterior del botón de reproducción
  static bool nextButtonState = HIGH; // Estado anterior del botón de siguiente
  static bool prevButtonState = HIGH; // Estado anterior del botón de anterior

  // Verificar el estado del botón de reproducción
  bool playButtonPressed = (digitalRead(PLAY_BUTTON) == LOW && playButtonState == HIGH);
  playButtonState = digitalRead(PLAY_BUTTON);

  // Verificar si se presiona el botón de reproducción
  if (playButtonPressed) {
    delay(200); // Debouncing
    if (!isPlaying && fileCount > 0) {
      currentIndex = (currentIndex + 1) % fileCount;
      playMP3(playlist[currentIndex].c_str());
    }
  }

  // Verificar si se presiona el botón de siguiente
  if (digitalRead(NEXT_BUTTON) == LOW) {
    delay(200); // Debouncing
    if (fileCount > 0 && !isPlaying) {
      playNext();
    }
  }

  // Verificar si se presiona el botón de anterior
  if (digitalRead(PREV_BUTTON) == LOW) {
    delay(200); // Debouncing
    if (fileCount > 0 && !isPlaying) {
      playPrevious();
    }
  }

  // Verificar si la reproducción está en curso
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      isPlaying = false;
      delete mp3;
      delete audioFile;
    }
  }
}


void playMP3(const char *filename) {
  audioFile = new AudioFileSourceSD(filename);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(audioFile, audioOutput);
  isPlaying = true;

  displaySongInfo(filename,true);
}

void displaySongInfo(const char *filename, bool isPlaying = false) {
  String fileStr = String(filename);
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
  
  if (isPlaying) {
    // Mostrar solo la canción que se está reproduciendo junto con el nombre del artista
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.println(songName);
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.println(artistName);
  } else {
    // Mostrar todos los nombres de las canciones en la lista
    display.setCursor(0, 0);
    display.println("Canciones:");
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println(songName);
  }
  
  display.display();
}

void listFiles() {
  dir = SD.open("/playlist");
  if (!dir) {
    Serial.println("Error al abrir la carpeta de la playlist.");
    return;
  }

  Serial.println("Archivos disponibles en la playlist:");
  fileCount = 0;
  while (true) {
    entry = dir.openNextFile();
    if (!entry) break;
    playlist[fileCount++] = entry.name();
    Serial.println(entry.name());
    entry.close();
    if (fileCount >= 20) break; // Limitar a 20 archivos para evitar desbordamiento
  }
  dir.close();
}

void playNext() {
  if (fileCount > 0) {
    currentIndex = (currentIndex + 1) % fileCount;
    playMP3(playlist[currentIndex].c_str());
  }
}

void playPrevious() {
  if (fileCount > 0) {
    currentIndex = (currentIndex - 1 + fileCount) % fileCount;
    playMP3(playlist[currentIndex].c_str());
  }
}
