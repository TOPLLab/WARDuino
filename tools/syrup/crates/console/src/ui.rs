use ratatui::{
    Frame,
    layout::{Alignment, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, BorderType, Borders, Clear, Paragraph, Wrap},
};

use crate::app::{App, TimelineEntry};

const BASE: Color = Color::Rgb(29, 29, 43);
const SURFACE: Color = Color::Rgb(41, 41, 58);
const SURFACE_ACTIVE: Color = Color::Rgb(58, 54, 80);
const TEXT: Color = Color::Rgb(215, 213, 231);
const ACCENT: Color = Color::Rgb(183, 162, 255);
const MUTED: Color = Color::Rgb(143, 139, 168);

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum LayoutMode {
    Wide,
    Stacked,
    Narrow,
    TooSmall,
}

#[derive(Clone, Copy, Debug)]
pub struct ResponsiveLayout {
    pub mode: LayoutMode,
    pub timeline: Rect,
    pub divider: Option<Rect>,
    pub result: Rect,
    pub prompt: Rect,
    pub footer: Option<Rect>,
}

pub fn calculate(area: Rect, _completion_count: usize, _has_feedback: bool) -> ResponsiveLayout {
    let empty = Rect::default();
    if area.width < 50 || area.height < 14 {
        return ResponsiveLayout {
            mode: LayoutMode::TooSmall,
            timeline: empty,
            divider: None,
            result: empty,
            prompt: empty,
            footer: None,
        };
    }

    let mode = if area.width >= 110 && area.height >= 24 {
        LayoutMode::Wide
    } else if area.width >= 80 {
        LayoutMode::Stacked
    } else {
        LayoutMode::Narrow
    };
    let below_header = area.height;
    let footer_height = u16::from(below_header >= 8);
    let command_height = 2 + footer_height;
    let main_height = below_header.saturating_sub(command_height + 2).max(5);
    let main_y = area.y + 1;

    let (timeline, divider, result) = if mode == LayoutMode::Wide {
        let timeline_width = ((area.width as u32 * 53) / 100) as u16;
        (
            Rect::new(area.x, main_y, timeline_width, main_height),
            Some(Rect::new(area.x + timeline_width, main_y, 1, main_height)),
            Rect::new(
                area.x + timeline_width + 1,
                main_y,
                area.width - timeline_width - 1,
                main_height,
            ),
        )
    } else {
        let result_height = match mode {
            LayoutMode::Stacked => (main_height / 3).clamp(3, 6),
            LayoutMode::Narrow => (main_height / 3).clamp(2, 4),
            _ => unreachable!(),
        };
        (
            Rect::new(area.x, main_y, area.width, main_height - result_height),
            None,
            Rect::new(
                area.x,
                main_y + main_height - result_height,
                area.width,
                result_height,
            ),
        )
    };
    let prompt_y = area.y + area.height - command_height;
    let prompt = Rect::new(area.x, prompt_y, area.width, 2);
    let footer = (footer_height == 1).then(|| Rect::new(area.x, prompt_y + 2, area.width, 1));
    ResponsiveLayout {
        mode,
        timeline,
        divider,
        result,
        prompt,
        footer,
    }
}

pub fn draw(frame: &mut Frame, app: &App) {
    let area = frame.area();
    let layout = calculate(area, app.completions.len(), app.feedback().is_some());
    if layout.mode == LayoutMode::TooSmall {
        render_too_small(frame, area);
        return;
    }
    frame.render_widget(Paragraph::new("").style(Style::default().bg(BASE)), area);
    if let Some(divider) = layout.divider {
        frame.render_widget(
            Paragraph::new("│")
                .style(Style::default().fg(MUTED))
                .alignment(Alignment::Center),
            divider,
        );
    }
    render_timeline(frame, app, layout.timeline, layout.mode);
    render_result(frame, app, layout.result);
    render_metadata(
        frame,
        app,
        Rect::new(
            area.x
                + if layout.mode == LayoutMode::Wide {
                    2
                } else {
                    1
                },
            layout.prompt.y.saturating_sub(1),
            area.width
                .saturating_sub(if layout.mode == LayoutMode::Wide {
                    4
                } else {
                    2
                }),
            1,
        ),
        layout.mode,
    );
    render_prompt(frame, app, layout.prompt);
    if let Some(footer) = layout.footer {
        render_footer(frame, app, footer, layout.mode);
    }
    if app.help_visible {
        render_help(frame, area);
    }
}

