#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>

#define D 0

// Pin definitions for SPI1 (ST7789 Display)
#define SPI1_MOSI 15
#define SPI1_MISO 12
#define SPI1_SCK  14
#define SPI1_CS   13
#define TFT_DC    4
#define TFT_RST   26

// Pin definitions for SPI0 (SD Card)
#define SPI0_MOSI 3
#define SPI0_MISO 0
#define SPI0_SCK  2
#define SPI0_CS   1

// Button pins for navigation and favorites
#define BUTTON_UP    29
#define BUTTON_DOWN  27
#define BUTTON_FAV   6

// Initialize display
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI1, SPI1_CS, TFT_DC, TFT_RST);

// File and text handling
int currentFileIndex = 0; // Current file index
String directoryPath = "/messages"; // Directory containing the .txt files
File currentFile; // Handle for the currently opened file
String textBuffer; // Buffer to hold file text

// Pagination variables
int totalPages = 0; // Total pages in the current file
int currentPage = 0; // Current page being displayed
unsigned long previousMillis = 0; // Timer for page switching
unsigned long pageInterval = 3000; // 3-second delay between pages

// Favorite management
String favoritesFile = "/favorites.txt";
int favoriteFileIndices[1000]; // Array to store favorite file indices
int totalFavorites = 0; // Keep track of how many favorites are marked

unsigned long lastFavPress = 0; // Variable to store the last time the button was pressed

int lastIndex = 0;  // Global variable to store the last file index

void setup() {
  if(D) Serial.begin(115200);
  delay(1000);
  if(D) Serial.println("Initializing...");

  // Initialize SPI1 for ST7789 display
  SPI1.setCS(SPI1_CS);
  SPI1.setSCK(SPI1_SCK);
  SPI1.setTX(SPI1_MOSI);
  SPI1.begin();

  tft.init(240, 320);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);

  // Initialize SPI0 for SD card
  SPI.setCS(SPI0_CS);
  SPI.setSCK(SPI0_SCK);
  SPI.setTX(SPI0_MOSI);
  SPI.setRX(SPI0_MISO);
  SPI.begin();

  if (!SD.begin(SPI0_CS)) {
    if(D) Serial.println("SD card initialization failed!");
    tft.println("SD card init failed!");
    while (1);
  }

  // Read favorites from the file at startup
  readFavorites();

  // Read lastIndex from info.txt
  readLastIndexFromFile();

  // Verify lastIndex is within valid range of message files
  // int totalFiles = countMessageFiles();
  // if (lastIndex != (totalFiles - 1)) {
  //   if(D) Serial.println("Error: lastIndex from info.txt doesn't match countMessageFiles() !");
  //   lastIndex = min(lastIndex, totalFiles);
  // }

  // Configure button pins for navigation and favorites
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_FAV, INPUT_PULLUP); // With pull-up resistor for button press

  displayFile(currentFileIndex);
}

void loop() {
  static int lastFileIndex = -1;

  // Handle up/down button presses for file navigation
  if (digitalRead(BUTTON_UP) == LOW) {
    currentFileIndex++;
    delay(200);  // Debounce delay
  }
  if (digitalRead(BUTTON_DOWN) == LOW) {
    currentFileIndex--;
    delay(200);  // Debounce delay
  }

  //wrapping
  if (currentFileIndex < 0) currentFileIndex = lastIndex;
  if (currentFileIndex > lastIndex) currentFileIndex = 0;

  // Display file when index changes
  if (currentFileIndex != lastFileIndex) {
    displayFile(currentFileIndex);
    lastFileIndex = currentFileIndex;
  }

  // Handle button press for marking/unmarking favorites
  if (digitalRead(BUTTON_FAV) == LOW) {
    toggleFavorite(currentFileIndex);
    delay(300);  // Simple debounce
  }

  // Handle automatic page switching
  if (totalPages > 1 && millis() - previousMillis > pageInterval) {
    currentPage++;
    if (currentPage >= totalPages) {
      currentPage = 0; // Loop back to the first page
    }
    displayPage(currentPage, textBuffer);
    previousMillis = millis();
  }

  // Handle button press for marking/unmarking favorites with software debouncing
  if (digitalRead(BUTTON_FAV) == LOW && (millis() - lastFavPress) > 300) {  // 300ms debounce time
    lastFavPress = millis();  // Update last button press time
    toggleFavorite(currentFileIndex);
    displayHeart(currentFileIndex);  // Refresh only the heart
  }
}

