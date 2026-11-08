#include "buzzer_audition.h"

namespace {

constexpr uint8_t BOOT_JINGLE_CANDIDATE_COUNT = 16;

template <size_t N>
constexpr uint8_t melodyLength(const buzzer_note_t (&)[N]) {
  static_assert(N <= 255, "Buzzer melody length must fit uint8_t");
  return static_cast<uint8_t>(N);
}

// The audition bank deliberately uses longer, phrase-shaped melodies than the
// actual boot default. This makes hardware judging useful without committing a
// multi-second song to every product boot before a shared Reflex theme wins.
const buzzer_note_t jingle_boot_classic_extended[] = {
  { NOTE_C5, DUR_EIGHTH }, { NOTE_E5, DUR_EIGHTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_C6, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_E5, DUR_EIGHTH },
  { NOTE_D5, DUR_EIGHTH }, { NOTE_C5, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_C5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_CLASSIC_EXTENDED = {
  jingle_boot_classic_extended, melodyLength(jingle_boot_classic_extended), 48
};

const buzzer_note_t jingle_boot_reflex_rise[] = {
  { NOTE_D5, DUR_EIGHTH }, { NOTE_FS5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_D6, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_C6, DUR_EIGHTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_FS5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_D6, DUR_EIGHTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_D6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_REFLEX_RISE = {
  jingle_boot_reflex_rise, melodyLength(jingle_boot_reflex_rise), 52
};

const buzzer_note_t jingle_boot_twin_pulse[] = {
  { NOTE_C5, DUR_SIXTEENTH }, { NOTE_C5, DUR_SIXTEENTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_REST, DUR_SIXTEENTH },
  { NOTE_E5, DUR_SIXTEENTH }, { NOTE_E5, DUR_SIXTEENTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_REST, DUR_SIXTEENTH },
  { NOTE_G5, DUR_SIXTEENTH }, { NOTE_G5, DUR_SIXTEENTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_TWIN_PULSE = {
  jingle_boot_twin_pulse, melodyLength(jingle_boot_twin_pulse), 54
};

const buzzer_note_t jingle_boot_prism[] = {
  { NOTE_C5, DUR_SIXTEENTH }, { NOTE_D5, DUR_SIXTEENTH },
  { NOTE_E5, DUR_SIXTEENTH }, { NOTE_G5, DUR_SIXTEENTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_REST, DUR_SIXTEENTH },
  { NOTE_G5, DUR_SIXTEENTH }, { NOTE_A5, DUR_SIXTEENTH },
  { NOTE_B5, DUR_SIXTEENTH }, { NOTE_D6, DUR_SIXTEENTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_D6, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_PRISM = {
  jingle_boot_prism, melodyLength(jingle_boot_prism), 52
};

const buzzer_note_t jingle_boot_nova[] = {
  { NOTE_A4, DUR_EIGHTH }, { NOTE_E5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_C6, DUR_QUARTER },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_E5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_A4, DUR_EIGHTH }, { NOTE_C5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_E6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_NOVA = {
  jingle_boot_nova, melodyLength(jingle_boot_nova), 54
};

const buzzer_note_t jingle_boot_arcade[] = {
  { NOTE_C5, DUR_SIXTEENTH }, { NOTE_E5, DUR_SIXTEENTH },
  { NOTE_G5, DUR_SIXTEENTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_REST, DUR_SIXTEENTH },
  { NOTE_G5, DUR_SIXTEENTH }, { NOTE_C6, DUR_SIXTEENTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_D6, DUR_EIGHTH },
  { NOTE_C6, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_A5, DUR_SIXTEENTH }, { NOTE_B5, DUR_SIXTEENTH },
  { NOTE_C6, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_ARCADE = {
  jingle_boot_arcade, melodyLength(jingle_boot_arcade), 58
};

const buzzer_note_t jingle_boot_signal[] = {
  { NOTE_E5, DUR_EIGHTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_E6, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_FS5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_GS5, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_CS6, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_GS5, DUR_EIGHTH },
  { NOTE_E6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_SIGNAL = {
  jingle_boot_signal, melodyLength(jingle_boot_signal), 54
};

const buzzer_note_t jingle_boot_minimal[] = {
  { NOTE_C5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_G5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_E5, DUR_QUARTER }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_C6, DUR_HALF }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_E5, DUR_QUARTER }, { NOTE_D5, DUR_QUARTER },
  { NOTE_C5, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_MINIMAL = {
  jingle_boot_minimal, melodyLength(jingle_boot_minimal), 66
};

const buzzer_note_t jingle_boot_power_parade[] = {
  { NOTE_C5, DUR_EIGHTH }, { NOTE_C5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_C6, DUR_QUARTER }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_E6, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_POWER_PARADE = {
  jingle_boot_power_parade, melodyLength(jingle_boot_power_parade), 52
};

const buzzer_note_t jingle_boot_neon_circuit[] = {
  { NOTE_E5, DUR_SIXTEENTH }, { NOTE_G5, DUR_SIXTEENTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_E6, DUR_EIGHTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_REST, DUR_SIXTEENTH },
  { NOTE_FS5, DUR_SIXTEENTH }, { NOTE_A5, DUR_SIXTEENTH },
  { NOTE_CS6, DUR_EIGHTH }, { NOTE_FS6, DUR_EIGHTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_CS6, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_E6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_NEON_CIRCUIT = {
  jingle_boot_neon_circuit, melodyLength(jingle_boot_neon_circuit), 56
};

const buzzer_note_t jingle_boot_pixel_horizon[] = {
  { NOTE_G4, DUR_EIGHTH }, { NOTE_C5, DUR_EIGHTH },
  { NOTE_D5, DUR_EIGHTH }, { NOTE_E5, DUR_QUARTER },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_E5, DUR_EIGHTH },
  { NOTE_D5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_G4, DUR_EIGHTH }, { NOTE_C5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_A5, DUR_QUARTER }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_D5, DUR_EIGHTH },
  { NOTE_C5, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_PIXEL_HORIZON = {
  jingle_boot_pixel_horizon, melodyLength(jingle_boot_pixel_horizon), 58
};

const buzzer_note_t jingle_boot_star_chaser[] = {
  { NOTE_A4, DUR_SIXTEENTH }, { NOTE_C5, DUR_SIXTEENTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_C6, DUR_EIGHTH }, { NOTE_E6, DUR_QUARTER },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_A5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_A6, DUR_EIGHTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_STAR_CHASER = {
  jingle_boot_star_chaser, melodyLength(jingle_boot_star_chaser), 52
};

const buzzer_note_t jingle_boot_boss_ready[] = {
  { NOTE_D4, DUR_EIGHTH }, { NOTE_D5, DUR_EIGHTH },
  { NOTE_F5, DUR_EIGHTH }, { NOTE_A5, DUR_QUARTER },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_F5, DUR_EIGHTH },
  { NOTE_D5, DUR_QUARTER }, { NOTE_REST, DUR_EIGHTH },
  { NOTE_C5, DUR_EIGHTH }, { NOTE_D5, DUR_EIGHTH },
  { NOTE_F5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_D6, DUR_QUARTER }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_D6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_BOSS_READY = {
  jingle_boot_boss_ready, melodyLength(jingle_boot_boss_ready), 60
};

const buzzer_note_t jingle_boot_victory_lap[] = {
  { NOTE_G5, DUR_EIGHTH }, { NOTE_G5, DUR_SIXTEENTH },
  { NOTE_A5, DUR_SIXTEENTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_D6, DUR_QUARTER }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_C6, DUR_EIGHTH }, { NOTE_E6, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_A5, DUR_EIGHTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_B5, DUR_EIGHTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_G6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_VICTORY_LAP = {
  jingle_boot_victory_lap, melodyLength(jingle_boot_victory_lap), 54
};

const buzzer_note_t jingle_boot_night_drive[] = {
  { NOTE_E4, DUR_EIGHTH }, { NOTE_B4, DUR_EIGHTH },
  { NOTE_E5, DUR_QUARTER }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_FS5, DUR_EIGHTH }, { NOTE_E5, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_D5, DUR_EIGHTH }, { NOTE_FS5, DUR_EIGHTH },
  { NOTE_A5, DUR_QUARTER }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_FS5, DUR_EIGHTH }, { NOTE_D5, DUR_QUARTER },
  { NOTE_B4, DUR_EIGHTH }, { NOTE_D5, DUR_EIGHTH },
  { NOTE_E5, DUR_EIGHTH }, { NOTE_B5, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_NIGHT_DRIVE = {
  jingle_boot_night_drive, melodyLength(jingle_boot_night_drive), 62
};

const buzzer_note_t jingle_boot_reflex_anthem[] = {
  { NOTE_C5, DUR_EIGHTH }, { NOTE_G5, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_E5, DUR_EIGHTH },
  { NOTE_F5, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_B5, DUR_EIGHTH }, { NOTE_G5, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_C5, DUR_EIGHTH }, { NOTE_E5, DUR_EIGHTH },
  { NOTE_G5, DUR_EIGHTH }, { NOTE_C6, DUR_QUARTER },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_E6, DUR_EIGHTH },
  { NOTE_D6, DUR_EIGHTH }, { NOTE_C6, DUR_QUARTER },
  { NOTE_REST, DUR_EIGHTH },
  { NOTE_A5, DUR_EIGHTH }, { NOTE_C6, DUR_EIGHTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_G6, DUR_EIGHTH },
  { NOTE_E6, DUR_EIGHTH }, { NOTE_D6, DUR_EIGHTH },
  { NOTE_C6, DUR_HALF },
};
const buzzer_melody_t MELODY_BOOT_REFLEX_ANTHEM = {
  jingle_boot_reflex_anthem, melodyLength(jingle_boot_reflex_anthem), 54
};

struct BootJingleCandidate {
  const char* name;
  const buzzer_melody_t* melody;
};

const BootJingleCandidate kBootJingleCandidates[BOOT_JINGLE_CANDIDATE_COUNT] = {
  { "Classic Extended", &MELODY_BOOT_CLASSIC_EXTENDED },
  { "Reflex Rise",      &MELODY_BOOT_REFLEX_RISE },
  { "Twin Pulse",       &MELODY_BOOT_TWIN_PULSE },
  { "Prism Run",        &MELODY_BOOT_PRISM },
  { "Nova Flight",      &MELODY_BOOT_NOVA },
  { "Arcade Quest",     &MELODY_BOOT_ARCADE },
  { "Signal Path",      &MELODY_BOOT_SIGNAL },
  { "Minimal Bloom",    &MELODY_BOOT_MINIMAL },
  { "Power Parade",     &MELODY_BOOT_POWER_PARADE },
  { "Neon Circuit",     &MELODY_BOOT_NEON_CIRCUIT },
  { "Pixel Horizon",    &MELODY_BOOT_PIXEL_HORIZON },
  { "Star Chaser",      &MELODY_BOOT_STAR_CHASER },
  { "Boss Ready",       &MELODY_BOOT_BOSS_READY },
  { "Victory Lap",      &MELODY_BOOT_VICTORY_LAP },
  { "Night Drive",      &MELODY_BOOT_NIGHT_DRIVE },
  { "Reflex Anthem",    &MELODY_BOOT_REFLEX_ANTHEM },
};

}  // namespace

uint8_t buzzerAuditionCandidateCount() {
  return BOOT_JINGLE_CANDIDATE_COUNT;
}

const char* buzzerAuditionCandidateName(uint8_t index) {
  return index < BOOT_JINGLE_CANDIDATE_COUNT
      ? kBootJingleCandidates[index].name
      : "Unknown";
}

const buzzer_melody_t* buzzerAuditionCandidateMelody(uint8_t index) {
  return index < BOOT_JINGLE_CANDIDATE_COUNT
      ? kBootJingleCandidates[index].melody
      : nullptr;
}
