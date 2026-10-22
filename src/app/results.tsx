import { Image } from 'expo-image';
import { Redirect, useRouter } from 'expo-router';
import { ActivityIndicator, Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { Button, IconButton } from '@/components/Button';
import { CARD_ASPECT } from '@/components/Flashcard';
import { Icon } from '@/components/Icon';
import { Screen, TopBar } from '@/components/Screen';
import { getSound, getWord, positionLabel, type WordEntry } from '@/data';
import { useSession } from '@/state/session';
import { useSpeaker } from '@/state/speaker';
import { retryFailed, useUploads } from '@/vowelsy/uploads';
import { colors, fonts, radius, shadow } from '@/theme';

function cheer(pct: number) {
  if (pct === 100) return { stars: 3, message: 'Perfect score!' };
  if (pct >= 80) return { stars: 3, message: 'Amazing work!' };
  if (pct >= 50) return { stars: 2, message: 'Great job!' };
  return { stars: 1, message: 'Nice effort — keep going!' };
}

export default function Results() {
  const router = useRouter();
  const insets = useSafeAreaInsets();
  const { lastSession } = useSession();
  const uploads = useUploads(lastSession?.roundId);
  const speaker = useSpeaker();
  if (!lastSession) return <Redirect href="/" />;

  const { sound: soundId, position, isRetry, results, recordings, sessionId, roundId } = lastSession;
  const recordedCount = results.filter((r) => recordings[r.id]).length;
  const sound = getSound(soundId);
  const lookup = (ids: string[]) => ids.map(getWord).filter((w): w is WordEntry => !!w);
  const correct = lookup(results.filter((r) => r.correct).map((r) => r.id));
  const missed = lookup(results.filter((r) => !r.correct).map((r) => r.id));
  const pct = Math.round((correct.length / results.length) * 100);
  const { stars, message } = cheer(pct);

  const practice = (words: WordEntry[]) =>
    router.replace({
      pathname: '/practice',
      params: { sound: soundId, position, shuffle: '1', ids: words.map((w) => w.id).join(','), session: sessionId },
    });

  return (
    <Screen>
      <TopBar
        left={<IconButton icon="home" label="Home" onPress={() => router.dismissTo('/')} />}
        title={`${sound?.label ?? ''} · ${isRetry ? 'Missed words' : positionLabel(position)}`}
      />
      <ScrollView contentContainerStyle={styles.content}>
        <View style={styles.hero}>
          <View style={styles.stars}>
            {[1, 2, 3].map((n) => (
              <View key={n} style={n === 2 && styles.middleStar}>
                <Icon
                  name="star"
                  size={n === 2 ? 56 : 44}
                  color={n <= stars ? colors.gold : colors.border}
                  filled
                  strokeWidth={1.5}
                />
              </View>
            ))}
          </View>
          <Text style={styles.message}>{message}</Text>
          <Text style={styles.score}>
            {correct.length}
            <Text style={styles.scoreOf}> / {results.length}</Text>
          </Text>
          <View style={styles.pctPill}>
            <Text style={styles.pctText}>{pct}% correct</Text>
          </View>
        </View>

        {recordedCount > 0 && (
          <UploadCard
            total={recordedCount}
            done={uploads.filter((u) => u.status === 'done').length}
            failed={uploads.filter((u) => u.status === 'failed')}
            noSpeaker={!speaker && uploads.length === 0}
            onRetry={() => retryFailed(roundId)}
          />
        )}

        {missed.length > 0 && (
          <WordGroup title="Needs practice" icon="x" color={colors.coral} soft={colors.coralSoft} words={missed} />
        )}
        {correct.length > 0 && (
          <WordGroup title="Said correctly" icon="check" color={colors.primary} soft={colors.primarySoft} words={correct} />
        )}
      </ScrollView>

      <View style={[styles.actions, { paddingBottom: Math.max(insets.bottom, 16) }]}>
        {missed.length > 0 ? (
          <Button label={`Practice missed words (${missed.length})`} icon="repeat" onPress={() => practice(missed)} />
        ) : null}
        <View style={styles.actionRow}>
          <Button
            label="Practice again"
            variant="secondary"
            onPress={() => practice([...correct, ...missed])}
            style={{ flex: 1 }}
          />
          <Button
            label="New sound"
            variant="secondary"
            onPress={() => router.dismissTo('/')}
            style={{ flex: 1 }}
          />
        </View>
      </View>
    </Screen>
  );
}

function UploadCard(props: {
  total: number;
  done: number;
  failed: { error?: string }[];
  noSpeaker: boolean;
  onRetry: () => void;
}) {
  const { total, done, failed } = props;
  const busy = !props.noSpeaker && done + failed.length < total;
  let title: string;
  let detail: string | undefined;
  if (props.noSpeaker) {
    title = `${total} recording${total === 1 ? '' : 's'} not sent`;
    detail = 'Add the client’s sex and age to send recordings to Vowelsy.';
  } else if (busy) {
    title = `Sending ${Math.min(done + failed.length + 1, total)} of ${total} recordings…`;
  } else if (failed.length) {
    title = `${failed.length} of ${total} recordings didn’t send`;
    detail = failed[0].error;
  } else {
    title = `${total} recording${total === 1 ? '' : 's'} sent to Vowelsy`;
  }

  return (
    <View style={styles.upload}>
      <View style={[styles.uploadIcon, failed.length || props.noSpeaker ? styles.uploadIconWarn : null]}>
        {busy ? (
          <ActivityIndicator color={colors.primary} />
        ) : (
          <Icon
            name={failed.length || props.noSpeaker ? 'upload' : 'check'}
            size={20}
            color={failed.length || props.noSpeaker ? colors.coral : colors.primary}
            strokeWidth={2.8}
          />
        )}
      </View>
      <View style={{ flex: 1 }}>
        <Text style={styles.uploadTitle}>{title}</Text>
        {detail ? (
          <Text style={styles.uploadDetail} numberOfLines={2}>
            {detail}
          </Text>
        ) : null}
      </View>
      {!busy && failed.length > 0 && (
        <Pressable accessibilityRole="button" onPress={props.onRetry} style={styles.retry} hitSlop={6}>
          <Text style={styles.retryText}>Retry</Text>
        </Pressable>
      )}
    </View>
  );
}

function WordGroup(props: { title: string; icon: 'check' | 'x'; color: string; soft: string; words: WordEntry[] }) {
  return (
    <View style={styles.group}>
      <View style={styles.groupHeader}>
        <View style={[styles.groupIcon, { backgroundColor: props.soft }]}>
          <Icon name={props.icon} size={16} color={props.color} strokeWidth={3} />
        </View>
        <Text style={styles.groupTitle}>{props.title}</Text>
        <Text style={[styles.groupCount, { color: props.color }]}>{props.words.length}</Text>
      </View>
      <View style={styles.wordGrid}>
        {props.words.map((w) => (
          <View key={w.id} style={[styles.wordChip, { borderColor: props.soft }]}>
            {w.image ? (
              <Image source={w.image} style={styles.thumb} contentFit="cover" />
            ) : (
              <View style={[styles.thumb, styles.thumbBlank, { backgroundColor: props.soft }]} />
            )}
            <Text style={styles.wordText} numberOfLines={1}>
              {w.word}
            </Text>
          </View>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  content: { paddingHorizontal: 20, paddingBottom: 24, width: '100%', maxWidth: 700, alignSelf: 'center' },
  hero: {
    alignItems: 'center',
    backgroundColor: colors.white,
    borderRadius: radius.lg,
    paddingVertical: 24,
    marginTop: 8,
    ...shadow,
  },
  stars: { flexDirection: 'row', alignItems: 'flex-end', gap: 6 },
  middleStar: { marginBottom: 10 },
  message: { fontFamily: fonts.display, fontSize: 24, color: colors.navy, marginTop: 8 },
  score: { fontFamily: fonts.displayHeavy, fontSize: 64, color: colors.primary, lineHeight: 74 },
  scoreOf: { fontSize: 32, color: colors.navyMuted },
  pctPill: { backgroundColor: colors.goldSoft, borderRadius: radius.pill, paddingHorizontal: 14, paddingVertical: 5 },
  pctText: { fontFamily: fonts.bodyBold, fontSize: 14, color: colors.navy },
  upload: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 14,
    backgroundColor: colors.white,
    borderRadius: radius.md,
    padding: 16,
    marginTop: 16,
    ...shadow,
  },
  uploadIcon: {
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: colors.primarySoft,
    alignItems: 'center',
    justifyContent: 'center',
  },
  uploadIconWarn: { backgroundColor: colors.coralSoft },
  uploadTitle: { fontFamily: fonts.bodyBold, fontSize: 16, color: colors.navy },
  uploadDetail: { fontFamily: fonts.body, fontSize: 13, color: colors.navyMuted, marginTop: 2 },
  retry: { backgroundColor: colors.coral, borderRadius: radius.pill, paddingHorizontal: 16, paddingVertical: 8 },
  retryText: { fontFamily: fonts.bodyBold, fontSize: 14, color: colors.white },
  group: { marginTop: 24 },
  groupHeader: { flexDirection: 'row', alignItems: 'center', gap: 10, marginBottom: 12 },
  groupIcon: { width: 28, height: 28, borderRadius: 14, alignItems: 'center', justifyContent: 'center' },
  groupTitle: { flex: 1, fontFamily: fonts.display, fontSize: 20, color: colors.navy },
  groupCount: { fontFamily: fonts.bodyBold, fontSize: 18 },
  wordGrid: { flexDirection: 'row', flexWrap: 'wrap', gap: 10 },
  wordChip: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 10,
    backgroundColor: colors.white,
    borderWidth: 2,
    borderRadius: radius.sm,
    paddingVertical: 6,
    paddingLeft: 6,
    paddingRight: 14,
  },
  thumb: { width: 36, height: 36 / CARD_ASPECT, borderRadius: 6 },
  thumbBlank: { opacity: 0.6 },
  wordText: { fontFamily: fonts.bodyBold, fontSize: 16, color: colors.navy, maxWidth: 140 },
  actions: {
    paddingHorizontal: 20,
    paddingTop: 12,
    gap: 10,
    borderTopWidth: 1,
    borderTopColor: colors.border,
    backgroundColor: colors.cream,
    width: '100%',
    maxWidth: 700,
    alignSelf: 'center',
  },
  actionRow: { flexDirection: 'row', gap: 10 },
});
