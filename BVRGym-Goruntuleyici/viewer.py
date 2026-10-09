"""Independent viewer for the exact CSV files exported by BVRGym's TacviewLogger."""
import argparse
from pathlib import Path
import sys

from recordings import RecordingStore
from server import make_server


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description='BVRGym Tacview CSV görüntüleyici')
    parser.add_argument('--recording', type=Path,
                        default=Path(__file__).resolve().parent.parent / 'Workspace/BVRGym/data_output/tacview',
                        help='TacviewLogger kayıt klasörü; oluşacak kayıtlar da otomatik açılır')
    parser.add_argument('--port', type=int, default=8765, help='Tarayıcı arayüzü portu')
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error('Port 1–65535 arasında olmalı')
    if args.recording.exists() and not args.recording.is_dir():
        parser.error('--recording bir klasör olmalı')
    store = RecordingStore(args.recording)
    try:
        server = make_server(store, args.port)
    except OSError as exc:
        parser.error(str(exc))
    print(f'Görüntüleyici: http://127.0.0.1:{server.server_port}', flush=True)
    print(f'Tacview CSV klasörü: {store.directory}', flush=True)
    print('Kayıt klasörü izleniyor. Ctrl+C yalnızca görüntüleyiciyi kapatır.', flush=True)
    store.start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        store.close()
        server.server_close()


if __name__ == '__main__':
    main()
