# ProyectoMP3
Aquest projecte implementa un reproductor de música MP3 utilitzant un ESP-WROOM-32 i una targeta SD per emmagatzemar els arxius d'àudio. El reproductor pot reproduir arxius MP3 emmagatzemats en una carpeta anomenada "playlist" a la targeta SD i ofereix funcions bàsiques com reproduir, pausar, avançar a la següent cançó i retrocedir a la cançó anterior.

## Componentes utilizados
1. Microcontrolador compatible amb Arduino
2. Targeta SD
3. Mòdul d'àudio
4. Pantalla OLED 

## Librerías necesarias
- SD.h: Per a l'accés a la targeta SD.
- Audio.h: Per a la reproducció d'àudio.
- Adafruit_GFX.h: Per a la manipulació de gràfics a la pantalla OLED (opcional).
- Adafruit_SSD1306.h: Per a controlar la pantalla OLED (opcional).

## Funcionament Bàsic
El programa busca els arxius MP3 a la carpeta "playlist" a la targeta SD al començament.
Els botons físics connectats a l'Arduino (play, next i previous) permeten controlar la reproducció de les cançons.
Quan es prem el botó de reproducció, el programa reprodueix la següent cançó de la llista.
Els botons de següent i anterior permeten avançar i retrocedir a la llista de reproducció, respectivament. Si una cançó s'esta reproduïnt, al avançar o retrocedir, inicia la cançó corresponent. 
S'utilitza una pantalla OLED per mostrar informació sobre la cançó que s'està reproduint.

## Problemes i limitacions en el projecte

-  __Limitacions en les biblioteques d'àudio__:  les biblioteques que requerim per la gestió d'arxius mp3 no tenen cap funció públic per gestionar funcions de temps. Per tant, el nostre MP3 no disposa de cap funció com pot ser la durada de la cançó, temps recorregut de la cançó ni pausar l'arxiu i reproduirlo en el mateix instant de temps.
- __ESP8266__: en un principi utilitzàvem el botó PREV en el GPIO 0, el problema amb aquesta biblioteca ha sigut que al reproduir un arxiu amb el botó Play, la biblioteca de ESP8266 utilitza el GPIO 0 per funcions especials d'arrencar, desconectant el botó del Pin, llavors deixava de funcionar completament. Al canviar de GPIO funciona correctament.
- __Programació dels botons__: la part de codi que més ens ha donat problemes ha sigut la implemntació dels botons, degut a que al principi no  estava ben implementat l'esquema per activació per flancs dels botons i els bloquejava degut a que canviava de valor constantment.
- __Llista d'arxius de la SD__: l'objecte File només disposa d'una funció anomenada OpenNextFile() que inseria els arxius saltant-se el primer que es troba  a playlist, havent-hi de implementar un sistema per ordenar els arxius.
- __Soldadura__: Per temes de temps, no esta soldada la placa amb els components.

## Funcionament per parts del codi

### Codi Complet
```cpp
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
```
__1. Inclusó de biblioteques__

```cpp
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
```
Aquestes línies inclouen les biblioteques necessàries per controlar la pantalla OLED, llegir la targeta SD, gestionar l'àudio i controlar els botons.

__2. Definició de pins digitals__

```cpp
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
```
Aquesta secció defineix els pins que es faran servir per a la comunicació SPI, I2S i I2C, així com els pins per als botons de control.

__3. Inicialització de la pantalla OLED__

```cpp
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
```
Aquí es defineixen les dimensions i l'adreça de la pantalla OLED i es crea un objecte per controlar-la.

__4. Declaració d'objectes i variables globals d'àudio__

```cpp
AudioFileSourceSD *audioFile;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *audioOutput;
const int Nfiles=15;
String playlist[Nfiles];
int fileCount = 0;
int currentIndex = 0;
bool isPlaying = false;
#define DEBOUNCE_DELAY 200
std::vector<String> filenames;
```
Es declaren les variables globals que s'utilitzaran per gestionar els arxius d'àudio i l'estat de reproducció.

__5. Estats dels botons__

```cpp
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
```
Es declaren variables per gestionar l'estat dels botons i evitar rebotaments.

__6. Declaració de funcions__

```cpp
void playMP3(const char *filename);
void listFiles();
void playNext();
void displaySongInfo(const char *filename);
void readButtons();
void displayCurrentSelection();
```
Aquí es declaren les funcions que es faran servir en el codi. Aquestes funcions seran definides més endavant.

__7. Configuració inicial (funció setup)__

