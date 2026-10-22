import { useEffect, useRef, useState } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import Animated, {
  cancelAnimation,
  useAnimatedStyle,
  useSharedValue,
  withRepeat,
  withTiming,
} from 'react-native-reanimated';

import VowelTracker from '../../modules/vowel-tracker';
import type { LiveReading, Recording } from '@/recording/use-recorder';
import type { TrackingStatus } from '@/recording/use-tracker-session';
import { colors, fonts, radius } from '@/theme';
import { Icon } from './Icon';

const SIZE = 76;

/** Press and hold to record; release to stop. */
export function MicButton(props: {
  recording: boolean;
  disabled: boolean;
  onPressIn: () => void;
  onPressOut: () => void;
}) {
  const pulse = useSharedValue(0);

  useEffect(() => {
    if (props.recording) {
      pulse.value = withRepeat(withTiming(1, { duration: 900 }), -1, false);
    } else {
      cancelAnimation(pulse);
      pulse.value = 0;
    }
  }, [props.recording, pulse]);

  const ring = useAnimatedStyle(() => ({
    opacity: 0.5 * (1 - pulse.value),
    transform: [{ scale: 1 + pulse.value * 0.45 }],
  }));

  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel="Hold to record"
      accessibilityState={{ disabled: props.disabled }}
      disabled={props.disabled}
      onPressIn={props.onPressIn}
      onPressOut={props.onPressOut}
      unstable_pressDelay={0}
      style={[styles.mic, props.disabled && styles.disabled]}
    >
      <View style={styles.micCircleWrap}>
        <Animated.View style={[styles.ring, ring]} />
        <View style={[styles.micCircle, props.recording && styles.micCircleActive]}>
          <Icon name="mic" size={34} color={props.recording ? colors.white : colors.navy} strokeWidth={2.6} />
        </View>
      </View>
      <Text style={[styles.micLabel, props.recording && { color: colors.navy }]}>
        {props.recording ? 'Recording' : 'Hold to talk'}
      </Text>
    </Pressable>
  );
}

/** Line above the buttons: live recording time, the card's take with a play button, or a hint. */
export function RecordStatus(props: {
  status: TrackingStatus;
  recording: boolean;
  live: LiveReading;
  take: Recording | undefined;
}) {
  const [playing, setPlaying] = useState(false);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => () => {
    if (timer.current) clearTimeout(timer.current);
  }, []);

  if (props.status === 'permission-denied') {
    return <Text style={styles.hint}>Turn on microphone access in Settings to record.</Text>;
  }
  if (props.status === 'error') {
    return <Text style={styles.hint}>No microphone found. Tap the mic to try again.</Text>;
  }

  if (props.recording) {
    return (
      <View style={styles.row}>
        <View style={[styles.voiceDot, props.live.voiced && styles.voiceDotOn]} />
        <Text style={styles.recordingText}>Listening… {props.live.seconds.toFixed(1)}s</Text>
      </View>
    );
  }

  if (props.take) {
    const take = props.take;
    const play = () => {
      VowelTracker.playSound(take.audioUri).catch((error) => console.warn('Could not play recording', error));
      setPlaying(true);
      if (timer.current) clearTimeout(timer.current);
      timer.current = setTimeout(() => setPlaying(false), take.duration * 1000);
    };
    return (
      <View style={styles.row}>
        <Pressable
          accessibilityRole="button"
          accessibilityLabel="Play recording"
          onPress={play}
          style={({ pressed }) => [styles.playPill, pressed && { opacity: 0.8 }]}
        >
          <Icon name="play" size={16} color={colors.white} filled strokeWidth={1.5} />
          <Text style={styles.playText}>{playing ? 'Playing…' : 'Play'}</Text>
          <Text style={styles.playDuration}>{take.duration.toFixed(1)}s</Text>
        </Pressable>
        {take.voicedFrames === 0 && <Text style={styles.warning}>No voice heard</Text>}
      </View>
    );
  }

  return <Text style={styles.hint}>Swipe right for correct · left to try again</Text>;
}

const styles = StyleSheet.create({
  mic: { alignItems: 'center', gap: 6 },
  disabled: { opacity: 0.4 },
  micCircleWrap: { width: SIZE, height: SIZE, alignItems: 'center', justifyContent: 'center' },
  ring: {
    position: 'absolute',
    width: SIZE,
    height: SIZE,
    borderRadius: SIZE / 2,
    backgroundColor: colors.sky,
  },
  micCircle: {
    width: SIZE,
    height: SIZE,
    borderRadius: SIZE / 2,
    backgroundColor: colors.sky,
    alignItems: 'center',
    justifyContent: 'center',
    shadowColor: colors.navy,
    shadowOpacity: 0.15,
    shadowRadius: 10,
    shadowOffset: { width: 0, height: 4 },
    elevation: 4,
  },
  micCircleActive: { backgroundColor: colors.navy, transform: [{ scale: 1.06 }] },
  micLabel: { fontFamily: fonts.bodyBold, fontSize: 15, color: colors.navyMuted },
  row: { flexDirection: 'row', alignItems: 'center', justifyContent: 'center', gap: 10, minHeight: 34 },
  hint: {
    textAlign: 'center',
    fontFamily: fonts.body,
    fontSize: 13,
    color: colors.navyMuted,
    minHeight: 34,
    lineHeight: 34,
  },
  voiceDot: { width: 10, height: 10, borderRadius: 5, backgroundColor: colors.border },
  voiceDotOn: { backgroundColor: colors.primary },
  recordingText: { fontFamily: fonts.bodyBold, fontSize: 15, color: colors.navy },
  playPill: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    backgroundColor: colors.navy,
    borderRadius: radius.pill,
    paddingHorizontal: 16,
    height: 34,
  },
  playText: { fontFamily: fonts.bodyBold, fontSize: 15, color: colors.white },
  playDuration: { fontFamily: fonts.body, fontSize: 13, color: colors.sky },
  warning: { fontFamily: fonts.bodyBold, fontSize: 13, color: colors.coral },
});
