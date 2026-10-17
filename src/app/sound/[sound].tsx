import { Redirect, useLocalSearchParams, useRouter } from 'expo-router';
import { useState } from 'react';
import { Pressable, ScrollView, StyleSheet, Switch, Text, View } from 'react-native';

import { IconButton } from '@/components/Button';
import { Icon } from '@/components/Icon';
import { Screen, TopBar } from '@/components/Screen';
import { POSITIONS, SOUNDS, getSound, getWords, phonemeText, type Position } from '@/data';
import { accents, colors, fonts, radius, shadow } from '@/theme';

export default function SoundScreen() {
  const router = useRouter();
  const { sound: soundId } = useLocalSearchParams<{ sound: string }>();
  const sound = getSound(soundId);
  const [picturesOnly, setPicturesOnly] = useState(false);
  const [shuffled, setShuffled] = useState(true);

  if (!sound) return <Redirect href="/" />;

  const accent = accents[SOUNDS.indexOf(sound) % accents.length];

  const start = (position: Position | 'all') =>
    router.push({
      pathname: '/practice',
      params: { sound: sound.id, position, pictures: picturesOnly ? '1' : '0', shuffle: shuffled ? '1' : '0' },
    });

  return (
    <Screen>
      <TopBar left={<IconButton icon="back" label="Back" onPress={() => router.back()} />} />
      <ScrollView contentContainerStyle={styles.content}>
        <View style={styles.hero}>
          <View style={[styles.badge, { backgroundColor: accent.solid }]}>
            <Text style={[styles.badgeText, { color: accent.onSolid }]}>{sound.label}</Text>
          </View>
          <View>
            <Text style={styles.title}>{sound.label} sound</Text>
            <Text style={styles.phoneme}>{phonemeText(sound.phonemes)}</Text>
          </View>
        </View>

        <Text style={styles.section}>Where is the sound?</Text>
        {POSITIONS.map((p) => {
          const count = getWords(sound.id, p.id, picturesOnly).length;
          return (
            <PositionCard
              key={p.id}
              label={p.label}
              hint={p.hint}
              count={count}
              slot={POSITIONS.indexOf(p)}
              color={accent.solid}
              onPress={() => start(p.id)}
            />
          );
        })}
        <PositionCard
          label="Mixed"
          hint="All positions together"
          count={getWords(sound.id, 'all', picturesOnly).length}
          slot={null}
          color={accent.solid}
          onPress={() => start('all')}
        />

        <Text style={styles.section}>Options</Text>
        <View style={styles.options}>
          <OptionRow label="Picture cards only" value={picturesOnly} onChange={setPicturesOnly} />
          <View style={styles.divider} />
          <OptionRow label="Shuffle cards" value={shuffled} onChange={setShuffled} />
        </View>
      </ScrollView>
    </Screen>
  );
}

function PositionCard(props: {
  label: string;
  hint: string;
  count: number;
  /** Which of the three word segments to highlight; null highlights all. */
  slot: number | null;
  color: string;
  onPress: () => void;
}) {
  const disabled = props.count === 0;
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel={`${props.label}, ${props.count} words`}
      disabled={disabled}
      onPress={props.onPress}
      style={({ pressed }) => [styles.card, pressed && styles.pressed, disabled && styles.disabled]}
    >
      <View style={styles.diagram}>
        {[0, 1, 2].map((i) => (
          <View
            key={i}
            style={[
              styles.segment,
              { backgroundColor: props.slot === null || props.slot === i ? props.color : colors.border },
            ]}
          />
        ))}
      </View>
      <View style={{ flex: 1 }}>
        <Text style={styles.cardTitle}>{props.label}</Text>
        <Text style={styles.cardHint}>{props.hint}</Text>
      </View>
      <View style={styles.countPill}>
        <Text style={styles.countText}>{props.count}</Text>
      </View>
      <View style={styles.chevron}>
        <Icon name="back" size={18} color={colors.navyMuted} />
      </View>
    </Pressable>
  );
}

function OptionRow({ label, value, onChange }: { label: string; value: boolean; onChange: (v: boolean) => void }) {
  return (
    <View style={styles.optionRow}>
      <Text style={styles.optionLabel}>{label}</Text>
      <Switch
        value={value}
        onValueChange={onChange}
        trackColor={{ true: colors.primary, false: colors.border }}
        thumbColor={colors.white}
        ios_backgroundColor={colors.border}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  content: { paddingHorizontal: 20, paddingBottom: 48, width: '100%', maxWidth: 700, alignSelf: 'center' },
  hero: { flexDirection: 'row', alignItems: 'center', gap: 16, marginTop: 4, marginBottom: 8 },
  badge: { width: 72, height: 72, borderRadius: 36, alignItems: 'center', justifyContent: 'center' },
  badgeText: { fontFamily: fonts.displayHeavy, fontSize: 30, lineHeight: 40 },
  title: { fontFamily: fonts.displayHeavy, fontSize: 32, color: colors.navy, lineHeight: 38 },
  phoneme: { fontFamily: fonts.bodyBold, fontSize: 16, color: colors.navyMuted },
  section: {
    fontFamily: fonts.bodyBold,
    fontSize: 14,
    color: colors.navyMuted,
    textTransform: 'uppercase',
    letterSpacing: 1,
    marginTop: 24,
    marginBottom: 10,
  },
  card: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 16,
    backgroundColor: colors.white,
    borderRadius: radius.md,
    padding: 18,
    marginBottom: 12,
    ...shadow,
  },
  pressed: { transform: [{ scale: 0.98 }], opacity: 0.9 },
  disabled: { opacity: 0.45 },
  diagram: { flexDirection: 'row', gap: 4 },
  segment: { width: 16, height: 16, borderRadius: 8 },
  cardTitle: { fontFamily: fonts.display, fontSize: 22, color: colors.navy, lineHeight: 28 },
  cardHint: { fontFamily: fonts.body, fontSize: 14, color: colors.navyMuted },
  countPill: {
    minWidth: 36,
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: radius.pill,
    backgroundColor: colors.goldSoft,
    alignItems: 'center',
  },
  countText: { fontFamily: fonts.bodyBold, fontSize: 15, color: colors.navy },
  chevron: { transform: [{ rotate: '180deg' }] },
  options: { backgroundColor: colors.white, borderRadius: radius.md, paddingHorizontal: 18, ...shadow },
  optionRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', paddingVertical: 14 },
  optionLabel: { fontFamily: fonts.bodyBold, fontSize: 16, color: colors.navy },
  divider: { height: 1, backgroundColor: colors.border },
});
