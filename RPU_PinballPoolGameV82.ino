/**************************************************************************
    Pinball Pool Game is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    See <https://www.gnu.org/licenses/>.
 
*/

#include "RPU_Config.h"
#include "RPU.h"
#include "PinballPoolGame.h"
#include "SelfTestAndAudit.h"
#include <EEPROM.h>


#define VERSION_NUMBER    82
#define DEBUG_MESSAGES     1
#define COIN_DOOR_TELEMETRY // If uncommented, coin door settings are sent to monitor on boot
#define IN_GAME_TELEMETRY   // If uncommented, sends game status to monitor
#define EXECUTION_MESSAGES  // If uncommented, sends game logic telemetry to monitor

#define USE_SCORE_OVERRIDES
#define USE_SCORE_ACHIEVEMENTS

// MachineState
//  0 - Attract Mode
//  negative - self-test modes
//  positive - game play
int MachineState = 0;
boolean MachineStateChanged = true;
#define MACHINE_STATE_ATTRACT         0
#define MACHINE_STATE_INIT_GAMEPLAY   1
#define MACHINE_STATE_INIT_NEW_BALL   2
#define MACHINE_STATE_UNVALIDATED     3
#define MACHINE_STATE_NORMAL_GAMEPLAY 4
#define MACHINE_STATE_COUNTDOWN_BONUS 90
#define MACHINE_STATE_MATCH_MODE      95
#define MACHINE_STATE_BALL_OVER       100
#define MACHINE_STATE_GAME_OVER       110

//
// Adjustment machine states
//
//                                                            Display value                                  
#define MACHINE_STATE_ADJUST_FREEPLAY                   -17   //  12
#define MACHINE_STATE_ADJUST_BALL_SAVE                  -18   //  13
#define MACHINE_STATE_ADJUST_CHASEBALL_DURATION         -19   //  14
#define MACHINE_STATE_ADJUST_SPINNER_COMBO_DURATION     -20   //  15
#define MACHINE_STATE_ADJUST_SPINNER_THRESHOLD          -21   //  16
#define MACHINE_STATE_ADJUST_POP_THRESHOLD              -22   //  17
#define MACHINE_STATE_ADJUST_NEXT_BALL_DURATION         -23   //  18
#define MACHINE_STATE_ADJUST_TILT_WARNING               -24   //  19
#define MACHINE_STATE_ADJUST_BALLS_OVERRIDE             -25   //  20
#define MACHINE_STATE_ADJUST_DONE                       -26   //  21

//
//  EEPROM Save Locations
//

#define EEPROM_FREE_PLAY_BYTE                           100
#define EEPROM_BALL_SAVE_BYTE                           101
#define EEPROM_CHASEBALL_DURATION_BYTE                  102
#define EEPROM_SPINNER_COMBO_DURATION_BYTE              103
#define EEPROM_SPINNER_THRESHOLD_BYTE                   104
#define EEPROM_POP_THRESHOLD_BYTE                       105
#define EEPROM_NEXT_BALL_DURATION_BYTE                  106
#define EEPROM_TILT_WARNING_BYTE                        107
#define EEPROM_BALLS_OVERRIDE_BYTE                      108

#define TIME_TO_WAIT_FOR_BALL         100

#define TILT_WARNING_DEBOUNCE_TIME    1500

//
// Sound Effects
//

#define SOUND_EFFECT_NONE               0
#define SOUND_EFFECT_ADD_PLAYER         1
#define SOUND_EFFECT_BALL_OVER          2
#define SOUND_EFFECT_GAME_OVER          3
#define SOUND_EFFECT_MACHINE_START      4
#define SOUND_EFFECT_ADD_CREDIT         5
#define SOUND_EFFECT_PLAYER_UP          6
#define SOUND_EFFECT_GAME_START         7
#define SOUND_EFFECT_EXTRA_BALL         8
#define SOUND_EFFECT_5K_CHIME           9
#define SOUND_EFFECT_BALL_CAPTURE      10
#define SOUND_EFFECT_POP_MODE          11
#define SOUND_EFFECT_POP_100           12
#define SOUND_EFFECT_POP_1000          13
#define SOUND_EFFECT_POP_1000b         14
#define SOUND_EFFECT_POP_1000c         15
#define SOUND_EFFECT_TILT_WARNING      16
#define SOUND_EFFECT_TILTED            17
#define SOUND_EFFECT_10_PTS            18
#define SOUND_EFFECT_100_PTS           19
#define SOUND_EFFECT_1000_PTS          20
#define SOUND_EFFECT_EXTRA             21
#define SOUND_EFFECT_SPINNER_COMBO     22
#define SOUND_EFFECT_KICKER_OUTLANE    23
#define SOUND_EFFECT_8_BALL_CAPTURE    24
#define SOUND_EFFECT_BALL_LOSS         25

//
// Game/machine global variables
//

unsigned long HighScore = 0;
unsigned long AwardScores[3];           // Score thresholds for awards
int Credits = 0;
int MaximumCredits = 20;
boolean FreePlayMode = false;
boolean MatchFeature = true;            //  Allows Match Feature to run

#define MAX_TILT_WARNINGS_MAX    2
#define MAX_TILT_WARNINGS_DEF    1      // Length of each segment
byte MaxTiltWarnings = MAX_TILT_WARNINGS_DEF;
byte NumTiltWarnings = 0;
unsigned long LastTiltWarningTime = 0;
boolean Tilted = false;

byte CurrentPlayer = 0;
byte CurrentBallInPlay = 1;
byte CurrentNumPlayers = 0;
unsigned long CurrentScores[4];
boolean SamePlayerShootsAgain = false;
byte CurrentAchievements[4];            // Score achievments

unsigned long CurrentTime = 0;
unsigned long BallTimeInTrough = 0;
unsigned long BallFirstSwitchHitTime = 0;

boolean BallSaveUsed = false;
#define BALLSAVENUMSECONDS_MAX   20
#define BALLSAVENUMSECONDS_DEF    0
byte BallSaveNumSeconds = BALLSAVENUMSECONDS_DEF;
#define BALLSPERGAME_MAX    5
#define BALLSPERGAME_DEF    3
#define BALLSPERGAME_MIN    3
byte BallsPerGame = BALLSPERGAME_DEF;

boolean HighScoreReplay = true;

byte Ten_Pts_Stack = 0;
byte Hundred_Pts_Stack = 0;
byte Thousand_Pts_Stack = 0;
int Silent_Thousand_Pts_Stack = 0;
byte Silent_Hundred_Pts_Stack = 0;
unsigned long ChimeScoringDelay = 0;

// Animation variables
boolean MarqueeDisabled = false;

// Attract mode variables

// byte AttractHeadMode = 255;
byte AttractPlayfieldMode = 255;
unsigned long AttractSweepTime = 0;
byte AttractSweepLights = 1;
int SpiralIncrement = 0;
unsigned long RackDelayLength = 850;

unsigned long AttractStepTime = 0;
byte AttractStepLights = 0;

// Display variables

int CreditsDispValue = 0;       //  Value to be displayed in Credit window
byte CreditsFlashing = false;   //  Allow credits display to flash if true
int BIPDispValue = 0;           //  Value to be displayed in Ball In Play window
byte BIPFlashing = false;       //  Allow BIP display to flash if true

#define OVERRIDE_PRIORITY_NEXTBALL       10
#define OVERRIDE_PRIORITY_SPINNERCOMBO   20
#define OVERRIDE_PRIORITY_CHASEBALLMODE3 30
byte OverrideScorePriority = 0; //  Priority of score overrides

//
// Global game logic variables
//
byte GameMode[4] = {1,1,1,1};                     // GameMode=1 normal play, GameMode=2 15 ball mode
byte FifteenBallCounter[4] = {0,0,0,0};           // Track how long in 15-Ball mode
unsigned long CheckBallTime = 0;                  // Debug timing variable
boolean FifteenBallQualified[4];                  // Set to true when all goals are met

byte ChaseBall = 0;                               // Game Mode 3 target ball
byte ChaseBallStage = 0;                          // Counter of Ball captured in Game Mode 3
unsigned long ChaseBallTimeStart = 0;             // Timer for ChaseBall
#define CHASEBALL_DURATION_MAX    31
#define CHASEBALL_DURATION_DEF    25              // Length of each segment
#define CHASEBALL_DURATION_MIN    19
byte ChaseBallDuration = CHASEBALL_DURATION_DEF;  // Length of each segment (seconds)


byte BankShotProgress = 0;                        // Track what Bank Shot step we are on
boolean SuperBonusLit  = false;                   // True if we have achieved rack of 8 balls
boolean EightBallTest[4] = {true,true,true,true}; // One time check per player.
unsigned long BankShotOffTime = 0;
unsigned long MarqueeOffTime = 0;
byte MarqueeMultiple = 0;

boolean SpinnerKickerLit  = false;          // Triggers spinner light and scoring, sets kicker on
unsigned long KickerOffTime = 0;            //  Delayed off time for Kicker
boolean KickerReady = false;
boolean KickerUsed = true;
unsigned int SpinnerCount[4] = {0,0,0,0};                 // Spinner counter
byte SpinnerMode[4] = {0,0,0,0};                          // Spinner mode threshold
unsigned long SuperSpinnerTime = 0;                       // Start of SuperSpinner mode
unsigned long SuperSpinnerDuration = 0;                   // Length of SuperSpinner mode once triggered
#define SPINNER_COMBO_DURATION_MAX   16
#define SPINNER_COMBO_DURATION_DEF   12                   // Default for EEPROM settings - sec
#define SPINNER_COMBO_DURATION_MIN    8
byte Spinner_Combo_Duration = SPINNER_COMBO_DURATION_DEF; // Duration in seconds

boolean SuperSpinnerAllowed[4] = {false, false, false, false};        // Locks out SuperSpinner mode
boolean SpinnerComboHalf = false;               // True if BankShot half of combo is hit
unsigned int SpinnerDelta = 0;
#define SPINNER_THRESHOLD_MAX    70
#define SPINNER_THRESHOLD_DEF    50             // Default for EEPROM settings - spins
#define SPINNER_THRESHOLD_MIN    30
byte Spinner_Threshold = SPINNER_THRESHOLD_DEF; // Spinner hits to achieve mode - 50


unsigned int PopCount[4] = {0,0,0,0};       // Pop bumper counter
byte PopMode[4] = {0,0,0,0};                // Pop bumper scoring threshold
unsigned int PopDelta = 0;
#define POP_THRESHOLD_MAX        50
#define POP_THRESHOLD_DEF        40         // Default for EEPROM settings - hits
#define POP_THRESHOLD_MIN        30
byte Pop_Threshold = POP_THRESHOLD_DEF;     // Pop bumper hits to achieve mode - 40

boolean ArrowsLit[4] = {false,false,false,false}; // True when lanes 1-4 are collected
boolean ArrowTest = true;                   // One time check flag
boolean RightArrow = false;                 // Always start with Right arrow

unsigned int Balls[4]={0,0,0,0};            // Ball status
byte Goals[4]={0,0,0,0};
boolean GoalsDisplayToggle;                 // Display achieved goals if true

boolean OutlaneSpecial[4] = {false,false,false,false};  // True when Balls 1 thru 7 are collected, then 8 ball collected
byte BonusMult = 1;                         // Bonus multiplier, defaults to 1X
boolean BonusMultTenX = false;              // Set when 10X achieved

#define NEXT_BALL_DURATION_MAX  19
#define NEXT_BALL_DURATION_DEF  15              // Default for EEPROM settings - sec
#define NEXT_BALL_DURATION_MIN  11
byte NextBallDuration = NEXT_BALL_DURATION_DEF; // Length in msec next ball award is available

unsigned long NextBallTime = 0;
byte NextBall=0;                            // Keeps track of ball after current one being captured

byte MatchDigit = 0;                        // Relocated to make global

//
// Function prototypes
//

// Default is Speed=100, CW=true
void MarqueeAttract(byte Segment, int Speed=100, boolean CW=true);
void BankShotScoring(byte Sound=1);
void FlashingArrows(int lamparray[], int rate, int numlamps=6);

void setup() {
  if (DEBUG_MESSAGES) {
    Serial.begin(115200);
  }

  // Tell the OS about game-specific lights and switches
  RPU_SetupGameSwitches(NUM_SWITCHES_WITH_TRIGGERS, NUM_PRIORITY_SWITCHES_WITH_TRIGGERS, TriggeredSwitches);

  if (DEBUG_MESSAGES) {
    Serial.write("Attempting to initialize the MPU\n");
  }
 
  // Set up the chips and interrupts
  RPU_InitializeMPU();
  RPU_DisableSolenoidStack();
  RPU_SetDisableFlippers(true);

  // Read parameters from EEProm
  ReadStoredParameters();
  RPU_SetCoinLockout((Credits >= MaximumCredits) ? true : false);

  byte dipBank = RPU_GetDipSwitches(0);

  // Use dip switches to set up game variables
  if (DEBUG_MESSAGES) {
    char buf[32];
    sprintf(buf, "DipBank 0 = 0x%02X\n", dipBank);
    Serial.write(buf);
  }
/*
  HighScore = RPU_ReadULFromEEProm(RPU_HIGHSCORE_EEPROM_START_BYTE, 10000);
  AwardScores[0] = RPU_ReadULFromEEProm(RPU_AWARD_SCORE_1_EEPROM_START_BYTE);
  AwardScores[1] = RPU_ReadULFromEEProm(RPU_AWARD_SCORE_2_EEPROM_START_BYTE);
  AwardScores[2] = RPU_ReadULFromEEProm(RPU_AWARD_SCORE_3_EEPROM_START_BYTE);
*/
  
  Credits = RPU_ReadByteFromEEProm(RPU_CREDITS_EEPROM_BYTE);
  if (Credits>MaximumCredits) Credits = MaximumCredits;

  //BallsPerGame = 3;

// Play Machine start tune - Have to set CurrentTime as we are not yet in the loop structure

  CurrentTime = millis();
  PlaySoundEffect(SOUND_EFFECT_MACHINE_START);

// Display SW Version

  CurrentNumPlayers = 1;
  CurrentScores[0] = VERSION_NUMBER;
  //CurrentScores[0] = 1234567;
    
  if (DEBUG_MESSAGES) {
    Serial.write("Done with setup\n");
  }

}

void ReadStoredParameters() {
  HighScore = RPU_ReadULFromEEProm(RPU_HIGHSCORE_EEPROM_START_BYTE, 10000);
  Credits = RPU_ReadByteFromEEProm(RPU_CREDITS_EEPROM_BYTE);
  if (Credits > MaximumCredits) Credits = MaximumCredits;

  ReadSetting(EEPROM_FREE_PLAY_BYTE, 0);
  FreePlayMode = (EEPROM.read(EEPROM_FREE_PLAY_BYTE)) ? true : false;

  BallSaveNumSeconds = ReadSetting(EEPROM_BALL_SAVE_BYTE, BALLSAVENUMSECONDS_DEF);
  if (BallSaveNumSeconds == 99) {                                         //  If set to 99
    BallSaveNumSeconds = BALLSAVENUMSECONDS_DEF;                          //  Set to default
    EEPROM.write(EEPROM_BALL_SAVE_BYTE, BALLSAVENUMSECONDS_DEF);          //  Write to EEPROM
  }
  if (BallSaveNumSeconds > BALLSAVENUMSECONDS_MAX) BallSaveNumSeconds = BALLSAVENUMSECONDS_MAX;


  MaxTiltWarnings = ReadSetting(EEPROM_TILT_WARNING_BYTE, MAX_TILT_WARNINGS_DEF);
  if (MaxTiltWarnings == 99) {                                            //  If set to 99
    MaxTiltWarnings = MAX_TILT_WARNINGS_DEF;                              //  Set to default
    EEPROM.write(EEPROM_TILT_WARNING_BYTE, MAX_TILT_WARNINGS_DEF);        //  Write to EEPROM
  }
  if (MaxTiltWarnings > MAX_TILT_WARNINGS_MAX) MaxTiltWarnings = MAX_TILT_WARNINGS_MAX;

  BallsPerGame = ReadSetting(EEPROM_BALLS_OVERRIDE_BYTE, BALLSPERGAME_DEF);
  if (BallsPerGame == 99) {                                               //  If set to 99
    BallsPerGame = BALLSPERGAME_DEF;                                      //  Set to default
    EEPROM.write(EEPROM_BALLS_OVERRIDE_BYTE, BALLSPERGAME_DEF);           //  Write to EEPROM
  }
  if (BallsPerGame > BALLSPERGAME_MAX) BallsPerGame = BALLSPERGAME_MAX;
  if (BallsPerGame < BALLSPERGAME_MIN) BallsPerGame = BALLSPERGAME_MIN;
  
  ChaseBallDuration = ReadSetting(EEPROM_CHASEBALL_DURATION_BYTE, CHASEBALL_DURATION_DEF);
  if (ChaseBallDuration == 99) {                                               //  If set to 99
    ChaseBallDuration = CHASEBALL_DURATION_DEF;                                //  Set to default
    EEPROM.write(EEPROM_CHASEBALL_DURATION_BYTE, CHASEBALL_DURATION_DEF);      //  Write to EEPROM
  }
  if (ChaseBallDuration > CHASEBALL_DURATION_MAX) ChaseBallDuration = CHASEBALL_DURATION_MAX;
  if (ChaseBallDuration < CHASEBALL_DURATION_MIN) ChaseBallDuration = CHASEBALL_DURATION_MIN;
  
  Spinner_Combo_Duration = ReadSetting(EEPROM_SPINNER_COMBO_DURATION_BYTE, SPINNER_COMBO_DURATION_DEF);
  if (Spinner_Combo_Duration == 99) {                                          //  If set to 99
    Spinner_Combo_Duration = SPINNER_COMBO_DURATION_DEF;                       //  Set to default
    EEPROM.write(EEPROM_SPINNER_COMBO_DURATION_BYTE, SPINNER_COMBO_DURATION_DEF);  //  Write to EEPROM
  }
  if (Spinner_Combo_Duration > SPINNER_COMBO_DURATION_MAX) Spinner_Combo_Duration = SPINNER_COMBO_DURATION_MAX;
  if (Spinner_Combo_Duration < SPINNER_COMBO_DURATION_MIN) Spinner_Combo_Duration = SPINNER_COMBO_DURATION_MIN;
  
  Spinner_Threshold = ReadSetting(EEPROM_SPINNER_THRESHOLD_BYTE, SPINNER_THRESHOLD_DEF);
  if (Spinner_Threshold == 99) {                                               //  If set to 99
    Spinner_Threshold = SPINNER_THRESHOLD_DEF;                                 //  Set to default
    EEPROM.write(EEPROM_SPINNER_THRESHOLD_BYTE, SPINNER_THRESHOLD_DEF);        //  Write to EEPROM
  }
  if (Spinner_Threshold > SPINNER_THRESHOLD_MAX) Spinner_Threshold = SPINNER_THRESHOLD_MAX;
  if (Spinner_Threshold < SPINNER_THRESHOLD_MIN) Spinner_Threshold = SPINNER_THRESHOLD_MIN;
    
  Pop_Threshold = ReadSetting(EEPROM_POP_THRESHOLD_BYTE, POP_THRESHOLD_DEF);
  if (Pop_Threshold == 99) {                                                   //  If set to 99
    Pop_Threshold = POP_THRESHOLD_DEF;                                         //  Set to default
    EEPROM.write(EEPROM_POP_THRESHOLD_BYTE, POP_THRESHOLD_DEF);                //  Write to EEPROM
  }
  if (Pop_Threshold > POP_THRESHOLD_MAX) Pop_Threshold = POP_THRESHOLD_MAX;
  if (Pop_Threshold < POP_THRESHOLD_MIN) Pop_Threshold = POP_THRESHOLD_MIN;
    
  NextBallDuration = ReadSetting(EEPROM_NEXT_BALL_DURATION_BYTE, NEXT_BALL_DURATION_DEF);
  if (NextBallDuration == 99) {                                                //  If set to 99
    NextBallDuration = NEXT_BALL_DURATION_DEF;                                 //  Set to default
    EEPROM.write(EEPROM_NEXT_BALL_DURATION_BYTE, NEXT_BALL_DURATION_DEF);      //  Write to EEPROM
  }
  if (NextBallDuration > NEXT_BALL_DURATION_MAX) NextBallDuration = NEXT_BALL_DURATION_MAX;
  if (NextBallDuration < NEXT_BALL_DURATION_MIN) NextBallDuration = NEXT_BALL_DURATION_MIN;
  
  AwardScores[0] = RPU_ReadULFromEEProm(RPU_AWARD_SCORE_1_EEPROM_START_BYTE);
  AwardScores[1] = RPU_ReadULFromEEProm(RPU_AWARD_SCORE_2_EEPROM_START_BYTE);
  AwardScores[2] = RPU_ReadULFromEEProm(RPU_AWARD_SCORE_3_EEPROM_START_BYTE);

}

byte ReadSetting(byte setting, byte defaultValue) {
  byte value = EEPROM.read(setting);
  if (value == 0xFF) {
    EEPROM.write(setting, defaultValue);
    return defaultValue;
  }
  return value;
}



//
//  ShowLampAnimation - Ver 2
//    Ver 1 - Modified from version in Meteor
//    Ver 2 - Altered to enable use of Aux board lamps
//

