"""Package only the read-only tester startup diagnostic."""
from pathlib import Path
import hashlib
import shutil
import zipfile

root=Path(__file__).resolve().parents[1]
folder=root/'_drafts/tester_diagnose'
shutil.copy2(root/'tools/diagnose_startup.ps1',folder/'MT2MP-Diagnose.ps1')
archive=root/'_drafts/tester_diagnose.zip'
names=['MT2MP-Diagnose.bat','MT2MP-Diagnose.ps1']
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as output:
    for name in names:
        output.write(folder/name,name)
with zipfile.ZipFile(archive) as source:
    assert source.namelist()==names
    for name in names:
        assert source.read(name)==(folder/name).read_bytes()
print('Verified diagnostic ZIP: '+str(archive)+' SHA256='+hashlib.sha256(archive.read_bytes()).hexdigest())
