export const colors = {
  primary: '#248F91', // Teal
  sky: '#74C9E5', // Main accent
  coral: '#FF5F57', // Energy accent
  gold: '#FFD77A', // Reward accent
  cream: '#FFF9EF', // Background
  navy: '#253B50', // Text

  white: '#FFFFFF',
  primarySoft: '#DDF0EF',
  skySoft: '#E3F4FA',
  coralSoft: '#FFE5E3',
  goldSoft: '#FFF3D6',
  navyMuted: '#5E7184',
  border: '#EFE6D6',
} as const;

/** Each sound tile gets an accent, rotating through the palette. */
export const accents = [
  { solid: colors.primary, soft: colors.primarySoft, onSolid: colors.white },
  { solid: colors.sky, soft: colors.skySoft, onSolid: colors.navy },
  { solid: colors.coral, soft: colors.coralSoft, onSolid: colors.white },
  { solid: colors.gold, soft: colors.goldSoft, onSolid: colors.navy },
] as const;

export const fonts = {
  display: 'Baloo2_700Bold',
  displayHeavy: 'Baloo2_800ExtraBold',
  body: 'Nunito_600SemiBold',
  bodyBold: 'Nunito_800ExtraBold',
} as const;

export const radius = { sm: 12, md: 20, lg: 28, pill: 999 } as const;

export const shadow = {
  shadowColor: '#253B50',
  shadowOpacity: 0.08,
  shadowRadius: 16,
  shadowOffset: { width: 0, height: 6 },
  elevation: 3,
} as const;