void ShowLampAnimation(byte animationNum[][8], byte frames, unsigned long divisor, 
unsigned long baseTime, byte subOffset, boolean dim, boolean reverse = false, 
byte keepLampOn = 99, boolean AuxLamps = false) {

  byte currentStep = (baseTime / divisor) % frames;
  if (reverse) currentStep = (frames - 1) - currentStep;

  byte lampNum = 0;
  for (int byteNum = 0; byteNum < ((AuxLamps)?2:8); byteNum++) {
//  for (int byteNum = 0; byteNum < 8; byteNum++) {
    for (byte bitNum = 0; bitNum < 8; bitNum++) {

      // if there's a subOffset, turn off lights at that offset
      if (subOffset) {
        byte lampOff = true;
        lampOff = animationNum[(currentStep + subOffset) % frames][byteNum] & (1 << bitNum);
        if (lampOff && lampNum != keepLampOn) RPU_SetLampState((lampNum + ((AuxLamps)?60:0) ), 0);
//        if (lampOff && lampNum != keepLampOn) RPU_SetLampState(lampNum, 0);
      }

      byte lampOn = false;
      lampOn = animationNum[currentStep][byteNum] & (1 << bitNum);
      if (lampOn) RPU_SetLampState((lampNum + ((AuxLamps)?60:0)), 1, dim);
//      if (lampOn) RPU_SetLampState(lampNum, 1, dim);

      lampNum += 1;
    }
#if not defined (BALLY_STERN_OS_SOFTWARE_DISPLAY_INTERRUPT)
    if (byteNum % 2) RPU_DataRead(0);
#endif
  }
}


// Mata Hari version - includes setting of game player lamps on backglass
//
//    PLAYER_x_UP are the lamps adjacent to each display
//    PLAYER_X are the lamps at bottom left of backglass indicating how many player game is running
//

void SetPlayerLamps(byte numPlayers, byte playerOffset = 0, int flashPeriod = 0) {
  // For Mata Hari, the "Player Up" lights are all +4 of the "Player" lights
  // so this function covers both sets of lights. Putting a 4 in playerOffset
  // will turn on/off the player up lights.
  // Offset of 0 means lower backglass lamps, offset of 4 mean player lamps next to the score displays
  for (int count = 0; count < 4; count++) {
    RPU_SetLampState(LA_1_PLAYER + playerOffset + count, (numPlayers == (count + 1)) ? 1 : 0, 0, flashPeriod);
  }
}

//Meteor version - updates audit EEPROM memory locations
void AddCoinToAudit(byte switchHit) {

  unsigned short coinAuditStartByte = 0;

  switch (switchHit) {
    case SW_COIN_3: coinAuditStartByte = RPU_CHUTE_3_COINS_START_BYTE; break;
    case SW_COIN_2: coinAuditStartByte = RPU_CHUTE_2_COINS_START_BYTE; break;
    case SW_COIN_1: coinAuditStartByte = RPU_CHUTE_1_COINS_START_BYTE; break;
  }

  if (coinAuditStartByte) {
    RPU_WriteULToEEProm(coinAuditStartByte, RPU_ReadULFromEEProm(coinAuditStartByte) + 1);
  }

}


// Mata Hari version (and Meteor)
void AddCredit(boolean playSound = false, byte numToAdd = 1) {
  if (Credits < MaximumCredits) {
    Credits += numToAdd;
    if (Credits > MaximumCredits) Credits = MaximumCredits;
    RPU_WriteByteToEEProm(RPU_CREDITS_EEPROM_BYTE, Credits);
    if (playSound) PlaySoundEffect(SOUND_EFFECT_ADD_CREDIT);
    CreditsDispValue = Credits;
    //RPU_SetDisplayCredits(Credits);
    RPU_SetCoinLockout(false);
  } else {
    CreditsDispValue = Credits;
    //RPU_SetDisplayCredits(Credits);
    RPU_SetCoinLockout(true);
  }
}

void AddSpecialCredit() {
  AddCredit(false, 1);
  RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true);
  RPU_WriteULToEEProm(RPU_TOTAL_REPLAYS_EEPROM_START_BYTE, RPU_ReadULFromEEProm(RPU_TOTAL_REPLAYS_EEPROM_START_BYTE) + 1);  
}

// Meteor version - uses updated function call in Attract Mode to allow game start following a
// a 4 player game
boolean AddPlayer(boolean resetNumPlayers = false) {

  if (Credits < 1 && !FreePlayMode) return false;
  if (resetNumPlayers) CurrentNumPlayers = 0;
  if (CurrentNumPlayers >= 4) return false;

  CurrentNumPlayers += 1;
  RPU_SetDisplay(CurrentNumPlayers - 1, 0);
  RPU_SetDisplayBlank(CurrentNumPlayers - 1, 0x30);

  if (!FreePlayMode) {
    Credits -= 1;
    RPU_WriteByteToEEProm(RPU_CREDITS_EEPROM_BYTE, Credits);
    CreditsDispValue = Credits;
    //RPU_SetDisplayCredits(Credits);
    RPU_SetCoinLockout(false);
  }
  if (CurrentNumPlayers > 1){
  PlaySoundEffect(SOUND_EFFECT_ADD_PLAYER);
  }
  //Serial.println(F("AddPlayer - SetPlayerLamps(CurrentNumPlayers)"));
  SetPlayerLamps(CurrentNumPlayers);

  RPU_WriteULToEEProm(RPU_TOTAL_PLAYS_EEPROM_START_BYTE, RPU_ReadULFromEEProm(RPU_TOTAL_PLAYS_EEPROM_START_BYTE) + 1);

  return true;
}



int InitNewBall(bool curStateChanged, byte playerNum, int ballNum) {  

  if (curStateChanged) {

    //
    // Set pre-ball conditions & variables
    //

    Serial.println(F("----------InitNewBall----------"));
    
    CheckBallTime = CurrentTime;
    BallFirstSwitchHitTime = 0;
    MarqueeOffTime = 0;

    CreditsDispValue = Credits;

    SetPlayerLamps(playerNum+1, 4, 500);

    NumTiltWarnings = 0;
    LastTiltWarningTime = 0;
    Tilted = false;

    BallSaveUsed = false;
    if (BallSaveNumSeconds>0) {
      RPU_SetLampState(SAME_PLAYER, 1, 0, 500);
    }
    
    ArrowTest = true;
    RPU_SetLampState(LA_LEFT_ARROW, 0);  // Reset arrows
    RPU_SetLampState(LA_RIGHT_ARROW, 0);  // Reset arrows 

    SpinnerKickerLit  = false;
    KickerUsed = true;
    KickerReady = false;
    SuperSpinnerTime = 0;
    SuperSpinnerDuration = Spinner_Combo_Duration;
    SpinnerComboHalf = false;
    SpinnerDelta = 0;

    PopDelta = 0;

    NextBallTime = 0;
    NextBall = 0;
    
    RPU_SetLampState(LA_SPINNER, 0);       // Spinner and Kicker are off at beginning of a new ball

    BankShotProgress = 0;                   // Reset Bank shot for each ball
    BankShotLighting();                     // Set Lights for ball start

    BonusMultiplier(1);                     // Reset bonus multiplier and lights
    BonusMultTenX = false;                  // Reset flag
    
    Ten_Pts_Stack = 0;                      // Reset Scoring variables
    Hundred_Pts_Stack = 0;
    Thousand_Pts_Stack = 0;
    Silent_Thousand_Pts_Stack = 0;
    Silent_Hundred_Pts_Stack = 0;
    ChimeScoringDelay = 0;

//
//  Chase Ball - Mode 3
//

ChaseBall = 0;                                 //  Game Mode 3 target ball
ChaseBallStage = 0;                            //  Counter of Ball captured in Game Mode 3
ChaseBallTimeStart = 0;                        //  Timer for ChaseBall


//
//  SuperBonus
//

    if (OutlaneSpecial[CurrentPlayer] == false) {
      RPU_SetLampState(LA_OUTLANE_SPECIAL, 0);                    // Turn off lamp
    } else {
      RPU_SetLampState(LA_OUTLANE_SPECIAL, 1);                    // If current player is true, turn on lamp
    }
    if (GameMode[CurrentPlayer] == 1) {
      RPU_SetLampState(LA_SUPER_BONUS, 0);                        // Turn off SuperBonus lamp
      if (Balls[CurrentPlayer] & (0b1<<15)) {                      // If player already has SuperBonus
        RPU_SetLampState(LA_SUPER_BONUS, 1);                      // Turn on SuperBonus lamp
      }
      if ((Balls[CurrentPlayer] & (0b1<<7)) == 128) {              // If 8 ball collected, we have achieved SuperBonus
        RPU_SetLampState(LA_SUPER_BONUS, 1);                      // Turn on SuperBonus lamp
        Balls[CurrentPlayer] = 0x8000;                             // Wipe out all collected balls, set SuperBonus
        SetGoals(1);                                               // Duplicate of above
        EightBallTest[CurrentPlayer] = true;                       // Reset to enable getting 8 ball again
        if (!OutlaneSpecial[CurrentPlayer]) {                      // Achieving SuperBonus turns on Outlane Special
          OutlaneSpecial[CurrentPlayer] = true;
          RPU_SetLampState(LA_OUTLANE_SPECIAL, 1);                // Turn lamp on, signifying mode is active
        }
        ArrowsLit[CurrentPlayer] = false;
      }
    }


//
//  15 Ball Mode
//

    if (GameMode[CurrentPlayer] == 2) {                  // if 15 Ball mode already active
      FifteenBallCounter[CurrentPlayer] += 1;
      RPU_SetLampState(LA_SUPER_BONUS, 1, 0, 750);     // Turn on slow blink for Mode 2
    }
    if (FifteenBallCounter[CurrentPlayer] > 2) {        // Maximum balls in Mode 2
      GameMode[CurrentPlayer] = 1;                      // Reset to Mode 1
      FifteenBallCounter[CurrentPlayer] = 0;            // Reset counter
      Balls[CurrentPlayer] = 0;                         // Reset all balls plus SuperBonus
      Goals[CurrentPlayer] = 0;                         // Reset all goals
      PopMode[CurrentPlayer] = 0;                       // Reset
      PopCount[CurrentPlayer] = 0;                      // Reset
      SpinnerMode[CurrentPlayer] = 0;                   // Reset
      SpinnerCount[CurrentPlayer] = 0;                  // Reset
      CurrentScores[CurrentPlayer] = CurrentScores[CurrentPlayer]/10*10;  // Remove goals
      RPU_SetLampState(LA_SUPER_BONUS, 0);             // Turn off
    }

    if (FifteenBallQualified[CurrentPlayer]) {
      FifteenBallQualified[CurrentPlayer] = false;  // Turn off
      GameMode[CurrentPlayer] = 2;                  // Set Game Mode to 15 Ball
      Balls[CurrentPlayer] = 0;                     // Reset all balls plus SuperBonus
      RPU_SetLampState(LA_SUPER_BONUS, 1, 0, 750); // Turn on slow blink for Mode 2
      EightBallTest[CurrentPlayer] = false;         // Turn off for Mode 2
      ArrowsLit[CurrentPlayer] = false;             // Turn off
      FifteenBallCounter[CurrentPlayer] = 1;        // Set to 1
    }

    SamePlayerShootsAgain = false;
    RPU_SetLampState(SAME_PLAYER, 0);
    RPU_SetLampState(LA_SPSA_BACK, 0);        

    RPU_SetDisableFlippers(false);
    RPU_EnableSolenoidStack(); 

    MarqueeDisabled = false;                        // In case SetGoals turned this off.
    //Serial.println("InitNewBall - BallLighting");
    BallLighting();                                 // Relight rack balls already collected
    
    if (RPU_ReadSingleSwitchState(SW_OUTHOLE)) {
      RPU_PushToTimedSolenoidStack(SOL_OUTHOLE, 4, CurrentTime + 100);
    }

    //ShowPlayerScores(0xFF, false, false); // Show player scores
    for (int count=0; count<CurrentNumPlayers; count++) {
      RPU_SetDisplay(count, CurrentScores[count], true, 2);
    }

    BIPDispValue = ballNum;
    RPU_SetLampState(BALL_IN_PLAY, 1);
    RPU_SetLampState(TILT, 0);

    CreditsFlashing = false;   //  credits display on steady
    BIPFlashing = false;       //  BIP display on steady

    OverrideScorePriority = 0; //  Set to default

  }
  
  // We should only consider the ball initialized when 
  // the ball is no longer triggering the SW_OUTHOLE
  if (RPU_ReadSingleSwitchState(SW_OUTHOLE)) {
    //Serial.println(F("--InitNewBall, ball still in outhole--"));
    return MACHINE_STATE_INIT_NEW_BALL;
  } else {
    return MACHINE_STATE_NORMAL_GAMEPLAY;
  }
  
}

////////////////////////////////////////////////////////////////////////////
//
//  Self test, audit, adjustments mode
//
////////////////////////////////////////////////////////////////////////////

#define ADJ_TYPE_LIST                 1
#define ADJ_TYPE_MIN_MAX              2
#define ADJ_TYPE_MIN_MAX_DEFAULT      3
#define ADJ_TYPE_SCORE                4
#define ADJ_TYPE_SCORE_WITH_DEFAULT   5
#define ADJ_TYPE_SCORE_NO_DEFAULT     6
byte AdjustmentType = 0;
byte NumAdjustmentValues = 0;
byte AdjustmentValues[8];
unsigned long AdjustmentScore;
byte *CurrentAdjustmentByte = NULL;
unsigned long *CurrentAdjustmentUL = NULL;
byte CurrentAdjustmentStorageByte = 0;
byte TempValue = 0;


int RunSelfTest(int curState, boolean curStateChanged) {
  int returnState = curState;
  CurrentNumPlayers = 0;

#if 0
  if (curStateChanged) {
    // Send a stop-all command and reset the sample-rate offset, in case we have
    //  reset while the WAV Trigger was already playing.
    StopAudio();
    PlaySoundEffect(SOUND_EFFECT_SELF_TEST_MODE_START-curState, 0);
  } else {
    if (SoundSettingTimeout && CurrentTime>SoundSettingTimeout) {
      SoundSettingTimeout = 0;
      StopAudio();
    }
  }
#endif


  // Any state that's greater than CHUTE_3 is handled by the Base Self-test code
  // Any that's less, is machine specific, so we handle it here.
  if (curState >= MACHINE_STATE_TEST_CHUTE_3_COINS) {
    returnState = RunBaseSelfTest(returnState, curStateChanged, CurrentTime, SW_CREDIT_RESET, SW_SLAM);
  } else {
    byte curSwitch = RPU_PullFirstFromSwitchStack();

    if (curSwitch == SW_SELF_TEST_SWITCH && (CurrentTime - GetLastSelfTestChangedTime()) > 250) {
      SetLastSelfTestChangedTime(CurrentTime);
      returnState -= 1;
    }

    if (curSwitch == SW_SLAM) {
      returnState = MACHINE_STATE_ATTRACT;
    }

    if (curStateChanged) {
      for (int count = 0; count < 4; count++) {
        RPU_SetDisplay(count, 0);
        RPU_SetDisplayBlank(count, 0x00);
      }
      Serial.print(F("Current Machine State is: "));
      Serial.println(curState, DEC);
      RPU_SetDisplayCredits(MACHINE_STATE_TEST_SOUNDS - curState);
      RPU_SetDisplayBallInPlay(0, false);
      CurrentAdjustmentByte = NULL;
      CurrentAdjustmentUL = NULL;
      CurrentAdjustmentStorageByte = 0;

      AdjustmentType = ADJ_TYPE_MIN_MAX;
      AdjustmentValues[0] = 0;
      AdjustmentValues[1] = 1;
      TempValue = 0;

      switch (curState) {
        case MACHINE_STATE_ADJUST_FREEPLAY:
          CurrentAdjustmentByte = (byte *)&FreePlayMode;
          CurrentAdjustmentStorageByte = EEPROM_FREE_PLAY_BYTE;
          break;
        case MACHINE_STATE_ADJUST_BALL_SAVE:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 6;
          AdjustmentValues[1] = 5;
          AdjustmentValues[2] = 10;
          AdjustmentValues[3] = 15;
          AdjustmentValues[4] = 20;
          AdjustmentValues[5] = 99;
          CurrentAdjustmentByte = &BallSaveNumSeconds;
          CurrentAdjustmentStorageByte = EEPROM_BALL_SAVE_BYTE;
          break;
        case MACHINE_STATE_ADJUST_CHASEBALL_DURATION:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 6;
          AdjustmentValues[0] = 19;
          AdjustmentValues[1] = 22;
          AdjustmentValues[2] = 25;
          AdjustmentValues[3] = 28;
          AdjustmentValues[4] = 31;
          AdjustmentValues[5] = 99;
          CurrentAdjustmentByte = &ChaseBallDuration;
          CurrentAdjustmentStorageByte = EEPROM_CHASEBALL_DURATION_BYTE;
          break;
        case MACHINE_STATE_ADJUST_SPINNER_COMBO_DURATION:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 6;
          AdjustmentValues[0] = 8;
          AdjustmentValues[1] = 10;
          AdjustmentValues[2] = 12;
          AdjustmentValues[3] = 14;
          AdjustmentValues[4] = 16;
          AdjustmentValues[5] = 99;
          CurrentAdjustmentByte = &Spinner_Combo_Duration;
          CurrentAdjustmentStorageByte = EEPROM_SPINNER_COMBO_DURATION_BYTE;
          break;
        case MACHINE_STATE_ADJUST_SPINNER_THRESHOLD:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 6;
          AdjustmentValues[0] = 30;
          AdjustmentValues[1] = 40;
          AdjustmentValues[2] = 50;
          AdjustmentValues[3] = 60;
          AdjustmentValues[4] = 70;
          AdjustmentValues[5] = 99;
          CurrentAdjustmentByte = &Spinner_Threshold;
          CurrentAdjustmentStorageByte = EEPROM_SPINNER_THRESHOLD_BYTE;
          break;
        case MACHINE_STATE_ADJUST_POP_THRESHOLD:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 6;
          AdjustmentValues[0] = 30;
          AdjustmentValues[1] = 35;
          AdjustmentValues[2] = 40;
          AdjustmentValues[3] = 45;
          AdjustmentValues[4] = 50;
          AdjustmentValues[5] = 99;
          CurrentAdjustmentByte = &Pop_Threshold;
          CurrentAdjustmentStorageByte = EEPROM_POP_THRESHOLD_BYTE;
          break;
        case MACHINE_STATE_ADJUST_NEXT_BALL_DURATION:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 6;
          AdjustmentValues[0] = 11;
          AdjustmentValues[1] = 13;
          AdjustmentValues[2] = 15;
          AdjustmentValues[3] = 17;
          AdjustmentValues[4] = 19;
          AdjustmentValues[5] = 99;
          CurrentAdjustmentByte = &NextBallDuration;
          CurrentAdjustmentStorageByte = EEPROM_NEXT_BALL_DURATION_BYTE;
          break;
        case MACHINE_STATE_ADJUST_TILT_WARNING:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 4;
          AdjustmentValues[0] = 0;
          AdjustmentValues[1] = 1;
          AdjustmentValues[2] = 2;
          AdjustmentValues[3] = 99;
          CurrentAdjustmentByte = &MaxTiltWarnings;
          CurrentAdjustmentStorageByte = EEPROM_TILT_WARNING_BYTE;
          break;
        case MACHINE_STATE_ADJUST_BALLS_OVERRIDE:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 3;
          AdjustmentValues[0] = 3;
          AdjustmentValues[1] = 5;
          AdjustmentValues[2] = 99;
          CurrentAdjustmentByte = &BallsPerGame;
          CurrentAdjustmentStorageByte = EEPROM_BALLS_OVERRIDE_BYTE;
          break;
        case MACHINE_STATE_ADJUST_DONE:
          returnState = MACHINE_STATE_ATTRACT;
          break;
      }

    }

    // Change value, if the switch is hit
    if (curSwitch == SW_CREDIT_RESET) {

      if (CurrentAdjustmentByte && (AdjustmentType == ADJ_TYPE_MIN_MAX || AdjustmentType == ADJ_TYPE_MIN_MAX_DEFAULT)) {
        byte curVal = *CurrentAdjustmentByte;
        curVal += 1;
        if (curVal > AdjustmentValues[1]) {
          if (AdjustmentType == ADJ_TYPE_MIN_MAX) curVal = AdjustmentValues[0];
          else {
            if (curVal > 99) curVal = AdjustmentValues[0];
            else curVal = 99;
          }
        }
        *CurrentAdjustmentByte = curVal;
        if (CurrentAdjustmentStorageByte) EEPROM.write(CurrentAdjustmentStorageByte, curVal);
/*        
        if (curState==MACHINE_STATE_ADJUST_SFX_AND_SOUNDTRACK) {
          StopAudio();
          PlaySoundEffect(SOUND_EFFECT_SELF_TEST_AUDIO_OPTIONS_START+curVal, 0);
          if (curVal>=3) SoundtrackSelection = curVal-3;
        } else if (curState==MACHINE_STATE_ADJUST_MUSIC_VOLUME) {
          if (SoundSettingTimeout) StopAudio();
          PlayBackgroundSong(MusicIndices[SoundtrackSelection][4] + RALLY_MUSIC_WAITING_FOR_SKILLSHOT);
          SoundSettingTimeout = CurrentTime+5000;
        } else if (curState==MACHINE_STATE_ADJUST_SFX_VOLUME) {
          if (SoundSettingTimeout) StopAudio();
          PlaySoundEffect(SOUND_EFFECT_GALAXY_LETTER_AWARD, ConvertVolumeSettingToGain(SoundEffectsVolume));
          SoundSettingTimeout = CurrentTime+5000;
        } else if (curState==MACHINE_STATE_ADJUST_CALLOUTS_VOLUME) {
          if (SoundSettingTimeout) StopAudio();
          PlaySoundEffect(SOUND_EFFECT_VP_SKILL_SHOT_1, ConvertVolumeSettingToGain(CalloutsVolume));
          SoundSettingTimeout = CurrentTime+3000;
        }*/
      } else if (CurrentAdjustmentByte && AdjustmentType == ADJ_TYPE_LIST) {
        byte valCount = 0;
        byte curVal = *CurrentAdjustmentByte;
        byte newIndex = 0;
        for (valCount = 0; valCount < (NumAdjustmentValues - 1); valCount++) {
          if (curVal == AdjustmentValues[valCount]) newIndex = valCount + 1;
        }
        *CurrentAdjustmentByte = AdjustmentValues[newIndex];
        if (CurrentAdjustmentStorageByte) EEPROM.write(CurrentAdjustmentStorageByte, AdjustmentValues[newIndex]);
      } else if (CurrentAdjustmentUL && (AdjustmentType == ADJ_TYPE_SCORE_WITH_DEFAULT || AdjustmentType == ADJ_TYPE_SCORE_NO_DEFAULT)) {
        unsigned long curVal = *CurrentAdjustmentUL;
        curVal += 5000;
        if (curVal > 100000) curVal = 0;
        if (AdjustmentType == ADJ_TYPE_SCORE_NO_DEFAULT && curVal == 0) curVal = 5000;
        *CurrentAdjustmentUL = curVal;
        if (CurrentAdjustmentStorageByte) RPU_WriteULToEEProm(CurrentAdjustmentStorageByte, curVal);
      }
/*
      if (curState == MACHINE_STATE_ADJUST_DIM_LEVEL) {
        RPU_SetDimDivisor(1, DimLevel);
      }*/
    }

    // Show current value
    if (CurrentAdjustmentByte != NULL) {
      RPU_SetDisplay(0, (unsigned long)(*CurrentAdjustmentByte), true);
    } else if (CurrentAdjustmentUL != NULL) {
      RPU_SetDisplay(0, (*CurrentAdjustmentUL), true);
    }

  }
/*
  if (curState == MACHINE_STATE_ADJUST_DIM_LEVEL) {
    //    for (int count = 0; count < 7; count++) RPU_SetLampState(MIDDLE_ROCKET_7K + count, 1, (CurrentTime / 1000) % 2);
  }*/

  if (returnState == MACHINE_STATE_ATTRACT) {
    // If any variables have been set to non-override (99), return
    // them to dip switch settings
    // Balls Per Game, Player Loses On Ties, Novelty Scoring, Award Score
    //    DecodeDIPSwitchParameters();
    RPU_SetDisplayCredits(Credits, !FreePlayMode);
    ReadStoredParameters();
  }

  return returnState;
}

