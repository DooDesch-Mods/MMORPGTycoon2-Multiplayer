"""Render the public roadmap and feature overview from docs/roadmap.json."""
import argparse
from collections import Counter
import html
import json
from pathlib import Path
import re
import sys
import textwrap

ROOT = Path(__file__).resolve().parents[1]
START, END = '<!-- roadmap:start -->', '<!-- roadmap:end -->'
COLORS = ('#64e4d5', '#ffca67', '#a8c1d5')
STATES = {'released': 'Released', 'experimental': 'Experimental', 'verified': 'Confirmed in game',
          'implemented': 'Implemented', 'planned': 'Planned'}
COVERAGE_COLORS = {'experimental': ('#ffca67', '#3e321f'), 'released': ('#64e4d5', '#15383d'), 'verified': ('#8cb9ff', '#1b304a'),
                   'implemented': ('#ffca67', '#3e321f'), 'planned': ('#a8c1d5', '#21303e')}
STATE_SCOPE = {'experimental': 'In the download', 'verified': 'Development build only',
               'implemented': 'Not confirmed in game', 'planned': 'Future scope'}


def text(x, y, value, size=22, fill='#b8cad8', weight=400):
    return (f'<text x="{x}" y="{y}" font-family="Arial" font-size="{size}" fill="{fill}" '
            f'font-weight="{weight}">{html.escape(value)}</text>')


def graphic(data, mobile=False):
    width, height = (600, 1030) if mobile else (1200, 475)
    title = f"{data['product']} roadmap"
    description = '. '.join(f"{stage['label']}: {stage['title']}. " + '; '.join(stage['items'])
                            for stage in data['stages'])
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
             'role="img" aria-labelledby="title description">',
             f'<title id="title">{html.escape(title)}</title>',
             f'<desc id="description">{html.escape(description)}</desc>',
             f'<path fill="#0b1723" d="M0 0h{width}v{height}H0z"/>',
             '<g font-family="Arial">',
             text(28, 46, data['graphic_title'], 30, '#f2f7fb', 700),
             text(28, 80, data['release_label'], 22)]
    for index, stage in enumerate(data['stages']):
        x, y, w, h = (76, 136 + index * 286, 492, 264) if mobile else (28 + index * 392, 150, 360, 276)
        color = COLORS[index]
        cx, cy = (32, y + 16) if mobile else (x + 16, 118)
        if index < 2:
            connection = f'v286' if mobile else 'h392'
            parts.append(f'<path d="M{cx} {cy}{connection}" stroke="{color}" '
                         f'stroke-width="2" stroke-dasharray="{0 if index == 0 else 6}" opacity=".5"/>')
        parts.append(f'<circle cx="{cx}" cy="{cy}" r="14" fill="#0b1723" stroke="{color}" stroke-width="3"/>')
        if index == 0:
            parts.append(f'<path d="m{cx - 6} {cy} 4 4 8-8" fill="none" stroke="{color}" stroke-width="3"/>')
        elif index == 1:
            parts.append(f'<circle cx="{cx}" cy="{cy}" r="5" fill="{color}"/>')
        parts.extend([text(x, y + 22, stage['label'], 23, color, 700),
                      text(x, y + 62, stage['title'], 27, '#f2f7fb', 700)])
        baseline = y + 104
        for item in stage['items']:
            lines = textwrap.wrap(item, width=36 if mobile else 25, break_long_words=False)
            parts.append(f'<circle cx="{x + 5}" cy="{baseline - 8}" r="3" fill="{color}"/>')
            for line in lines:
                parts.append(text(x + 20, baseline, line, 24 if mobile else 22))
                baseline += 30 if mobile else 29
            baseline += 10
        if baseline - 10 > y + h - 10:
            raise ValueError(f"Roadmap text exceeds the {stage['label']} panel")
    parts.extend([text(28, height - 20, 'Priorities can change. No release dates promised.', 19), '</g></svg>'])
    return '\n'.join(parts) + '\n'


def coverage_counts(data):
    if not data['features']:
        raise ValueError('Progress requires a nonempty feature inventory')
    for key in ('id', 'name'):
        values = [row[key] for row in data['features']]
        if len(set(values)) != len(values):
            raise ValueError('Duplicate feature ' + key + ' would inflate progress counts')
    counts = Counter(row['status'] for row in data['features'])
    if counts.keys() - STATES.keys():
        raise ValueError('Progress contains an unknown feature status')
    return counts


def validate_data(data):
    counts = coverage_counts(data)
    groups = [group['id'] for group in data['groups']]
    if len(set(groups)) != len(groups):
        raise ValueError('Duplicate feature group')
    for row in data['features']:
        if not re.fullmatch(r'[a-z][a-z0-9-]*', row['id']):
            raise ValueError('Feature needs a stable lowercase ID: ' + row['id'])
        if row['group'] not in groups:
            raise ValueError('Unknown feature group: ' + row['group'])
        reference = data['references'].get(row['evidence'])
        if not reference or reference['status'] != row['status']:
            raise ValueError('Feature status differs from its evidence: ' + row['id'])
        for key in ('name', 'scope'):
            if not row[key].strip() or any(c in row[key] for c in ('|', '\n', '\r')):
                raise ValueError('Feature cells must be nonempty single lines: ' + row['id'])
    if set(groups) != {row['group'] for row in data['features']}:
        raise ValueError('Empty feature group')
    return counts


