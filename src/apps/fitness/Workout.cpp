#include <BoardSupport.h>
#include <Exercise.h>
#include <Logging.h>
#include <Session.h>

#include <cstdio>
#include <cstdlib>

#include "App.h"
#include "Figure.h"
#include "Sound.h"

namespace {

constexpr const char* TAG = "WORKOUT";
constexpr const char* kCueNames[] = {"get ready", "go", "rest", "halfway", "three", "two", "one", "done"};

const char* nameOf(const fit::Step* st) { return st ? fit::exercise(st->exercise).name : nullptr; }

class WorkoutScreen : public Screen {
 public:
  void enter() override {
    if (!fig_.getBuffer()) {
      fig_.setColorDepth(16);
      if (!fig_.createSprite(figure::kW, figure::kH)) LOG_ERR(TAG, "Figure sprite alloc failed");
    }
    askEnd_ = false;
    day_ = fit::currentDay(app::progress);
    workout_ = app::buildWorkout(day_);
    fit::Cue cues[fit::kMaxCues];
    const int n = fit::start(session_, &workout_, app::profile.weightKg, cues, fit::kMaxCues);
    startMs_ = lastMs_ = millis();
    animMs_ = 0;
    handle(cues, n);
    LOG_INF(TAG, "Day %d: %d steps, %lu s planned", day_ + 1, workout_.count,
            static_cast<unsigned long>(fit::totalSeconds(workout_)));
    dirty = true;
  }

  void update(const ui::Input& in) override {
    const uint32_t now = millis();
    if (!session_.paused) animMs_ += now - lastMs_;
    fit::Cue cues[fit::kMaxCues];
    int n = fit::update(session_, now - lastMs_, cues, fit::kMaxCues);
    lastMs_ = now;
    handle(cues, n);

    if (askEnd_) {
      if (in.green) {
        board::stopAudio();
        LOG_INF(TAG, "End at step %d", session_.step);
        app::go(ScreenId::Home);
        return;
      }
      if (in.blue) {
        askEnd_ = false;
        dirty = true;
      }
      return;
    }
    if (in.blue && session_.paused) {
      askEnd_ = true;
      dirty = true;
      return;
    }
    if (in.blue || (in.green && session_.paused)) {
      togglePause();
      return;
    }
    if (in.green) {
      board::stopAudio();
      LOG_INF(TAG, "Skip step %d", session_.step);
      n = fit::skip(session_, cues, fit::kMaxCues);
      handle(cues, n);
      dirty = true;
    }
    if (session_.finished) {
      finish();
      return;
    }
    if (secondsLeft() != shownSec_) dirty = true;
  }

  void leave() override {
    if (!session_.paused) togglePause();
  }

  void draw() override {
    if (askEnd_) {
      ui::confirmDraw("End workout?", "end");
      return;
    }
    ui::clear();
    const fit::Step* st = fit::currentStep(session_);
    const bool rest = st && st->rest;
    const uint16_t color = !st ? ui::kAmber : (rest ? ui::kBlue : ui::kGreen);
    const uint32_t dur = fit::stepDurationMs(session_);
    ui::progressRing(dur ? static_cast<float>(fit::remainingMs(session_)) / dur : 0.0f, color);

    char line[40];
    snprintf(line, sizeof(line), "%d / %d", fit::exerciseNumber(session_), fit::exerciseTotal(workout_));
    ui::text(line, ui::cx(), 62, &fonts::DejaVu18, ui::kGrey);
    const char* title = session_.paused ? "PAUSED" : (!st ? "GET READY" : (rest ? "REST" : nameOf(st)));
    ui::text(title, ui::cx(), 100, &fonts::DejaVu24, session_.paused ? ui::kAmber : color);

    if (const fit::Step* shown = shownStep()) figure::draw(ui::canvas(), figX(), kFigY, shown->exercise, animMs_);

    shownSec_ = secondsLeft();
    snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(shownSec_));
    ui::text(line, ui::cx(), 312, &fonts::DejaVu56);

    if (session_.paused) {
      ui::hints(ui::kHintsY, "end", "resume");
      return;
    }
    if (st && !rest) {
      ui::text(fit::exercise(st->exercise).cue, ui::cx(), 360, &fonts::DejaVu12, ui::kGrey);
    } else if (const char* next = nameOf(fit::nextExercise(session_))) {
      snprintf(line, sizeof(line), "Next: %s", next);
      ui::text(line, ui::cx(), 360, &fonts::DejaVu18);
    }
    ui::hints(ui::kHintsY, "pause", "skip");
  }