////////////////////////////////////////////////////////////////////////////
//
//  Display Management functions
//
////////////////////////////////////////////////////////////////////////////
unsigned long LastTimeScoreChanged = 0;
unsigned long LastTimeOverrideAnimated = 0;
unsigned long LastFlashOrDash = 0;
#ifdef USE_SCORE_OVERRIDES
unsigned long ScoreOverrideValue[4] = {0, 0, 0, 0};
byte ScoreOverrideStatus = 0;
#define DISPLAY_OVERRIDE_BLANK_SCORE 0xFFFFFFFF
#endif
byte LastScrollPhase = 0;

byte MagnitudeOfScore(unsigned long score) {
  if (score == 0) return 0;

  byte retval = 0;
  while (score > 0) {
    score = score / 10;
    retval += 1;
  }
  return retval;
}

#ifdef USE_SCORE_OVERRIDES
void OverrideScoreDisplay(byte displayNum, unsigned long value, boolean animate) {
  if (displayNum > 3) return;
  ScoreOverrideStatus |= (0x10 << displayNum);
  if (animate) ScoreOverrideStatus |= (0x01 << displayNum);
  else ScoreOverrideStatus &= ~(0x01 << displayNum);
  ScoreOverrideValue[displayNum] = value;
}
#endif

byte GetDisplayMask(byte numDigits) {
  byte displayMask = 0;
  for (byte digitCount = 0; digitCount < numDigits; digitCount++) {
    displayMask |= (0x20 >> digitCount);
  }
  return displayMask;
}


void ShowPlayerScores(byte displayToUpdate, boolean flashCurrent, boolean dashCurrent, unsigned long allScoresShowValue = 0) {

#ifdef USE_SCORE_OVERRIDES
  if (displayToUpdate == 0xFF) ScoreOverrideStatus = 0;
#endif

  byte displayMask = 0x3F;
  unsigned long displayScore = 0;
  unsigned long overrideAnimationSeed = CurrentTime / 150;  // Default is 250 for animated scores to slide back and forth
  byte scrollPhaseChanged = false;

  byte scrollPhase = ((CurrentTime - LastTimeScoreChanged) / 100) % 16;  // Speed of score scrolling
  if (scrollPhase != LastScrollPhase) {
    LastScrollPhase = scrollPhase;
    scrollPhaseChanged = true;
  }

  boolean updateLastTimeAnimated = false;

  for (byte scoreCount = 0; scoreCount < 4; scoreCount++) {   // Loop on scores

#ifdef USE_SCORE_OVERRIDES
    // If this display is currently being overriden, then we should update it
    if (allScoresShowValue == 0 && (ScoreOverrideStatus & (0x10 << scoreCount))) {
      displayScore = ScoreOverrideValue[scoreCount];
      if (displayScore != DISPLAY_OVERRIDE_BLANK_SCORE) {
        byte numDigits = MagnitudeOfScore(displayScore);
        if (numDigits == 0) numDigits = 1;
        if (numDigits < (RPU_OS_NUM_DIGITS - 1) && (ScoreOverrideStatus & (0x01 << scoreCount))) {
          // This score is going to be animated (back and forth)
          if (overrideAnimationSeed != LastTimeOverrideAnimated) {
            updateLastTimeAnimated = true;
            byte shiftDigits = (overrideAnimationSeed) % (((RPU_OS_NUM_DIGITS + 1) - numDigits) + ((RPU_OS_NUM_DIGITS - 1) - numDigits));
            if (shiftDigits >= ((RPU_OS_NUM_DIGITS + 1) - numDigits)) shiftDigits = (RPU_OS_NUM_DIGITS - numDigits) * 2 - shiftDigits;
            byte digitCount;
            displayMask = GetDisplayMask(numDigits);
            for (digitCount = 0; digitCount < shiftDigits; digitCount++) {
              displayScore *= 10;
              displayMask = displayMask >> 1;
            }
            RPU_SetDisplayBlank(scoreCount, 0x00);
            RPU_SetDisplay(scoreCount, displayScore, false);
            RPU_SetDisplayBlank(scoreCount, displayMask);
          }
        } else {
          RPU_SetDisplay(scoreCount, displayScore, true, 1);
        }
      } else {
        RPU_SetDisplayBlank(scoreCount, 0);
      }

    } else {    // Start of non-overridden
#endif
#ifdef USE_SCORE_ACHIEVEMENTS
      boolean showingCurrentAchievement = false;
#endif      
      // No override, update scores designated by displayToUpdate
      if (allScoresShowValue == 0) {
        displayScore = CurrentScores[scoreCount];
#ifdef USE_SCORE_ACHIEVEMENTS
        displayScore += (CurrentAchievements[scoreCount]%10);
        if (CurrentAchievements[scoreCount]) showingCurrentAchievement = true;
#endif 
      }
      else displayScore = allScoresShowValue;

      // If we're updating all displays, or the one currently matching the loop, or if we have to scroll
      if (displayToUpdate == 0xFF || displayToUpdate == scoreCount || displayScore > RPU_OS_MAX_DISPLAY_SCORE || showingCurrentAchievement) {

        // Don't show this score if it's not a current player score (even if it's scrollable)
        if (displayToUpdate == 0xFF && (scoreCount >= CurrentNumPlayers && CurrentNumPlayers != 0) && allScoresShowValue == 0) {
          RPU_SetDisplayBlank(scoreCount, 0x00);
          continue;
        }

        if (displayScore > RPU_OS_MAX_DISPLAY_SCORE) {    //  Need to scroll
          // Score needs to be scrolled 
          //if ((CurrentTime - LastTimeScoreChanged) < 1000) {  // How long to wait to before 1st scroll
          if ((CurrentTime - LastTimeScoreChanged) < ((MachineState == 0)?1000:4000) ) {  // How long to wait to before 1st scroll
            // show score for four seconds after change
            RPU_SetDisplay(scoreCount, displayScore % (RPU_OS_MAX_DISPLAY_SCORE + 1), false);
            byte blank = RPU_OS_ALL_DIGITS_MASK;
#ifdef USE_SCORE_ACHIEVEMENTS
            if (showingCurrentAchievement && (CurrentTime/200)%2) {
              blank &= ~(0x01<<(RPU_OS_NUM_DIGITS-1));
            }
#endif            
            RPU_SetDisplayBlank(scoreCount, blank);            // Sets all digits on except singles for blinking
          } else {                          // Greater than x seconds so scroll
            // Scores are scrolled 10 digits and then we wait for 6
            if (scrollPhase < 11 && scrollPhaseChanged) {   // Scroll phase 0-10 is for actual scrolling
              byte numDigits = MagnitudeOfScore(displayScore);

              // Figure out top part of score
              unsigned long tempScore = displayScore;
              if (scrollPhase < RPU_OS_NUM_DIGITS) {    // Scrolling lowest 6 digits or less
                displayMask = RPU_OS_ALL_DIGITS_MASK;
                for (byte scrollCount = 0; scrollCount < scrollPhase; scrollCount++) {  
                  displayScore = (displayScore % (RPU_OS_MAX_DISPLAY_SCORE + 1)) * 10;  // *10 shift for each scrolled digit up to scrollPhase
                  displayMask = displayMask >> 1;                               // mask moves up to keep digits behind score blank.
                }
              } else {    // scrollPhase > num digits but less than 11, score is gone and display is now blank
                displayScore = 0;
                displayMask = 0x00;
              }

              // Add in lower part of score (front and back can be on screen at the same time)
              if ((numDigits + scrollPhase) > 10) {             // if total is > 10, score scrolled within 10 space window
                byte numDigitsNeeded = (numDigits + scrollPhase) - 10;  // eg. 12345 & 7 = 12-2 = 2
                for (byte scrollCount = 0; scrollCount < (numDigits - numDigitsNeeded); scrollCount++) {
                  tempScore /= 10;                              // Divide down to get the front end of number needed
                }
                displayMask |= GetDisplayMask(MagnitudeOfScore(tempScore));
                displayScore += tempScore;
              }
              RPU_SetDisplayBlank(scoreCount, displayMask);
              RPU_SetDisplay(scoreCount, displayScore);
            }
          }
        } else {      // End of scrolling portion above
          if (flashCurrent && displayToUpdate == scoreCount) {  // If flashing requested and this display is being updated
            unsigned long flashSeed = CurrentTime / 250;
            if (flashSeed != LastFlashOrDash) {
              LastFlashOrDash = flashSeed;
              if (((CurrentTime / 250) % 2) == 0) RPU_SetDisplayBlank(scoreCount, 0x00);
              else RPU_SetDisplay(scoreCount, displayScore, true, 2);
            }
          } else if (dashCurrent && displayToUpdate == scoreCount) {  // If dashing requested
            unsigned long dashSeed = CurrentTime / 50;
            if (dashSeed != LastFlashOrDash) {
              LastFlashOrDash = dashSeed;
              byte dashPhase = (CurrentTime / 60) % 36;
              byte numDigits = MagnitudeOfScore(displayScore);
              if (dashPhase < 12) {
                displayMask = GetDisplayMask((numDigits == 0) ? 2 : numDigits);
                if (dashPhase < 7) {          // Wipe out all the digits up to 6 based on dashPhase
                  // Wipes out digits progressively from left to right
                  for (byte maskCount = 0; maskCount < dashPhase; maskCount++) {
                    displayMask &= ~(0x01 << maskCount);
                  }
                } else {        //  for dashPhase from 7-11
                  // Show digits again from right to left
                  for (byte maskCount = 12; maskCount > dashPhase; maskCount--) {
                    displayMask &= ~(0x20 >> (maskCount - dashPhase - 1));
                  }
                }
                RPU_SetDisplay(scoreCount, displayScore);
                RPU_SetDisplayBlank(scoreCount, displayMask);
              } else {    // if not in dashPhase from 1-12, then up to 36 do nothing
                RPU_SetDisplay(scoreCount, displayScore, true, 2);
              }
            }
          } else {      // End of dashing
#ifdef USE_SCORE_ACHIEVEMENTS
            byte blank;
            blank = RPU_SetDisplay(scoreCount, displayScore, false, 2);
            if (showingCurrentAchievement && (CurrentTime/200)%2) {
              blank &= ~(0x01<<(RPU_OS_NUM_DIGITS-1));
            }
            RPU_SetDisplayBlank(scoreCount, blank);
#else
            RPU_SetDisplay(scoreCount, displayScore, true, 2);
#endif
          }
        }
      } // End if this display should be updated
#ifdef USE_SCORE_OVERRIDES
    } // End on non-overridden
#endif
  } // End loop on scores

  if (updateLastTimeAnimated) {
    LastTimeOverrideAnimated = overrideAnimationSeed;
  }

}

void ShowFlybyValue(byte numToShow, unsigned long timeBase) {
  byte shiftDigits = (CurrentTime - timeBase) / 120;
  byte rightSideBlank = 0;

  unsigned long bigVersionOfNum = (unsigned long)numToShow;
  for (byte count = 0; count < shiftDigits; count++) {
    bigVersionOfNum *= 10;
    rightSideBlank /= 2;
    if (count > 2) rightSideBlank |= 0x20;
  }
  bigVersionOfNum /= 1000;

  byte curMask = RPU_SetDisplay(CurrentPlayer, bigVersionOfNum, false, 0);
  if (bigVersionOfNum == 0) curMask = 0;
  RPU_SetDisplayBlank(CurrentPlayer, ~(~curMask | rightSideBlank));
}




//
//  Attract Mode
//

byte AttractLastHeadMode = 255;
unsigned long AttractOffset = 0;

