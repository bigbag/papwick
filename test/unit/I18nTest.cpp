// Arduino defines these names as macros. The keys must still work.
#define LOW 0x0
#define HIGH 0x1
#define DEC 10

#include <cstring>

#include "I18n.h"
#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("I18n");
  bool allSet = true;
  for (int i = 0; i < static_cast<int>(StrId::STR__COUNT); ++i) {
    const char* text = i18n::get(static_cast<StrId>(i));
    allSet = allSet && text && *text;
  }
  runner.expectTrue(allSet, "each key has a text");
  runner.expectEqual("back", tr(BACK), "tr() returns the English text");
  runner.expectEqual("Low", tr(LOW), "an Arduino macro name works as a key");
  runner.expectEqual("Dec", tr(DEC), "DEC works as a key");
  runner.expectEqual("", i18n::get(StrId::STR__COUNT), "an id out of range gives an empty text");
  runner.expectTrue(std::strchr(tr(FMT_ENTRIES), '%') != nullptr, "a format key holds a format");
  return runner.exitCode();
}