  void animate() override {
    const fit::Step* shown = shownStep();
    if (session_.paused || !shown || !fig_.getBuffer()) return;
    fig_.fillScreen(TFT_BLACK);
    figure::draw(fig_, 0, 0, shown->exercise, animMs_);
    fig_.pushSprite(figX(), kFigY);
  }

 private:
  static constexpr int kFigY = 122;

  static int figX() { return ui::cx() - figure::kW / 2; }

  void togglePause() {
    fit::togglePause(session_);
    board::vibrate(60);
    LOG_INF(TAG, "%s at step %d", session_.paused ? "Paused" : "Resumed", session_.step);
    dirty = true;
  }

  uint32_t secondsLeft() const { return (fit::remainingMs(session_) + 999) / 1000; }

  // The figure shows the current exercise, or the next one during rest and get ready.
  const fit::Step* shownStep() const {
    const fit::Step* st = fit::currentStep(session_);
    return st && !st->rest ? st : fit::nextExercise(session_);
  }

  void handle(const fit::Cue* cues, int n) {
    for (int i = 0; i < n; ++i) {
      LOG_INF(TAG, "Cue %s, step %d %s", kCueNames[static_cast<int>(cues[i])], session_.step,
              nameOf(fit::currentStep(session_)) ?: "-");
      sound::cue(cues[i]);
      dirty = true;
    }
  }

  void finish() {
    app::last = {day_, session_.workMs / 1000, (millis() - startMs_) / 1000, session_.kcal};
    LOG_INF(TAG, "Done day %d: active %lu s, total %lu s, %.1f kcal", day_ + 1,
            static_cast<unsigned long>(app::last.activeSec), static_cast<unsigned long>(app::last.totalSec),
            app::last.kcal);
    fit::markDone(app::progress, app::last.day);
    ++app::progress.workouts;
    app::progress.activeSec += app::last.activeSec;
    app::progress.kcal += app::last.kcal;
    app::saveProgress();
    app::go(ScreenId::Summary);
  }

  M5Canvas fig_{&M5.Display};
  int day_ = 0;
  fit::Workout workout_;
  fit::Session session_;
  uint32_t startMs_ = 0, lastMs_ = 0, animMs_ = 0, shownSec_ = 0;
  bool askEnd_ = false;
};

const ui::Rect kFeedback[] = {{70, 300, 104, 54}, {182, 300, 104, 54}, {294, 300, 104, 54}};

class Summary : public Screen {
 public:
  void update(const ui::Input& in) override {
    for (int i = 0; i < 3; ++i) {
      if (in.tap && kFeedback[i].hit(in.x, in.y)) {
        save(static_cast<fit::Feedback>(i));
        return;
      }
    }
    if (in.green) save(fit::Feedback::Ok);
  }

  void draw() override {
    ui::clear();
    ui::progressRing(1.0f, ui::kGreen);
    char line[32];
    ui::text("Workout complete", ui::cx(), 100, &fonts::DejaVu24, ui::kGreen);
    snprintf(line, sizeof(line), "%lu:%02lu", static_cast<unsigned long>(app::last.totalSec / 60),
             static_cast<unsigned long>(app::last.totalSec % 60));
    ui::text(line, ui::cx(), 165, &fonts::DejaVu56);
    snprintf(line, sizeof(line), "~%.0f kcal", app::last.kcal);
    ui::text(line, ui::cx(), 220, &fonts::DejaVu24);
    ui::text("How was it?", ui::cx(), 268, &fonts::DejaVu18, ui::kGrey);
    ui::button(kFeedback[0], "Hard", ui::kRed);
    ui::button(kFeedback[1], "OK", ui::kDark);
    ui::button(kFeedback[2], "Easy", ui::kDark);
    ui::hints(ui::kHintsY, nullptr, "OK");
  }

 private:
  void save(fit::Feedback f) {
    fit::applyFeedback(app::progress, f);
    app::saveProgress();
    LOG_INF(TAG, "Feedback %d, adjust now %d s", static_cast<int>(f), app::progress.adjustSec);
    app::go(ScreenId::Home);
  }
};

}  // namespace

Screen& app::workoutScreen() {
  static WorkoutScreen s;
  return s;
}

Screen& app::summaryScreen() {
  static Summary s;
  return s;
}