```cpp
void setup() {
  Serial.begin(115200);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);

  // Inicialitzar la targeta SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicialitzar la targeta SD.");
    return;
  }
  Serial.println("Targeta SD inicialitzada correctament.");

  // Inicialitzar la comunicació I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("Comunicació I2C inicialitzada correctament.");

  // Inicialitzar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Error en la inicialització de la pantalla OLED."));
    for (;;);
  }
  Serial.println("Pantalla OLED inicialitzada correctament.");

  // Configurar pins per a l'àudio
  audioOutput = new AudioOutputI2S();
  audioOutput->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audioOutput->SetGain(0.125);
  Serial.println("Pins d'àudio configurats correctament.");

  // Configurar pins de botons
  pinMode(BUTTON_PLAY, INPUT_PULLUP);
  pinMode(BUTTON_PREV, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  Serial.println("Pins de botons configurats correctament.");

  // Llistar els arxius disponibles a la carpeta "playlist"
  listFiles();
  Serial.println("Arxius a la playlist llistats correctament.");

  // Mostrar la primera cançó a la llista
  displayCurrentSelection();
  Serial.println("Cançó inicial mostrada correctament.");
}
```
La funció setup s'executa una vegada quan el microcontrolador s'engega. Aquí s'inicialitzen la targeta SD, la comunicació I2C, la pantalla OLED, els pins per a l'àudio i els botons, es llisten els arxius de la playlist i es mostra la primera cançó.

__8. Bucle principal (funció loop)__

```cpp
void loop() {
  // Llegir l'estat dels botons
  readButtons();

  // Verificar si la reproducció està en curs
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      // La cançó actual ha acabat, reproduir la següent automàticament
      playNext();
      Serial.println("Cançó següent reproduïda automàticament.");
    }
  }
}

```
La funció loop s'executa repetidament. Aquí es llegeixen els botons i es controla la reproducció automàtica de la següent cançó si la cançó actual ha acabat.

__9. Reproduir arxiu MP3 (funció playMP3)__

```cpp
void playMP3(const char *filename) {
  if (isPlaying && mp3) {
    mp3->stop();
    isPlaying = false;
    Serial.println("Reproducció aturada.");
  }

  Serial.print("Reproduint arxiu MP3: ");
  Serial.println(filename);

  audioFile = new AudioFileSourceSD(filename);
  mp3 = new AudioGeneratorMP3();
  if (mp3->begin(audioFile, audioOutput)) {
    isPlaying = true;
    displaySongInfo(filename);
    Serial.println("Reproducció iniciada correctament.");
  } else {
    Serial.println("Error al iniciar la reproducció de l'arxiu MP3.");
    delete mp3;
    delete audioFile;
    mp3 = nullptr;
    audioFile = nullptr;
  }
}

```
Aquesta funció gestiona la reproducció d'un arxiu MP3. Atura la reproducció actual si n'hi ha una en curs, inicialitza la reproducció del nou arxiu i mostra la informació de la cançó.

__10. Mostrar informació de la cançó (funció displaySongInfo)__

```cpp
void displaySongInfo(const char *filename) {
  Serial.print("Mostrant informació per: ");
  Serial.println(filename);

  String fileStr = String(filename);
  fileStr.replace("/playlist/", ""); // Eliminar la ruta
  fileStr.replace(".mp3", ""); // Eliminar l'extensió .mp3

  int separatorIndex = fileStr.indexOf('_');
  String songName, artistName;

  if (separatorIndex != -1) {
    songName = fileStr.substring(0, separatorIndex);
    artistName = fileStr.substring(separatorIndex + 1);
  } else {
    songName = fileStr; // Si no hi ha un guionet baix, usar tot el nom del fitxer com a nom de la cançó
    artistName = "Desconegut"; // Artista desconegut
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
```
Mostra informació bàsica de la cançó a partir del nom del fitxer en una pantalla OLED, utilitzant operacions senzilles de manipulació de cadenes i control d'interfícies de visualització.

__11. Llistar arxius (funció listFiles)__

```cpp
void listFiles() {
  filenames.clear(); // Netejar vector de noms d'arxiu
  
  File dir = SD.open("/playlist");
  if (!dir) {
    Serial.println("Error al obrir la carpeta de la playlist.");
    return;
  }

  Serial.println("Arxius disponibles a la playlist:");
  fileCount = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    filenames.push_back(String("/playlist/") + entry.name());
  }
  dir.close();

  // Ordenar els noms d'arxiu segons sigui necessari
  std::sort(filenames.begin(), filenames.end());

  // Copiar els noms ordenats al array playlist
  fileCount = filenames.size();
  for (int i = 0; i < fileCount; ++i) {
    playlist[i] = filenames[i];
    Serial.println(playlist[i]);
  }
  
  Serial.println("Arxius ordenats i llistats correctament.");
}

```
Aquesta funció busca tots els arxius dins del directori "/playlist" de la targeta SD, els guarda en un vector, els ordena alfabèticament i després els copia a un array anomenat playlist. Cada pas està dissenyat per assegurar que els arxius són trobats, ordenats i processats correctament per a qualsevol operació posterior com la reproducció de fitxers dins d'una playlist.

__12. Reproduir la següent cançó (funció playNext)__

