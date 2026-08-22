use ratatui::{
    Frame,
    layout::Rect,
    style::{Modifier, Style},
    text::{Line, Span},
    widgets::Paragraph,
};

use crate::app::App;

use super::{ACCENT, MUTED, TEXT};

const GAP: usize = 3;
const ELLIPSIS: &str = "...";

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum VisibleItem {
    Ellipsis,
    Completion(usize),
    TruncatedCompletion(usize, usize),
}

pub fn render(frame: &mut Frame, app: &App, area: Rect) {
    if app.completions.is_empty() || area.width == 0 {
        return;
    }

    let labels = app
        .completions
        .iter()
        .map(|completion| completion.command)
        .collect::<Vec<_>>();
    let items = visible_items(&labels, app.completion_index, area.width as usize);
    let mut spans = Vec::new();
    for (position, item) in items.into_iter().enumerate() {
        if position > 0 {
            spans.push(Span::raw(" ".repeat(GAP)));
        }
        match item {
            VisibleItem::Ellipsis => {
                spans.push(Span::styled(ELLIPSIS, Style::default().fg(MUTED)));
            }
            VisibleItem::Completion(index) | VisibleItem::TruncatedCompletion(index, _) => {
                let active = index == app.completion_index;
                let style = Style::default()
                    .fg(if active { ACCENT } else { TEXT })
                    .add_modifier(if active {
                        Modifier::BOLD
                    } else {
                        Modifier::empty()
                    });
                let label = match item {
                    VisibleItem::TruncatedCompletion(_, width) => {
                        labels[index].chars().take(width).collect::<String>()
                    }
                    _ => labels[index].to_owned(),
                };
                spans.push(Span::styled(label, style));
            }
        }
    }
    frame.render_widget(Paragraph::new(Line::from(spans)), area);
}

fn visible_items(labels: &[&str], active: usize, width: usize) -> Vec<VisibleItem> {
    if labels.is_empty() || width == 0 {
        return Vec::new();
    }
    let active = active.min(labels.len() - 1);
    let active_width = labels[active].chars().count();
    if active_width > width {
        return vec![VisibleItem::TruncatedCompletion(active, width)];
    }

    let mut start = 0;
    while start < active && range_width(labels, start, active) > width {
        start += 1;
    }
    if range_width(labels, start, active) > width {
        return vec![VisibleItem::Completion(active)];
    }

    let mut end = active;
    while end + 1 < labels.len() && range_width(labels, start, end + 1) <= width {
        end += 1;
    }

    let mut items = Vec::new();
    if start > 0 {
        items.push(VisibleItem::Ellipsis);
    }
    items.extend((start..=end).map(VisibleItem::Completion));
    if end + 1 < labels.len() {
        items.push(VisibleItem::Ellipsis);
    }
    items
}

fn range_width(labels: &[&str], start: usize, end: usize) -> usize {
    let hidden_left = usize::from(start > 0);
    let hidden_right = usize::from(end + 1 < labels.len());
    let item_count = end - start + 1 + hidden_left + hidden_right;
    labels[start..=end]
        .iter()
        .map(|label| label.chars().count())
        .sum::<usize>()
        + (hidden_left + hidden_right) * ELLIPSIS.len()
        + item_count.saturating_sub(1) * GAP
}

#[cfg(test)]
mod tests {
    use super::{VisibleItem, visible_items};

    #[test]
    fn all_completions_are_visible_when_they_fit() {
        assert_eq!(
            visible_items(&["step", "stepover"], 0, 20),
            vec![VisibleItem::Completion(0), VisibleItem::Completion(1)]
        );
    }

    #[test]
    fn active_completion_pushes_the_visible_window_forward() {
        let labels = ["continue", "pause", "step", "stepover"];
        assert_eq!(
            visible_items(&labels, 0, 24),
            vec![
                VisibleItem::Completion(0),
                VisibleItem::Completion(1),
                VisibleItem::Ellipsis,
            ]
        );
        assert_eq!(
            visible_items(&labels, 2, 24),
            vec![
                VisibleItem::Ellipsis,
                VisibleItem::Completion(1),
                VisibleItem::Completion(2),
                VisibleItem::Ellipsis,
            ]
        );
    }

    #[test]
    fn narrow_width_keeps_the_active_completion_visible() {
        let labels = ["continue", "pause", "step"];
        assert_eq!(
            visible_items(&labels, 1, 5),
            vec![VisibleItem::Completion(1)]
        );
        assert_eq!(
            visible_items(&labels, 1, 3),
            vec![VisibleItem::TruncatedCompletion(1, 3)]
        );
    }
}