fn render_metadata(frame: &mut Frame, app: &App, area: Rect, mode: LayoutMode) {
    let state = app.vm_state.label().to_ascii_lowercase();
    let color = match app.vm_state {
        crate::app::VmState::Paused => Color::Rgb(228, 184, 106),
        crate::app::VmState::Disconnected => Color::Rgb(231, 130, 132),
        _ => Color::Rgb(119, 199, 160),
    };
    let left = if mode == LayoutMode::Narrow {
        app.vm_name.clone()
    } else {
        format!("{} · {}", app.vm_name, app.connection)
    };
    let right = if mode == LayoutMode::Narrow {
        state
    } else {
        format!("connected · {state}")
    };
    let left_width = area.width.saturating_sub(right.chars().count() as u16 + 1);
    frame.render_widget(
        Paragraph::new(trim_text(&left, left_width as usize)).style(Style::default().fg(MUTED)),
        Rect::new(area.x, area.y, left_width, 1),
    );
    frame.render_widget(
        Paragraph::new(right)
            .style(Style::default().fg(color))
            .alignment(Alignment::Right),
        area,
    );
}

fn render_timeline(frame: &mut Frame, app: &App, area: Rect, mode: LayoutMode) {
    if area.height == 0 {
        return;
    }
    frame.render_widget(
        Paragraph::new(Span::styled(
            " Session ",
            Style::default()
                .fg(TEXT)
                .bg(SURFACE_ACTIVE)
                .add_modifier(Modifier::BOLD),
        )),
        Rect::new(area.x, area.y, area.width, 1),
    );
    let rows = area.height.saturating_sub(1) as usize;
    if rows == 0 {
        return;
    }
    let start = app.selected.saturating_add(1).saturating_sub(rows);
    for (row, (index, entry)) in app
        .timeline
        .iter()
        .enumerate()
        .skip(start)
        .take(rows)
        .enumerate()
    {
        let y = area.y + 1 + row as u16;
        let selected = index == app.selected;
        let style = if selected {
            Style::default().fg(TEXT).bg(SURFACE_ACTIVE)
        } else {
            Style::default().fg(TEXT)
        };
        frame.render_widget(
            Paragraph::new(timeline_line(entry, area.width, mode)).style(style),
            Rect::new(area.x, y, area.width, 1),
        );
    }
    if app.newer_count() > 0 && area.width >= 12 {
        let message = format!("+{} newer", app.newer_count());
        let x = area.x + area.width - message.len() as u16;
        frame.render_widget(
            Paragraph::new(message).style(Style::default().fg(ACCENT)),
            Rect::new(x, area.y, area.width - (x - area.x), 1),
        );
    }
}

fn timeline_line(entry: &TimelineEntry, width: u16, mode: LayoutMode) -> Line<'static> {
    let sequence = format!("{:>6}", entry.sequence);
    let prefix = format!("{} {} ", sequence, entry.direction.symbol());
    let kind_width = if mode == LayoutMode::Narrow { 18 } else { 22 };
    let used = prefix.chars().count() as u16 + kind_width + 1;
    let summary_width = width.saturating_sub(used);
    let kind = trim_text(&entry.kind, kind_width as usize);
    let mut spans = vec![
        Span::styled(sequence, Style::default().fg(MUTED)),
        Span::raw(" "),
        Span::styled(entry.direction.symbol(), Style::default().fg(ACCENT)),
        Span::raw(" "),
        Span::raw(pad_text(&kind, kind_width as usize)),
    ];
    if (mode != LayoutMode::Narrow || summary_width >= 12) && summary_width >= 12 {
        spans.push(Span::raw(" "));
        spans.push(Span::styled(
            trim_text(&entry.summary, summary_width as usize),
            Style::default().fg(MUTED),
        ));
    }
    Line::from(spans)
}

fn render_result(frame: &mut Frame, app: &App, area: Rect) {
    if area.height == 0 {
        return;
    }
    frame.render_widget(
        Paragraph::new(Span::styled(
            " Result ",
            Style::default()
                .fg(TEXT)
                .bg(SURFACE_ACTIVE)
                .add_modifier(Modifier::BOLD),
        )),
        Rect::new(area.x, area.y, area.width, 1),
    );
    if area.height < 2 {
        return;
    }
    let body = app
        .selected_entry()
        .map(|entry| entry.effect.clone())
        .unwrap_or_default();
    let text = if body.is_empty() {
        vec![Line::from(Span::styled(
            "No output",
            Style::default().fg(MUTED),
        ))]
    } else {
        body.into_iter().map(Line::from).collect()
    };
    frame.render_widget(
        Paragraph::new(text).wrap(Wrap { trim: false }),
        Rect::new(area.x, area.y + 1, area.width, area.height - 1),
    );
}

