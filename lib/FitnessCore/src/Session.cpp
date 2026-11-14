#include "Session.h"

#include "Body.h"
#include "Exercise.h"

namespace fit {

namespace {

constexpr uint32_t kHalfwayMinMs = 20000;
constexpr uint32_t kCountdownMinMs = 5000;

struct CueOut {
  Cue* out;
  int max;
  int count = 0;
  void push(Cue c) {
    if (count < max) out[count++] = c;
  }
};

void enterStep(Session& s, int step, CueOut& cues) {
  s.step = step;
  s.elapsedMs = 0;
  s.halfwayCued = false;
  s.countdown = 3;
  if (s.step >= s.workout->count) {
    s.finished = true;
    cues.push(Cue::Done);
    return;
  }
  cues.push(s.workout->steps[s.step].rest ? Cue::Rest : Cue::Go);
}

}  // namespace

int start(Session& s, const Workout* w, float weightKg, Cue* out, int maxOut) {
  s = Session{};
  s.workout = w;
  s.weightKg = weightKg;
  CueOut cues{out, maxOut};
  if (!w || w->count == 0) {
    s.finished = true;
    cues.push(Cue::Done);
  } else {
    cues.push(Cue::GetReady);
  }
  return cues.count;
}

int update(Session& s, uint32_t dtMs, Cue* out, int maxOut) {
  CueOut cues{out, maxOut};
  if (!s.workout || s.finished || s.paused) return 0;

  while (dtMs > 0 && !s.finished) {
    const uint32_t dur = stepDurationMs(s);
    const uint32_t take = dtMs < dur - s.elapsedMs ? dtMs : dur - s.elapsedMs;
    s.elapsedMs += take;
    dtMs -= take;

    const Step* st = currentStep(s);
    const bool work = st && !st->rest;
    if (work) {
      s.workMs += take;
      s.kcal += kcal(exercise(st->exercise).met, s.weightKg, take / 1000.0f);
      if (!s.halfwayCued && dur >= kHalfwayMinMs && s.elapsedMs >= dur / 2) {
        s.halfwayCued = true;
        cues.push(Cue::Halfway);
      }
    }

    const uint32_t remaining = dur - s.elapsedMs;
    int pending = 0;
    while (dur >= kCountdownMinMs && s.countdown >= 1 && remaining <= static_cast<uint32_t>(s.countdown) * 1000) {
      pending = s.countdown--;
    }
    if (pending > 0 && remaining > 0) cues.push(pending == 3 ? Cue::Three : (pending == 2 ? Cue::Two : Cue::One));

    if (s.elapsedMs >= dur) enterStep(s, s.step + 1, cues);
  }
  return cues.count;
}

int skip(Session& s, Cue* out, int maxOut) {
  CueOut cues{out, maxOut};
  if (!s.workout || s.finished) return 0;
  enterStep(s, s.step + 1, cues);
  return cues.count;
}

void togglePause(Session& s) {
  if (!s.finished) s.paused = !s.paused;
}

const Step* currentStep(const Session& s) {
  if (!s.workout || s.step < 0 || s.step >= s.workout->count) return nullptr;
  return &s.workout->steps[s.step];
}

const Step* nextExercise(const Session& s) {
  if (!s.workout) return nullptr;
  const Step* cur = currentStep(s);
  for (int i = s.step + ((cur && !cur->rest) ? 1 : 0); i < s.workout->count; ++i) {
    if (i >= 0 && !s.workout->steps[i].rest) return &s.workout->steps[i];
  }
  return nullptr;
}

uint32_t stepDurationMs(const Session& s) {
  const Step* st = currentStep(s);
  return st ? st->seconds * 1000u : kGetReadyMs;
}

uint32_t remainingMs(const Session& s) {
  const uint32_t dur = stepDurationMs(s);
  return s.elapsedMs < dur ? dur - s.elapsedMs : 0;
}

int exerciseNumber(const Session& s) {
  if (!s.workout) return 0;
  int n = 0;
  for (int i = 0; i <= s.step && i < s.workout->count; ++i) {
    if (!s.workout->steps[i].rest) ++n;
  }
  const Step* cur = currentStep(s);
  return (cur && !cur->rest) ? n : n + 1;
}

int exerciseTotal(const Workout& w) {
  int n = 0;
  for (int i = 0; i < w.count; ++i) {
    if (!w.steps[i].rest) ++n;
  }
  return n;
}

}  // namespace fit
