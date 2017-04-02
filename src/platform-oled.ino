#include <Adafruit_SSD1306.h>
#include <TimeLib.h>

#define SSD1306_128_64

#define GREEN 6  // output pin for GREEN departure signal
#define RED   9    // output pin for RED departure signal

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define TEXT_ROW_1 0  // yellow
#define TEXT_ROW_1B 8 // yellow
#define TEXT_ROW_2 16
#define TEXT_ROW_3 24
#define TEXT_ROW_4 32
#define TEXT_ROW_5 40

const char* STATION = "S-Bahn Marienplatz";
const char* ARRIVING = "Now Arriving";
const char* BOARDING = "-- BOARDING --";


// train lineup, every 15 minutes, westbound end station (eastbound end stations in comments)

// names shortened due to memory constraints
// need to figure out how to avoid strings,
// and better memory management in general
//
// WESTBOUND - PLATFORM 1
//
const byte PLATFORM = 1;
const char* trainNames[] = {
  "S7 Wolfratshausen",
  "S6 Tutzing",
  "S5 Herrsching",
  "S4 Leuchtenbergring",
  "S2 Petershausen"
};
// make sure data starts with smalles time first,
// otherwise strange things can happen on initialization
const byte trainTimes[5][4] = {
  { 2, 17, 32, 47 },  // S7
  { 5, 20, 35, 50 },  // S6
  { 8, 23, 38, 53 },  // S5
  { 11, 26, 41, 56 }, // S4
  { 14, 29, 44, 59 }  // S2
};

//
// EASTBOUND - PLATFORM 2
//
/*
const byte PLATFORM = 2;
const char* trainNames[] = {
  "S7 Kreuzstrasse",
  "S6 Erding",
  "S5 Ebersberg",
  "S4 Geltendorf",
  "S2 Holzkirchen"
};
// make sure data starts with smalles time first,
// otherwise strange things can happen on initialization
const byte trainTimes[5][4] = {
  { 0, 15, 30, 45 },  // S2
  { 3, 18, 33, 48 },  // S7
  { 6, 21, 36, 51 },  // S6
  { 9, 24, 39, 54 },  // S5
  { 12, 27, 42, 57 }, // S4
};
*/

struct Train {
  byte index;
  byte timeIndex;
};

Train arriving[3];  // current train lineup
bool inStation = false;
long timeDeparted = -1;
byte boardingBlinkCounter = 0;
byte scrollStep = 0;
void setup() {

  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);

  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  // set system time, until we can get serialization sync working
  setTime(10, 58, 40, 15, 11, 1985);

  initializeTrains();
}

void loop() {

  display.clearDisplay();

  // todo: blink every 500ms
  if (boardingBlinkCounter > 6) {
    boardingBlinkCounter = 0;
  }

  if (scrollStep > 5) {
    scrollStep = 0;
  }

  // default test size and color
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // print station name and time
  displayHeader();

  byte mm = minute();  // current time in minutes
  byte ss = second();  // current time in seconds
  Train t = arriving[0];  // current train

  if (trainTimes[t.index][t.timeIndex] == mm && ss < 15) { // train in station
    displayNowBoarding();
    inStation = true;
  } else if (ss >= 45 && trainTimes[t.index][t.timeIndex] == mm + 1) { // now arriving
    displayNowArriving();
  } else {  // no train
    if (inStation) { // check if train has just left, we need to update
      updateTimetable();
      inStation = false;
      timeDeparted = now();
    }
    displayTimetable();
  }

  if (now() - timeDeparted > 15) {
    // reset departure light
    timeDeparted = -1;
  }

  // todo: look into analogRead(PIN) to determine if a reed switch has been triggered
  // https://www.arduino.cc/en/Reference/AnalogRead
  // interesting stuff!

  if (timeDeparted > -1) { // train just left station
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, HIGH);
  } else {
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, LOW);
  }

  display.display();

  boardingBlinkCounter++;
  scrollStep++;

  delay(200);
}

void displayHeader(void) {
  printStation();
  printPlatform();
  printTime();
}

void printStation() {
  display.setCursor(0, TEXT_ROW_1);
  pad(1); display.println(STATION);
}

void printPlatform() {
  char line[10];
  sprintf(line, "PLATFORM %d", PLATFORM);
  pad(5); display.println(line);
}

void printTime() {
  char fmtTime[14];
  sprintf(fmtTime, "TIME: %2d:%02d:%02d", hour(), minute(), second());
  pad(3); display.println(fmtTime);
}

void displayTimetable() {
  display.setCursor(0, TEXT_ROW_4);
  for (byte i = 0; i < 3; i++) {
    printArrivingIn(i);
  }
}

void printArrivingIn(byte timetableIndex) {
  Train t = arriving[timetableIndex];
  printTrainName(t.index, 14);
  pad(1);
  if (strlen(trainNames[t.index]) < 14) {
    pad(14 - strlen(trainNames[t.index]));
  }
  printArrivingInMinutes(t.index, t.timeIndex);
  display.println();
}