```cpp
void playNext() {
  if (fileCount > 0) {
    currentIndex = (currentIndex + 1) % fileCount;

    if (currentIndex < fileCount) {
      playMP3(playlist[currentIndex].c_str());
      Serial.print("Reproducció següent cançó: ");
      Serial.println(playlist[currentIndex]);
    } else {
      currentIndex = 0;
      Serial.println("Tornant a l'inici de la llista.");
      playMP3(playlist[currentIndex].c_str());
      Serial.print("Reproduint següent cançó: ");
      Serial.println(playlist[currentIndex]);
    }
  } else {
    Serial.println("No hi ha arxius a la llista.");
  }
}

```
playNext s'encarrega de gestionar la reproducció seqüencial d'arxius MP3 d'una llista de reproducció, amb control d'índexos per evitar errors i informació de debug utilitzant Serial per monitorar l'estat de la reproducció.

__13. Llegir els botons (funció readButtons)__

```cpp
void readButtons() {
  prevButtonStateRaw = digitalRead(BUTTON_PREV);
  playButtonStateRaw = digitalRead(BUTTON_PLAY);
  nextButtonStateRaw = digitalRead(BUTTON_NEXT);

  unsigned long currentTime = millis();

  if (prevButtonStateRaw != prevButtonLastState) {
    lastDebounceTimePrev = currentTime;
  }
  if ((currentTime - lastDebounceTimePrev) > DEBOUNCE_DELAY) {
    if (prevButtonStateRaw != prevButtonState) {
      prevButtonState = prevButtonStateRaw;
      if (prevButtonState == LOW) {
        currentIndex = (currentIndex - 1 + fileCount) % fileCount;
        Serial.println("Valor current index");
        Serial.println(currentIndex);
        if (isPlaying) {
          playMP3(playlist[currentIndex].c_str());
        } else {
          displayCurrentSelection();
        }
        Serial.println("Cançó anterior seleccionada correctament.");
      }
    }
  }
  prevButtonLastState = prevButtonStateRaw;

  if (playButtonStateRaw != playButtonLastState) {
    lastDebounceTimePlay = currentTime;
  }
  if ((currentTime - lastDebounceTimePlay) > DEBOUNCE_DELAY) {
    if (playButtonStateRaw != playButtonState) {
      playButtonState = playButtonStateRaw;
      if (playButtonState == LOW) {
        if (!isPlaying) {
          playMP3(playlist[currentIndex].c_str());
          Serial.println("Cançó reproduïda correctament.");
        } else {
          mp3->stop();
          displayCurrentSelection();
          Serial.println("Reproducció aturada.");
          isPlaying = false;
        }
      }
    }
  }
  playButtonLastState = playButtonStateRaw;

  if (nextButtonStateRaw != nextButtonLastState) {
    lastDebounceTimeNext = currentTime;
  }
  if ((currentTime - lastDebounceTimeNext) > DEBOUNCE_DELAY) {
    if (nextButtonStateRaw != nextButtonState) {
      nextButtonState = nextButtonRaw;
      if (nextButtonState == LOW) {
        currentIndex = (currentIndex + 1) % fileCount;
        Serial.println("Valor current index");
        Serial.println(currentIndex);
        if (isPlaying) {
          playMP3(playlist[currentIndex].c_str());
        } else {
          displayCurrentSelection();
        }
        Serial.println("Cançó següent seleccionada correctament.");
      }
    }
  }
  nextButtonLastState = nextButtonStateRaw;
}
```
- Lectura de botons: Llegeix l'estat actual dels botons connectats als pins digitals.

- Debouncing: Evita els rebots dels botons assegurant que només es detecti un canvi d'estat després d'un temps mínim (DEBOUNCE_DELAY).

- Botó de retrocedir (BUTTON_PREV): Canvia a la pista anterior i inicia la reproducció si està activa.

- Botó de reproduir (BUTTON_PLAY): Inicia o atura la reproducció de la pista actual.

- Botó d'avançar (BUTTON_NEXT): Canvia a la pista següent i inicia la reproducció si està activa.

- Actualització d'estats: Emmagatzema l'estat actual dels botons per a la següent iteració i utilitza comunicació sèrie per a la sortida de missatges d'estat.

__14. Mostrar la selecció actual (funció displayCurrentSelection)__

```cpp
void displayCurrentSelection(){
  if (fileCount > 0) {
    String fileStr = playlist[currentIndex];
    fileStr.replace("/playlist/", "");
    fileStr.replace(".mp3", "");

  int separatorIndex = fileStr.indexOf('_');
  String songName, artistName;

  if (separatorIndex != -1) {
    songName = fileStr.substring(0, separatorIndex);
    artistName = fileStr.substring(separatorIndex + 1);
  } else {
    songName = fileStr;
    artistName = "Desconegut";
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
```
- Verifica si hi ha fitxers a la llista de reproducció.
- Extreu el nom del fitxer actual, eliminant parts com "/playlist/" i ".mp3".
- Separa el nom de la cançó i l'artista utilitzant el caràcter '_' com a separador. Si no es troba aquest caràcter, assumeix que l'artista és "Desconegut".
- Configura la pantalla per mostrar el nom de la cançó i l'artista.
- Actualitza la pantalla per reflectir els canvis.
