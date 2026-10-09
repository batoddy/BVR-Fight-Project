"""In-memory native CSV fixtures for reader tests; no production upload interface."""
import io
from pathlib import Path
from recordings import read_csv, metadata, assemble

def read_exports(files):
    return assemble([read_csv(Path(f['name']),io.StringIO(f['text'])) for f in files],
                    [metadata(Path(f['name'])) for f in files])
