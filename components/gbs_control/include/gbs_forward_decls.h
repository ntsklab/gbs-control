/**
 * gbs_forward_decls.h - Auto-generated forward declarations
 * Generated from gbs_control.cpp function definitions
 */
#ifndef GBS_FORWARD_DECLS_H_
#define GBS_FORWARD_DECLS_H_

#include <stdint.h>
#include <stddef.h>
#include "Arduino.h"

// Global command variables (set by WebUI, Serial, or GPIO buttons;
// processed in gbs_loop main context)
extern char serialCommand;

uint8_t getMovingAverage(uint8_t item);
void externalClockGenResetClock();
void externalClockGenSyncInOutRate();
void externalClockGenDetectAndInitialize();
static inline void writeOneByte(uint8_t slaveRegister, uint8_t value);
static inline void writeBytes(uint8_t slaveRegister, uint8_t *values, uint8_t numValues);
void copyBank(uint8_t *bank, const uint8_t *programArray, uint16_t *index);
boolean videoStandardInputIsPalNtscSd();
void zeroAll();
void loadHdBypassSection();
void loadPresetDeinterlacerSection();
void loadPresetMdSection();
void writeProgramArrayNew(const uint8_t *programArray, boolean skipMDSection);
void activeFrameTimeLockInitialSteps();
void resetInterruptSogSwitchBit();
void resetInterruptSogBadBit();
void resetInterruptNoHsyncBadBit();
void setResetParameters();
void OutputComponentOrVGA();
void applyComponentColorMixing();
void toggleIfAutoOffset();
void applyYuvPatches();
void applyRGBPatches();
void setAdcParametersGainAndOffset();
void updateHVSyncEdge();
void prepareSyncProcessor();
void setAndUpdateSogLevel(uint8_t level);
void goLowPowerWithInputDetection();
boolean optimizePhaseSP();
void optimizeSogLevel();
uint8_t detectAndSwitchToActiveInput();
uint8_t inputAndSyncDetect();
uint8_t getSingleByteFromPreset(const uint8_t *programArray, unsigned int offset);
static inline void readFromRegister(uint8_t reg, int bytesToRead, uint8_t *output);
void printReg(uint8_t seg, uint8_t reg);
void dumpRegisters(byte segment);
void resetPLLAD();
void latchPLLAD();
void resetPLL();
void ResetSDRAM();
void resetDigital();
void resetSyncProcessor();
void resetModeDetect();
void shiftHorizontal(uint16_t amountToShift, bool subtracting);
void shiftHorizontalLeft();
void shiftHorizontalRight();
void shiftHorizontalLeftIF(uint8_t amount);
void shiftHorizontalRightIF(uint8_t amount);
void scaleHorizontal(uint16_t amountToScale, bool subtracting);
void moveHS(uint16_t amountToAdd, bool subtracting);
void moveVS(uint16_t amountToAdd, bool subtracting);
void invertHS();
void invertVS();
void scaleVertical(uint16_t amountToScale, bool subtracting);
void shiftVertical(uint16_t amountToAdd, bool subtracting);
void shiftVerticalUp();
void shiftVerticalDown();
void shiftVerticalUpIF();
void shiftVerticalDownIF();
void setHSyncStartPosition(uint16_t value);
void setHSyncStopPosition(uint16_t value);
void setMemoryHblankStartPosition(uint16_t value);
void setMemoryHblankStopPosition(uint16_t value);
void setDisplayHblankStartPosition(uint16_t value);
void setDisplayHblankStopPosition(uint16_t value);
void setVSyncStartPosition(uint16_t value);
void setVSyncStopPosition(uint16_t value);
void setMemoryVblankStartPosition(uint16_t value);
void setMemoryVblankStopPosition(uint16_t value);
void setDisplayVblankStartPosition(uint16_t value);
void setDisplayVblankStopPosition(uint16_t value);
uint16_t getCsVsStart();
uint16_t getCsVsStop();
void setCsVsStart(uint16_t start);
void setCsVsStop(uint16_t stop);
void printVideoTimings();
void set_htotal(uint16_t htotal);
void set_vtotal(uint16_t vtotal);
void resetDebugPort();
void readEeprom();
void setIfHblankParameters();
void fastGetBestHtotal();
boolean runAutoBestHTotal();
boolean applyBestHTotal(uint16_t bestHTotal);
float getSourceFieldRate(boolean useSPBus);
float getOutputFrameRate();
uint32_t getPllRate();
void doPostPresetLoadSteps();
void applyPresets(uint8_t result);
void unfreezeVideo();
void freezeVideo();
uint8_t getVideoMode();
boolean getSyncPresent();
boolean getStatus00IfHsVsStable();
boolean getStatus16SpHsStable();
void setOverSampleRatio(uint8_t newRatio, boolean prepareOnly);
void togglePhaseAdjustUnits();
void advancePhase();
void movePhaseThroughRange();
void setAndLatchPhaseSP();
void setAndLatchPhaseADC();
void nudgeMD();
void updateSpDynamic(boolean withCurrentVideoModeCheck);
void updateCoastPosition(boolean autoCoast);
void updateClampPosition();
void setOutModeHdBypass(bool regsInitialized);
void bypassModeSwitch_RGBHV();
void runAutoGain();
void enableScanlines();
void disableScanlines();
void enableMotionAdaptDeinterlace();
void disableMotionAdaptDeinterlace();
boolean snapToIntegralFrameRate(void);
void printInfo();
void stopWire();
void startWire();
void fastSogAdjust();
void runSyncWatcher();
boolean checkBoardPower();
void calibrateAdcOffset();
void loadDefaultUserOptions();
uint8_t readButtons(void);
void debounceButtons(void);
bool buttonDown(uint8_t pos);
void handleButtons(void);
void discardSerialRxData();
void updateWebSocketData();
void handleWiFi(boolean instant);
void handleType2Command(char argument);
void startWebserver();
void initUpdateOTA();
void StrClear(char *str, uint16_t length);
const uint8_t *loadPresetFromSPIFFS(byte forVideoMode);
void savePresetToSPIFFS();
void saveUserPrefs();
void settingsMenuOLED();
void pointerfunction();
void subpointerfunction();

#endif // GBS_FORWARD_DECLS_H_
