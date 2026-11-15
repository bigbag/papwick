#pragma once

#include <cstdint>

// X(key, English text). Each user interface string has one key. A translation gives a text for each key.
// FMT_ keys are printf formats.
// clang-format off
#define PAPWICK_STRINGS(X) \
  /* Button hints */ \
  X(BACK, "back") \
  X(NEXT, "next") \
  X(OPEN, "open") \
  X(YES, "yes") \
  X(NO, "no") \
  X(CANCEL, "cancel") \
  X(DONE, "done") \
  X(UNDO, "undo") \
  X(START, "start") \
  X(PAUSE, "pause") \
  X(RESUME, "resume") \
  X(SKIP, "skip") \
  X(NEW_GAME, "new game") \
  X(NEXT_LEVEL, "next level") \
  X(HOTSPOT, "hotspot") \
  X(FORGET, "forget") \
  X(RETRY, "retry") \
  X(TALK, "talk") \
  X(STOP, "stop") \
  /* Options */ \
  X(ON, "On") \
  X(OFF, "Off") \
  X(LOW, "Low") \
  X(MEDIUM, "Med") \
  X(HIGH, "High") \
  X(MAX, "Max") \
  X(SLEEP_30S, "30 s") \
  X(SLEEP_1MIN, "1 min") \
  X(SLEEP_2MIN, "2 min") \
  X(SLEEP_5MIN, "5 min") \
  X(NEVER, "Never") \
  X(METRIC, "Metric") \
  X(IMPERIAL, "Imperial") \
  X(SOFT, "Soft") \
  X(BRIGHT, "Bright") \
  X(EASY, "Easy") \
  X(NORMAL, "Normal") \
  X(HARD, "Hard") \
  X(AUTO, "Auto") \
  X(MANUAL, "Manual") \
  X(ANALOG, "Analog") \
  X(DIGITAL, "Digital") \
  /* Shell */ \
  X(HOME, "Home") \
  X(SETTINGS, "Settings") \
  X(POWER_OFF_Q, "Power off?") \
  X(SCREEN_OFF_Q, "Screen off? (USB)") \
  X(POWER_OFF, "Power off") \
  X(OUT_OF_MEMORY, "Out of memory") \
  /* Settings */ \
  X(DEVICE, "Device") \
  X(TIME, "Time") \
  X(WIFI, "Wi-Fi") \
  X(APPS, "Apps") \
  X(ABOUT, "About") \
  X(FMT_APPS_PAGE, "Apps %d/%d") \
  X(BRIGHTNESS, "Brightness") \
  X(SLEEP, "Sleep") \
  X(SOUND, "Sound") \
  X(VIBRATION, "Vibration") \
  X(UNITS, "Units") \
  X(ZONE, "Zone") \
  X(TIME_ZONE, "Time zone") \
  X(SET_TIME, "Set time") \
  X(SYNC_NOW, "Sync now") \
  X(NOT_SYNCED, "never") \
  X(HOUR, "Hour") \
  X(MINUTE, "Minute") \
  X(DAY, "Day") \
  X(MONTH, "Month") \
  X(YEAR, "Year") \
  X(ADD_NETWORK, "Add network") \
  X(NAME, "Name") \
  X(VERSION, "Version") \
  X(BATTERY, "Battery") \
  X(FREE_RAM, "Free RAM") \
  X(RESET_ALL, "Reset all") \
  X(CLEAR_DATA, "Clear data") \
  X(STORAGE, "Storage") \
  X(FMT_ENTRIES, "%u entries") \
  X(FORGET_Q, "Forget network?") \
  X(CLEAR_DATA_Q, "Clear app data?") \
  X(RESET_ALL_Q, "Reset all data?") \
  /* Time sync and Wi-Fi setup */ \
  X(SYNC_CONNECTING, "connecting") \
  X(SYNC_GETTING_TIME, "getting time") \
  X(SYNC_DONE, "done") \
  X(SYNC_FAILED, "failed") \
  X(SYNC_NO_WIFI, "no Wi-Fi") \
  X(LOOKING_FOR_NETWORKS, "Looking for networks") \
  X(CONNECTING_TO, "Connecting to") \
  X(OPEN_ON_PHONE, "Open on phone") \
  X(JOIN_WITH_PHONE, "1. Join with phone") \
  X(OPEN_THE_PAGE, "2. Open the page") \
  X(FMT_PASSWORD, "password %s") \
  X(TIME_SYNC, "Time sync") \
  X(SAVED, "Saved") \
  X(WIFI_START_FAILED, "Wi-Fi start failed") \
  /* Games */ \
  X(NEW_GAME_Q, "New game?") \
  X(GAME_OVER, "Game over") \
  X(SOLVED, "Solved!") \
  X(WIN_2048, "2048!") \
  X(THEME, "Theme") \
  X(FMT_SCORE, "%lu   best %lu") \
  X(FMT_MOVES, "%lu moves") \
  X(LIGHTS_OUT, "Lights Out") \
  X(SOKOBAN, "Sokoban") \
  X(LEVEL, "Level") \
  X(FMT_LEVEL, "Level %d") \
  X(FMT_MOVES_PUSHES, "%lu moves  %lu pushes") \
  X(RESTART_LEVEL_Q, "Restart level?") \
  X(RESET_TIMER_Q, "Reset the timer?") \
  X(CONNECT4, "Four in a Row") \
  X(DIFFICULTY, "Difficulty") \
  X(YOUR_TURN, "Your turn") \
  X(THINKING, "Thinking") \
  X(YOU_WIN, "You win") \
  X(DEVICE_WINS, "Device wins") \
  X(DRAW, "Draw") \
  X(FMT_TOTALS, "you %u : %u device") \
  /* Pomodoro */ \
  X(POMODORO, "Pomodoro") \
  X(FOCUS, "Focus") \
  X(SHORT_BREAK, "Short break") \
  X(LONG_BREAK, "Long break") \
  X(FMT_SESSION, "session %u of 4") \
  X(READY, "ready") \
  X(PAUSED, "paused") \
  X(START_BREAK, "start the break") \
  X(START_FOCUS, "start to focus") \
  X(START_MODE, "Start mode") \
  /* Walkie-talkie */ \
  X(WALKIE_TALKIE, "Walkie-talkie") \
  X(NO_WIFI_NETWORK, "No Wi-Fi network") \
  X(ADD_IN_SETTINGS, "Add one in Settings") \
  X(CONNECTING, "Connecting") \
  X(NO_CONNECTION, "No connection") \
  X(NO_OTHER_DEVICES, "No other devices") \
  X(SEARCHING, "Searching") \
  X(SWIPE_TO_SEARCH, "Swipe down to search") \
  X(TALKING, "Talking") \
  X(PUSH_TO_TALK, "Push green to talk") \
  X(PUSH_TO_STOP, "Push green to stop") \
  X(TAP_TO_CONNECT, "Tap a device to connect") \
  X(CONFIRM_ON_OTHER, "Confirm on the other device") \
  X(FMT_CONNECT_Q, "Connect %s?") \
  X(FMT_FORGET_Q, "Forget %s?") \
  X(FMT_ROW, "%s (%s)") \
  X(STATUS_NEW, "new") \
  X(STATUS_WAITING, "waiting") \
  X(STATUS_OFFLINE, "offline") \
  X(WAIT, "Wait") \
  X(IS_TALKING, "is talking") \
  X(BUSY, "Busy") \
  X(REFUSED, "Refused") \
  X(NO_ANSWER, "No answer") \
  X(CONNECTED, "Connected") \
  X(FORGOTTEN, "Forgotten") \
  X(NOBODY_ONLINE, "No paired device online") \
  X(LIST_FULL, "The list is full") \
  X(MIC_ERROR, "Microphone error") \
  /* Clock */ \
  X(CLOCK, "Clock") \
  X(FACE, "Face") \
  X(MON, "Mon") X(TUE, "Tue") X(WED, "Wed") X(THU, "Thu") X(FRI, "Fri") X(SAT, "Sat") X(SUN, "Sun") \
  X(JAN, "Jan") X(FEB, "Feb") X(MAR, "Mar") X(APR, "Apr") X(MAY, "May") X(JUN, "Jun") \
  X(JUL, "Jul") X(AUG, "Aug") X(SEP, "Sep") X(OCT, "Oct") X(NOV, "Nov") X(DEC, "Dec")
// clang-format on

enum class StrId : uint16_t {
#define PAPWICK_STRING_ID(key, text) STR_##key,
  PAPWICK_STRINGS(PAPWICK_STRING_ID)
#undef PAPWICK_STRING_ID
      STR__COUNT
};

namespace i18n {

// Returns the text of id in the current language. Only English exists now.
// Call it at run time, not in a static initializer. Then a language that loads at start-up applies to all texts.
const char* get(StrId id);

}  // namespace i18n

// The key is not macro-expanded, so keys such as LOW and DEC do not clash with Arduino macros.
#define tr(key) ::i18n::get(StrId::STR_##key)