fn render_prompt(frame: &mut Frame, app: &App, area: Rect) {
    let surface = Style::default().bg(if app.prompt.is_empty() {
        SURFACE
    } else {
        SURFACE_ACTIVE
    });
    frame.render_widget(Paragraph::new(" ").style(surface), area);
    frame.render_widget(
        Paragraph::new("▌\n▌").style(Style::default().fg(ACCENT)),
        Rect::new(area.x, area.y, 1, area.height),
    );
    let input = trim_text(&app.prompt, area.width.saturating_sub(6) as usize);
    let ghost = if app.cursor == app.prompt.chars().count()
        && !app.prompt.chars().any(char::is_whitespace)
    {
        app.active_completion()
            .and_then(|completion| completion.command.strip_prefix(&app.prompt))
            .unwrap_or_default()
    } else {
        ""
    };
    let line = if input.is_empty() {
        Line::from(vec![
            Span::styled("› ", Style::default().fg(ACCENT)),
            Span::styled("Type a command…", Style::default().fg(MUTED)),
        ])
    } else {
        Line::from(vec![
            Span::styled("› ", Style::default().fg(ACCENT)),
            Span::styled(input, Style::default().fg(TEXT)),
            Span::styled(ghost, Style::default().fg(MUTED)),
        ])
    };
    frame.render_widget(
        Paragraph::new(line),
        Rect::new(area.x + 2, area.y, area.width.saturating_sub(4), 1),
    );
    if let Some(feedback) = app.feedback() {
        frame.render_widget(
            Paragraph::new(trim_text(feedback, area.width.saturating_sub(4) as usize)).style(
                Style::default().fg(if app.notice.is_some() {
                    Color::Rgb(231, 130, 132)
                } else {
                    MUTED
                }),
            ),
            Rect::new(area.x + 2, area.y + 1, area.width.saturating_sub(4), 1),
        );
    }
}

fn render_help(frame: &mut Frame, area: Rect) {
    let width = area.width.saturating_sub(4).min(48);
    let height = 8.min(area.height.saturating_sub(4));
    let popup = Rect::new(
        area.x + area.width.saturating_sub(width) / 2,
        area.y + area.height.saturating_sub(height) / 2,
        width,
        height,
    );
    frame.render_widget(Clear, popup);
    let block = Block::default()
        .title(Span::styled(
            " Help ",
            Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
        ))
        .borders(Borders::ALL)
        .border_type(BorderType::Rounded)
        .border_style(Style::default().fg(ACCENT))
        .style(Style::default().bg(BASE).fg(TEXT));
    let lines = vec![
        Line::from(Span::styled(
            "Supported commands",
            Style::default().fg(TEXT).add_modifier(Modifier::BOLD),
        )),
        Line::from(""),
        Line::from(vec![
            Span::styled("  continue    ", Style::default().fg(ACCENT)),
            Span::styled("continue", Style::default().fg(MUTED)),
        ]),
        Line::from(vec![
            Span::styled("  pause       ", Style::default().fg(ACCENT)),
            Span::styled("pause", Style::default().fg(MUTED)),
        ]),
        Line::from(""),
        Line::from(Span::styled("? / esc  close", Style::default().fg(MUTED))),
    ];
    frame.render_widget(Paragraph::new(lines).block(block), popup);
}

fn render_footer(frame: &mut Frame, app: &App, area: Rect, mode: LayoutMode) {
    let text = match mode {
        LayoutMode::Narrow => Line::from(vec![
            Span::styled(
                "↑↓",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(
                if app.completions.is_empty() {
                    " history  "
                } else {
                    " choice  "
                },
                Style::default().fg(MUTED),
            ),
            Span::styled(
                "enter",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(" send  ", Style::default().fg(MUTED)),
            Span::styled(
                "esc",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(
                "    ?",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(" help", Style::default().fg(MUTED)),
        ]),
        _ => Line::from(vec![
            Span::styled(
                "↑↓",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(
                if app.completions.is_empty() {
                    " history    "
                } else {
                    " choice    "
                },
                Style::default().fg(MUTED),
            ),
            Span::styled(
                "tab",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(
                if app.completions.is_empty() {
                    " complete    "
                } else {
                    " accept    "
                },
                Style::default().fg(MUTED),
            ),
            Span::styled(
                "enter",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(" send    ", Style::default().fg(MUTED)),
            Span::styled(
                "esc",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(" cancel", Style::default().fg(MUTED)),
            Span::styled(
                "    ?",
                Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
            ),
            Span::styled(" help", Style::default().fg(MUTED)),
        ]),
    };
    frame.render_widget(Paragraph::new(text), area);
}

fn render_too_small(frame: &mut Frame, area: Rect) {
    let message = "Terminal too small — minimum 50×14";
    let width = message.chars().count().min(area.width as usize) as u16;
    let x = area.x + area.width.saturating_sub(width) / 2;
    let y = area.y + area.height / 2;
    frame.render_widget(
        Paragraph::new(message).alignment(Alignment::Center),
        Rect::new(x, y, width, 1),
    );
}

fn trim_text(text: &str, width: usize) -> String {
    if Line::from(text).width() <= width {
        return text.to_owned();
    }
    if width == 0 {
        return String::new();
    }
    if width == 1 {
        return "…".into();
    }
    let mut output = String::new();
    for character in text.chars() {
        let candidate = format!("{output}{character}…");
        if Line::from(candidate.as_str()).width() > width {
            break;
        }
        output.push(character);
    }
    output.push('…');
    output
}

fn pad_text(text: &str, width: usize) -> String {
    let current = Line::from(text).width();
    format!("{text}{:width$}", "", width = width.saturating_sub(current))
}
