import { useEffect, useImperativeHandle, type Ref } from 'react';
import { StyleSheet, Text, View, useWindowDimensions } from 'react-native';
import { GestureDetector, usePanGesture } from 'react-native-gesture-handler';
import Animated, {
  Extrapolation,
  interpolate,
  useAnimatedStyle,
  useSharedValue,
  withSpring,
  withTiming,
} from 'react-native-reanimated';
import { scheduleOnRN } from 'react-native-worklets';

import type { WordEntry } from '@/data';
import { colors, fonts, radius, shadow } from '@/theme';
import { Flashcard } from './Flashcard';
import { Icon } from './Icon';

export type SwipeCardHandle = { swipe: (correct: boolean) => void };

type Props = {
  word: WordEntry;
  /** 0 = top card, 1 = next card peeking behind, 2 = further back. */
  depth: number;
  width: number;
  /** Top-left position of the card inside the deck area. */
  left: number;
  top: number;
  onSwiped: (id: string, correct: boolean) => void;
  ref?: Ref<SwipeCardHandle>;
};

const SPRING = { damping: 18, stiffness: 180 };

/** A flashcard that can be swiped right (correct) or left (incorrect). */
export function SwipeCard({ word, depth, width, left, top, onSwiped, ref }: Props) {
  const { width: screenWidth } = useWindowDimensions();
  const tx = useSharedValue(0);
  const ty = useSharedValue(0);
  const stack = useSharedValue(depth);
  const leaving = useSharedValue(false);

  useEffect(() => {
    stack.value = withSpring(depth, SPRING);
  }, [depth, stack]);

  const threshold = screenWidth * 0.25;
  const offscreen = screenWidth * 1.4;

  const fling = (correct: boolean) => {
    'worklet';
    if (leaving.value) return;
    leaving.value = true;
    tx.value = withTiming(correct ? offscreen : -offscreen, { duration: 240 }, (finished) => {
      if (finished) scheduleOnRN(onSwiped, word.id, correct);
    });
  };

  useImperativeHandle(ref, () => ({ swipe: (correct: boolean) => fling(correct) }));

  const pan = usePanGesture({
    enabled: depth === 0,
    activeOffsetX: [-12, 12],
    onUpdate: (e) => {
      'worklet';
      if (leaving.value) return;
      tx.value = e.translationX;
      ty.value = e.translationY * 0.25;
    },
    onDeactivate: (e) => {
      'worklet';
      // A quick flick counts as well as a long drag.
      const projected = e.translationX + e.velocityX * 0.15;
      if (Math.abs(projected) > threshold) {
        fling(projected > 0);
      } else {
        tx.value = withSpring(0, SPRING);
      }
      ty.value = withSpring(0, SPRING);
    },
  });

  const cardStyle = useAnimatedStyle(() => ({
    zIndex: 10 - Math.round(stack.value),
    transform: [
      { translateX: tx.value },
      { translateY: ty.value + stack.value * 28 },
      { rotate: `${interpolate(tx.value, [-screenWidth, 0, screenWidth], [-14, 0, 14])}deg` },
      { scale: 1 - stack.value * 0.06 },
    ],
    opacity: interpolate(stack.value, [0, 1, 2, 3], [1, 1, 0.7, 0], Extrapolation.CLAMP),
  }));

  const correctStyle = useAnimatedStyle(() => ({
    opacity: interpolate(tx.value, [0, threshold], [0, 1], Extrapolation.CLAMP),
  }));
  const incorrectStyle = useAnimatedStyle(() => ({
    opacity: interpolate(tx.value, [-threshold, 0], [1, 0], Extrapolation.CLAMP),
  }));

  return (
    <GestureDetector gesture={pan} touchAction="pan-y">
      <Animated.View style={[styles.wrap, { width, left, top }, cardStyle]}>
        <Flashcard word={word} />
        <Animated.View pointerEvents="none" style={[styles.overlay, styles.correctWash, correctStyle]}>
          <Stamp label="Correct" color={colors.primary} icon="check" />
        </Animated.View>
        <Animated.View pointerEvents="none" style={[styles.overlay, styles.incorrectWash, incorrectStyle]}>
          <Stamp label="Try again" color={colors.coral} icon="x" />
        </Animated.View>
      </Animated.View>
    </GestureDetector>
  );
}

function Stamp({ label, color, icon }: { label: string; color: string; icon: 'check' | 'x' }) {
  return (
    <View style={[styles.stamp, { borderColor: color }]}>
      <Icon name={icon} size={28} color={color} strokeWidth={3.2} />
      <Text style={[styles.stampText, { color }]}>{label}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    position: 'absolute',
    borderRadius: radius.lg,
    backgroundColor: colors.white,
    ...shadow,
  },
  overlay: {
    ...StyleSheet.absoluteFill,
    borderRadius: radius.lg,
    borderWidth: 4,
    alignItems: 'center',
    paddingTop: 28,
  },
  correctWash: { borderColor: colors.primary, backgroundColor: 'rgba(36,143,145,0.10)' },
  incorrectWash: { borderColor: colors.coral, backgroundColor: 'rgba(255,95,87,0.10)' },
  stamp: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    backgroundColor: colors.white,
    borderWidth: 3,
    borderRadius: radius.pill,
    paddingHorizontal: 18,
    paddingVertical: 8,
  },
  stampText: { fontFamily: fonts.display, fontSize: 22 },
});
