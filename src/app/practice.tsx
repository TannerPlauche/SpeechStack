import * as Haptics from 'expo-haptics';
import { Redirect, useLocalSearchParams, useRouter } from 'expo-router';
import { useRef, useState } from 'react';
import { Platform, Pressable, StyleSheet, Text, View, type LayoutChangeEvent } from 'react-native';
import Animated from 'react-native-reanimated';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { IconButton } from '@/components/Button';
import { CARD_ASPECT } from '@/components/Flashcard';
import { Icon } from '@/components/Icon';
import { Screen, TopBar } from '@/components/Screen';
import { SwipeCard, type SwipeCardHandle } from '@/components/SwipeCard';
import { getSound, getWord, getWords, positionLabel, shuffle, type Position, type WordEntry } from '@/data';
import { useSession, type CardResult } from '@/state/session';
import { colors, fonts, radius } from '@/theme';

type Params = {
  sound: string;
  position?: string;
  pictures?: string;
  shuffle?: string;
  /** Comma-separated word ids; when present, practice exactly these (e.g. missed words). */
  ids?: string;
};

function buildDeck(p: Params): WordEntry[] {
  const words = p.ids
    ? p.ids.split(',').map(getWord).filter((w): w is WordEntry => !!w)
    : getWords(p.sound, (p.position ?? 'initial') as Position | 'all', p.pictures === '1');
  return p.shuffle === '1' ? shuffle(words) : words;
}

function haptic(correct: boolean) {
  if (Platform.OS === 'web') return;
  Haptics.notificationAsync(
    correct ? Haptics.NotificationFeedbackType.Success : Haptics.NotificationFeedbackType.Warning,
  );
}

export default function Practice() {
  const router = useRouter();
  const params = useLocalSearchParams<Params>();
  const { setLastSession } = useSession();
  const insets = useSafeAreaInsets();

  const [deck] = useState(() => buildDeck(params));
  const [answers, setAnswers] = useState<CardResult[]>([]);
  const [area, setArea] = useState({ width: 0, height: 0 });
  const topCard = useRef<SwipeCardHandle>(null);

  const sound = getSound(params.sound);
  if (!sound || deck.length === 0) return <Redirect href="/" />;

  const index = answers.length;
  const correctCount = answers.filter((a) => a.correct).length;
  const title = params.ids ? `${sound.label} · Practice missed` : `${sound.label} · ${positionLabel(params.position ?? '')}`;

  const handleSwiped = (id: string, correct: boolean) => {
    haptic(correct);
    const next = [...answers, { id, correct }];
    setAnswers(next);
    if (next.length === deck.length) {
      setLastSession({
        sound: sound.id,
        position: params.position ?? 'all',
        isRetry: !!params.ids,
        results: next,
      });
      router.replace('/results');
    }
  };

  const undo = () => setAnswers((a) => a.slice(0, -1));

  const onLayout = (e: LayoutChangeEvent) => {
    const { width, height } = e.nativeEvent.layout;
    setArea({ width, height });
  };
  // Fit the card inside the deck area, leaving room for the stacked cards peeking below.
  const cardWidth = Math.min(area.width - 40, (area.height - 64) * CARD_ASPECT, 520);
  const cardLeft = (area.width - cardWidth) / 2;
  const cardTop = (area.height - cardWidth / CARD_ASPECT - 40) / 2;
  const visible = deck.slice(index, index + 3);

  return (
    <Screen>
      <TopBar
        left={<IconButton icon="x" label="End practice" onPress={() => router.back()} />}
        title={title}
        right={index > 0 ? <IconButton icon="undo" label="Undo last card" onPress={undo} /> : undefined}
      />

      <View style={styles.progressRow}>
        <Text style={styles.progressText}>
          {Math.min(index + 1, deck.length)} of {deck.length}
        </Text>
        <View style={styles.tally}>
          <Tally icon="check" color={colors.primary} count={correctCount} />
          <Tally icon="x" color={colors.coral} count={index - correctCount} />
        </View>
      </View>
      <ProgressBar value={index / deck.length} />

      <View style={styles.deck} onLayout={onLayout}>
        {cardWidth > 0 &&
          visible
            .map((word, i) => (
              <SwipeCard
                key={word.id}
                ref={i === 0 ? topCard : undefined}
                word={word}
                depth={i}
                width={cardWidth}
                left={cardLeft}
                top={cardTop}
                onSwiped={handleSwiped}
              />
            ))
            .reverse()}
      </View>

      <Text style={styles.hint}>Swipe right for correct · left to try again</Text>

      <View style={[styles.actions, { paddingBottom: Math.max(insets.bottom, 16) + 8 }]}>
        <AnswerButton label="Try again" icon="x" color={colors.coral} onPress={() => topCard.current?.swipe(false)} />
        <AnswerButton label="Correct" icon="check" color={colors.primary} onPress={() => topCard.current?.swipe(true)} />
      </View>
    </Screen>
  );
}

function ProgressBar({ value }: { value: number }) {
  return (
    <View style={styles.track}>
      <Animated.View
        style={[styles.fill, { width: `${value * 100}%`, transitionProperty: 'width', transitionDuration: 250 }]}
      />
    </View>
  );
}

function Tally({ icon, color, count }: { icon: 'check' | 'x'; color: string; count: number }) {
  return (
    <View style={styles.tallyItem}>
      <Icon name={icon} size={16} color={color} strokeWidth={3} />
      <Text style={[styles.tallyText, { color }]}>{count}</Text>
    </View>
  );
}

function AnswerButton(props: { label: string; icon: 'check' | 'x'; color: string; onPress: () => void }) {
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel={props.label}
      onPress={props.onPress}
      style={({ pressed }) => [styles.answer, pressed && { transform: [{ scale: 0.94 }] }]}
    >
      <View style={[styles.answerCircle, { backgroundColor: props.color }]}>
        <Icon name={props.icon} size={38} color={colors.white} strokeWidth={3.4} />
      </View>
      <Text style={[styles.answerLabel, { color: props.color }]}>{props.label}</Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  progressRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 24,
    marginTop: 4,
  },
  progressText: { fontFamily: fonts.bodyBold, fontSize: 15, color: colors.navyMuted },
  tally: { flexDirection: 'row', gap: 14 },
  tallyItem: { flexDirection: 'row', alignItems: 'center', gap: 4 },
  tallyText: { fontFamily: fonts.bodyBold, fontSize: 15 },
  track: {
    height: 10,
    borderRadius: radius.pill,
    backgroundColor: colors.border,
    marginHorizontal: 24,
    marginTop: 8,
    overflow: 'hidden',
  },
  fill: { height: '100%', borderRadius: radius.pill, backgroundColor: colors.gold },
  deck: { flex: 1, marginTop: 12 },
  hint: { textAlign: 'center', fontFamily: fonts.body, fontSize: 13, color: colors.navyMuted, marginTop: 6 },
  actions: { flexDirection: 'row', justifyContent: 'center', gap: 56, paddingTop: 14 },
  answer: { alignItems: 'center', gap: 6 },
  answerCircle: {
    width: 76,
    height: 76,
    borderRadius: 38,
    alignItems: 'center',
    justifyContent: 'center',
    shadowColor: colors.navy,
    shadowOpacity: 0.15,
    shadowRadius: 10,
    shadowOffset: { width: 0, height: 4 },
    elevation: 4,
  },
  answerLabel: { fontFamily: fonts.bodyBold, fontSize: 15 },
});
