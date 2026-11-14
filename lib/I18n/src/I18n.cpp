#include "I18n.h"

namespace i18n {

namespace {

constexpr const char* kEnglish[] = {
#define PAPWICK_STRING_TEXT(key, text) text,
    PAPWICK_STRINGS(PAPWICK_STRING_TEXT)
#undef PAPWICK_STRING_TEXT
};
static_assert(sizeof(kEnglish) / sizeof(*kEnglish) == static_cast<int>(StrId::STR__COUNT), "one text for each key");

}  // namespace

const char* get(StrId id) {
  const auto i = static_cast<uint16_t>(id);
  return i < static_cast<uint16_t>(StrId::STR__COUNT) ? kEnglish[i] : "";
}

}  // namespace i18n
