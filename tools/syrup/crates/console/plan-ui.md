# SYRUP Console UI Plan

## Goal

Build a quiet Ratatui console for debugging Waffyr through the adapter layer. The interface should make two things effortless:

1. Read the session timeline.
2. Type and complete debugger commands.

The selected item has one concise `RESULT` view. It shows only that command or event's output/effect; it never becomes a permanent dashboard for the current VM state.

## V1 scope

Include only:

- a compact connection/status header;
- a scrollable session timeline;
- a selection-driven result view;
- a command prompt;
- command autocomplete and one usage hint;
- a minimal, context-sensitive key footer.

Exclude watches, persistent locals, call-stack panels, memory views, tabs, graphs, mouse controls, configuration screens, and resizable panes. These can be added later without changing the basic layout.

## Visual hierarchy

The interface has four levels, in this order:

1. **Command prompt** — the active working surface.
2. **Selected timeline row and result** — the immediate response to the user's action.
3. **Remaining timeline** — session history and orientation.
4. **Header and footer** — quiet status and discoverability.

Use one lavender accent for the product name, active selection, completion, and active key names. Everything else uses normal text or muted gray. Avoid panel boxes, heavy borders, icons, and status badges.

## Wide layout

Use this layout when the terminal is at least `110` columns wide and `24` rows high.

- **Header:** 2 rows including its bottom divider.
- **Main area:** consumes all remaining flexible height.
  - `SESSION`: approximately 53% width.
  - one thin vertical divider.
  - `RESULT`: remaining width.
- **Command area:** variable height above the footer.
  - completion list: 0–3 rows;
  - usage hint: 0–1 row;
  - one blank separator row when space permits;
  - prompt: 1 row;
  - footer: 1 row.

The command area grows only while completions are visible. The main area receives all unused height.

### Header

- Left: `SYRUP   Waffyr · /dev/ttyACM0`
- Right: the state only, for example `PAUSED`, `RUNNING`, or `DISCONNECTED`.
- Do not repeat the selected stop reason or location here.
- Truncate the endpoint before hiding the product or state.

### Session timeline

Render a flat, chronological list. Each row has:

| Column | Preferred width | Content |
|---|---:|---|
| Sequence | 6 | Adapter/session sequence number |
| Direction | 3 | `→` outgoing command, `←` incoming response/event |
| Kind | 22 | Short command or event name |
| Summary | flexible | One concise distinguishing value |

Example:

```text
1042   →   continue-for   count=100
1043   ←   resumed        thread 1
1044   ←   snapshot       pc=0x003ad8
1045   ←   stopped        breakpoint 3
1046   ←   locals         7 values
```

Rules:

- Keep every row to one visual line.
- Use a subtle full-row background for the selected entry; do not add a border.
- Muted rows remain readable but should not compete with the selected row.
- Follow the newest item by default.
- When the user selects older history, stop auto-following until they jump back to the latest entry.
- Preserve the selection when new events arrive.
- Show an unobtrusive `+N newer` indicator only while auto-follow is suspended.

### Result view

The title is always `RESULT`.

This pane is a renderer for the selected timeline item, not a live context inspector:

- An outgoing command shows its associated response and effects.
- An incoming response or event shows only its concise rendered payload.
- An item with no output shows `No output` in muted text.
- Long output wraps and scrolls inside this pane.
- Do not show unrelated state, locals, stack frames, heap statistics, or connection metadata.

For a selected `continue-for count=100`, the result may be:

```text
continued 100 events
stopped at breakpoint 3
function 12 · 0x003ad8
```

The adapter/application layer supplies already-associated `effect` lines. The UI must not guess causality from timestamps. Standard DAP responses use `request_seq`; run-control events and protocol extensions should be associated with the originating command before they reach the renderer.

### Autocomplete

Autocomplete appears directly above the prompt only when useful.

- Filter from the first token as the user types.
- Use case-insensitive prefix matching in V1; do not add a fuzzy-matching dependency.
- Show at most three command names.
- Highlight only the active command name, not the full row.
- Show one muted usage line for the active completion, such as `continue-for <count>`.
- `Up`/`Down` changes the completion; `Tab` inserts it.
- Once the command token is complete, retain the usage hint while arguments are entered.
- Later, argument-specific completions can reuse the same list without changing the layout.

### Prompt

- Keep the prompt permanently visible.
- Prefix input with `›` in lavender.
- Indicate focus with a slim lavender left edge; avoid a full box.
- Support insertion, deletion, Home/End, and left/right movement.
- `Enter` submits a valid command.
- Invalid input stays in place and shows a single-line error where the usage hint normally appears.
- `Esc` closes completion first; a second `Esc` clears the input.
- Preserve submitted commands in a small in-memory history navigated with `Up`/`Down` when completion is closed.

### Footer

The footer is contextual and contains only currently valid actions:

```text
↑↓ select    tab complete    enter send    esc cancel
```

Key names use the accent; descriptions use muted gray. Hide the footer before sacrificing prompt, result, or timeline space.

## Responsive layouts

### Medium: 80–109 columns

Use a single-column main area:

1. `SESSION` timeline fills the flexible upper region.
2. `RESULT` becomes a short strip below it, sized to 3–6 content rows.
3. Autocomplete, usage hint, prompt, and footer remain at the bottom.