int RunAttractMode(int curState, boolean curStateChanged) {

  int returnState = curState;

  // If this is the first time in the attract mode loop
  if (curStateChanged) {

//
// Reset EEPROM memory locations - uncomment, load and boot machine, comment, reload again.
//

#if 0
  RPU_WriteByteToEEProm(RPU_CREDITS_EEPROM_BYTE, 5);
  RPU_WriteULToEEProm(RPU_TOTAL_PLAYS_EEPROM_START_BYTE, 0);
  RPU_WriteULToEEProm(RPU_HIGHSCORE_EEPROM_START_BYTE, 101000);
  RPU_WriteULToEEProm(RPU_AWARD_SCORE_1_EEPROM_START_BYTE, 270000);
  RPU_WriteULToEEProm(RPU_AWARD_SCORE_2_EEPROM_START_BYTE, 430000);
  RPU_WriteULToEEProm(RPU_AWARD_SCORE_3_EEPROM_START_BYTE, 650000);
  RPU_WriteULToEEProm(RPU_TOTAL_REPLAYS_EEPROM_START_BYTE, 0);
  RPU_WriteULToEEProm(RPU_TOTAL_HISCORE_BEATEN_START_BYTE, 0);
  RPU_WriteULToEEProm(RPU_CHUTE_1_COINS_START_BYTE, 0);
  RPU_WriteULToEEProm(RPU_CHUTE_2_COINS_START_BYTE, 0);
  RPU_WriteULToEEProm(RPU_CHUTE_3_COINS_START_BYTE, 0);
#endif
#ifdef COIN_DOOR_TELEMETRY    // Send values to monitor if defined

    Serial.println();
    Serial.print(F("Version Number:        "));
    Serial.println(VERSION_NUMBER, DEC);
    Serial.println();
    Serial.println(F("EEPROM Values: "));
    Serial.println();
    Serial.print(F("Credits:                 "));
    Serial.println(RPU_ReadByteFromEEProm(RPU_CREDITS_EEPROM_BYTE), DEC);
    Serial.print(F("AwardScores[0]:          "));
    Serial.println(RPU_ReadULFromEEProm(RPU_AWARD_SCORE_1_EEPROM_START_BYTE), DEC);
    Serial.print(F("AwardScores[1]:          "));
    Serial.println(RPU_ReadULFromEEProm(RPU_AWARD_SCORE_2_EEPROM_START_BYTE), DEC);
    Serial.print(F("AwardScores[2]:          "));
    Serial.println(RPU_ReadULFromEEProm(RPU_AWARD_SCORE_3_EEPROM_START_BYTE), DEC);
    Serial.print(F("High Score:              "));
    Serial.println(RPU_ReadULFromEEProm(RPU_HIGHSCORE_EEPROM_START_BYTE, 10000), DEC);
    Serial.print(F("BALLS_OVERRIDE:          "));
    Serial.println(ReadSetting(EEPROM_BALLS_OVERRIDE_BYTE, 99), DEC);
    Serial.print(F("FreePlay  :              "));
    Serial.println(ReadSetting(EEPROM_FREE_PLAY_BYTE, 0), DEC);
    Serial.print(F("BallSaveNumSeconds       "));
    Serial.println(ReadSetting(EEPROM_BALL_SAVE_BYTE, 15), DEC);
    Serial.print(F("ChaseBallDuration:       "));
    Serial.println(ReadSetting(EEPROM_CHASEBALL_DURATION_BYTE, 20), DEC);
    Serial.print(F("Spinner_Combo_Duration:  "));
    Serial.println(ReadSetting(EEPROM_SPINNER_COMBO_DURATION_BYTE, 12), DEC);
    Serial.print(F("Spinner_Threshold:       "));
    Serial.println(ReadSetting(EEPROM_SPINNER_THRESHOLD_BYTE, 50), DEC);
    Serial.print(F("Pop_Threshold:           "));
    Serial.println(ReadSetting(EEPROM_POP_THRESHOLD_BYTE, 40), DEC);
    Serial.print(F("NextBallDuration:        "));
    Serial.println(ReadSetting(EEPROM_NEXT_BALL_DURATION_BYTE, 15), DEC);
    Serial.print(F("MaxTiltWarnings:         "));
    Serial.println(ReadSetting(EEPROM_TILT_WARNING_BYTE, 2), DEC);

    Serial.println();
    Serial.println(F("Variable Values: "));
    Serial.println();

    Serial.print(F("BallsPerGame:             "));
    Serial.println(BallsPerGame, DEC);
    Serial.print(F("BallSaveNumSeconds:       "));
    Serial.println(BallSaveNumSeconds, DEC);
    Serial.print(F("ChaseBallDuration:        "));
    Serial.println(ChaseBallDuration, DEC);
    Serial.print(F("Spinner_Combo_Duration:   "));
    Serial.println(Spinner_Combo_Duration, DEC);
    Serial.print(F("Spinner_Threshold:        "));
    Serial.println(Spinner_Threshold, DEC);
    Serial.print(F("Pop_Threshold:            "));
    Serial.println(Pop_Threshold, DEC);
    Serial.print(F("NextBallDuration:         "));
    Serial.println(NextBallDuration, DEC);

    Serial.print(F("Current Scores  :         "));
    Serial.print(CurrentScores[0], DEC);
    Serial.print(F("  "));
    Serial.print(CurrentScores[1], DEC);
    Serial.print(F("  "));
    Serial.print(CurrentScores[2], DEC);
    Serial.print(F("  "));
    Serial.println(CurrentScores[3], DEC);


#endif


    RPU_DisableSolenoidStack();
    RPU_TurnOffAllLamps();

    RPU_SetDisableFlippers(true);
    for (int count=0; count<4; count++) {
      RPU_SetDisplayBlank(count, 0x00);     
    }
    CreditsDispValue = Credits;
    CreditsFlashing = false;
    BIPDispValue = 0;
    BIPFlashing = false;

    //  If previous game has been played turn Match on and leave match value on display
    if (CurrentNumPlayers) {    
      BIPDispValue = ((int)MatchDigit * 10);
      RPU_SetLampState(MATCH, 1);
    }
   
    RPU_SetLampState(GAME_OVER, 1);

    AttractLastHeadMode = 255;
    AttractPlayfieldMode = 255;

    AttractOffset = CurrentTime;  // Create offset for starting Classic Attract
  } // End of CurrentStateChanged

  if ((CurrentTime/6000)%2==0) {          // Displays during attract, X seconds, two states.
  
    if (AttractLastHeadMode!=1) {         // 
      RPU_SetLampState(HIGH_SCORE, 1, 0, 250);
      RPU_SetLampState(GAME_OVER, 0);
      //Serial.println(F("RunnAttractModeMode - SetPlayerLamps(0, 4)"));
      SetPlayerLamps(0, 4);

      LastTimeScoreChanged = CurrentTime;
      //ShowPlayerScores(0xFF, false, false, HighScore);
/*      for (int count=0; count<4; count++) {
        RPU_SetDisplay(count, HighScore, true, 2);
      }*/
      CreditsDispValue = Credits;
    }
    ShowPlayerScores(0xFF, false, false, HighScore);
    AttractLastHeadMode = 1;
    } else {
      if (AttractLastHeadMode!=2) {
        RPU_SetLampState(HIGH_SCORE, 0);
        RPU_SetLampState(GAME_OVER, 1);
        CreditsDispValue = Credits;
        LastTimeScoreChanged = CurrentTime;
        /*for (int count=0; count<4; count++) {
          if (CurrentNumPlayers>0) {
            if (count<CurrentNumPlayers) {
              RPU_SetDisplay(count, CurrentScores[count], true, 2); 
            } else {
              RPU_SetDisplayBlank(count, 0x00);
              RPU_SetDisplay(count, 0);          
            }          
          } else {
            RPU_SetDisplayBlank(count, 0x30);
            RPU_SetDisplay(count, 0);          
          }
        }*/
      }
      SetPlayerLamps(((CurrentTime/250)%4) + 1, 4);
      AttractLastHeadMode = 2;
      ShowPlayerScores(0xFF, false, false); // Show player scores
    }

//
//      Playfield Attract Modes
//

    if ( ((CurrentTime-AttractOffset)/13250)%5 == 0) {
    // 
    //  Classic Eight Ball Attract Mode
    //
      if (AttractPlayfieldMode != 5) {
        //Serial.print(F("Classic Attract set up"));
        //Serial.println();
        RPU_TurnOffAttractLamps();
        AttractStepLights = 0;
        if (DEBUG_MESSAGES) {
        Serial.println("Classic Attract");
        }
      }
      if ((CurrentTime-AttractStepTime)>RackDelayLength) { // global variable msec delay between steps
        if (AttractStepLights==14) {
          RackDelayLength = 5000;
        } else {
          RackDelayLength = 850;
        }
        AttractStepTime = CurrentTime;
        if (AttractStepLights>14) {
          AttractStepLights = 0;                    // Reset after filling rack
          RPU_TurnOffAttractLamps();
        }
        RPU_SetLampState((RACK_BALL_START+AttractStepLights), 1, 0, 0);
        AttractStepLights +=1;
      }
        AttractPlayfieldMode = 5;

    } else {

//
// Second leg of main attract modes
//      
    if (( (CurrentTime-AttractOffset+6750)/10000)%2==0) {
      if (AttractPlayfieldMode!=2) {
        //Spiral animation - code from FG2021 attract
        Serial.write("Spiral Attract\n\r");
        RPU_TurnOffAttractLamps(); // Start mode by blanking all lamps
        AttractSweepLights = 0;
        SpiralIncrement = 1;
      }

#if 0
//  Lamp animations by frames

      //ShowLampAnimation(LampAnimation1, 12, 60, CurrentTime, 11, false, false, 99, false); // Rotating bars rack region
      //ShowLampAnimation(LampAnimation1a, 15, 60, CurrentTime, 9, false, false, 99, false); // Rotating bars outer lamps
      //ShowLampAnimation(LampAnimation2, 13, 30, CurrentTime, 7, false, false, 99, false); // Left-right Sweep
      //ShowLampAnimation(LampAnimation3, 15, 70, CurrentTime, 13, false, false, 99, false); // 3 way sweep
      //ShowLampAnimation(LampAnimation4, 5, 35, CurrentTime, 2, false, false, 99, false); // Right Arrow
      //ShowLampAnimation(LampAnimation5, 5, 35, CurrentTime, 2, false, false, 99, false); // Left Arrow
      //ShowLampAnimation(LampAnimation6, 5, 70, CurrentTime, 3, false, false, 99, false); // Downward V
      ShowLampAnimation(LampAnimation7, 14, 45, CurrentTime, 11, false, false, 99, false); // Downward sweep      
#else
      if ((CurrentTime-AttractSweepTime)>30) { // Value in msec delay between animation steps
        AttractSweepTime = CurrentTime;
        for (byte lightcountdown=0; lightcountdown<NUM_OF_TRIANGLE_LAMPS_CW; lightcountdown++) {
          byte dist = AttractSweepLights - AttractLampsDown[lightcountdown].rowDown;
          RPU_SetLampState(AttractLampsDown[lightcountdown].lightNumDown, (dist<8), (dist==0)?0:dist/3, (dist>5)?(100+AttractLampsDown[lightcountdown].lightNumDown):0);
          if (lightcountdown==(NUM_OF_TRIANGLE_LAMPS_CW/2)) RPU_DataRead(0);
        }

        AttractSweepLights = AttractSweepLights + SpiralIncrement;
        if (AttractSweepLights <= 0 || AttractSweepLights >= 50) {
          SpiralIncrement = -SpiralIncrement;
        }
      }
#endif

      AttractPlayfieldMode = 2;
      
    } else /*if ((CurrentTime/10000)%2==2)*/ {
      if (AttractPlayfieldMode!=3) {
//    Rotating Marquee Lights
        RPU_TurnOffAttractLamps();
        if (DEBUG_MESSAGES) {
          Serial.write("Marquee Attract\n\r");
        }    
      }
//    Marquee Attract function for full playfield.
      MarqueeAttract(4, 100, true);
      AttractPlayfieldMode = 3;
    }

    } // End of loop around playfield animations

// Decide whether we stay in Attract or not.  Watch switches for adding a player or credits.
// Change return state for either INIT_GAMEPLAY or start self test.

  byte switchHit;
  while ( (switchHit=RPU_PullFirstFromSwitchStack())!=SWITCH_STACK_EMPTY ) {
    if (switchHit==SW_CREDIT_RESET) {
      if (AddPlayer(true)) returnState = MACHINE_STATE_INIT_GAMEPLAY;
    } else if (switchHit==SW_COIN_1 || switchHit==SW_COIN_2 || switchHit==SW_COIN_3) {
      AddCoinToAudit(switchHit);
      AddCredit(true);
      CreditsDispValue = Credits;
    } else if (switchHit==SW_SELF_TEST_SWITCH && (CurrentTime-GetLastSelfTestChangedTime())>500) {
      returnState = MACHINE_STATE_TEST_LIGHTS;
      SetLastSelfTestChangedTime(CurrentTime);
    } else {
#ifdef DEBUG_MESSAGES
      char buf[128];
      sprintf(buf, "Switch 0x%02X\n", switchHit);
      Serial.write(buf);
#endif      
    }
  }
//  }
  return returnState;
}


boolean PlayerUpLightBlinking = false;
boolean MarqeeToggle;

int NormalGamePlay() {
  int returnState = MACHINE_STATE_NORMAL_GAMEPLAY;

#if 1
  // If the playfield hasn't been validated yet, flash score and player up num
  if (BallFirstSwitchHitTime==0) {
    if (!PlayerUpLightBlinking) {
      SetPlayerLamps((CurrentPlayer+1), 4, 250);
      PlayerUpLightBlinking = true;
    }
  } else {
    if (PlayerUpLightBlinking) {
      SetPlayerLamps((CurrentPlayer+1), 4);
      PlayerUpLightBlinking = false;

    }
  }
  ShowPlayerScores(CurrentPlayer, (BallFirstSwitchHitTime == 0) ? true : false, 
  (BallFirstSwitchHitTime > 0 && ((CurrentTime - LastTimeScoreChanged) > 4000)) ? true : false);

  if ( (BallFirstSwitchHitTime == 0) && GoalsDisplayValue(Goals[CurrentPlayer]) ) {      // If ball not in play and if any goals have been reached
  //if ( (BallFirstSwitchHitTime == 0) && (CurrentScores[CurrentPlayer] > 100) ) {      // Skip first ball of the game
    //OverrideScoreDisplay( (CurrentPlayer + (CurrentPlayer%2 ? -1:1)), GoalsDisplayValue(Goals[CurrentPlayer]), true);  // Show achieved goals
    for (byte count = 0; count < 4; count++) {
      if (count != CurrentPlayer) {
        OverrideScoreDisplay(count, GoalsDisplayValue(Goals[CurrentPlayer]), false);  // Show achieved goals
      }
    }
    GoalsDisplayToggle = true;
  } else if ( (BallFirstSwitchHitTime > 0) && GoalsDisplayToggle) {
    ShowPlayerScores(0xFF, false, false);                                             //  Reset all score displays
    GoalsDisplayToggle = false;
  }
  
  ShowShootAgainLamp(); // Check if Ball save blinking should finish

#else

  // If the playfield hasn't been validated yet, flash score and player up num
  if (BallFirstSwitchHitTime==0) {
    //RPU_SetDisplayFlash(CurrentPlayer, CurrentScores[CurrentPlayer], CurrentTime, 500, 2);
    if (!PlayerUpLightBlinking) {
      SetPlayerLamps((CurrentPlayer+1), 4, 250);
      PlayerUpLightBlinking = true;
    }
  } else {
    if (PlayerUpLightBlinking) {
      SetPlayerLamps((CurrentPlayer+1), 4);
      PlayerUpLightBlinking = false;

    }
  }
  ShowPlayerScores(CurrentPlayer, (BallFirstSwitchHitTime == 0) ? true : false, 
  (BallFirstSwitchHitTime > 0 && ((CurrentTime - LastTimeScoreChanged) > 4000)) ? true : false);
  ShowShootAgainLamp(); // Check if Ball save blinking should finish

#endif

//
// Game Mode stuff here
//

// Various telemetry here.

#ifdef IN_GAME_TELEMETRY

  if ((CurrentTime - CheckBallTime) > 5000) {  // Check every X seconds
    CheckBallTime = CurrentTime; 
    Serial.println();
    Serial.println(F("----------- Game Telemetry -----------"));  
    Serial.println();
//    Serial.print(F("CheckBallTime is:              "));
//    Serial.println(CheckBallTime, DEC);  
//    Serial.print(F("Current Balls1to7:             "));
//    Serial.println(Balls1to7, BIN);  
    Serial.print(F("GameMode[CrtPlyr] is: "));
    Serial.println(GameMode[CurrentPlayer], DEC);
    Serial.print(F("FifteenBallCounter[CrtPlyr] is: "));
    Serial.println(FifteenBallCounter[CurrentPlayer], DEC);
    Serial.println(F("Current Balls[]: 1------81------8"));
    Serial.print(F("                 "));
    Serial.println(Balls[CurrentPlayer], BIN);  

    Serial.print("Goals CurPlyr is: ");
    Serial.println(Goals[CurrentPlayer], BIN);
    //Serial.println(Goals[0], BIN);
    //Serial.println(Goals[1], BIN);
    //Serial.println(Goals[2], BIN);
    //Serial.println(Goals[3], BIN);
    Serial.println();

//    Serial.print("BallFirstSwitchHitTime is: ");
//    Serial.println(BallFirstSwitchHitTime, DEC);  

//    Serial.print(F("ArrowsLit Crt Plr is:         "));
//    Serial.println(ArrowsLit[CurrentPlayer], DEC);
//    Serial.print(F("ArrowTest is:                 "));
//    Serial.println(ArrowTest, DEC);

//    Serial.print(F("SpinnerKickerLit is:           "));
//    Serial.println(SpinnerKickerLit, DEC);  

//    Serial.println();
//    Serial.print(F("BankShotProgress is:           "));
//    Serial.println(BankShotProgress, DEC);  

//    Serial.print(F("BonusMult is:                  "));
//    Serial.println(BonusMult, DEC);  

//    Serial.print(F("OutlaneSpecial CurPlr is:     "));
//    Serial.println(OutlaneSpecial[CurrentPlayer], DEC);  

//    Serial.print(F("EightBallTest - Cur Plr is:    "));
//    Serial.println(EightBallTest[CurrentPlayer], DEC);  

//    Serial.print(F("SuperBonusLit       is:        "));
//    Serial.println(SuperBonusLit, DEC);  

//    Serial.print(F("PopCount is:                   "));
//    Serial.println(PopCount[CurrentPlayer], DEC);  
//    Serial.print(F("PopMode is:                    "));
//    Serial.println(PopMode[CurrentPlayer], DEC);  
//    Serial.print(F("SpinnerCount is:                   "));
//    Serial.println(SpinnerCount[CurrentPlayer], DEC);  
//    Serial.print("SuperSpinnerAllowed CrtPlyr is: ");
//    Serial.println(SuperSpinnerAllowed[CurrentPlayer], DEC);  

//    Serial.print(F("CurrentNumPlayers:           "));
//    Serial.println(CurrentNumPlayers, DEC);  
//    Serial.println();

    Serial.print(F("ChaseBall stage is: "));
    Serial.println(ChaseBallStage, DEC);
    Serial.print(F("ChaseBall is:       "));
    Serial.println(ChaseBall, DEC);

    Serial.print(F("NextBall is:        "));
    Serial.println(NextBall, DEC);
    Serial.print(F("NextBallTime is:    "));
    Serial.println(NextBallTime, DEC);

    Serial.print(F("OverrideScorePriority is:        "));
    Serial.println(OverrideScorePriority, DEC);

    Serial.println();
    Serial.println();  

  }
#endif

//
//  15 Ball mode qualified animation
//

if (FifteenBallQualified[CurrentPlayer]) {
  ShowLampAnimation(LampAnimation7, 14, 45, CurrentTime, 11, false, false, 99, false); // Downward sweep
  //ShowLampAnimation(LampAnimation7, 14, 40, CurrentTime, 5, false, false, 99, false); // Downward sweep
}


//
//  Chase Ball - Mode 3
//

  if ( GameMode[CurrentPlayer] == 3  && ((CurrentTime - ChaseBallTimeStart) < (ChaseBallDuration*1000)) ) {
    // Speed up based on duration
    int LampBlinkTime = 300;
    int AnimationRate = 70;
/*    if ((CurrentTime - ChaseBallTimeStart) < CHASEBALL_DURATION) {
      // 450 is total blink duration reduction (from 500 down to 50)
      LampBlinkTime = (300 - ((CurrentTime - ChaseBallTimeStart)*260/CHASEBALL_DURATION));
      AnimationRate = (150 - ((CurrentTime - ChaseBallTimeStart)*110/CHASEBALL_DURATION));
    } else {
      LampBlinkTime = 40;
      AnimationRate = 40;
    }*/

    if ((CurrentTime - ChaseBallTimeStart) < ((ChaseBallDuration*1000)/2)) {
      LampBlinkTime = 200;
      AnimationRate = 100;
      //Serial.println("Step 1");
    } else if ((CurrentTime - ChaseBallTimeStart) < ((ChaseBallDuration*1000)*3/4)) {
      LampBlinkTime = 100;
      AnimationRate = 75; 
      //Serial.println("Step 2--");
    } else {
      LampBlinkTime = 40;
      AnimationRate = 40;
      //Serial.println("Step 3---");
    }

    RPU_SetLampState((LA_SMALL_1 - 1 + ChaseBall), 1, 0, LampBlinkTime);
    ShowLampAnimation(LampAnimation3, 15, (AnimationRate), CurrentTime, 13, false, false, 99, false); // 3 way sweep

    if (OVERRIDE_PRIORITY_CHASEBALLMODE3 > OverrideScorePriority) {   // Check if priority should be raised
      OverrideScorePriority = OVERRIDE_PRIORITY_CHASEBALLMODE3;
    }
    if (OverrideScorePriority == OVERRIDE_PRIORITY_CHASEBALLMODE3) {
      for (byte count = 0; count < 4; count++) {
        if (count != CurrentPlayer) {
          OverrideScoreDisplay(count, (ChaseBallDuration - ((CurrentTime - ChaseBallTimeStart)/1000)), true); // Show time left
        }
      }
    }
  } else if ( GameMode[CurrentPlayer] == 3 ) {
    // ChaseBall has timed out, reset back to Mode 1 regular play
    PlaySoundEffect(SOUND_EFFECT_BALL_LOSS);
    GameMode[CurrentPlayer] = 1;
    ChaseBallTimeStart = 0;                 // Zero clock to halt mode
    ChaseBall = 0;
    BallLighting();
    BankShotLighting();
    OverrideScorePriority = 0;              //  Set back to zero
    ShowPlayerScores(0xFF, false, false);   //  Reset all score displays
  }


//
// Next Ball
//

  if ( (NextBall != 0) && ((CurrentTime - NextBallTime) < (NextBallDuration*1000)) ) {
    if (!(GameMode[CurrentPlayer] == 3)) {
      // Set next ball to rapid flashing
      RPU_SetLampState((LA_SMALL_1 + (NextBall-1) + (CurrentPlayer%2 ? 8:0)), 1, 0, 100);
      
      if (OVERRIDE_PRIORITY_NEXTBALL > OverrideScorePriority) {       // Check if priority should be raised
        OverrideScorePriority = OVERRIDE_PRIORITY_NEXTBALL;
      }
      if (OverrideScorePriority == OVERRIDE_PRIORITY_NEXTBALL) {
        for (byte count = 0; count < 4; count++) {
          if (count != CurrentPlayer) {
            // Show time left
            OverrideScoreDisplay(count, (NextBallDuration - ((CurrentTime - NextBallTime)/1000)), false);
          }
        }
      } 
    }
  } else if (NextBallTime != 0){
    #ifdef EXECUTION_MESSAGES
    Serial.println("---- Set NextBall lamp to steady ----");
    #endif
    if (!(GameMode[CurrentPlayer] == 3)) {
      RPU_SetLampState((LA_SMALL_1 + (NextBall-1) + (CurrentPlayer%2 ? 8:0)), 1);  // Revert to steady on
    }
    NextBall = 0;
    NextBallTime = 0;
    //BallLighting();
    OverrideScorePriority = 0;              //  Set back to zero
    ShowPlayerScores(0xFF, false, false);   //  Reset all score displays
  }


//
//  ChimeScoring
//

  if ((Silent_Hundred_Pts_Stack != 0) || (Silent_Thousand_Pts_Stack != 0) || (Ten_Pts_Stack != 0) || (Hundred_Pts_Stack != 0) || (Thousand_Pts_Stack != 0)) {
    ChimeScoring();
  }

//
// 8 Ball target lighting - If 7 balls collected, turn on 8 Ball target
// Rev 2
//

  if ( (((Balls[CurrentPlayer] & (0b01111111<<(CurrentPlayer%2 ? 8:0)))>>(CurrentPlayer%2 ? 8:0))==127) && (EightBallTest[CurrentPlayer]) ) {
    EightBallTest[CurrentPlayer] = false;
    //Serial.println(F("Turning on small ball 8"));
    //Serial.println();
    RPU_SetLampState(LA_SMALL_8, 1);
  }


//
//  ArrowsLit mode - Set to 1 when lanes 1-4 collected
//  Ver 4 - Split GameMode logic

  if (GameMode[CurrentPlayer] == 1) {
    if ( (((Balls[CurrentPlayer] & (0b1111<<(CurrentPlayer%2 ? 8:0)))>>(CurrentPlayer%2 ? 8:0))==15) && (ArrowTest) ) {
      #ifdef EXECUTION_MESSAGES
      Serial.println(F("ArrowsLit Test - GameMode = 1"));
      #endif
      ArrowsLit[CurrentPlayer] = true;
      ArrowTest = false;
      ArrowToggle();
    }
  } else if (GameMode[CurrentPlayer] == 2) {
    if ( ((Balls[CurrentPlayer] & (0b1111) | ((Balls[CurrentPlayer] & (0b1111<<8)) >> 8))==15) && (ArrowTest) ) {
      #ifdef EXECUTION_MESSAGES
      Serial.println(F("ArrowsLit Test - GameMode = 2"));
      #endif
      ArrowsLit[CurrentPlayer] = true;
      ArrowTest = false;
      ArrowToggle();
    }
  }

//
// Super Spinner Mode - Ver 2 - Spinner Combo
// 

  if ( (SuperSpinnerAllowed[CurrentPlayer]) && ((CurrentTime - SuperSpinnerTime) < (SuperSpinnerDuration*1000)) ) {
    if (OVERRIDE_PRIORITY_SPINNERCOMBO > OverrideScorePriority) {       // Check if priority should be raised
      OverrideScorePriority = OVERRIDE_PRIORITY_SPINNERCOMBO;
    }
    if (OverrideScorePriority == OVERRIDE_PRIORITY_SPINNERCOMBO) {
      for (byte count = 0; count < 4; count++) {
        if (count != CurrentPlayer) {
          // Show time left
          OverrideScoreDisplay(count, (SuperSpinnerDuration - ((CurrentTime - SuperSpinnerTime)/1000)), false);
        }
      }
    }    
  } else if (SuperSpinnerAllowed[CurrentPlayer]) {
    SpinnerComboFinish();                       // Turn off mode, release marquee animation, etc
    BallLighting();
    if (SpinnerKickerLit) {
      RPU_SetLampState(LA_SPINNER, 1);        
    } else if ( (!SpinnerKickerLit) && (KickerOffTime != 0) ) {
      RPU_SetLampState(LA_SPINNER, 1, 0, 100);
    } else {
      RPU_SetLampState(LA_SPINNER, 0);
    }
  }


//
//  SpinnerKickerLit mode - delayed Kicker turn off
//

  if ( ((CurrentTime - KickerOffTime) > 3000) && (KickerOffTime != 0) ) {
    KickerReady = false;
    RPU_SetLampState(LA_SPINNER, 0);
    KickerOffTime = 0;
  }

// Bank Shot Animation
  if ( ((CurrentTime - BankShotOffTime) < (750+(BankShotProgress*100)) /*&& (BankShotOffTime != 0)*/) ) {
    //Serial.println(F("Bank Shot Animation running"));
    // Call Marquee Attract for BankShot portion
    MarqueeAttract(2 , 80, false);
  } else if (BankShotOffTime != 0) {
    //Serial.println(F("Bank Shot Animation calling BankShotLighting"));
    //Serial.println();
    BankShotLighting();
    BankShotOffTime = 0;
  }

// Marquee Animation - Ver 3
  if ( (CurrentTime - MarqueeOffTime) < (600 * MarqueeMultiple) ) {
    MarqueeAttract(1 , 100, MarqeeToggle);        // Call Ball Rack portion only
    if (Balls[CurrentPlayer] & (0b10000000)) {    // Call additional portions if 8 ball captured
      MarqueeAttract(2 , 100, MarqeeToggle);
      MarqueeAttract(3 , 100, MarqeeToggle);
    }
  } else if (MarqueeOffTime != 0) {
    //Serial.print(F("MarqueeDisabled is:"));
    //Serial.println(MarqueeDisabled, DEC);
    if (!MarqueeDisabled) BallLighting();
    if (Balls[CurrentPlayer] & (0b10000000)) {
      BonusMultiplier(BonusMult);
      BankShotLighting();
    }
    MarqueeOffTime = 0;
    MarqeeToggle = !MarqeeToggle;
  }


  // Check to see if ball is in the outhole
  if (RPU_ReadSingleSwitchState(SW_OUTHOLE)) {
    if (BallTimeInTrough==0) {
      BallTimeInTrough = CurrentTime;
    } else {  // -1-
      // Make sure the ball stays on the sensor for at least 
      // 0.5 seconds to be sure that it's not bouncing
      if ((CurrentTime-BallTimeInTrough)>500) {  // -2-
        if (BallFirstSwitchHitTime == 0 && !Tilted) {
//        if (BallFirstSwitchHitTime == 0 && NumTiltWarnings <= MaxTiltWarnings) {
          // Nothing hit yet, so return the ball to the player
          RPU_PushToTimedSolenoidStack(SOL_OUTHOLE, 4, CurrentTime);
          BallTimeInTrough = 0;
          returnState = MACHINE_STATE_NORMAL_GAMEPLAY;
        } else {  // -3-
//        if (BallFirstSwitchHitTime==0) BallFirstSwitchHitTime = CurrentTime;
        
        // if we haven't used the ball save, and we're under the time limit, then save the ball
        if (  !BallSaveUsed && 
              ((CurrentTime-BallFirstSwitchHitTime)/1000)<((unsigned long)BallSaveNumSeconds) ) {
        
          RPU_PushToTimedSolenoidStack(SOL_OUTHOLE, 4, CurrentTime + 100);
          if (BallFirstSwitchHitTime>0) {
            BallSaveUsed = true;
            RPU_SetLampState(SAME_PLAYER, 0);
//            RPU_SetLampState(HEAD_SAME_PLAYER, 0);
          }
          BallTimeInTrough = CurrentTime;

          returnState = MACHINE_STATE_NORMAL_GAMEPLAY;          
        } else {
          if (GameMode[CurrentPlayer] == 3) {           // GameMode 3 ends with the ball
            (GameMode[CurrentPlayer] = 1);
          }
          ChaseBall = 0;                                // Reset Mode 3 in case active
          ChaseBallStage = 0;
          BIPDispValue = CurrentBallInPlay;             // Restore display in case pop count is displayed
          CreditsDispValue = Credits;                   // Restore display in case spinner count is displayed
          returnState = MACHINE_STATE_COUNTDOWN_BONUS;
        }
        } // -3-
      }  // -2-
    } // -1-
  } else {
    BallTimeInTrough = 0;
  }

  return returnState;
}



