// Control code for Arduino management of Bruker 2P setup for Team Specialk
// Jeremy Delahanty Mar. 2021
// Adapted from DISC_V7.ino by Kyle Fischer and Mauri van der Huevel Oct. 2019
// digitalWriteFast.h written by ???
// SerialTransfer.h written by PowerBroker2

//// PACKAGES ////
#include <Adafruit_MPR121.h> // Adafruit MPR121 capicitance board recording
#include <digitalWriteFast.h> // Speeds up communication for digital srite
#include <Wire.h> // Enhances comms with MPR121
#include <SerialTransfer.h> // Enables serial comms between Python config and Arduino
// rename SerialTransfer for ease of use
SerialTransfer myTransfer;

//// BITSHIFT OPERATIONS DEF: CAPACITANCE ////
#ifndef _BV
#define _BV(bit) (1 << (bit)) // capacitance detection using bitshift operations, need to learn about what these are - JD
#endif

//// PIN ASSIGNMENT: Stimuli and Solenoids ////
// input
const int lickPin = 2; // input from MPR121
//const int airPin = 3; // measure delay from solenoid to mouse
// output
const int solPin_air = 22; // solenoid for air puff control
const int vacPin = 24; // solenoid for vacuum control
const int solPin_liquid = 26; // solenoid for liquid control: sucrose, water, EtOH
const int speakerPin = 12; // speaker control pin


//// PIN ASSIGNMENT: NIDAQ ////
const int NIDAQ_READY = 9; // how do we do this with Bruker?
// NIDAQ output
const int airDeliveryPin = 23; // airpuff delivery
const int sucroseDeliveryPin = 27; // sucrose delivery
const int lickDetectPin = 29; // detect sucrose licks
const int speakerDeliveryPin = 31; // noise delivery

//// VARIABLE ASSIGNMENT ////
long ms; // is this for milliseconds?
// flags
boolean needVariables = true;
boolean newTrial = false;
boolean ITI = false;
boolean newUSDelivery = false;
boolean solenoidOn = false;
boolean vacOn = false;
boolean consume = false;
boolean cleanIt = false;
boolean sucrose = false;
boolean airpuff = false;
boolean noise = false;
boolean noiseDAQ = false;
boolean acquireTrials = true;


//// EXPERIMENT VARIABLES ////
const int totalNumberOfTrials = 20;
const int percentNegativeTrials = 50;
const int baseITI = 3000; // 3s inter-trial interval
const int noiseDuration = 2000;
const int USDeliveryTime_Sucrose = 5; // opens Sucrose solenoid for 50 ms, currently 5us b/c using water 3-30-21
const int USDeliveryTime_Air = 10; // opens airpuff solenoid for 10 ms
const int USConsumptionTime_Sucrose = 800; // wait 1s for animal to consume, currently 800ms b/c using water 3-30-21
const int minITIJitter = 0; // min inter-trial jitter
const int maxITIJitter = 0; // max inter-trial jitter

int BRUKER_VALUE = 0;

// stop times
long ITIend;
long rewardDelayMS;
long sucroseDelayMS;
long USDeliveryMS_Sucrose;
long sucroseConsumptionMS;
long vacTime;
long airDelayMS;
long USDeliveryMS_Air;
long USDeliveryMS;
long noiseDeliveryMS;
long noiseListeningMS;
long noiseDAQMS;

// trial variables (0 negative [air], 1 positive [sucrose])
int trialType;
int currentTrial = 0;

// lick variables
Adafruit_MPR121 cap = Adafruit_MPR121(); // renames MPR121 functions to cap? - JD
uint16_t currtouched = 0; // not sure why unsigned 16int used, why not long? - JD
uint16_t lasttouched = 0;

// vac variables
const int vacDelay = 500; // vacuum delay


// arrays
int trialTypeArray[totalNumberOfTrials];
int ITIArray[totalNumberOfTrials];
int trialArray[totalNumberOfTrials];
byte trialIndex = 0;


