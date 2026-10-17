# SpeechStack

An articulation flashcard app for speech-language pathologists and their clients. Built with Expo (SDK 58), React Native, and Expo Router.

**How it works:** pick a sound (R, S, Z, TH, K, G, SH, CH, J), then a word position (initial, medial, final, or mixed), and practice. Mark each card **correct** or **try again**, either with the buttons or by swiping right or left. At the end you get a score, the words said correctly, and the words that need practice, plus a button to drill just the missed words.

## Running

```bash
npm install
npx expo start          # then press i (iOS), a (Android) or w (web)
```

Scripts: `npm run typecheck`, `npm run build:wordlists`.

## Project layout

```
src/app/                Expo Router screens
  index.tsx             Sound picker
  sound/[sound].tsx     Position picker + options (picture cards only, shuffle)
  practice.tsx          Swipe deck
  results.tsx           Score, correct / needs-practice lists
src/components/         Flashcard, SwipeCard, Button, Screen, Icon
src/data/               Word data (generated) + helpers
src/state/session.tsx   Holds the last practice round for the results screen
src/theme.ts            Palette, fonts, radii
assets/flashcards/      253 picture cards (960×1152 WebP)
data/source/            Original JSON and review notes from the flashcard package
licenses/               Vecteezy stock license PDFs
scripts/build-wordlists.mjs
```

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