unsigned long InitGameStartTime = 0;
unsigned long InitGamePlayChime = 0;
boolean ChimeTrigger = true;

int InitGamePlay(boolean curStateChanged) {
  int returnState = MACHINE_STATE_INIT_GAMEPLAY;

  if (curStateChanged) {
    InitGameStartTime = CurrentTime;
    RPU_SetCoinLockout((Credits>=MaximumCredits)?true:false);
    RPU_SetDisableFlippers(true);
    RPU_DisableSolenoidStack();
    RPU_TurnOffAllLamps();
    BIPDispValue = 1;
    //RPU_SetDisplayBallInPlay(1);
    if (Credits > 0) {
      RPU_SetLampState(LA_CREDIT_INDICATOR, 1);
    }
    InitGamePlayChime = CurrentTime+2500;
    ChimeTrigger = true;

    // Combined array variables for initialization

    for (int count=0; count<4; count++) {  // Set all to not captured
      OutlaneSpecial[count] = false;
      ArrowsLit[count] = false;
      EightBallTest[count] = true;
      PopCount[count]=0;
      //PopCountTest[count] = true;
      PopMode[count] = 0;
      Balls[count] = 0;
      Goals[count] = 0;
      FifteenBallQualified[count] = false;
      GameMode[count] = 1;              // default GameMode is 1
      FifteenBallCounter[count] = 0;
      SpinnerCount[count]=0;
      SpinnerMode[count]=0;
      SuperSpinnerAllowed[count] = false;
    }

    // Test value - force ball settings
    //           1   1 1
    //           6---2-0987654321
    //Balls[0] = 0b0101010100000000;
    //Balls[1] = 0b0000000000010101;
    //GameMode[0]=2;
    //
    // Bit 1 - SuperBonus reached
    // Bit 2 - NextBall met once
    // Bit 3 - Pop Mode > x
    // Bit 4 - Spinner Mode > x
    // Bit 5 - Spinner Combo achieved x times
    // Bit 6 - Scramble Ball
    // Bit 7 - 3 Goals achieved
    // Bit 8 - 5 Goals achieved
    //Goals[0]=0b00111100;
    
    // Turn on all small ball lights to start game
    for (int count=0; count<7; count++) {
      RPU_SetLampState((LA_SMALL_1+count+8), 1);
    }
    Serial.write("InitGame - state changed\n");

    // Set up general game variables

    CurrentPlayer = 0;
    CurrentNumPlayers = 1; // Added to match Meteor code
    CurrentBallInPlay = 1;
    SetPlayerLamps(CurrentNumPlayers);
    
    for (int count=0; count<4; count++) CurrentScores[count] = 0;
    // if the ball is in the outhole, then we can move on
    if (RPU_ReadSingleSwitchState(SW_OUTHOLE)) {
      Serial.println(F("InitGame - Ball in trough"));
      RPU_EnableSolenoidStack();
      RPU_SetDisableFlippers(false);
      returnState = MACHINE_STATE_INIT_NEW_BALL;
    } else {
   
    // Otherwise, let's see if it's in a spot where it could get trapped,
    // for instance, a saucer (if the game has one)
    //RPU_PushToSolenoidStack(SOL_SAUCER, 5, true);

    // And then set a base time for when we can continue
    InitGameStartTime = CurrentTime;
    }

    for (int count=0; count<4; count++) {
      RPU_SetDisplay(count, 0);
      RPU_SetDisplayBlank(count, 0x00);
    }
    ShowPlayerScores(0xFF, false, false);   //  Reset all score displays
  }


//
// Play game start tune
//

  if (ChimeTrigger) {
    ChimeTrigger = false;
    PlaySoundEffect(SOUND_EFFECT_GAME_START);
  }

  // Wait for TIME_TO_WAIT_FOR_BALL seconds, or until the ball appears
  // The reason to bail out after TIME_TO_WAIT_FOR_BALL is just
  // in case the ball is already in the shooter lane.
  if ((CurrentTime-InitGameStartTime)>TIME_TO_WAIT_FOR_BALL || RPU_ReadSingleSwitchState(SW_OUTHOLE)) {
    RPU_EnableSolenoidStack();
    RPU_SetDisableFlippers(false);
    returnState = MACHINE_STATE_INIT_NEW_BALL;
  }

  if (CurrentTime<=InitGamePlayChime) {
    returnState = MACHINE_STATE_INIT_GAMEPLAY;
  }

  return returnState;  
}


//
// Bonus Countdown - Ver 4
// SuperBonus handling added
// 15 Ball mode partially added
//

boolean CountdownDelayActive = false;
byte ChimeLoop = 0;
byte MaskCount = 0;
int BonusSpace = 117;
unsigned long NextBonusUpdate = 0;
unsigned long BonusWaitTime = 0;
unsigned int BonusBalls = 0;

int CountdownBonus(boolean curStateChanged) {

  if (curStateChanged) {
    CreditsFlashing = false;                //  Stop flashing
    BIPFlashing = false;                    //  Stop flashing
    ShowPlayerScores(0xFF, false, false);   //  Reset all score displays
    BallLighting();                         // If we tilted balls would be out.
    RPU_SetLampState(LA_SPINNER, 0);
    if (GameMode[CurrentPlayer] == 2) RPU_SetLampState(LA_SUPER_BONUS, 0);
    MaskCount = 0;
    ChimeLoop = 0;
    BonusSpace = (!Tilted?267:134); // Interchime is 117, group gap is 267 msec
    NextBonusUpdate = CurrentTime;
    CountdownDelayActive = false;
    BonusWaitTime = 0;
    BonusBalls = Balls[CurrentPlayer];
    Serial.println(F("-----------------CountdownBonus-----------------"));
  }

// Code for counting down balls

  if (MaskCount < 15) {                                   // Count out all 15 balls
    if (BonusBalls & (0b1<<MaskCount)) {                  // If true, ball has been captured
      if ( ChimeLoop < (3 * ((!Tilted)?BonusMult:1)) ) {  // Add bonus multiplier to total
        if ((CurrentTime - NextBonusUpdate) > BonusSpace) {
          if (!Tilted) {
            RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, CurrentTime + 0, true);
            CurrentScores[CurrentPlayer] += 1000;
            if (GameMode[CurrentPlayer] == 2) {
              RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, CurrentTime + 0, true);
              CurrentScores[CurrentPlayer] += 1000;
            }
          }
          NextBonusUpdate = CurrentTime;
          ++ChimeLoop;
          if (ChimeLoop == 1) BonusSpace = ((!Tilted)?117:58);  // Create gap between chime groupings
        }
      } else {
          RPU_SetLampState((LA_BIG_1 + MaskCount), 0);                // Turn off lamp after chime sequence
          ChimeLoop = 0;
          BonusSpace = ((!Tilted)?267:134);  // Reset back to interchime spacing
//          BonusSpace = ((NumTiltWarnings < MaxTiltWarnings)?267:134);  // Reset back to interchime spacing
          ++MaskCount;
        }
    } else {
        ++MaskCount;
      }
    RPU_SetDisplay(CurrentPlayer, CurrentScores[CurrentPlayer], true);  
    return MACHINE_STATE_COUNTDOWN_BONUS;
  } 
  if (MaskCount == 15 && CountdownDelayActive == false) {
    BonusWaitTime = CurrentTime + 1000;
    CountdownDelayActive = true;
    return MACHINE_STATE_COUNTDOWN_BONUS;
  }
  if (CurrentTime < BonusWaitTime && CountdownDelayActive == true) {  //Delays transition back to regular play
    return MACHINE_STATE_COUNTDOWN_BONUS;
  }

  // Add loop to deal with SuperBonus
  if (BonusBalls & (0b1<<15)) {                            // Check if SuperBonus achieved
    MaskCount = 0;
    ChimeLoop = 0;
    BonusSpace = 267;                                   
    NextBonusUpdate = CurrentTime;
    CountdownDelayActive = false;
    BonusWaitTime = 0;
    RPU_SetLampState(LA_SUPER_BONUS, 0);
    if (GameMode[CurrentPlayer] == 1) {
      BonusBalls = (0b11111111<<(CurrentPlayer%2 ? 7:0)); // Reassign without SuperBonus so BonusCountdown can exit.
      //  Light 8 balls
      for(int i=0; i<8; i++) {
        RPU_SetLampState((LA_BIG_1 + i + (CurrentPlayer%2 ? 7:0)), 1);
      }
    } else {                                              // if 15 Ball Mode
      BonusBalls = 0x7FFF; // Reassign without SuperBonus so BonusCountdown can exit.
      //  Light 15 balls
      for(int i=0; i<15; i++) {
        RPU_SetLampState((LA_BIG_1 + i), 1);
      }
    }
    return MACHINE_STATE_COUNTDOWN_BONUS;
  }

    Serial.print(F("Bonus Countdown - end\n"));
    return MACHINE_STATE_BALL_OVER;

// End of CoundownBonus
} 

unsigned int BallTemp;

