import { Pressable, StyleSheet, Text, View, type StyleProp, type ViewStyle } from 'react-native';

import { colors, fonts, radius } from '@/theme';
import { Icon, type IconName } from './Icon';

type Variant = 'primary' | 'secondary' | 'ghost';

type Props = {
  label: string;
  onPress: () => void;
  icon?: IconName;
  variant?: Variant;
  style?: StyleProp<ViewStyle>;
};

const variants = {
  primary: { bg: colors.primary, fg: colors.white, border: colors.primary },
  secondary: { bg: colors.white, fg: colors.navy, border: colors.border },
  ghost: { bg: 'transparent', fg: colors.primary, border: 'transparent' },
} as const;

export function Button({ label, onPress, icon, variant = 'primary', style }: Props) {
  const v = variants[variant];
  return (
    <Pressable
      accessibilityRole="button"
      onPress={onPress}
      style={({ pressed }) => [
        styles.base,
        { backgroundColor: v.bg, borderColor: v.border },
        pressed && styles.pressed,
        style,
      ]}
    >
      <View style={styles.row}>
        {icon && <Icon name={icon} size={20} color={v.fg} />}
        <Text style={[styles.label, { color: v.fg }]}>{label}</Text>
      </View>
    </Pressable>
  );
}

/** Round icon button used in top bars. */
export function IconButton({ icon, onPress, label }: { icon: IconName; onPress: () => void; label: string }) {
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel={label}
      onPress={onPress}
      hitSlop={8}
      style={({ pressed }) => [styles.iconButton, pressed && styles.pressed]}
    >
      <Icon name={icon} size={22} color={colors.navy} />
    </Pressable>
  );
}

const styles = StyleSheet.create({
  base: {
    minHeight: 56,
    borderRadius: radius.pill,
    borderWidth: 2,
    paddingHorizontal: 24,
    alignItems: 'center',
    justifyContent: 'center',
  },
  row: { flexDirection: 'row', alignItems: 'center', gap: 10 },
  label: { fontFamily: fonts.bodyBold, fontSize: 17 },
  pressed: { opacity: 0.85, transform: [{ scale: 0.97 }] },
  iconButton: {
    width: 44,
    height: 44,
    borderRadius: 22,
    backgroundColor: colors.white,
    borderWidth: 2,
    borderColor: colors.border,
    alignItems: 'center',
    justifyContent: 'center',
  },
});