void displayFile(int fileIndex) {
  if (currentFile) {
    currentFile.close();
  }

  String fileName = directoryPath + "/" + String(fileIndex) + ".txt";
  if(D) Serial.print("Opening file: ");
  if(D) Serial.println(fileName);

  currentFile = SD.open(fileName);
  if (!currentFile) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 10);
    tft.println("File not found:");
    tft.println(fileName);
    if(D) Serial.println("File not found!");
    return;
  }

  // Read the first line to extract metadata
  String firstLine = currentFile.readStringUntil('\n');
  int firstSpace = firstLine.indexOf(' ');
  int secondSpace = firstLine.indexOf(' ', firstSpace + 1);
  int thirdSpace = firstLine.indexOf(' ', secondSpace + 1);

  int senderID = firstLine.substring(0, firstSpace).toInt();
  int pageIndex = firstLine.substring(firstSpace + 1, secondSpace).toInt();
  String date = firstLine.substring(secondSpace + 1, thirdSpace);
  String time = firstLine.substring(thirdSpace + 1);

  // Calculate total pages (pageIndex + 1)
  totalPages = pageIndex + 1;

  // Display sender dot and metadata
  tft.fillScreen(ST77XX_BLACK);

  int dotX = 10, dotY = 20, dotRadius = 10;
  int textOffsetX = 2;
  int textOffsetY = 3;

  if (senderID == 0) {
    tft.fillCircle(dotX, dotY, dotRadius, ST77XX_GREEN);
    tft.setCursor(dotX - textOffsetX, dotY - textOffsetY);
    tft.setTextColor(ST77XX_BLACK);
    tft.print("I");
  } else if (senderID == 1) {
    tft.fillCircle(dotX, dotY, dotRadius, ST77XX_BLUE);
    tft.setCursor(dotX - textOffsetX, dotY - textOffsetY);
    tft.setTextColor(ST77XX_BLACK);
    tft.print("G");
  }

  tft.setCursor(dotX + dotRadius + 10, dotY - 10); // Move time a bit higher
  tft.setTextColor(ST77XX_ORANGE);
  tft.print(date + " " + time);

  // Display the message number
  tft.setCursor(dotX + dotRadius + 10, dotY + 5); // Display number under time
  tft.setTextColor(ST77XX_ORANGE);
  tft.print(currentFileIndex + 1); // Display current file number (1-based index)
  tft.print("/");
  tft.print(lastIndex); // Display total file count


  // Read the remaining file content
  textBuffer = currentFile.readString();
  currentPage = 0;
  previousMillis = millis();

  if (totalPages == 1) {
    tft.setCursor(10, 40);
    tft.setTextColor(ST77XX_WHITE);
    tft.println(textBuffer);
  } else {
    displayPage(0, textBuffer);
  }

  displayHeart(currentFileIndex);

  currentFile.close();
}

void displayPage(int pageIndex, String content) {
  tft.fillRect(0, 40, tft.width(), tft.height() - 40, ST77XX_BLACK);  // Clear the text area

  const int charsPerLine = 50;  // Maximum characters per line
  const int linesPerPage = 25;  // Maximum lines per page
  const int startLine = pageIndex * linesPerPage; // Starting line for the current page

  int cursorX = 10, cursorY = 40;
  tft.setTextColor(ST77XX_WHITE);

  String currentLine = "";  // Buffer to hold the current line
  int lineCount = 0;        // Total lines printed so far
  int pageLineCount = 0;    // Lines displayed for the current page

  for (int i = 0; i < content.length(); i++) {
    char c = content[i];

    // Handle newline or max characters per line
    if (c == '\n' || currentLine.length() == charsPerLine) {
      // Count the line
      lineCount++;

      // Check if this line is within the range for the current page
      if (lineCount > startLine && pageLineCount < linesPerPage) {
        tft.setCursor(cursorX, cursorY);
        tft.print(currentLine);
        cursorY += 8;  // Move cursor down
        pageLineCount++;
      }

      currentLine = "";  // Clear the line buffer

      // Stop rendering if we've filled this page
      if (pageLineCount >= linesPerPage) break;

      continue;
    }

    currentLine += c;  // Add character to the current line

    // Handle end of content
    if (i == content.length() - 1) {
      lineCount++;

      if (lineCount > startLine && pageLineCount < linesPerPage) {
        tft.setCursor(cursorX, cursorY);
        tft.print(currentLine);
        pageLineCount++;
      }
    }
  }

  // Draw the pill for current page/total pages
  if (totalPages > 1) {
      int dotRadius = 10;           // Match the circle radius used earlier
      int pillHeight = 2 * dotRadius; // Pill height = diameter of the circle
      int pillPadding = 10;         // Padding around the text
      String pageText = String(currentPage + 1) + "/" + String(totalPages);

      // Calculate text width (6 pixels per character, approximately)
      int textWidth = pageText.length() * 6;

      // Calculate pill width dynamically based on text length + padding
      int pillWidth = textWidth + 2 * pillPadding;

      // Position the pill near the top-right
      int pillX = 200;              // X-coordinate for the pill
      int pillY = 10;               // Y-coordinate for the pill

      // Draw the yellow pill (rounded rectangle)
      tft.fillRoundRect(pillX, pillY, pillWidth, pillHeight, dotRadius, ST77XX_YELLOW);

      // Calculate text position for centering inside the pill
      int textX = pillX + (pillWidth - textWidth) / 2;
      int textY = pillY + (pillHeight - 8) / 2; // Center text vertically based on font height

      // Print the page number text inside the pill
      tft.setCursor(textX, textY);
      tft.setTextColor(ST77XX_BLACK);
      tft.print(pageText);
  }
}