int RunGamePlayMode(int curState, boolean curStateChanged) {
  int returnState = curState;
  unsigned long scoreAtTop = CurrentScores[CurrentPlayer];
  
  // Very first time into gameplay loop
  if (curState==MACHINE_STATE_INIT_GAMEPLAY) {
    returnState = InitGamePlay(curStateChanged);    
  } else if (curState==MACHINE_STATE_INIT_NEW_BALL) {
    returnState = InitNewBall(curStateChanged, CurrentPlayer, CurrentBallInPlay);
  } else if (curState==MACHINE_STATE_NORMAL_GAMEPLAY) {
    returnState = NormalGamePlay();
  } else if (curState==MACHINE_STATE_COUNTDOWN_BONUS) {
    returnState = CountdownBonus(curStateChanged);
  } else if (curState==MACHINE_STATE_BALL_OVER) {
    if (SamePlayerShootsAgain) {
      returnState = MACHINE_STATE_INIT_NEW_BALL;
    } else {
      CurrentPlayer+=1;
      if (CurrentPlayer>=CurrentNumPlayers) {
        CurrentPlayer = 0;
        CurrentBallInPlay+=1;
      }
        
      if (CurrentBallInPlay>BallsPerGame) {
        CheckHighScores();
        PlaySoundEffect(SOUND_EFFECT_GAME_OVER);
        SetPlayerLamps(0, 4);
        for (int count=0; count<CurrentNumPlayers; count++) {
          RPU_SetDisplay(count, CurrentScores[count], true, 2);
        }

        returnState = MACHINE_STATE_MATCH_MODE;
      }
      else returnState = MACHINE_STATE_INIT_NEW_BALL;
    }
  } else if (curState==MACHINE_STATE_MATCH_MODE) {
    //Serial.print(F("Run ShowMatchSequence\n"));
    returnState = ShowMatchSequence(curStateChanged);
//  This line does nothing but force another pass until Attract gets called since nothing matches state
//    returnState = MACHINE_STATE_GAME_OVER;    
  } else {
    returnState = MACHINE_STATE_ATTRACT;
  }

  byte switchHit;
    while ( (switchHit=RPU_PullFirstFromSwitchStack())!=SWITCH_STACK_EMPTY ) {   // -A-

      if (!Tilted) {
  
      switch (switchHit) {
        case SW_SELF_TEST_SWITCH:
          returnState = MACHINE_STATE_TEST_LIGHTS;
          SetLastSelfTestChangedTime(CurrentTime);
          break; 

  //
  // Eight Ball game switch responses here
  //
  
        case SW_10_PTS:
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          Ten_Pts_Stack += 1;
          //RPU_PushToSolenoidStack(SOL_CHIME_10, 3);
          //CurrentScores[CurrentPlayer] += 10;
          if (ArrowsLit[CurrentPlayer]) ArrowToggle();
          break;
        case SW_TOP_LEFT_TARGET:
          //Hundred_Pts_Stack += 5;
          //
          // Scramble ball
          //
          //if ((PopMode[CurrentPlayer]) > 0) {
          if ( ((PopMode[CurrentPlayer]) > 0) && !(GameMode[CurrentPlayer] == 3) ){
            //Serial.print(F("CurrentTime%6 is : "));
            //Serial.println(CurrentTime%6,DEC);
            //Serial.println();
            for (int i = 0; i < (1 + CurrentTime%5); i++) {   // Rotate balls by 1 to 6 slots
              BallTemp  = ( (Balls[CurrentPlayer] & (0b11111100111111)) << 1) + ( (Balls[CurrentPlayer] & (0b100000001000000)) >> 6);
              Balls[CurrentPlayer] = Balls[CurrentPlayer] & (0b1000000010000000);                 // Clear ball bits
              Balls[CurrentPlayer] = Balls[CurrentPlayer] | BallTemp;                             // Set ball bits
              #ifdef EXECUTION_MESSAGES
              Serial.print(F("ScrambleBall jumped by: "));
              Serial.println((1 + CurrentTime%5),DEC);
              #endif
            }
            SetGoals(6);                                      // Set goal as completed
            NextBall = 0;                                     // Cancel NextBall in case active
            NextBallTime = 0;
            MarqueeTiming();
            PlaySoundEffect(SOUND_EFFECT_BALL_CAPTURE);
            Silent_Thousand_Pts_Stack += 2;
            Silent_Hundred_Pts_Stack += 5;
          } else {
            Hundred_Pts_Stack += 5;
          }
          break;
        case SW_7_15_BALL:
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(7)) {  // If ball already captured, CaptureBall is false
            Hundred_Pts_Stack += 5;
          }
          break;
        case SW_6_14_BALL:
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(6)) {  // If ball already captured, CaptureBall is false
            Hundred_Pts_Stack += 5;
          }
          break;
        case SW_5_13_BALL:
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(5)) {  // If ball already captured, CaptureBall is false
            Hundred_Pts_Stack += 5;
          }
          break;
        case SW_4_12_BALL:
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(4)) {  // If ball already captured, CaptureBall is false
            Hundred_Pts_Stack += 5;
          }
          break;
        case SW_3_11_BALL: // Balls[]
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(3) || (GameMode[CurrentPlayer] == 2)) {  // If ball already captured, CaptureBall is false
            if (ArrowsLit[CurrentPlayer] && RightArrow) {
              if (BankShotProgress < 4) {
                if (GameMode[CurrentPlayer] != 2) {
                  Hundred_Pts_Stack += 5;
                } 
              } else {
                Silent_Hundred_Pts_Stack += 5;
              }
              BankShotScoring((GameMode[CurrentPlayer] == 2)? 0:1); // Call BankShotScoring
            } else {
              if (GameMode[CurrentPlayer] != 2) {
                Hundred_Pts_Stack += 5;
              } /*else {
                Silent_Hundred_Pts_Stack += 5;
              }*/
            }
          }
          break;
        case SW_2_10_BALL: // Balls[]
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(2) || (GameMode[CurrentPlayer] == 2)) {  // If ball already captured, CaptureBall is false
            if (ArrowsLit[CurrentPlayer] && !RightArrow) {
              if (BankShotProgress < 4) {
                if (GameMode[CurrentPlayer] != 2) {
                  Hundred_Pts_Stack += 5;
                }
              } else {
                Silent_Hundred_Pts_Stack += 5;
              }
              BankShotScoring((GameMode[CurrentPlayer] == 2)? 0:1); // Call BankShotScoring
            } else {
              if (GameMode[CurrentPlayer] != 2) {
                Hundred_Pts_Stack += 5;
              } /*else {
                Silent_Hundred_Pts_Stack += 5;
              }*/
            }
          }
          break;
        case SW_1_9_BALL: // Balls[]
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          if (!CaptureBall(1)) {  // If ball already captured, CaptureBall is false
            Hundred_Pts_Stack += 5;
          }
          break;
        case SW_8_BALL:
          switch (GameMode[CurrentPlayer]) {
            case 1:
              if (((Balls[CurrentPlayer] & (0b11111111<<(CurrentPlayer%2 ? 7:0)))>>(CurrentPlayer%2 ? 7:0))==(CurrentPlayer%2 ? 254:127)) { // Check 8 bits, shift by 7 since 8 is common.
                // Collect 8 Ball turn out small lamp
                RPU_SetLampState(LA_SMALL_8, (MarqueeDisabled)?0:1, 0, 75);
                MarqueeTiming();    // Call multiple times for longer animmation length
                MarqueeTiming();
                MarqueeTiming();
                MarqueeTiming();
                #ifdef EXECUTION_MESSAGES
                Serial.print(F("-------- Regular 8 Ball awarded --------"));
                #endif
                RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 900, true);
                Balls[CurrentPlayer] = (Balls[CurrentPlayer] | 0b10000000);   // Raise flag for 8 Ball
                PlaySoundEffect(SOUND_EFFECT_8_BALL_CAPTURE);
                Silent_Hundred_Pts_Stack += 5;
                } else {
                  Hundred_Pts_Stack += 5;
                }
                break;
            case 2:
              if ((Balls[CurrentPlayer] & (0x7FFF))==(0x7F7F)) { // Check balls 1-7, 9-15.
                // Collect 8 Ball turn out small lamp
                RPU_SetLampState(LA_SMALL_8, (MarqueeDisabled)?0:1, 0, 75);
                MarqueeTiming();    // Call multiple times for longer animmation length
                MarqueeTiming();
                MarqueeTiming();
                MarqueeTiming();
                MarqueeTiming();
                MarqueeTiming();
                #ifdef EXECUTION_MESSAGES
                Serial.print(F("-------- 15 Ball 8 Ball awarded --------"));
                #endif
                RPU_SetDisableFlippers(true);
                FifteenBallCounter[CurrentPlayer] += 5;                       // Force mode to reset
                SamePlayerShootsAgain = true;                                 // Save players ball
                RPU_SetLampState(SAME_PLAYER, 1, 0, 150);
                RPU_SetLampState(LA_SPSA_BACK, 1, 0, 175);
                RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 900, true);
                RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1000, true);
                RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1100, true);
                Balls[CurrentPlayer] = (Balls[CurrentPlayer] | 0b10000000);   // Raise flag for 8 Ball
                PlaySoundEffect(SOUND_EFFECT_8_BALL_CAPTURE);
                Silent_Thousand_Pts_Stack += 100;
                } else {
                  Hundred_Pts_Stack += 5;
                }
                break;
          }
          if (!SpinnerKickerLit) {
            SpinnerKickerLit = true;
            RPU_SetLampState(LA_SPINNER, 1);
            KickerReady = true;  // KickerReady always true when SpinnerKickerLit
            KickerUsed = false;
            KickerOffTime = 0;
          }  
          break;
        case SW_SPINNER:
          SpinnerCount[CurrentPlayer] = ++SpinnerCount[CurrentPlayer];
          if (SpinnerCount[CurrentPlayer] > (Spinner_Threshold - 1)) {
            SpinnerMode[CurrentPlayer] += 1;
            SetGoals(4);
            SpinnerCount[CurrentPlayer] = 0;
            if (SpinnerMode[CurrentPlayer] > 1) {
              SuperSpinnerAllowed[CurrentPlayer] = true;
              MarqueeDisabled = true;
              ClearRack();
              FlashingArrows(RackRightArrow, 125);
            }
          }
          #ifdef EXECUTION_MESSAGES
          Serial.print(F("Spinner Mode is:  "));
          Serial.println(SpinnerMode[CurrentPlayer], DEC);  
          Serial.print(F("Spinner Count is: "));
          Serial.println(SpinnerCount[CurrentPlayer], DEC);  
          #endif
          // Update timer
          if ( (SpinnerMode[CurrentPlayer] > 1) && (SuperSpinnerAllowed[CurrentPlayer] == true)/* && (SuperSpinnerTime == 0)*/ ){
            SuperSpinnerTime = CurrentTime;
            //Serial.print(F("SuperSpinnerTime is: "));
            //Serial.println(SuperSpinnerTime, DEC);  
          }
          //
          // Award SuperSpinnerCombo
          //
          if ( (SuperSpinnerAllowed[CurrentPlayer] == true) && (SpinnerComboHalf) ) {
            //SuperSpinnerAllowed[CurrentPlayer] == false;
            SpinnerComboHalf = false;
            SuperSpinnerDuration = 0;
            Silent_Thousand_Pts_Stack +=25;
            PlaySoundEffect(SOUND_EFFECT_SPINNER_COMBO);
            SetGoals(5);
          }
          if ( (SpinnerCount[CurrentPlayer] > (Spinner_Threshold - (Spinner_Threshold*.66))) && (SpinnerCount[CurrentPlayer] < Spinner_Threshold) ) {
          //if ( (SpinnerCount[CurrentPlayer] > (Spinner_Threshold - SPINNER_SHOW_IN_DISPLAY)) && (SpinnerCount[CurrentPlayer] < Spinner_Threshold) ) {
            CreditsFlashing = true;
            CreditsDispValue = (Spinner_Threshold - SpinnerCount[CurrentPlayer]);
          } else {
            CreditsFlashing = false;
            CreditsDispValue = Credits;
            //RPU_SetDisplayCredits(Credits);
          }
          // Spinner scoring
          if (!SpinnerKickerLit) {
            PlaySoundEffect(SOUND_EFFECT_10_PTS);
            CurrentScores[CurrentPlayer] += 10;
          } else if (SpinnerMode[CurrentPlayer] < 1){ 
            PlaySoundEffect(SOUND_EFFECT_1000_PTS);
            CurrentScores[CurrentPlayer] += 1000;
          } else {
            PlaySoundEffect(SOUND_EFFECT_10_PTS + SpinnerDelta);
            SpinnerDelta += 1;
            if (SpinnerDelta > 2) SpinnerDelta = 0;
            CurrentScores[CurrentPlayer] += ((SpinnerMode[CurrentPlayer] == 1)?500:0);
            //Silent_Hundred_Pts_Stack +=((SpinnerMode[CurrentPlayer] == 1)?5:0);
            Silent_Thousand_Pts_Stack +=1;
            if (SpinnerMode[CurrentPlayer] > 1) {
              PlaySoundEffect(SOUND_EFFECT_EXTRA);
              Silent_Thousand_Pts_Stack +=1;
            }
          }
          if (ArrowsLit[CurrentPlayer]) ArrowToggle();
          break;
        case SW_LEFT_OUTLANE:
          if (!KickerReady) {
            RPU_PushToSolenoidStack(SOL_CHIME_1000, 3);
            CurrentScores[CurrentPlayer] += 1000;
          } else if ((KickerReady) && (!KickerUsed)) {
              SpinnerKickerLit = false;                   //Kicker turns off SpinnerKicker mode, leaves KickerReady running
              RPU_SetLampState(LA_SPINNER, 1, 0, 100);   // Rapid blinking
              RPU_PushToSolenoidStack(SOL_KICKER, 4);
              Thousand_Pts_Stack += 5;
              KickerUsed = true;
              KickerOffTime = CurrentTime; // X seconds delay starting point
          } else {  // if ((KickerReady) && (KickerUsed)) - only case left
              RPU_PushToSolenoidStack(SOL_KICKER, 4);
              PlaySoundEffect(SOUND_EFFECT_KICKER_OUTLANE);
              //MultiChime(SOL_CHIME_EXTRA, 5);           // Sound to indicate ball saved, no extra scoring
          }
          break;
        case SW_RIGHT_OUTLANE:
          if (!OutlaneSpecial[CurrentPlayer]) {
          RPU_PushToSolenoidStack(SOL_CHIME_1000, 3);
          CurrentScores[CurrentPlayer] += 1000;          
          } else {
          Thousand_Pts_Stack += 15;
          RPU_SetLampState(LA_OUTLANE_SPECIAL, 0);  // Turn lamp off after collecting Special
          OutlaneSpecial[CurrentPlayer] = false;     // Set to false, now collected.
          }
          break;
        case SW_SLAM:
          RPU_DisableSolenoidStack();
          RPU_SetDisableFlippers(true);
          RPU_TurnOffAllLamps();
          RPU_SetLampState(GAME_OVER, 1);
          delay(1000);
          return MACHINE_STATE_ATTRACT;
          break;
        case SW_TILT:
          // This should be debounced
          //Serial.print(F("SW_TILT was hit and NumTiltWarnings is:             "));
          //Serial.println(NumTiltWarnings, DEC);
          //Serial.print(F("Current Time is:                                    "));
          //Serial.println(CurrentTime, DEC);
          //Serial.print(F("LastTiltWarningTime is:                             "));
          //Serial.println(LastTiltWarningTime, DEC);
          //Serial.print(F("(CurrentTime - LastTiltWarningTime) is:             "));
          //Serial.println((CurrentTime - LastTiltWarningTime), DEC);
          //Serial.println();
          if ((CurrentTime - LastTiltWarningTime) > TILT_WARNING_DEBOUNCE_TIME) {
            //Serial.println(F("Greater than Tilt Debounce time"));
            LastTiltWarningTime = CurrentTime;
            NumTiltWarnings += 1;
            //Serial.print(F("NumTiltWarnings after increment is:                 "));
            //Serial.println(NumTiltWarnings, DEC);
            if (NumTiltWarnings > MaxTiltWarnings) {
              //Serial.print(F("(NumTiltWarnings > MaxTiltWarnings) is:              "));
              //Serial.println((NumTiltWarnings > MaxTiltWarnings), DEC);
              Tilted = true;
              #ifdef EXECUTION_MESSAGES
              Serial.println(F("!!!! Tilted !!!!"));
              #endif
              RPU_SetDisableFlippers(true);
              RPU_TurnOffAllLamps();
              PlaySoundEffect(SOUND_EFFECT_TILTED);
              RPU_SetLampState(TILT, 1);
              RPU_DisableSolenoidStack();
            }
            #ifdef EXECUTION_MESSAGES
            Serial.println(F("Tilt Warning!!!!"));
            #endif
            PlaySoundEffect(SOUND_EFFECT_TILT_WARNING);
          }
          break;
          case SW_BANK_SHOT:
          BankShotScoring(); // Call BankShotScoring
          if (SuperSpinnerAllowed[CurrentPlayer] == true){
            SpinnerComboHalf = true;
            SuperSpinnerDuration = Spinner_Combo_Duration;
            SuperSpinnerTime = CurrentTime;
            //Serial.print(F("SuperSpinnerTime is: "));
            //Serial.println(SuperSpinnerTime, DEC);  
            ClearRack();
            FlashingArrows(RackLeftArrow, 125);
          }
          break;
        case SW_BUMPER_BOTTOM:
          PopDelta = ++PopDelta;
        case SW_BUMPER_RIGHT:
          PopDelta = ++PopDelta;
        case SW_BUMPER_LEFT:
          PopCount[CurrentPlayer] = ++PopCount[CurrentPlayer];
          if (PopCount[CurrentPlayer] > (Pop_Threshold-1)) {
            PopMode[CurrentPlayer] += 1;
            SetGoals(3);
            PopCount[CurrentPlayer] = 0;
            PlaySoundEffect(SOUND_EFFECT_POP_MODE);
          }
          #ifdef EXECUTION_MESSAGES
          Serial.print(F("PopMode is:  "));
          Serial.println(PopMode[CurrentPlayer], DEC);  
          Serial.print(F("PopCount is: "));
          Serial.println(PopCount[CurrentPlayer], DEC);  
          #endif
          if ( (PopCount[CurrentPlayer] > (Pop_Threshold*.33)) && (PopCount[CurrentPlayer] < Pop_Threshold) ) {
              //  Set display flashing and change to pop hits left until mode achieved
              BIPFlashing = true;
              BIPDispValue = (Pop_Threshold - PopCount[CurrentPlayer]);
            } else {
              //  Set display static and set back to BIP
              BIPFlashing = false;
              BIPDispValue = CurrentBallInPlay;
            }
          if ((PopMode[CurrentPlayer]) < 1) {
            PlaySoundEffect(SOUND_EFFECT_POP_100);
            Silent_Hundred_Pts_Stack +=1;
            //CurrentScores[CurrentPlayer] += 100;
          } else if ((PopMode[CurrentPlayer]) == 1) {
            PlaySoundEffect(SOUND_EFFECT_POP_1000 + PopDelta);        
            Silent_Thousand_Pts_Stack += 1;
            //CurrentScores[CurrentPlayer] += 1000;          
          } else if ((PopMode[CurrentPlayer]) > 1) {
            PlaySoundEffect(SOUND_EFFECT_POP_1000 + PopDelta);        
            Silent_Thousand_Pts_Stack += 2;
            //CurrentScores[CurrentPlayer] += 2000;          
          }
          PopDelta = 0;
          if (ArrowsLit[CurrentPlayer]) ArrowToggle();
          break;
        case SW_RIGHT_SLING:
        case SW_LEFT_SLING:
          RPU_PushToSolenoidStack(SOL_CHIME_100, 3);
          CurrentScores[CurrentPlayer] += 100;
          if (ArrowsLit[CurrentPlayer]) ArrowToggle();
          break;
        case SW_COIN_1:
        case SW_COIN_2:
        case SW_COIN_3:
          AddCoinToAudit(switchHit);
          AddCredit(true);
          CreditsDispValue = Credits;
          //RPU_SetDisplayCredits(Credits, true);
          if (Credits > 0) {
            RPU_SetLampState(LA_CREDIT_INDICATOR, 1);
          }
          break;
        case SW_CREDIT_RESET:
          if (CurrentBallInPlay<2) {
            // If we haven't finished the first ball, we can add players
            AddPlayer();
            if (Credits == 0) {
              RPU_SetLampState(LA_CREDIT_INDICATOR, 0);
            }
          } else {
            // If the first ball is over, pressing start again resets the game
            if (Credits >= 1 || FreePlayMode) {
              if (!FreePlayMode) {
                Credits -= 1;
                RPU_WriteByteToEEProm(RPU_CREDITS_EEPROM_BYTE, Credits);
                CreditsDispValue = Credits;
                //RPU_SetDisplayCredits(Credits);
              }
              returnState = MACHINE_STATE_INIT_GAMEPLAY;
            }
          }
          if (DEBUG_MESSAGES) {
            Serial.write("Start game button pressed\n\r");
          }
          break;        
      }
     }
    }  // -A-

  if (scoreAtTop != CurrentScores[CurrentPlayer]) {
    //Serial.print(F("Score changed\n"));
    LastTimeScoreChanged = CurrentTime;                 // To control Score dashing 
      for (int awardCount = 0; awardCount < 3; awardCount++) {
        if (AwardScores[awardCount] != 0 && scoreAtTop < AwardScores[awardCount] && CurrentScores[CurrentPlayer] >= AwardScores[awardCount]) {
          // Player has just passed an award score, so we need to award it
          if (!SamePlayerShootsAgain) {
            SamePlayerShootsAgain = true;
            RPU_SetLampState(SAME_PLAYER, 1);
            RPU_SetLampState(LA_SPSA_BACK, 1);        
            PlaySoundEffect(SOUND_EFFECT_EXTRA_BALL);
          }
        }
      }
  }
  
  return returnState;
}


void loop() {
  // This line has to be in the main loop
  RPU_DataRead(0);

  CurrentTime = millis();
  int newMachineState = MachineState;

  // Machine state is self-test/attract/game play
  if (MachineState<0) {
    newMachineState = RunSelfTest(MachineState, MachineStateChanged);
  } else if (MachineState==MACHINE_STATE_ATTRACT) {
    newMachineState = RunAttractMode(MachineState, MachineStateChanged);
  } else {
    newMachineState = RunGamePlayMode(MachineState, MachineStateChanged);
  }

  if (newMachineState!=MachineState) {
    MachineState = newMachineState;
    MachineStateChanged = true;
  } else {
    MachineStateChanged = false;
  }

  RPU_ApplyFlashToLamps(CurrentTime);
  RPU_UpdateTimedSolenoidStack(CurrentTime);

  // Toggle Credits and BIP displays when needed
  if (MachineState >= 0) {
    RPU_SetDisplayCredits(CreditsDispValue, (CreditsFlashing)?((1-(CurrentTime/125)%2)):1, true);
    RPU_SetDisplayBallInPlay(BIPDispValue, (BIPFlashing)?((CurrentTime/125)%2):1, true);
  }

}

//
// **************************
// *  Eight Ball functions  *
// **************************
//

unsigned long GoalsDisplayValue(byte currentgoals) {
  unsigned long Result = 0;
  for(int i=0; i<6; i++) {                     // Filter lower 6 goals
    Result = Result * 10;
    if ( Goals[CurrentPlayer] & (0b100000 >> i)) {
      Result +=1;
    }
    //Serial.print("Result is: ");
    //Serial.println(Result, DEC);
  }
  return Result;
}



void ShowShootAgainLamp() {

  if (!BallSaveUsed && BallSaveNumSeconds > 0 && (CurrentTime - BallFirstSwitchHitTime) < ((unsigned long)(BallSaveNumSeconds - 1) * 1000)) {
    unsigned long msRemaining = ((unsigned long)(BallSaveNumSeconds - 1) * 1000) - (CurrentTime - BallFirstSwitchHitTime);
    RPU_SetLampState(SAME_PLAYER, 1, 0, (msRemaining < 1000) ? 100 : 500);
  } else {
    RPU_SetLampState(SAME_PLAYER, SamePlayerShootsAgain);
  }
}

//  SpinnerComboFinish - Completes Spinner Combo mode

void SpinnerComboFinish() {
  SuperSpinnerAllowed[CurrentPlayer] = false;
  MarqueeDisabled = false;
  SuperSpinnerDuration = Spinner_Combo_Duration;
  SpinnerComboHalf = false;
  OverrideScorePriority = 0;              //  Set back to zero
  ShowPlayerScores(0xFF, false, false);   //  Reset all score displays
}

//
//  SetGoals Ver 3b - trigger 15-Ball mode, and Ball Chase, Scramble Ball
//
// Bit 1 - SuperBonus reached
// Bit 2 - NextBall met once
// Bit 3 - Pop Mode > x
// Bit 4 - Spinner Mode > x
// Bit 5 - Spinner Combo achieved x times
// Bit 6 - Scramble Ball
// Bit 7 - 3 Goals achieved
// Bit 8 - 5 Goals achieved

void SetGoals(byte goalnum) {   // Set goal flag and update display score

  Goals[CurrentPlayer] = (Goals[CurrentPlayer] | (0b1<<(goalnum - 1))); // Raise flag

  // Count how many goals are met and update display
  unsigned int countOnes = Goals[CurrentPlayer];
  byte numOnes = 0;
  for (int count = 0; count < 6; count++) {
    if ( (countOnes & 0b1) == 1 ) {
      numOnes++;
    }
    countOnes >>= 1;
    //Serial.print(F("numOnes :  "));
    //Serial.println(numOnes);
  }
  #ifdef EXECUTION_MESSAGES
  Serial.print(F("SetGoals - This many goals met: "));
  Serial.println(numOnes, DEC);
  #endif
  CurrentScores[CurrentPlayer] = (CurrentScores[CurrentPlayer]/10*10 + numOnes);

  // Chase Ball
  if ( numOnes == 3 && !(Goals[CurrentPlayer] & (0b1<<6)) ) {       // Start Ball chase
    #ifdef EXECUTION_MESSAGES
    Serial.println(F("SetGoals - Chase Ball section, 3 goals met"));
    #endif
    Goals[CurrentPlayer] = (Goals[CurrentPlayer] | (0b1<<6));       // Raise 7th bit
    ClearRack();
    ClearSmallBalls();
    //NextBall = 0;             // Cannot cancel NextBall here, over-ridden by CaptureBall
    //NextBallTime = 0;

    // Triple knocker signifying 3 goals met
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 750, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 900, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1250, true);
    //MarqueeDisabled = true;
    GameMode[CurrentPlayer] = 3;                                    // Trigger Mode 3
    ChaseBallStage = 1;                                             // First part of challenge
    ChaseBallTimeStart = CurrentTime;
    // Chase ball is not shifted by player number
    ChaseBall = ( 1 + CurrentTime%7 );                              // ChaseBall is 1-7
    ShowPlayerScores(0xFF, true, false);                            // Set displays back to normal
    #ifdef EXECUTION_MESSAGES
    Serial.print(F("SetGoals - ChaseBall is set to : "));
    Serial.println(ChaseBall, DEC);
    Serial.println();
    #endif
    //BallLighting();
  }

  // 15 Ball Mode
  if ( numOnes == 5 && !(Goals[CurrentPlayer] & (0b1<<7)) ) {       // Start 15 Ball mode
    #ifdef EXECUTION_MESSAGES
    Serial.println(F("SetGoals - 5 goals met"));
    #endif
    Goals[CurrentPlayer] = (Goals[CurrentPlayer] | (0b1<<7));       // Raise 8th bit
    if (goalnum == 5) {
      SpinnerComboFinish();
    }
    ClearRack();
    FlashingArrows(DownwardV, 125, 9);
    //RPU_TurnOffAttractLamps;
    SamePlayerShootsAgain = true;
    RPU_SetLampState(SAME_PLAYER, 1, 0, 150);
    RPU_SetLampState(LA_SPSA_BACK, 1, 0, 175);
    //PlaySoundEffect(SOUND_EFFECT_EXTRA_BALL);
    PlaySoundEffect(SOUND_EFFECT_8_BALL_CAPTURE);
    // 5 knocker hits signifying 5 goals met.  Sound effect includes 1 knocker hit at 900 msec
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1050, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1350, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1450, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1550, true);
    //RPU_DisableSolenoidStack();
    RPU_SetDisableFlippers(true);
    MarqueeDisabled = true;
    FifteenBallQualified[CurrentPlayer] = true;
  }
}

