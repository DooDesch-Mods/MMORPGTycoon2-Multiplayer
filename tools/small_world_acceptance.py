"""Run the bounded native placement check in the small owned tutorial-free profile."""
import sys
from pathlib import Path
from local_acceptance import AcceptanceRunner, parser, utc, log

ROOT = Path(__file__).resolve().parents[1]

class SmallWorldRunner(AcceptanceRunner):
    def run(self):
        try:
            self.command('stop-small-owned', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/stop_local_test.py'), '--test-root', str(self.root)])
            self.command('prepare-small-profile', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/prepare_small_world_profile.py'), '--test-root', str(self.root)])
            self.command('start-small-owned', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/local_test.py'), '--test-root', str(self.root), '--ready-timeout', str(self.args.ready_timeout)])
            self.args.restart = False
        except Exception as error:
            self.summary.update(state='failed-small-profile-setup', passed=False,
                                error={'step': self.summary['step'], 'cause': f'{type(error).__name__}: {error}', 'mutation_replayed': False})
            self.summary['finished_utc'] = utc()
            self.persist()
            log('error', f'small world profile setup failed: {error}; native mutations skipped')
            return 1
        return super().run()

def main():
    args = parser().parse_args()
    args.restart = False
    return SmallWorldRunner(args).run()

if __name__ == '__main__':
    sys.exit(main())
