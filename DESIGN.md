# Design System

## Theme & Palette
- **Background Root**: `#0a0e17` (Deep slate neutral)
- **Card Background**: `#131a26` (Layer 1 surface)
- **Sub-surface / Inputs**: `#0b1019` (Layer 2 surface with `#232f42` border)
- **Primary Action (Emerald/Green)**: `#238636` / Hover: `#2ea043`
- **Accent (Cyan/Blue)**: `#388bfd` / Hover: `#58a6ff` / Soft: `rgba(56, 139, 253, 0.12)`
- **Danger (Coral/Red)**: `#da3633` / Hover: `#f85149` / Soft: `rgba(248, 81, 73, 0.14)`
- **Warning (Amber/Gold)**: `#d29922` / Soft: `rgba(210, 153, 34, 0.15)`
- **Text Ink**: `#f0f6fc` (Primary 100%), `#8b949e` (Dim/Muted 60%)

## Typography
- **Stack**: `-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif`
- **Monospace Stack**: `ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace`
- **Numeric Display**: `font-feature-settings: "tnum" 1, "zero" 1;` (Tabular fixed-width numbers)

## Components & Layout
- **Container**: Max width 1200px, responsive 1-column mobile $\rightarrow$ 2-column desktop grid.
- **Dials & Gauges**: High-contrast SVG vector dials with target cursor and real-time needle.
- **Buttons**: Consistent 8px radius, crisp 1px borders, subtle active scale `0.98`, clear focus rings.
- **Inputs & Sliders**: Custom-styled range tracks with high-visibility thumbs and direct numeric inputs.
