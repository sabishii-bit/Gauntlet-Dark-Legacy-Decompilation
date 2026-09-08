// Use the pinned optional native package on every supported host. No downloads.
const { resolveBinaryPath } = require('@ast-grep/cli/postinstall');
const binary = resolveBinaryPath();
if (!binary) {
  console.error('ast-grep native package unavailable; run pnpm install --frozen-lockfile');
  process.exitCode = 2;
} else {
  process.stdout.write(binary);
}
