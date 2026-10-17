import { useRouter } from 'expo-router';
import { Pressable, ScrollView, StyleSheet, Text, View, useWindowDimensions } from 'react-native';

import { Screen } from '@/components/Screen';
import { SOUNDS, WORDS, phonemeText } from '@/data';
import { accents, colors, fonts, radius, shadow } from '@/theme';

export default function Home() {
  const router = useRouter();
  const { width } = useWindowDimensions();
  const columns = width >= 700 ? 3 : 2;
  const gap = 14;
  const tileWidth = (Math.min(width, 900) - 40 - gap * (columns - 1)) / columns;

  return (
    <Screen>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.kicker}>Let’s practice!</Text>
        <Text style={styles.title}>Pick a sound</Text>

        <View style={[styles.grid, { gap }]}>
          {SOUNDS.map((sound, i) => {
            const accent = accents[i % accents.length];
            const words = WORDS.filter((w) => w.sound === sound.id);
            const pictures = words.filter((w) => w.image).length;
            return (
              <Pressable
                key={sound.id}
                accessibilityRole="button"
                accessibilityLabel={`${sound.label} sound`}
                onPress={() => router.push({ pathname: '/sound/[sound]', params: { sound: sound.id } })}
                style={({ pressed }) => [styles.tile, { width: tileWidth }, pressed && styles.pressed]}
              >
                <View style={[styles.badge, { backgroundColor: accent.solid }]}>
                  <Text style={[styles.badgeText, { color: accent.onSolid }]}>{sound.label}</Text>
                </View>
                <Text style={styles.phoneme}>{phonemeText(sound.phonemes)}</Text>
                <Text style={styles.meta}>
                  {words.length} words · {pictures} pictures
                </Text>
              </Pressable>
            );
          })}
        </View>
      </ScrollView>
    </Screen>
  );
}

const styles = StyleSheet.create({
  content: { padding: 20, paddingBottom: 48, width: '100%', maxWidth: 900, alignSelf: 'center' },
  kicker: { fontFamily: fonts.bodyBold, fontSize: 16, color: colors.primary, marginTop: 12 },
  title: { fontFamily: fonts.displayHeavy, fontSize: 40, color: colors.navy, lineHeight: 48, marginBottom: 20 },
  grid: { flexDirection: 'row', flexWrap: 'wrap' },
  tile: {
    backgroundColor: colors.white,
    borderRadius: radius.lg,
    paddingVertical: 22,
    paddingHorizontal: 16,
    alignItems: 'center',
    ...shadow,
  },
  pressed: { transform: [{ scale: 0.97 }], opacity: 0.9 },
  badge: {
    width: 84,
    height: 84,
    borderRadius: 42,
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 12,
  },
  badgeText: { fontFamily: fonts.displayHeavy, fontSize: 36, lineHeight: 46 },
  phoneme: { fontFamily: fonts.bodyBold, fontSize: 17, color: colors.navy },
  meta: { fontFamily: fonts.body, fontSize: 13, color: colors.navyMuted, marginTop: 4 },
});