//// SETUP ////
void setup() {
  // -- DEFINE BITRATE -- //
  Serial.begin(9600);
  Serial1.begin(115200);
  myTransfer.begin(Serial1);
  
  // -- DEFINE PINS -- //
  // input
  pinMode(lickPin, INPUT);
  //output
  pinMode(solPin_air, OUTPUT);
  pinMode(solPin_liquid, OUTPUT);
  pinMode(vacPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);
  pinMode(speakerDeliveryPin, OUTPUT);

  // -- INITIALIZE TOUCH SENSOR -- //
  Serial.println("MPR121 CHECK...");
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  } // need to learn what value 0x5A represents - JD
  Serial.println("MPR121 FOUND");

  // -- INITIALIZE TRIAL TYPES -- //
  defineTrialTypes(totalNumberOfTrials, percentNegativeTrials);

  // -- POPULATE DELAY TIME ARRAYS -- //
  fillDelayArray(ITIArray, totalNumberOfTrials, baseITI, minITIJitter, maxITIJitter);

//  // -- WAIT FOR SIGNAL THAT DAQ IS ONLINE -- //
//  BRUKER_VALUE = digitalRead(NIDAQ_READY);
//  while (BRUKER_VALUE == LOW) {
//    BRUKER_VALUE = digitalRead(NIDAQ_READY);
//  } // what does == LOW mean? -JD

  newTrial = true;
  
}

//// THE BIZ ////
void loop() {
//  trials_rx();
  if (currentTrial < totalNumberOfTrials) {
    ms = millis();
    lickDetect();
    startITI(ms);
    tonePlayer(ms);
    toneDelivery(ms);
    onTone(ms);
    USDelivery(ms);
    onSolenoid(ms);
    consuming(ms);
    vacuum(ms);
  }
}

//// RECIEVE TRIALS FUNCTION ////
void trials_rx() {
  if (acquireTrials) {
    Serial.println("STARTING TRANSFER");
    if (myTransfer.available())
    { Serial.println("TRANSFERING...");
      uint16_t recSize = 0;
      recSize = myTransfer.rxObj(trialArray, recSize);
      myTransfer.sendData(myTransfer.bytesRead);
    }
    acquireTrials = false;
    newTrial = false;
  }
}

//// TRIAL FUNCTIONS ////
void lickDetect() {
  currtouched = cap.touched(); // Get currently touched contacts
  // if it is *currently* touched and *wasn't* touched before, alert!
  if ((currtouched & _BV(1)) && !(lasttouched & _BV(2))) {
    digitalWriteFast(lickDetectPin, HIGH);
  }
  // if it *was* touched and now *isn't*, alert!
  if (!(currtouched & _BV(1)) && (lasttouched & _BV(1))) {
    digitalWriteFast(lickDetectPin, LOW);
  }
  lasttouched = currtouched;
}

void startITI(long ms) {
  if (newTrial) {                                 // start new ITI
    Serial.print("staring trial ");
    Serial.println(currentTrial);
    trialType = trialTypeArray[currentTrial];     // assign trial type
    newTrial = false;
    ITI = true;
    int thisITI = ITIArray[currentTrial];         // get ITI for this trial
    ITIend = ms + thisITI;
    // turn off when done
  } else if (ITI && (ms >= ITIend)) {             // ITI is over
    ITI = false;
    newUSDelivery = true;
    noise = true;
    noiseDAQ = true;
  }
}

void tonePlayer(long ms) {
  if (noise) {
    Serial.println("Playing Tone");
    switch (trialType) {
      case 0:
        Serial.println("AIR");
        tone(speakerPin, 2000, noiseDuration);
        noise = false;
        noiseListeningMS = ms + noiseDuration;
        break;
      case 1:
        Serial.println("SUCROSE");
        tone(speakerPin, 9000, noiseDuration);
        noise = false;
        noiseListeningMS = ms + noiseDuration;
        break;
    }
  }
}

