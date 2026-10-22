# SpeechStack

An articulation flashcard app for speech-language pathologists and their clients. Built with Expo (SDK 58), React Native, and Expo Router.

**How it works:** pick a sound (R, S, Z, TH, K, G, SH, CH, J), then a word position (initial, medial, final, or mixed), and practice. Mark each card **correct** or **try again**, either with the buttons or by swiping right or left. At the end you get a score, the words said correctly, and the words that need practice, plus a button to drill just the missed words.

## Running

The app uses a custom native module (the Vowelsy formant tracker), so it **does not run in Expo Go**. Use a development build:

```bash
npm install
npx expo run:ios                 # builds and installs on the simulator
npx expo run:ios --device        # on a connected iPhone (needs code signing)
npx expo start --dev-client      # later: just start Metro for an installed build
```

The simulator usually has no microphone input, so test recording on a real device. The web build runs without recording.

Scripts: `npm run typecheck`, `npm run build:wordlists`.

## Project layout

```
src/app/                Expo Router screens
  index.tsx             Sound picker
  sound/[sound].tsx     Position picker + options (picture cards only, shuffle)
  practice.tsx          Swipe deck
  results.tsx           Score, correct / needs-practice lists
src/components/         Flashcard, SwipeCard, RecordControls, Button, Screen, Icon
src/recording/          Tracker session + hold-to-record hook (ported from vowelsy)
src/vowelsy/            Vowelsy server client, config and upload queue
src/state/speaker.ts    Client (speaker) details and voice type
modules/vowel-tracker/  Native formant tracker (copied from vowelsy)
plugins/                with-scene-lifecycle (iOS 27 UIScene requirement, from vowelsy)
src/data/               Word data (generated) + helpers
src/state/session.tsx   Holds the last practice round for the results screen
src/theme.ts            Palette, fonts, radii
assets/flashcards/      253 picture cards (960×1152 WebP)
data/source/            Original JSON and review notes from the flashcard package
licenses/               Vecteezy stock license PDFs
scripts/build-wordlists.mjs
```

## Recording and Vowelsy uploads

On each card, press and hold the mic button to record and let go to stop (it stops on its own at 5 s). **Play** appears once the card has a take; only the last take per card is kept.

Recording and analysis use `modules/vowel-tracker`, a copy of the native module from vowelsy (`mobile/modules/vowel-tracker`): the same FTracker formants and YIN pitch on 48 kHz audio, the same per-frame `{t, f0, f1, f2, f3}` frames, voiced-window trim and `FEATURE_VERSION`. **Keep the copy in sync with vowelsy** when the tracker changes. The tracker's voice type comes from the client: under 13 → child, otherwise adult male/female (`src/state/speaker.ts`).

When a round ends, each recorded card is uploaded to the Vowelsy server (`src/vowelsy/`), as the collector does:

1. `POST /audio` — the WAV
2. `POST /samples` — `phoneme` = the word's target IPA, speaker name/sex/age, frames, trim
3. `POST /samples/{id}/ratings` — the clinician's correct/try-again call, confidence 1.0, `rater_id: "speechstack"`

The server has no fields for the word, so the rating's `notes` holds JSON: `source: "speechstack"`, `session_id`, `round_id`, `word_id`, `word`, `ipa`, `sound`, `position`, `retry_round`. Upload progress and a Retry button are on the results screen; a retry resumes where it failed, so it never duplicates a sample.

The server address is `SERVER_URL` in `src/vowelsy/config.ts`. Client details are a placeholder (`DEV_SPEAKER` in `src/state/speaker.ts`, dev builds only) until the app has inputs for them; call `setSpeaker` from those inputs. Without a speaker, recordings are kept for playback but not uploaded.

## Word data

`data/source/wordlists.json` has 540 words (9 sounds × 3 positions × 20). 253 of them have finished picture cards; the other 287 are shown as text "word cards" until artwork is added. Turn on **Picture cards only** to hide them.

To add artwork, drop the new `.webp` files into `assets/flashcards/`, update `data/source/wordlists.json` (set `flashcard_status: "ready"`, `flashcard_file` and `flashcard_target_text_span`), then run:

```bash
npm run build:wordlists
```

That regenerates `src/data/wordlists.generated.ts`, which contains the static `require()` calls Metro needs to bundle the images.

The word list is marked as a starter draft; SLP review is recommended before clinical use. Stock image licenses and attribution requirements in `data/source/credits.json` and `licenses/` still apply.

## Palette

| Role | Color |
|---|---|
| Primary | Teal `#248F91` |
| Main accent | Sky Blue `#74C9E5` |
| Energy accent | Coral-Red `#FF5F57` |
| Reward accent | Golden Yellow `#FFD77A` |
| Background | Warm Cream `#FFF9EF` |
| Text | Deep Navy `#253B50` |