void printArrivingInMinutes(byte trainIndex, byte timeIndex) {
  byte arrives = trainTimes[trainIndex][timeIndex];
  byte mm = minute();
  byte min = arrives - mm;
  if (min > 60) {
    min += 60;
  } else if (min < 0) {
    min += 60;
  }

  char line[5];
  sprintf(line, "%2dmin", min);
  display.print(line);
}

void pad(byte padLen) {
  printChar(' ', padLen);
}

void printChar(char c, byte times) {
  if (times > 0) {
    for (byte i = 0; i < times; i++) {
      display.print(c);
    }
  }
}

const char LT = '<';
const char GT = '>';
void displayNowArriving() {
  //if (boardingBlinkCounter < 3) {
    display.setCursor(0, TEXT_ROW_3);

  /*  changes with scrollStep value [0-5]
  0        NOW ARRIVING
  1    >   NOW ARRIVING   <
  2    >>  NOW ARRIVING  <<
  3    >>> NOW ARRIVING <<<
  4     >> NOW ARRIVING <<
  5      > NOW ARRIVING <

   */
   display.println();
  switch(scrollStep) {
    case 0: pad(4); break;
    case 1: printChar(GT, 1); pad(3); break;
    case 2: printChar(GT, 2); pad(2); break;
    case 3: printChar(GT, 3); pad(1); break;
    case 4: pad(1); printChar(GT, 2); pad(1); break;
    case 5: pad(2); printChar(GT, 1); pad(1); break;
  }

  display.print(ARRIVING);

  switch(scrollStep) {
    //case 0: break;
    case 1: pad(3); printChar(LT, 1); break;
    case 2: pad(2); printChar(LT, 2); break;
    case 3: pad(1); printChar(LT, 3); break;
    case 4: pad(1); printChar(LT, 2); break;
    case 5: pad(1); printChar(LT, 1); break;
  }
  display.println();

  display.setTextSize(2);
  printTrainName(arriving[0].index, 10);
  display.println();
  display.setTextSize(1);
}


void printTrainName(byte index, byte maxLen) {
  display.print(String(trainNames[index]).substring(0,maxLen));
}

void displayNowBoarding() {
  display.println();
  if (boardingBlinkCounter < 3) {
    display.setCursor(0, TEXT_ROW_3);
    pad(3);
    display.println(BOARDING);
  } else {
    display.setCursor(0, TEXT_ROW_4);
  }

  display.setTextSize(2);
  printTrainName(arriving[0].index, 10);
  display.println();
  display.setTextSize(1);

  // show number of seconds until departs. Assuming 15 sedconsd station time for now
  char line[16];
  sprintf(line, "Departs in %2d", (15 - second()));
  pad(3); display.println(line);

}

byte findNextTrainTime(byte currentMinute) {
  for (byte tt = 0; tt < 4; tt++) {
    for (byte ti = 0; ti < 5; ti++) {
      if (trainTimes[ti][tt] >= currentMinute) {
        return trainTimes[ti][tt];
      }
    }
  }
  return trainTimes[0][0];
}

byte findTimeIndex(byte minute) {
  for (byte tt = 0; tt < 4; tt++) {
    for (byte ti = 0; ti < 5; ti++) {
      if (trainTimes[ti][tt] == minute) {
        return tt;
      }
    }
  }
  return 0;
}

byte findTrainIndex(byte minute) {
  for (byte tt = 0; tt < 4; tt++) {
    for (byte ti = 0; ti < 5; ti++) {
      if (trainTimes[ti][tt] == minute) {
        return ti;
      }
    }
  }
  return 0;
}

// update current trains by removing first, and adding new to end
void updateTimetable() {

  byte tIndex = arriving[2].index;
  byte mIndex = arriving[2].timeIndex;

  // pop first item
  for (byte i = 1; i < 3; i++) {
    arriving[i - 1] = arriving[i];
  }

  tIndex++;
  if (tIndex > 4) {
    tIndex = 0;
    mIndex++;
    // I had this at 4, resulted in bizzzar buffer overrun
    if (mIndex > 3) {
      mIndex = 0;
    }
  }

  arriving[2].index = tIndex;
  arriving[2].timeIndex = mIndex;

}

// given system time, find next 3 upcoming trainsm in order
void initializeTrains() {
  // find current time, then find next 3 trains
  byte mm = minute();
  byte ss = second();
  if (ss > 15) {
    mm++;
  }

  byte trainTime = findNextTrainTime(mm);
  byte mIndex = findTimeIndex(trainTime);
  byte tIndex = findTrainIndex(trainTime);
  byte lastTime = trainTimes[tIndex][mIndex];

  for (byte i = 0; i < 3; i++) {
    if (tIndex > 4) { // if we get to last train, start over
      tIndex = 0;
      mIndex++;
      // I had this at 4, resulted in bizzzar buffer overrun
      if (mIndex > 3) {
        mIndex = 0;
      }
    }

    arriving[i].index = tIndex;
    arriving[i].timeIndex = mIndex;
    tIndex++;
  }
}