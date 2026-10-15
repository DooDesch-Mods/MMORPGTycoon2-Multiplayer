# Repository language

Write repository documentation and development-facing text in English.

# Progress page

The user-approved progress page is `_drafts/progress.html`. Preserve its compact
overview and click-through details. Change this structure only when the user
explicitly requests a new design.

- The main page contains only short bullet lists under `Erledigt`, `In Arbeit`
  and `Offen`, plus a brief version/status line and the archive link.
- Clicking a bullet opens that item's details and matching real game images.
  Keep explanations, limits, technical evidence and images in the detail view.
  Do not put screenshots, logs, long reports or milestone history on the overview.
- Update milestone content in the existing `progress-data` JSON block. Preserve
  the current renderer, navigation, layout and stable item IDs for content edits.
- Keep previous history in `_drafts/progress-archive.html`. Do not restore the
  former long dashboard or append historical sections to the current page.
- Before each edit, read the current page from disk, even when session history or
  a handoff contains an older copy. Merge concurrent changes rather than replacing
  the page from stale session context. Existing sessions follow this same rule.
- Historical page-writing scripts can contain the obsolete layout. Do not run
  them unchanged against the current page. Adapt updates to `progress-data` first.
