import Svg, { Path } from 'react-native-svg';

const paths = {
  check: 'M5 12.5l4.5 4.5L19 7.5',
  x: 'M6.5 6.5l11 11M17.5 6.5l-11 11',
  back: 'M15 5l-7 7 7 7',
  undo: 'M9 14L4 9l5-5M4 9h10.5a5.5 5.5 0 010 11H11',
  repeat: 'M17 2l4 4-4 4M3 11V10a4 4 0 014-4h14M7 22l-4-4 4-4M21 13v1a4 4 0 01-4 4H3',
  home: 'M3 10.5L12 3l9 7.5M5 9v11h5v-6h4v6h5V9',
  star: 'M12 2.8l2.85 5.78 6.38.93-4.62 4.5 1.09 6.36L12 17.37l-5.7 3 1.09-6.36-4.62-4.5 6.38-.93z',
} as const;

export type IconName = keyof typeof paths;

type Props = { name: IconName; size?: number; color: string; strokeWidth?: number; filled?: boolean };

export function Icon({ name, size = 24, color, strokeWidth = 2.6, filled = false }: Props) {
  return (
    <Svg width={size} height={size} viewBox="0 0 24 24" fill="none">
      <Path
        d={paths[name]}
        stroke={color}
        strokeWidth={strokeWidth}
        strokeLinecap="round"
        strokeLinejoin="round"
        fill={filled ? color : 'none'}
      />
    </Svg>
  );
}