void toneDelivery(long ms) {
  if (noiseDAQ && (ms <= noiseListeningMS)) {
    digitalWriteFast(speakerDeliveryPin, HIGH);
    noiseDAQMS = ms + noiseDuration;
  }
}

void onTone(long ms) {
  if (noiseDAQ && (ms >= noiseListeningMS)){
    digitalWriteFast(speakerDeliveryPin, LOW);
    noiseDAQ = false;
    noise = false;
  }
}

void USDelivery(long ms) {
  if (newUSDelivery && (ms >= noiseListeningMS)) {
    Serial.println("Delivering US");
    Serial.println(trialType);
    switch (trialType) {
      case 0: 
        Serial.println("Delivering Airpuff");
        newUSDelivery = false;
        solenoidOn = true;
        USDeliveryMS = (ms + USDeliveryTime_Air);
        digitalWriteFast(solPin_air, HIGH);
        digitalWriteFast(airDeliveryPin, HIGH);
        break;
      case 1:
        Serial.println("Delivering Sucrose");
        newUSDelivery = false;
        solenoidOn = true;
        USDeliveryMS = (ms + USDeliveryTime_Sucrose);    
        digitalWriteFast(solPin_liquid, HIGH);
        digitalWriteFast(sucroseDeliveryPin, HIGH);
        break;
    }
  }
}

void onSolenoid(long ms) {
  if (solenoidOn && (ms >= USDeliveryMS)) {
    switch (trialType) {
      case 0:
        Serial.println("air solenoid off");
        solenoidOn = false;
        digitalWriteFast(solPin_air, LOW);
        digitalWriteFast(airDeliveryPin, LOW);
        newTrial = true;
        currentTrial++;
        break;
      case 1:
        Serial.println("liquid solenoid off");
        solenoidOn = false;
        consume = true;
        sucroseConsumptionMS = (ms + USConsumptionTime_Sucrose);
        digitalWriteFast(solPin_liquid, LOW);
        digitalWriteFast(sucroseDeliveryPin, LOW);
        break;
    }
  }
}

void consuming(long ms){
  if (consume && (ms >= sucroseConsumptionMS)) {         // move on after allowed to consume
    Serial.println("consuming...");
    consume = false;
    cleanIt = true;
  }
}

// Vacuum Control
void vacuum(long ms) {
  if (cleanIt) {
    Serial.println("cleaning...");
    cleanIt = false;
    vacOn = true;
    vacTime = ms + vacDelay;
    digitalWriteFast(vacPin, HIGH);
  }
  else if (vacOn && (ms >= vacTime)) {
    Serial.println("stop cleaning...");
    vacOn = false;
    digitalWriteFast(vacPin, LOW);
    newTrial = true;
    currentTrial++;
  }
}


////// ARRAY FUNCTIONS ////
void defineTrialTypes(int trialNumber, float percentNeg) { // TODO: generate random trial externally order and store on board?
  // initialize array with all positive (1) trials
  for (int i = 0; i < trialNumber; i++) {
    trialTypeArray[i] = 1;
  }
  if (percentNeg > 0) {
    randomSeed(analogRead(0));
    int negTrialNum = round(trialNumber * (percentNeg/100));
    // randomly choose negTrialNum indexes to make negative (0) trials
    int indexCount = 0;
    while (indexCount < negTrialNum) {
      int negIndex = random(trialNumber);
      // only make negative if it isn't already
      if (trialTypeArray[negIndex]) {
        trialTypeArray[negIndex] = 0;
        indexCount++;
      }
    }
  }
}

void fillDelayArray(int delayArray[], int trialNumber, int baseLength, int minJitter, int maxJitter) {
  randomSeed(analogRead(0));
  // initialize array with all positive (1) trials
  for (int i = 0; i < trialNumber; i++) {
    delayArray[i] = baseLength + random(minJitter, maxJitter);
  }
}