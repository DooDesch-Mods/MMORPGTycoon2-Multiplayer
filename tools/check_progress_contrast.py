"""Measure the existing progress-page text palette against its inherited background."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
html = (root/'_drafts/progress.html').read_text(encoding='utf-8')
style = re.search(r'<style>(.*?)</style>', html, re.S).group(1)
rules = dict(re.findall(r'([^{}]+)\{([^{}]+)\}', style))
background = re.search(r'background:(#[0-9a-f]{6})', rules['body']).group(1)

def luminance(color):
    channels = [int(color[i:i+2],16)/255 for i in (1,3,5)]
    linear = [c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4 for c in channels]
    return sum(c*w for c,w in zip(linear,(0.2126,0.7152,0.0722)))

rows = []
for selector in ('body','a','.pass','.open','small'):
    color = re.search(r'(?:^|;)color:(#[0-9a-f]{6})',rules[selector]).group(1)
    first,second = sorted((luminance(color),luminance(background)))
    ratio = (second+0.05)/(first+0.05)
    rows.append({'selector':selector,'foreground':color,'background':background,'ratio':round(ratio,3),'aa_body':ratio>=4.5})
if not all(row['aa_body'] for row in rows):
    raise RuntimeError('progress-page text contrast fails AA: '+json.dumps(rows))
receipt = root/'artifacts/progress_contrast.json'
receipt.write_text(json.dumps({'passed':True,'rows':rows,'scope':'CSS text; original game screenshots are unmodified evidence'},indent=2),encoding='utf-8')
print(json.dumps({'passed':True,'rows':rows,'receipt':str(receipt)}))
