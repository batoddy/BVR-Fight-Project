"""Open Windows' native folder dialog; never upload or copy recording files."""
import base64
import os
from pathlib import Path
import subprocess
import shutil
import threading

_picker_lock = threading.Lock()
_SCRIPT = r'''
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.Application]::EnableVisualStyles()
$dialog = New-Object System.Windows.Forms.FolderBrowserDialog
if ($dialog.PSObject.Properties['AutoUpgradeEnabled']) { $dialog.AutoUpgradeEnabled = $true }
$dialog.Description = 'Kayıt klasörünü seç'
$dialog.ShowNewFolderButton = $false
$dialog.SelectedPath = $env:BVRGYM_PICKER_INITIAL_DIR
if ($dialog.PSObject.Properties['UseDescriptionForTitle']) { $dialog.UseDescriptionForTitle = $true }
$owner = New-Object System.Windows.Forms.Form
$owner.ShowInTaskbar = $false
$owner.TopMost = $true
$owner.Opacity = 0
try {
    $owner.Show()
    if ($dialog.ShowDialog($owner) -eq [System.Windows.Forms.DialogResult]::OK) {
        [Console]::Write($dialog.SelectedPath)
    }
} finally { $dialog.Dispose(); $owner.Dispose() }
'''


def choose_directory(initial):
    if os.name != 'nt':
        raise ValueError('Yerleşik klasör seçimi Windows üzerinde kullanılabilir')
    if not _picker_lock.acquire(blocking=False):
        raise ValueError('Klasör seçme penceresi zaten açık')
    try:
        environment = os.environ.copy()
        environment['BVRGYM_PICKER_INITIAL_DIR'] = str(initial)
        executable = shutil.which('pwsh') or str(Path(environment.get('SystemRoot', r'C:\Windows')) / 'System32/WindowsPowerShell/v1.0/powershell.exe')
        command = base64.b64encode(_SCRIPT.encode('utf-16-le')).decode('ascii')
        result = subprocess.run([str(executable), '-NoProfile', '-STA', '-EncodedCommand', command],
                                env=environment, capture_output=True, encoding='utf-8',
                                creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:
            raise ValueError('Windows klasör seçme penceresi açılamadı')
        return result.stdout.strip() or None
    finally:
        _picker_lock.release()