//
// CaptureBall Ver 4 - Mode 2 and 3 implemented
//
boolean CaptureBall(byte ballswitchnum) {
  #ifdef EXECUTION_MESSAGES
  Serial.print(F("CaptureBall - GameMode Crt Plyr is: "));
  Serial.println(GameMode[CurrentPlayer], DEC);
  #endif
  if (GameMode[CurrentPlayer] == 1) {                  // Normal game play
    // Call with 1 for balls 1-7 or 9-15
    if (((Balls[CurrentPlayer] & (0b1<<((ballswitchnum-1) + (CurrentPlayer%2 ? 8:0))))>>((ballswitchnum-1) + (CurrentPlayer%2 ? 8:0))) == false) { // If flag is 0
      Balls[CurrentPlayer] = (Balls[CurrentPlayer] | (0b1<<((ballswitchnum-1) + (CurrentPlayer%2 ? 8:0))));           // Raise flag
      RPU_SetLampState((LA_SMALL_1 + (ballswitchnum-1) + (CurrentPlayer%2 ? 8:0)), (MarqueeDisabled)?0:1, 0, 75);    // Set to rapid flashing upon capture
      MarqueeTiming();
      Silent_Hundred_Pts_Stack +=5;                     // Award ball capture points
      //  Check if next ball award should be given
      if (ballswitchnum == NextBall) {
        //Serial.println(F("NextBall awarded"));
        //Serial.println();
        Silent_Thousand_Pts_Stack +=5;                  // Award additional NextBall score
        PlaySoundEffect(SOUND_EFFECT_5K_CHIME);
        SetGoals(2);
      } else {                                          // Was not NextBall
        PlaySoundEffect(SOUND_EFFECT_BALL_CAPTURE);
        ShowPlayerScores(0xFF, false, false);           //  Reset all score displays
      }
      //  Check if next ball is a valid one for reward
      byte NextBallCheck = (ballswitchnum + 1);
      if (NextBallCheck > 7) NextBallCheck = 1;
      // If next ball is uncollected, set NextBall, if collected set to 0.
      if (((Balls[CurrentPlayer] & (0b1<<((NextBallCheck-1) + (CurrentPlayer%2 ? 8:0))))>>((NextBallCheck-1) + (CurrentPlayer%2 ? 8:0))) == false) {
        //Serial.println(F("NextBall is available"));
        //Serial.println();
        NextBall = NextBallCheck;
        NextBallTime = CurrentTime;
      } else {
        //Serial.println(F("NextBall is taken"));
        //Serial.println();
        NextBall = 0;
        NextBallTime = 0;
        ShowPlayerScores(0xFF, false, false);           //  Reset all score displays
      }
      return true;  // Returns true if ball not already captured
    } else {
      return false; // Returns false if previously captured
    }
  } else if (GameMode[CurrentPlayer] == 2){         // GameMode == 2, 15 Ball mode
  
  // 15 Ball code
  if ( (!(Balls[CurrentPlayer] & (0b1<<(ballswitchnum-1)))) && (!(Balls[CurrentPlayer] & (0b1<<((ballswitchnum-1) + (8))))) ) {
    #ifdef EXECUTION_MESSAGES
    Serial.println(F("15 ball - Set to 0 1"));
    #endif
    Balls[CurrentPlayer] = (Balls[CurrentPlayer] | (0b1<<(ballswitchnum-1)));           // Raise flag
    RPU_SetLampState((LA_SMALL_1 + (ballswitchnum-1) + 0), (MarqueeDisabled)?0:1, 0, 75);    // Set to rapid flashing upon capture
    MarqueeTiming();
    //BallLighting();
    Silent_Thousand_Pts_Stack +=5;                  // Award ball capture points
    PlaySoundEffect(SOUND_EFFECT_BALL_CAPTURE);
  } else if ( ( (Balls[CurrentPlayer] & (0b1<<(ballswitchnum-1)))) && (!(Balls[CurrentPlayer] & (0b1<<((ballswitchnum-1) + (8))))) ) {
    #ifdef EXECUTION_MESSAGES
    Serial.println(F("15 ball - Balls are 1 1"));
    #endif
    Balls[CurrentPlayer] = (Balls[CurrentPlayer] | (0b1<<((ballswitchnum-1) + 8)));     // Raise flag
    RPU_SetLampState((LA_SMALL_1 + (ballswitchnum-1) + 8), (MarqueeDisabled)?0:1, 0, 75);    // Set to rapid flashing upon capture
    MarqueeTiming();
    //BallLighting();
    Silent_Thousand_Pts_Stack +=5;                  // Award ball capture points
    PlaySoundEffect(SOUND_EFFECT_BALL_CAPTURE);
  } else if ( ( (Balls[CurrentPlayer] & (0b1<<(ballswitchnum-1)))) && ( (Balls[CurrentPlayer] & (0b1<<((ballswitchnum-1) + (8))))) ) {
    #ifdef EXECUTION_MESSAGES
    Serial.println(F("15 ball - Balls are 0 1"));
    #endif
    Balls[CurrentPlayer] = Balls[CurrentPlayer] & (~(0b1<<(ballswitchnum-1)));             // Clear low ball bit
    RPU_SetLampState((LA_SMALL_1 + (ballswitchnum-1) + 0), 1, 0, (MarqueeDisabled)?0:75); // Turn on
    MarqueeTiming();
    //BallLighting();
    Silent_Thousand_Pts_Stack -=10;                  // Award ball capture points
    //CurrentScores[CurrentPlayer] -=5000;          // Take away points
    PlaySoundEffect(SOUND_EFFECT_BALL_LOSS);
  } else { /*( ( (Balls[CurrentPlayer] & (0b1<<(ballswitchnum-1)))) && (!(Balls[CurrentPlayer] & (0b1<<((ballswitchnum-1) + (8))))) )*/
    #ifdef EXECUTION_MESSAGES
    Serial.println(F("15 ball - Balls are 0 0"));
    #endif
    Balls[CurrentPlayer] = Balls[CurrentPlayer] & (~(0b1<<(ballswitchnum-1) + 8));         // Clear high ball bit
    RPU_SetLampState((LA_SMALL_1 + (ballswitchnum-1) + 8), 1, 0, (MarqueeDisabled)?0:75); // Turn on
    MarqueeTiming();
    //BallLighting();
    Silent_Thousand_Pts_Stack -=10;                  // Award ball capture points
    //CurrentScores[CurrentPlayer] -=5000;          // Take away points
    PlaySoundEffect(SOUND_EFFECT_BALL_LOSS);
  }
  return true;    // 15-Ball mode never returns false, ball change always happening
  } else if (GameMode[CurrentPlayer] == 3) {

  // ChaseBall code
    if (ChaseBall != ballswitchnum) {
      #ifdef EXECUTION_MESSAGES
      Serial.println(F("Invalid ChaseBall"));
      #endif
      return false;                                 // Return false defaults to 500 pts
    } else if (ChaseBallStage == 1) {               // Chase Ball mode is in 1st stage and is true
      #ifdef EXECUTION_MESSAGES
      Serial.print(F("CaptureBall - ChaseBall stage is: "));
      Serial.println(ChaseBallStage, DEC);
      Serial.print(F("CaptureBall - Valid ChaseBall was: "));
      Serial.println(ChaseBall, DEC);
      #endif
      ChaseBallTimeStart = CurrentTime;             // Reset clock
      Silent_Thousand_Pts_Stack +=5;                // Award score
      PlaySoundEffect(SOUND_EFFECT_5K_CHIME);
      RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 600, true);
      ChaseBallStage = 2;                           // Now chasing a 2nd Ball
      ClearSmallBalls();                            // Cancel 1st ChaseBall lamp
      #ifdef EXECUTION_MESSAGES
      Serial.print(F("CaptureBall - ChaseBall stage (from stage 1) is: "));
      Serial.println(ChaseBallStage, DEC);
      #endif
      // Update which ball is next target to chase, min increment is 1, max 6
      ChaseBall += (1 + CurrentTime%6);
      if ( ChaseBall > 7) {
        ChaseBall -= 7;
      }
      #ifdef EXECUTION_MESSAGES
      Serial.print(F("CaptureBall - Next ChaseBall is: "));
      Serial.println(ChaseBall, DEC);
      #endif
      return true;                                  // Ball captured here
    } else {                                        // ChaseBall Stage 2
      Thousand_Pts_Stack +=20;
      GameMode[CurrentPlayer] = 1;                  // Exit back to regular play
      BallLighting();
      BankShotLighting();
      ChaseBallTimeStart = 0;                       // Zero clock to halt mode
      ChaseBall = 0;                                // Reset to 0
      ShowPlayerScores(0xFF, false, false);         //  Reset all score displays
      OverrideScorePriority = 0;                    //  Set back to zero
      RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 750, true);
      RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1100, true);
      RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 1250, true);
      #ifdef EXECUTION_MESSAGES
      Serial.print(F("CaptureBall - ChaseBall stage (from stage 2) is: "));
      Serial.println(ChaseBallStage, DEC);
      Serial.print(F("CaptureBall - 2nd ChaseBall was: "));
      Serial.println(ChaseBall, DEC);
      #endif
      return true;                                  // Ball captured here
    }
  }
}


void ClearSmallBalls() {
  for(int i=0; i<15; i++) {                     // Turn out all small ball lamps
    //RPU_SetLampState((LA_BIG_1 + i), 0);  
    RPU_SetLampState((LA_SMALL_1 + i), 0);
  }
}

void ClearRack() {
  for(int i=0; i<15; i++) {                     // Turn out all ball lamps
    RPU_SetLampState((LA_BIG_1 + i), 0);  
    //RPU_SetLampState((LA_SMALL_1 + i), 0);
  }
}

void FlashingArrows(int lamparray[], int rate, int numlamps) {
  for (int i = 0; i < numlamps; i++) {
    RPU_SetLampState((lamparray[i]), 1, 0, rate);
    //RPU_SetLampState((RackRightArrow[i]), 1, 0, rate);  
  }
}

#if 1

//
// BonusMultiplier - call to reset to 1x, or 2,3,5x
// Ver 2 - Extended range
//
void BonusMultiplier(byte mult){
  switch (mult){
    case 2: // 2X
      RPU_SetLampState(LA_2X, 1);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 0);
      BonusMult = 2;
      break;
    case 3: // 3X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 1);
      RPU_SetLampState(LA_5X, 0);
      BonusMult = 3;
      break;
    case 5: // 5X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 1);
      BonusMult = 5;
      break;
    case 7: // 7X
      RPU_SetLampState(LA_2X, 1, 0, 1000);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 1, 0, 1000);
      BonusMult = 7;
      break;
    case 8: // 8X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 1, 0, 750);
      RPU_SetLampState(LA_5X, 1, 0, 750);
      BonusMult = 8;
      break;
    case 10: // 10X
      RPU_SetLampState(LA_2X, 1, 0, 400);
      RPU_SetLampState(LA_3X, 1, 0, 500);
      RPU_SetLampState(LA_5X, 1, 0, 600);
      BonusMult = 10;
      break;
    default: // Set to default 1X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 0);
      BonusMult = 1;
      break;
    }
  #ifdef EXECUTION_MESSAGES
  Serial.println(F("BonusMultiplier()"));
  #endif
}

#else

//
// BonusMultiplier - call to reset to 1x, or 2,3,5x
//

void BonusMultiplier(byte mult){
  switch (mult){
    case 2: // 2X
      RPU_SetLampState(LA_2X, 1);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 0);
      BonusMult = 2;
      break;
    case 3: // 3X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 1);
      RPU_SetLampState(LA_5X, 0);
      BonusMult = 3;
      break;
    case 5: // 5X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 1);
      BonusMult = 5;
      break;
    default: // Set to default 1X
      RPU_SetLampState(LA_2X, 0);
      RPU_SetLampState(LA_3X, 0);
      RPU_SetLampState(LA_5X, 0);
      BonusMult = 1;
      break;
    }
  #ifdef EXECUTION_MESSAGES
  Serial.println(F("BonusMultiplier()"));
  #endif
}

#endif

//
// Bank Shot Lighting - handle post animation lighting
// Ver 2 - Extended BankShot range

void BankShotLighting(){
  #ifdef EXECUTION_MESSAGES
  Serial.println(F("BankShotLighting()"));
  #endif
  RPU_SetLampState(LA_BANK_SHOT_300, ((BankShotProgress - 0) ? 0:1));
  RPU_SetLampState(LA_BANK_SHOT_600, ((BankShotProgress - 1) ? 0:1));
  RPU_SetLampState(LA_BANK_SHOT_900, ((BankShotProgress - 2) ? 0:1));
  RPU_SetLampState(LA_BANK_SHOT_1200, ((BankShotProgress - 3) ? 0:1));
  RPU_SetLampState(LA_BANK_SHOT_1500, ((BankShotProgress - 4) ? 0:1));
  RPU_SetLampState(LA_BANK_SHOT_5000, ( ((BankShotProgress - 5) && (BankShotProgress - 6)) ? 0:1) );
  if (BankShotProgress > 6) {
    switch (BankShotProgress) {
      case 7:
      case 10:
      case 13:
        RPU_SetLampState(LA_BANK_SHOT_5000, 1, 0, 600);
        break;
      case 8:
      case 11:
      case 14:
        RPU_SetLampState(LA_BANK_SHOT_5000, 1, 0, 300);
        break;
      case 15:
        RPU_SetLampState(LA_BANK_SHOT_5000, 1, 0, 100);
        break;
      default:
        RPU_SetLampState(LA_BANK_SHOT_5000, 1);
        break;
    }
  }
}

//
// Bank Shot Scoring - handle scoring and sound only
// Ver 2 - Silent mode
// Ver 3 - Extended range

void BankShotScoring(byte sound){
  //Serial.print(F("BankShotScoring\n"));
  BankShotOffTime = CurrentTime;
  BankShotProgress += 1;
  if (BankShotProgress >= 16) BankShotProgress=16;
  switch (BankShotProgress) {
    case 1: // Just scoring
      if (sound) {
        Hundred_Pts_Stack +=3;
      } else {
        Silent_Hundred_Pts_Stack +=3;
      }
      break;
    case 2: // Scoring plus 2X
      if (sound) {
        Hundred_Pts_Stack +=6;
      } else {
        Silent_Hundred_Pts_Stack +=6;
      }
      BonusMultiplier(2); // Set to 2X 
      break;
    case 3: // Scoring plus 3X
      if (sound) {
        Hundred_Pts_Stack +=9;
      } else {
        Silent_Hundred_Pts_Stack +=9;
      }
      BonusMultiplier(3); // Set to 3X 
      break;
    case 4: // Scoring plus 5X
      if (sound) {
        Hundred_Pts_Stack +=12;
      } else {
        Silent_Hundred_Pts_Stack +=12;
      }
      BonusMultiplier(5); // Set to 5X 
      break;
    case 5:
      Silent_Hundred_Pts_Stack +=15;
      // Light and set SPSA
      SamePlayerShootsAgain = true;
      RPU_SetLampState(SAME_PLAYER, 1);
      RPU_SetLampState(LA_SPSA_BACK, 1);        
      if (sound) PlaySoundEffect(SOUND_EFFECT_EXTRA_BALL);
      break;
    case 6:
      Silent_Thousand_Pts_Stack +=5;
      if (sound) PlaySoundEffect(SOUND_EFFECT_5K_CHIME);
      // Nothing extra for this light, just the score
      break;
    case 7:
    case 8:
    case 10:
    case 11:
    case 13:
    case 14:
    case 15:
      Silent_Thousand_Pts_Stack +=5;
      if (sound) PlaySoundEffect(SOUND_EFFECT_5K_CHIME);
      break;
    case 9:
      Silent_Thousand_Pts_Stack +=5;
      if (sound) PlaySoundEffect(SOUND_EFFECT_MACHINE_START);
      BonusMultiplier(7); // Set to 7X       
      break;
    case 12:
      Silent_Thousand_Pts_Stack +=5;
      if (sound) PlaySoundEffect(SOUND_EFFECT_MACHINE_START);
      BonusMultiplier(8); // Set to 8X       
      break;
    case 16:
      if (BonusMultTenX) {                //  If already 10X 
        Silent_Thousand_Pts_Stack +=5;
        if (sound) PlaySoundEffect(SOUND_EFFECT_5K_CHIME);
        //BonusMultiplier(10); // Set to 10X       
      } else {                            //  One time extra when achieving 10X
        Silent_Thousand_Pts_Stack +=10;
        if (sound) PlaySoundEffect(SOUND_EFFECT_8_BALL_CAPTURE);
        BonusMultiplier(10); // Set to 10X
        BonusMultTenX = true;
      }
      break;
  }
}


//
// Arrow Toggle
//

void ArrowToggle() {
  if (RightArrow == false) {
    RPU_SetLampState(LA_RIGHT_ARROW, 1);
    RPU_SetLampState(LA_LEFT_ARROW, 0);
    RightArrow = true;
  } else {
    RPU_SetLampState(LA_RIGHT_ARROW, 0);
    RPU_SetLampState(LA_LEFT_ARROW, 1);
    RightArrow = false;    
  }
}


// Borrowed from Mata Hari code
//
// PlaySoundEffect
//
/*
#define SOUND_EFFECT_NONE               0
#define SOUND_EFFECT_ADD_PLAYER         1
#define SOUND_EFFECT_BALL_OVER          2
#define SOUND_EFFECT_GAME_OVER          3
#define SOUND_EFFECT_MACHINE_START      4
#define SOUND_EFFECT_ADD_CREDIT         5
#define SOUND_EFFECT_PLAYER_UP          6
#define SOUND_EFFECT_GAME_START         7
#define SOUND_EFFECT_EXTRA_BALL         8
#define SOUND_EFFECT_5K_CHIME           9
#define SOUND_EFFECT_BALL_CAPTURE      10
#define SOUND_EFFECT_POP_MODE          11
#define SOUND_EFFECT_POP_100           12
#define SOUND_EFFECT_POP_1000          13
#define SOUND_EFFECT_POP_1000b         14
#define SOUND_EFFECT_POP_1000c         15
#define SOUND_EFFECT_TILT_WARNING      16
#define SOUND_EFFECT_TILTED            17
#define SOUND_EFFECT_10_PTS            18
#define SOUND_EFFECT_100_PTS           19
#define SOUND_EFFECT_1000_PTS          20
#define SOUND_EFFECT_EXTRA             21
#define SOUND_EFFECT_SPINNER_COMBO     22
#define SOUND_EFFECT_KICKER_OUTLANE    23
#define SOUND_EFFECT_8_BALL_CAPTURE    24
#define SOUND_EFFECT_BALL_LOSS         25
*/

// Array of sound effect durations

int SoundEffectDuration[26] = 
{0, 400, 1250, 1458, 2513, 575, 900, 2579, 600, 800,
400, 1575, 0, 5, 5, 5, 200, 2000, 30, 30,
30, 30, 1305, 400, 900, 200};

unsigned long NextSoundEffectTime = 0;  

void PlaySoundEffect(byte soundEffectNum) {

unsigned long TimeStart;

  if (CurrentTime > NextSoundEffectTime) { // No sound effect running
    NextSoundEffectTime = CurrentTime + SoundEffectDuration[soundEffectNum];
    TimeStart = CurrentTime;
/*
    Serial.println(F("No sound was playing"));
    Serial.print(F("CurrentTime is:            "));
    Serial.println(CurrentTime, DEC);
    Serial.print(F("NextSoundEffectTime        "));
    Serial.println(NextSoundEffectTime, DEC);
    Serial.println((CurrentTime - NextSoundEffectTime), DEC);
    Serial.println();
*/
  } else {                                 // A sound effect is in process if here
      TimeStart = NextSoundEffectTime;
      #ifdef EXECUTION_MESSAGES
      Serial.println(F("Sound already playing"));
      #endif
      if ((NextSoundEffectTime - CurrentTime) > 250) {
        #ifdef EXECUTION_MESSAGES
        Serial.println(F("Break out of playing sound"));
        #endif
        return;
      }
      NextSoundEffectTime = NextSoundEffectTime + 20 + SoundEffectDuration[soundEffectNum];
      
/*      Serial.print(F("soundEffectNum: "));
      Serial.println(soundEffectNum, DEC);
      Serial.print(F("SoundEffectDuration: "));
      Serial.println(SoundEffectDuration[soundEffectNum], DEC);
      Serial.print(F("NextSoundEffectTime: "));
      Serial.println(NextSoundEffectTime, DEC);
      Serial.print(F("CurentTime: "));
      Serial.println(CurrentTime, DEC);
      Serial.println();*/
    
  }

//  unsigned long count;  // not used?

  switch (soundEffectNum) {
    case SOUND_EFFECT_NONE:
      break;
    case SOUND_EFFECT_ADD_PLAYER:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart +200, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart +400, true);
      break;
    case SOUND_EFFECT_BALL_OVER:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+166, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+250, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+500, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+750, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1000, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+1166, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart+1250, true);
      break;
    case SOUND_EFFECT_GAME_OVER: //  Ver 2
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+0, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+104, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart+217, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+404, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+517, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+629, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart+817, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart+925, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1042, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1229, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1342, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1458, true);
      break;
    case SOUND_EFFECT_MACHINE_START:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+217, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+308, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart+412, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+579, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+679, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+779, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart+946, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart+1046, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1150, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1313, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1417, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart+1513, true);
      break;
    case SOUND_EFFECT_ADD_CREDIT:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 111, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 209, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 310, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 477, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 575, true);
      break;
    case SOUND_EFFECT_PLAYER_UP:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 1, TimeStart+500, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart+600, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart+900, true);
      break;
    case SOUND_EFFECT_GAME_START:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 4, TimeStart + 500);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 679);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 846);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1013);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1208);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1313);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1408);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 1546);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 1746);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 1842);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 1942);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 2079);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 2246);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 2413);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 2579);
      break;
    case SOUND_EFFECT_EXTRA_BALL:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 100, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 200, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 300, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 500, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 600, true);
      break;
    case SOUND_EFFECT_5K_CHIME:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart +  0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart +  50);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart +   150);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart +  200);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart +   300);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart +  350);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart +   450);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart +  500);
      break;
    case SOUND_EFFECT_BALL_CAPTURE:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 100);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 200);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 300);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 400);
    break;
    case SOUND_EFFECT_POP_MODE:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 100);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 200);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 300);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 425);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 525);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 625);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 725);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 850);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 950);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 1050);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 1150);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1275);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1375);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1475);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1575);
    break;
    case SOUND_EFFECT_POP_100:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 0);
    break;
    case SOUND_EFFECT_POP_1000:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 10);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 20);
    break;
    case SOUND_EFFECT_POP_1000b:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 10);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 20);
    break;
    case SOUND_EFFECT_POP_1000c:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 10);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 20);
    break;
    case SOUND_EFFECT_TILT_WARNING:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 200);
    break;
    case SOUND_EFFECT_TILTED:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 0, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 400, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 800, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 1200, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 1600, true);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 2000, true);
    break;
    case SOUND_EFFECT_10_PTS:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 0);
    break;
    case SOUND_EFFECT_100_PTS:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 0);
    break;
    case SOUND_EFFECT_1000_PTS:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 0);
    break;
    case SOUND_EFFECT_EXTRA:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 0);
    break;
    case SOUND_EFFECT_SPINNER_COMBO:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 75);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 135);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 435);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 510);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 570);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 870);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 945);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 1005);
      RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, TimeStart + 1105);
    break;
    case SOUND_EFFECT_KICKER_OUTLANE:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 100);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 200);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 300);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 400);
    break;
    case SOUND_EFFECT_8_BALL_CAPTURE:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 100);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 200);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 300);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 400);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 500);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, TimeStart + 600);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 700);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 800);
      RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 900);
    break;
    case SOUND_EFFECT_BALL_LOSS:
      RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, TimeStart + 0);
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 200);
      //RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, TimeStart + 200);
      //RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 3, TimeStart + 300);
    break;
  }
}

