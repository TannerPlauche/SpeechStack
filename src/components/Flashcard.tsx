import { Image } from 'expo-image';
import { StyleSheet, Text, View } from 'react-native';

import type { WordEntry } from '@/data';
import { colors, fonts, radius } from '@/theme';

/** The source cards are 960 × 1152. */
export const CARD_ASPECT = 960 / 1152;

/**
 * Picture flashcard. The artwork already includes the word and the coral
 * target underline; words still awaiting artwork get a matching text card.
 */
export function Flashcard({ word }: { word: WordEntry }) {
  return (
    <View style={styles.card}>
      {word.image ? (
        <Image
          source={word.image}
          style={StyleSheet.absoluteFill}
          contentFit="cover"
          transition={120}
          accessibilityLabel={word.word}
        />
      ) : (
        <TextCard word={word} />
      )}
    </View>
  );
}

function TextCard({ word }: { word: WordEntry }) {
  const [start, end] = word.targetSpan ?? [0, 0];
  const hasSpan = end > start;
  return (
    <View style={styles.textCard}>
      <View style={styles.chip}>
        <Text style={styles.chipText}>Word card</Text>
      </View>
      <Text style={styles.word} adjustsFontSizeToFit numberOfLines={2} accessibilityLabel={word.word}>
        {hasSpan ? (
          <>
            {word.word.slice(0, start)}
            <Text style={styles.target}>{word.word.slice(start, end)}</Text>
            {word.word.slice(end)}
          </>
        ) : (
          word.word
        )}
      </Text>
      <Text style={styles.ipa}>/{word.ipa}/</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    width: '100%',
    aspectRatio: CARD_ASPECT,
    borderRadius: radius.lg,
    backgroundColor: colors.white,
    overflow: 'hidden',
  },
  textCard: { flex: 1, alignItems: 'center', justifyContent: 'center', padding: 28, gap: 18 },
  chip: { backgroundColor: colors.skySoft, borderRadius: radius.pill, paddingHorizontal: 14, paddingVertical: 6 },
  chipText: { fontFamily: fonts.bodyBold, fontSize: 13, color: colors.primary, letterSpacing: 0.5 },
  word: { fontFamily: fonts.display, fontSize: 56, color: colors.navy, textAlign: 'center' },
  target: { color: colors.coral, textDecorationLine: 'underline', textDecorationColor: colors.coral },
  ipa: { fontFamily: fonts.body, fontSize: 18, color: colors.navyMuted },
});