Timeline columns retain sequence, direction, and kind. The summary receives the remaining width and truncates with `…`. The result strip may scroll if its content exceeds the allocated height.

### Narrow: 50–79 columns

- Header becomes `SYRUP` on the left and VM state on the right; hide endpoint and VM name as space requires.
- Timeline shows `sequence`, `direction`, and `kind` first.
- Omit the summary column if fewer than 12 columns remain for it.
- Stack `RESULT` beneath the timeline with 2–4 content rows.
- Completion shows command names only; keep the usage hint if one row is available.
- Footer reduces to `tab complete   enter send   esc` or disappears.
- Truncate by display width, never byte length.

### Very small or short terminals

Support a practical minimum of `50×14`. Below that, render a centered message: `Terminal too small — minimum 50×14`.

When height is constrained, allocate rows in this priority order:

1. prompt;
2. one completion and/or error row when active;
3. at least two result rows;
4. at least three timeline rows;
5. header;
6. footer;
7. additional completions and timeline history.

This prevents the command line from becoming unusable merely because the terminal is short.

## Focus and navigation

Keep the interaction model small:

- Printable input focuses the prompt and inserts the character.
- `Shift+Up` / `Shift+Down` changes timeline selection without leaving the prompt.
- `PageUp` / `PageDown` scrolls the timeline by a viewport.
- `End` while the input is empty returns the timeline to the newest entry and resumes following.
- `Up` / `Down` selects autocomplete while it is open; otherwise it navigates command history.
- `Tab` accepts a completion.
- `Enter` sends the command.
- `Esc` dismisses the most local transient state.
- `Ctrl+C` exits cleanly.

The selected timeline row and result are always synchronized. There is no independent result focus in V1; scrolling long result output can be added with `Alt+Up` / `Alt+Down` if real sessions require it.

## UI state

Keep UI state independent from protocol details:

```rust
struct App {
    connection: ConnectionLabel,
    vm_state: VmState,
    timeline: Vec<TimelineEntry>,
    selected: usize,
    follow_latest: bool,
    prompt: PromptState,
    completions: Vec<Completion>,
    completion_index: usize,
    notice: Option<Notice>,
}

struct TimelineEntry {
    sequence: u64,
    direction: Direction,
    kind: String,
    summary: String,
    effect: Vec<String>,
}
```

`effect` is the only data rendered by `RESULT`. Rich typed protocol values should be converted to concise display lines outside the drawing code.

## Ratatui structure

Keep rendering as small pure functions:

```text
ui::draw
├── layout::calculate(area) -> ResponsiveLayout
├── header::render(frame, app, rect)
├── timeline::render(frame, app, rect)
├── result::render(frame, app, rect)
├── completion::render(frame, app, rect)
├── prompt::render(frame, app, rect)
└── footer::render(frame, app, rect)
```

Prefer Ratatui's `Paragraph`, `List`, and `Layout`. A `List` is enough for the timeline; manually compose each `Line` so column collapse is explicit and predictable. Avoid a component framework.

## Dependencies

Use only runtime dependencies already justified by the console:

- `ratatui` for layout and rendering;
- `crossterm` for terminal input and lifecycle;
- the internal adapter crate for debugger communication.

Do not add an input widget, fuzzy matcher, async runtime, or styling crate for V1. Store prompt text and cursor position directly. Use `std::sync::mpsc` for adapter events if a channel is needed, and drain it from the terminal event loop.

## Implementation sequence

1. **Define display state** — add `App`, `TimelineEntry`, `PromptState`, completion metadata, and sample session data.
2. **Implement responsive layout calculation** — wide two-pane, medium/narrow stacked, minimum-size fallback.
3. **Render the static mock** — reproduce the screenshot's hierarchy, spacing, colors, truncation, and selected row.
4. **Add timeline behavior** — selection, scrolling, follow-latest, and result synchronization.
5. **Add the prompt editor** — cursor-safe insertion/deletion, command history, submit, clear, and inline errors.
6. **Add autocomplete** — prefix filtering, three-result limit, completion insertion, and usage hint.
7. **Connect adapter events** — append normalized timeline items and consume pre-associated effect lines.
8. **Polish terminal lifecycle** — restore terminal on errors/panic, handle resize, disconnect, and `Ctrl+C`.

## Verification

Use Ratatui's `TestBackend` with fixed terminal sizes:

- `140×40`: two-pane layout matching the mock;
- `100×30`: stacked timeline and result;
- `72×24`: collapsed columns and compact footer;
- `50×14`: minimum usable layout;
- `49×13`: minimum-size message.

Test layout rectangles, visible labels, selection preservation, truncation, completion filtering, command editing, and row-priority behavior. Avoid a snapshot-test dependency initially; assert against the rendered buffer and layout results directly.

## V1 acceptance criteria

- The prompt is always visible at every supported size.
- No pane displays global debugger context unless the selected item itself produced it.
- Selecting a timeline item immediately changes `RESULT` to that item's effect.
- Autocomplete shows no more than three filtered commands and one usage line.
- Resizing never overlaps, clips across pane boundaries, or makes the prompt inaccessible.
- Wide, medium, and narrow layouts are covered by deterministic backend tests.
- The console adds no runtime dependency beyond Ratatui, Crossterm, and the internal adapter crate.