//
// BallLighting - Reset lights - Ver 3
//  - Reset lights - using Ball[] variable
//  - 15 ball capable version
//
void BallLighting() {
//void BallLighting(byte Mode) {

// 'if' selects balls 1-7 for players 1,3 and 9-15 for player 2,4.  Bit mask is shifted 8 spots or not depending 
// on player number.  Deals with all 15 lamps.

  for(int i=0; i<15; i++) {                     // Turn out all ball lamps
    RPU_SetLampState((LA_BIG_1 + i), 0);  
    RPU_SetLampState((LA_SMALL_1 + i), 0);
  }
  if (GameMode[CurrentPlayer] == 1) {  // Mode 1
    for(int i=0; i<7; i++) {                    // Cycle through 7 balls to set the lamps
      if (((Balls[CurrentPlayer] & (0b1<<(i + (CurrentPlayer%2 ? 8:0))))>>(i + (CurrentPlayer%2 ? 8:0))) == true) {
        RPU_SetLampState((LA_BIG_1 + i + (CurrentPlayer%2 ? 8:0)), 1);       // if true turn large center lamp on
      } else {
        RPU_SetLampState((LA_SMALL_1 + i + (CurrentPlayer%2 ? 8:0)), 1);     // if large lamp is off, small lamp is on
      }
    }
    // Turn on Ball 8 as special case, check first if Ball 8 is captured, if not check if 7 balls captured.
    if ((Balls[CurrentPlayer] & (0b10000000)) == 128) {
      //Serial.println(F("Ball Lighting - Ball 8 is true, turn it on"));
      RPU_SetLampState(LA_BIG_8, 1);
    } else {
      if (((Balls[CurrentPlayer] & (0b01111111<<(0 + (CurrentPlayer%2 ? 8 : 0))))>>(CurrentPlayer%2 ? 8 : 0)) == 127) { // 7 balls captured, ready to capture ball 8
        RPU_SetLampState(LA_SMALL_8, 1);
      }
    }
  } // end of Mode 1

  if (GameMode[CurrentPlayer] == 2) {
    for(int i=0; i<15; i++) {                      // Cycle through 15 balls to set the lamps
      if (i == 7) {
        i = 8;                                     // Skip 8 ball
      }
      if ( ((Balls[CurrentPlayer] & (0b1<<i)) >> i ) == true) {
        RPU_SetLampState((LA_BIG_1 + i), 1);      // if true turn large center lamp on
      } else {
        RPU_SetLampState((LA_SMALL_1 + i), 1);    // if large lamp is off, small lamp is on
      }
    }
    // Turn on Ball 8 as special case, check first if Ball 8 is captured, if not check if 7 balls captured.
    if ((Balls[CurrentPlayer] & (0b10000000)) == 128) {
      //Serial.println(F("Ball Lighting - Ball 8 is true, turn it on"));
      RPU_SetLampState(LA_BIG_8, 1);
    } else {
      if ( (Balls[CurrentPlayer] & (0b0111111101111111) ) == 0x7F7F) { // 7 balls captured, ready to capture ball 8. (32639)
        RPU_SetLampState(LA_SMALL_8, 1);
      }
    }
  }

  #ifdef EXECUTION_MESSAGES
  Serial.print(F("BallLighting - Mode: "));
  Serial.println(GameMode[CurrentPlayer], DEC);
  Serial.println();
  #endif
// End of BallLighting
  }


// 
// Marquee Animation Timing - Ver 2
//

void MarqueeTiming() {
  if (MarqueeOffTime == 0) { 
    MarqueeOffTime = CurrentTime;
    MarqueeMultiple = 1;
    //Serial.println(F("MarqueeOffTime set to CurrentTime."));
  } else {
    MarqueeMultiple += 1;
    //Serial.println(F("MarqueeMultiple incremented."));
  }
}

//
// CheckHighScores
//

void CheckHighScores() {
  unsigned long highestScore = 0;
  int highScorePlayerNum = 0;
  for (int count = 0; count < CurrentNumPlayers; count++) {
    if (CurrentScores[count] > highestScore) highestScore = CurrentScores[count];
    highScorePlayerNum = count;
  }
  Serial.println();
  Serial.print(F("HighScore : "));
  Serial.println(HighScore, DEC);
  Serial.print(F("highestScore : "));
  Serial.println(highestScore, DEC);
  Serial.println();

  // Remove number of goals reached from high score
  highestScore = highestScore/10*10;

  if (highestScore > HighScore) {
    HighScore = highestScore;
    //Serial.println(F("highestScore was higher than HighScore: "));
    //Serial.println();
    if (HighScoreReplay) {
      AddCredit(false, 3);
      RPU_WriteULToEEProm(RPU_TOTAL_REPLAYS_EEPROM_START_BYTE, RPU_ReadULFromEEProm(RPU_TOTAL_REPLAYS_EEPROM_START_BYTE) + 3);
    }
    RPU_WriteULToEEProm(RPU_HIGHSCORE_EEPROM_START_BYTE, highestScore);
    RPU_WriteULToEEProm(RPU_TOTAL_HISCORE_BEATEN_START_BYTE, RPU_ReadULFromEEProm(RPU_TOTAL_HISCORE_BEATEN_START_BYTE) + 1);

    for (int count = 0; count < 4; count++) {
      if (count == highScorePlayerNum) {
        RPU_SetDisplay(count, CurrentScores[count], true, 2);
      } else {
        RPU_SetDisplayBlank(count, 0x00);
      }
    }

    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 250, true);
    RPU_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 500, true);
  }
}

void MarqueeAttract(byte Segment, int Speed, boolean CW) {

if (MarqueeDisabled) {
  return;
}

// Default speeds, Segment 1 = 100
  
  byte MQPhase1 = (CurrentTime/Speed)%4; // 4 steps
  byte MQPhase2 = (CurrentTime/(Speed*3))%3; // 3 steps
  if ((Segment == 1) || (Segment == 4)) {
    // Rotate CW
    // First leg
    RPU_SetLampState(LA_BIG_1, (CW == true)?(MQPhase1==0):(MQPhase1==3), 0); 
    RPU_SetLampState(LA_BIG_2, (CW == true)?(MQPhase1==1):(MQPhase1==2), 0);
    RPU_SetLampState(LA_BIG_3, (CW == true)?(MQPhase1==2):(MQPhase1==1), 0);
    RPU_SetLampState(LA_BIG_4, (CW == true)?(MQPhase1==3):(MQPhase1==0), 0);
    // Second leg
    RPU_SetLampState(LA_BIG_5, (CW == true)?(MQPhase1==0):(MQPhase1==3), 0); 
    RPU_SetLampState(LA_BIG_9, (CW == true)?(MQPhase1==1):(MQPhase1==2), 0);
    RPU_SetLampState(LA_BIG_12, (CW == true)?(MQPhase1==2):(MQPhase1==1), 0);
    RPU_SetLampState(LA_BIG_14, (CW == true)?(MQPhase1==3):(MQPhase1==0), 0);
    // Third leg
    RPU_SetLampState(LA_BIG_15, (CW == true)?(MQPhase1==0):(MQPhase1==3), 0); 
    RPU_SetLampState(LA_BIG_13, (CW == true)?(MQPhase1==1):(MQPhase1==2), 0);
    RPU_SetLampState(LA_BIG_10, (CW == true)?(MQPhase1==2):(MQPhase1==1), 0);
    RPU_SetLampState(LA_BIG_6, (CW == true)?(MQPhase1==3):(MQPhase1==0), 0);
    // Inner rack lamps 
    RPU_SetLampState(LA_BIG_7, (CW == true)?(MQPhase2==0):(MQPhase2==2), 0); 
    RPU_SetLampState(LA_BIG_11, (CW == true)?(MQPhase2==1):(MQPhase2==1), 0);
    RPU_SetLampState(LA_BIG_8, (CW == true)?(MQPhase2==2):(MQPhase2==0), 0);
  }

  byte MQPhase3 = (CurrentTime/(Speed*54/100))%6; //  6 steps
  if ((Segment == 2) || (Segment == 4)) {
    // Bank Shot lamps
    RPU_SetLampState(LA_BANK_SHOT_5000, ((CW == true)?(MQPhase3==0):(MQPhase3==5))||((CW == true)?(MQPhase3==1):(MQPhase3==0)), ((CW == true)?(MQPhase3==1):(MQPhase3==0))); 
    RPU_SetLampState(LA_BANK_SHOT_1500, ((CW == true)?(MQPhase3==1):(MQPhase3==4))||((CW == true)?(MQPhase3==2):(MQPhase3==5)), ((CW == true)?(MQPhase3==2):(MQPhase3==5)));
    RPU_SetLampState(LA_BANK_SHOT_1200, ((CW == true)?(MQPhase3==2):(MQPhase3==3))||((CW == true)?(MQPhase3==3):(MQPhase3==4)), ((CW == true)?(MQPhase3==3):(MQPhase3==4)));
    RPU_SetLampState(LA_BANK_SHOT_900, ((CW == true)?(MQPhase3==3):(MQPhase3==2))||((CW == true)?(MQPhase3==4):(MQPhase3==3)), ((CW == true)?(MQPhase3==4):(MQPhase3==3)));
    RPU_SetLampState(LA_BANK_SHOT_600, ((CW == true)?(MQPhase3==4):(MQPhase3==1))||((CW == true)?(MQPhase3==5):(MQPhase3==2)), ((CW == true)?(MQPhase3==5):(MQPhase3==2)));
    RPU_SetLampState(LA_BANK_SHOT_300, ((CW == true)?(MQPhase3==5):(MQPhase3==0))||((CW == true)?(MQPhase3==0):(MQPhase3==1)), ((CW == true)?(MQPhase3==0):(MQPhase3==1)));
  }

  byte MQPhase4 = (CurrentTime/(Speed*3/2))%3; //  3 steps
  byte MQPhase5 = (CurrentTime/(Speed*60/100))%8; //  8 steps

  if ((Segment == 3) || (Segment == 4)) {
    // Multiplier lamps
    RPU_SetLampState(LA_5X, MQPhase4==0||MQPhase4==1, 0); 
    RPU_SetLampState(LA_3X, MQPhase4==1||MQPhase4==2, 0);
    RPU_SetLampState(LA_2X, MQPhase4==2||MQPhase4==0, 0);
  
    // Upper alley Marquee
    // Top Row
    RPU_SetLampState(LA_SMALL_1, MQPhase5==0||MQPhase5==1||MQPhase5==2, MQPhase5==2); 
    RPU_SetLampState(LA_SMALL_2, MQPhase5==1||MQPhase5==2||MQPhase5==3, MQPhase5==3);
    RPU_SetLampState(LA_SMALL_3, MQPhase5==2||MQPhase5==3||MQPhase5==4, MQPhase5==4);
    RPU_SetLampState(LA_SMALL_4, MQPhase5==3||MQPhase5==4||MQPhase5==5, MQPhase5==5);
    // Bottom Row
    RPU_SetLampState(LA_SMALL_12, MQPhase5==4||MQPhase5==5||MQPhase5==6, MQPhase5==6); 
    RPU_SetLampState(LA_SMALL_11, MQPhase5==5||MQPhase5==6||MQPhase5==7, MQPhase5==7);
    RPU_SetLampState(LA_SMALL_10, MQPhase5==6||MQPhase5==7||MQPhase5==0, MQPhase5==0);
    RPU_SetLampState(LA_SMALL_9, MQPhase5==7||MQPhase5==0||MQPhase5==1, MQPhase5==1);
  }

  byte MQPhase6 = (CurrentTime/Speed)%4; // 4 steps
  if (Segment == 4) {
    // Player Lamps
    RPU_SetLampState(LA_1_PLAYER, MQPhase6==0||MQPhase6==1, 0); 
    RPU_SetLampState(LA_2_PLAYER, MQPhase6==1||MQPhase6==2, 0);
    RPU_SetLampState(LA_3_PLAYER, MQPhase6==2||MQPhase6==3, 0);
    RPU_SetLampState(LA_4_PLAYER, MQPhase6==3||MQPhase6==0, 0);

    // Remaining lamps
    RPU_SetLampState(LA_SMALL_5, 1, 0, 125);
    RPU_SetLampState(LA_SMALL_13, 1, 0, 250);
    RPU_SetLampState(LA_SMALL_6, 1, 0, 125);
    RPU_SetLampState(LA_SMALL_14, 1, 0, 250);
    RPU_SetLampState(LA_SMALL_7, 1, 0, 125);
    RPU_SetLampState(LA_SMALL_15, 1, 0, 250);
  
    RPU_SetLampState(LA_SMALL_8, 1, 0, 250);
    RPU_SetLampState(SAME_PLAYER, 1, 0, 500);
    RPU_SetLampState(LA_SUPER_BONUS, 1, 0, 500);
    RPU_SetLampState(LA_SPINNER, 1, 0, 250);
    RPU_SetLampState(LA_OUTLANE_SPECIAL, 1, 0, 125);

  }
}

//
//  ChimeScoring Ver 3 - Handles negative numbers
//

void ChimeScoring() {

int WaitTimeShort = 25;
int WaitTimeLong = 100;

//  if (ChimeScoringDelay == 0) {
//    ChimeScoringDelay = CurrentTime;
//  }

  if ( (CurrentTime - ChimeScoringDelay) > (((Silent_Thousand_Pts_Stack) || (Silent_Hundred_Pts_Stack))?WaitTimeShort:WaitTimeLong) ) {

    if (Silent_Hundred_Pts_Stack > 0) {
      CurrentScores[CurrentPlayer] += 100;
      Silent_Hundred_Pts_Stack -= 1;
      //Serial.print(F("Silent_Hundred_Pts_Stack is: "));
      //Serial.println(Silent_Hundred_Pts_Stack, DEC);
    } else if (Silent_Thousand_Pts_Stack > 0) {          // Stack counts down
      CurrentScores[CurrentPlayer] += 1000;
      Silent_Thousand_Pts_Stack -= 1;
      //Serial.print(F("Silent_Thousand_Pts_Stack is: "));
      //Serial.println(Silent_Thousand_Pts_Stack, DEC);
    } else if (Silent_Thousand_Pts_Stack < 0) {          // Stack counts up
      CurrentScores[CurrentPlayer] -= 1000;
      Silent_Thousand_Pts_Stack += 1;
      //Serial.print(F("Silent_Thousand_Pts_Stack is: "));
      //Serial.println(Silent_Thousand_Pts_Stack, DEC);
    } else if (Ten_Pts_Stack > 0) {
      PlaySoundEffect(SOUND_EFFECT_10_PTS);
      //RPU_PushToTimedSolenoidStack(SOL_CHIME_10, 3, CurrentTime + 0);
      CurrentScores[CurrentPlayer] += 10;
      Ten_Pts_Stack -= 1;
      //Serial.print(F("Ten_Pts_Stack is: "));
      //Serial.println(Ten_Pts_Stack, DEC);
    } else if (Hundred_Pts_Stack > 0) {
      PlaySoundEffect(SOUND_EFFECT_100_PTS);
      //RPU_PushToTimedSolenoidStack(SOL_CHIME_100, 3, CurrentTime + 0);
      CurrentScores[CurrentPlayer] += 100;
      Hundred_Pts_Stack -= 1;
      //Serial.print(F("Hundred_Pts_Stack is: "));
      //Serial.println(Hundred_Pts_Stack, DEC);
    } else if (Thousand_Pts_Stack > 0) {
      PlaySoundEffect(SOUND_EFFECT_1000_PTS);
      //RPU_PushToTimedSolenoidStack(SOL_CHIME_1000, 3, CurrentTime + 0);
      CurrentScores[CurrentPlayer] += 1000;
      Thousand_Pts_Stack -= 1;
      //Serial.print(F("Thousand_Pts_Stack is: "));
      //Serial.println(Thousand_Pts_Stack, DEC);
    }
    
    if (Silent_Thousand_Pts_Stack > 0) {
      ChimeScoringDelay = ChimeScoringDelay + WaitTimeShort;
    } else {
      ChimeScoringDelay = ChimeScoringDelay + WaitTimeLong;
    }
    //Serial.print(F("ChimeScoringDelay is: "));
    //Serial.println(ChimeScoringDelay, DEC);

    ChimeScoringDelay = CurrentTime;
  }
}


//
// Match Mode - ShowMatchSequence
//

unsigned long MatchSequenceStartTime = 0;
unsigned long MatchDelay = 150;
//byte MatchDigit = 0;
byte NumMatchSpins = 0;
byte ScoreMatches = 0;

int ShowMatchSequence(boolean curStateChanged) {
  if (!MatchFeature) return MACHINE_STATE_ATTRACT;

  if (curStateChanged) {
    MatchSequenceStartTime = CurrentTime;
    MatchDelay = 2250;          // Wait this long after game end to start match mode
    //MatchDigit = random(0, 10);
    //MatchDigit = 1;    // Test value, force to be 1 (10)
    MatchDigit = CurrentTime%10;
    NumMatchSpins = 0;
    RPU_SetLampState(MATCH, 1, 0);
    RPU_SetDisableFlippers();
    ScoreMatches = 0;
  }

  if (NumMatchSpins < 40) {
    if ( (CurrentTime - MatchSequenceStartTime) > (MatchDelay) ) {
//    if (CurrentTime > (MatchSequenceStartTime + MatchDelay)) {
      MatchDigit += 1;
      if (MatchDigit > 9) MatchDigit = 0;
      //PlaySoundEffect(10+(MatchDigit%2));
      RPU_PushToTimedSolenoidStack(SOL_CHIME_EXTRA, 1, 0, true);
      BIPDispValue = ((int)MatchDigit * 10);
      MatchDelay += 50 + 4 * NumMatchSpins;
      NumMatchSpins += 1;
      RPU_SetLampState(MATCH, NumMatchSpins % 2, 0);

      if (NumMatchSpins == 40) {
        RPU_SetLampState(MATCH, 0);
        MatchDelay = CurrentTime - MatchSequenceStartTime;
      }
    }
  }

  if (NumMatchSpins >= 40 && NumMatchSpins <= 43) {
    if (CurrentTime > (MatchSequenceStartTime + MatchDelay)) {
      if ( (CurrentNumPlayers > (NumMatchSpins - 40)) && ((CurrentScores[NumMatchSpins - 40] / 10) % 10) == MatchDigit) {
        ScoreMatches |= (1 << (NumMatchSpins - 40));
        AddSpecialCredit();
        MatchDelay += 1000;
        NumMatchSpins += 1;
        RPU_SetLampState(MATCH, 1);
      } else {
        NumMatchSpins += 1;
      }
      if (NumMatchSpins == 44) {
        MatchDelay += 5000;
      }
    }
  }

  if (NumMatchSpins > 43) {
    RPU_SetLampState(MATCH, 1);
    if (CurrentTime > (MatchSequenceStartTime + MatchDelay)) {
      return MACHINE_STATE_ATTRACT;
    }
  }

  for (int count = 0; count < 4; count++) {
    if ((ScoreMatches >> count) & 0x01) {
      // If this score matches, we're going to flash the last two digits
      if ( (CurrentTime / 200) % 2 ) {
        RPU_SetDisplayBlank(count, RPU_GetDisplayBlank(count) & 0x0F);
      } else {
        RPU_SetDisplayBlank(count, RPU_GetDisplayBlank(count) | 0x30);
      }
    }
  }

  return MACHINE_STATE_MATCH_MODE;
}