// Check if a file index is marked as favorite
bool isFavorite(int fileIndex) {
  for (int i = 0; i < totalFavorites; i++) {
    if (favoriteFileIndices[i] == fileIndex) {
      return true;
    }
  }
  return false;
}

// Read favorite indices from the favorites file
void readFavorites() {
  File favFile = SD.open(favoritesFile);
  if (!favFile) {
    if(D) Serial.println("No favorites file found, starting fresh.");
    return;
  }

  // Read the favorite file indices into the array
  while (favFile.available()) {
    int favIndex = favFile.parseInt();
    if (favIndex >= 0) {
      favoriteFileIndices[totalFavorites++] = favIndex;
    }
  }

  favFile.close();
}

void toggleFavorite(int fileIndex) {
  bool isAlreadyFavorite = isFavorite(fileIndex);

  // If it is already a favorite, remove it
  if (isAlreadyFavorite) {
    for (int i = 0; i < totalFavorites; i++) {
      if (favoriteFileIndices[i] == fileIndex) {
        // Shift remaining favorites down
        for (int j = i; j < totalFavorites - 1; j++) {
          favoriteFileIndices[j] = favoriteFileIndices[j + 1];
        }
        totalFavorites--;
        break;
      }
    }
  } else {
    // If it is not a favorite, add it
    favoriteFileIndices[totalFavorites++] = fileIndex;
  }

  // Update the favorites file
  File favFile = SD.open(favoritesFile, FILE_WRITE);
  if (favFile) {
    favFile.truncate(0); // Clear the contents of the file

    // Write updated favorite indices to the file
    for (int i = 0; i < totalFavorites; i++) {
      favFile.println(favoriteFileIndices[i]);
    }

    favFile.close();
  }
}

void displayHeart(int fileIndex) {
  bool isFav = isFavorite(fileIndex);

  // Clear the corner where the heart will be drawn
  tft.fillRect(tft.width() - 30, 10, 20, 20, ST77XX_BLACK); // Clear previous heart

  // Draw the heart glyph (0x03) at the top-right corner
  tft.setCursor(tft.width() - 30, 10);
  tft.setTextColor(isFav ? ST77XX_RED : ST77XX_WHITE);
  tft.setTextSize(2);  // Adjust size for heart glyph
  tft.write(0x03);  // Print the heart symbol
  tft.setTextSize(1);  // Back to normal
}

// Function to read lastIndex from info.txt
void readLastIndexFromFile() {
  File infoFile = SD.open("/info.txt");

  if (infoFile) {
    lastIndex = infoFile.parseInt();  // Read the last file index from info.txt
    infoFile.close();
  } else {
    if(D) Serial.println("info.txt not found. Starting from file 0.");
    lastIndex = 0;  // Default to the first file if info.txt is not found
  }
}

int countMessageFiles() {
  int fileCount = 0;
  File dir = SD.open("/messages");

  if (dir) {
    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;  // No more files
      if (!entry.isDirectory() && entry.name()[0] != '.') {
        fileCount++;
      }
      entry.close();
    }
    dir.close();
  }

  return fileCount;
}


