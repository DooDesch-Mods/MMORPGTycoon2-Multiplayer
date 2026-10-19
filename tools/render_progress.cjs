const fs = require('fs');
const path = require('path');
const {chromium} = require('C:/Users/denni/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async () => {
  const root = 'F:/Projects/Mods/MMORPGTycoon2';
  const browser = await chromium.launch({headless:true, executablePath:'C:/Users/denni/AppData/Local/ms-playwright/chromium_headless_shell-1234/chrome-headless-shell-win64/chrome-headless-shell.exe'});
  try {
    const page = await browser.newPage({viewport:{width:1440,height:1000}});
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto('file:///' + root + '/_drafts/progress.html');
    const sectionId = process.argv[2] || 'current-candidates';
    if (!/^[a-z][a-z0-9-]*$/.test(sectionId)) throw new Error('invalid progress section ID');
    const section = page.locator('#' + sectionId);
    if (await section.count() !== 1) throw new Error('current candidate section missing');
    const images = await section.locator('img').evaluateAll(nodes => nodes.map(n => ({src:n.src,complete:n.complete,width:n.naturalWidth})));
    if (images.some(i => !i.complete || !i.width)) throw new Error('candidate image failed to load: ' + JSON.stringify(images));
    if (errors.length) throw new Error(errors.join('\n'));
    const output = path.join(root, '_drafts/media/' + sectionId + '.png');
    await section.screenshot({path:output});
    console.log(JSON.stringify({state:'rendered',path:output,images}));
  } finally { await browser.close(); }
})().catch(e => { console.error('error: progress artifact render: ' + e.stack); process.exitCode = 1; });