def progress_summary(data):
    counts = coverage_counts(data)
    total = sum(counts.values())
    return ', '.join(f'{counts[status]} {STATES[status].lower()}' for status in STATES if counts[status])


def coverage_graphic(data, mobile=False):
    counts = coverage_counts(data)
    total = sum(counts.values())
    statuses = [status for status in STATES if counts[status]]
    delivery_note = ('Experimental features are included in this download.' if counts['experimental']
                     else 'Development features are not in the current download.')
    width = 600 if mobile else 1200
    columns = 3 if len(statuses) > 4 else len(statuses)
    first_row = 160 + len(statuses) * 64 if mobile else 240 + ((len(statuses) - 1) // 4) * 72
    row_height = 106 if mobile else 62
    height = first_row + len(data['groups']) * row_height + 100
    description = progress_summary(data)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
           'role="img" aria-labelledby="title description">',
           f'<title id="title">{html.escape(data["coverage_title"])}</title>',
           f'<desc id="description">{html.escape(description)}. Each segment is one named feature. '
           'Development features are not included in the current download. '
           'Counts and percentages describe this inventory, not overall completion.</desc>',
           f'<path fill="#0b1723" d="M0 0h{width}v{height}H0z"/>',
           text(28, 44, data['coverage_title'], 30, '#f2f7fb', 700),
           text(28, 80, data['release_label'], 22)]
    for index, status in enumerate(statuses):
        color = COVERAGE_COLORS[status][0]
        percent = 100 * counts[status] / total
        if mobile:
            x, y = 28, 126 + index * 64
        else:
            x, y = 28 + (index % columns) * (1144 / columns), 130 + (index // columns) * 72
        scope = 'In ' + data['version_label'] if status == 'released' else STATE_SCOPE[status]
        out.extend([text(x, y, f'{counts[status]} {STATES[status]}', 24, color, 700),
                    text(x, y + 27, f'{percent:.1f}% - {scope}', 18)])
    out.append(text(28, first_row - 28, 'One segment = one feature. See the list for its scope.', 20))
    for index, group in enumerate(data['groups']):
        rows = [row for row in data['features'] if row['group'] == group['id']]
        y = first_row + index * row_height
        out.append(text(28, y + 24, group['name'], 23, '#f2f7fb', 600))
        x, bar_y = (28, y + 42) if mobile else (322, y + 4)
        step = 36 if mobile else 56
        for row_index, row in enumerate(rows):
            color, fill = COVERAGE_COLORS[row['status']]
            title = row['name'] + ': ' + STATES[row['status']] + '. ' + row['scope']
            out.append(f'<rect data-feature="{row["id"]}" x="{x + row_index * step}" y="{bar_y}" '
                       f'width="{step - 5}" height="28" fill="{fill}" stroke="{color}">'
                       f'<title>{html.escape(title)}</title></rect>')
        count_x = x + len(rows) * step + 10
        out.append(text(count_x, bar_y + 23, str(len(rows)), 21))
    out.extend([text(28, height - 64, f'{total} named features - updated {data["updated_on"]}', 20),
                text(28, height - 36, 'This is feature progress, not a project completion score.', 20),
                text(28, height - 10, delivery_note, 20), '</svg>'])
    return '\n'.join(out) + '\n'


def feature_table(data):
    lines = []
    for group in data['groups']:
        lines.extend([f"### {group['name']}", '', '| Feature | Status | Scope |', '| --- | --- | --- |'])
        for row in data['features']:
            if row['group'] == group['id']:
                label = STATES[row['status']]
                if row['status'] in ('released', 'experimental'):
                    label += ' - ' + data['version_label']
                elif row['status'] in ('verified', 'implemented'):
                    label += ' - development'
                lines.append(f"| {row['name']} | {label} | {row['scope']} |")
        lines.append('')
    return '\n'.join(lines)


def status_key(data):
    counts = coverage_counts(data)
    lines = ['| Status | Features | What it means |', '| --- | ---: | --- |']
    meanings = {'experimental': 'Included in ' + data['version_label'] + ' as experimental. End-to-end game confirmation remains incomplete.', 'released': 'Included in ' + data['version_label'] + ', within the scope listed below.',
                'verified': 'Works in the development build within the listed scope. Not released yet.',
                'implemented': 'Code is integrated, but successful in-game behavior is not yet confirmed.',
                'planned': 'No completed implementation or in-game confirmation is claimed for this scope.'}
    lines.extend(f'| {STATES[status]} | {counts[status]} | {meanings[status]} |' for status in STATES if counts[status])
    return '\n'.join(lines)


def recent_progress(data):
    recent = [row for row in data['features'] if row.get('milestone') == data['version_label'] and row['status'] == 'released']
    experimental = [row for row in data['features'] if row['status'] == 'experimental']
    verified = [row for row in data['features'] if row['status'] == 'verified']
    lines = ['## Recent progress', '', '### New in ' + data['version_label'], '']
    lines.extend('- ' + row['name'] + '.' for row in recent)
    if experimental:
        lines.extend(['', '### Experimental - included in this download', ''])
        lines.extend('- ' + row['name'] + '.' for row in experimental)
    if verified:
        lines.extend(['', '### Confirmed since the release - development only', ''])
        lines.extend('- ' + row['name'] + '.' for row in verified)
    return '\n'.join(lines)


def readme_block(data):
    assets = data['asset_directory']
    raw = data['repository_url'].replace('https://github.com/', 'https://raw.githubusercontent.com/') + '/main/'
    alt = 'Roadmap: ' + '. '.join(f"{stage['label']} - {stage['title']}" for stage in data['stages'])
    summary = progress_summary(data)
    return '\n'.join([START, f"## {data['overview_heading']}", '', data['overview_intro'], '',
        '<picture>', f'  <source media="(max-width: 600px)" srcset="{raw}{assets}/coverage-mobile.svg">',
        f'  <img src="{assets}/coverage.svg" alt="Feature progress: {summary}. See each feature delivery status below." width="1200">',
        '</picture>', '',
        status_key(data), '',
        'Each feature has its own status. Counts describe this list, not how close the whole project is to completion.', '',
        '<details>', '<summary>Feature details - what is included</summary>', '',
        feature_table(data), '', '</details>', '',
        '[See recent progress and every feature](docs/ROADMAP.md#recent-progress).', '', '## Roadmap', '',
        '<picture>', f'  <source media="(max-width: 600px)" srcset="{raw}{assets}/roadmap-mobile.svg">',
        f'  <img src="{assets}/roadmap.svg" alt="{html.escape(alt, quote=True)}" width="1200">',
        '</picture>', '', '[Read the roadmap in text](docs/ROADMAP.md).', END])


def roadmap_text(data):
    lines = [f"# {data['product']} roadmap", '',
             f"> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/{data['support_slug']}](https://support.doodesch.de/{data['support_slug']}).", '',
             data['overview_intro'], '', f"Updated {data['updated_on']}. Development entries do not change the current download.", '',
             status_key(data), '', recent_progress(data), '', '## Feature list', '', feature_table(data), '']
    for stage in data['stages']:
        lines.extend([f"## {stage['label']}: {stage['title']}", ''])
        lines.extend('- ' + item for item in stage['items'])
        lines.append('')
    lines.extend(['Priorities can change. No release dates are promised.', ''])
    counts = coverage_counts(data)
    total = sum(counts.values())
    lines.extend(['## How progress is counted', '',
                  'Each stable feature ID counts once, in exactly one status. A feature moves between statuses; it is not counted again when it advances. '
                  'The listed scope is the claim, not every variant of a larger game system.', ''])
    lines.extend(f'- {STATES[status]}: {counts[status]}/{total} ({100 * counts[status] / total:.1f}%)'
                 for status in STATES)
    lines.extend(['', 'This inventory is not an exhaustive completion score for the game. '
                  'It replaces the old 13-area count, so those percentages are not comparable. '
                  'Adding a genuinely new scope changes the denominator; it does not indicate lost progress.', '',
                  'A successful build alone cannot promote a feature to Confirmed in game. '
                  'Development confirmation alone cannot promote it to Released.', '',
                  'Source: [roadmap.json](roadmap.json). '
                  'Development scope: [progress record](evidence/development-progress.json).', ''])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Reject stale generated output without writing')
    args = parser.parse_args()
    data = json.loads((ROOT / 'docs/roadmap.json').read_text(encoding='utf-8'))
    validate_data(data)
    if len(data['stages']) != 3:
        raise ValueError('The public roadmap needs available, next and later stages')
    readme = (ROOT / 'README.md').read_text(encoding='utf-8')
    if readme.count(START) != 1 or readme.count(END) != 1:
        raise ValueError('README must contain exactly one roadmap marker pair')
    start, end = readme.index(START), readme.index(END) + len(END)
    outputs = {'README.md': readme[:start] + readme_block(data) + readme[end:],
               'docs/ROADMAP.md': roadmap_text(data),
               data['asset_directory'] + '/roadmap.svg': graphic(data),
               data['asset_directory'] + '/roadmap-mobile.svg': graphic(data, mobile=True),
               data['asset_directory'] + '/coverage.svg': coverage_graphic(data),
               data['asset_directory'] + '/coverage-mobile.svg': coverage_graphic(data, mobile=True)}
    stale = []
    for name, expected in outputs.items():
        path = ROOT / name
        if path.exists() and path.read_text(encoding='utf-8') == expected:
            continue
        stale.append(name)
        if not args.check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(expected, encoding='utf-8', newline='\n')
    if args.check and stale:
        raise ValueError('Stale roadmap output: ' + ', '.join(stale) + '; run python tools/render_roadmap.py')
    print('Roadmap: ' + ('current' if args.check else 'generated') + f" ({data['version_label']})")


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        print('error: ' + str(error), file=sys.stderr)
        sys.exit(1)
