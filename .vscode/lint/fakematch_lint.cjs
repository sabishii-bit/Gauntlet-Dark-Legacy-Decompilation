// Launches the repository-local fakematch-lint binary so nothing has to be on
// PATH. Install or update it from the in-repo linter source:
//   pnpm run lint:install   (cargo install --path tools/fakematch-linter --locked --root tools/fakematch-lint)
// (tools/fakematch-lint/ is gitignored). All arguments are passed through.
//
// In `--format problems` mode the absolute paths the linter prints are re-cased
// to the root path this process was launched with. VS Code keys diagnostics by
// exact URI, so a canonical "W:/..." path never attaches to an editor opened
// from a "w:\..." workspace folder.
const { spawn, spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const root = path.resolve(__dirname, '..', '..');
const rootPosix = root.split(path.sep).join('/');
const exe = process.platform === 'win32' ? 'fakematch-lint.exe' : 'fakematch-lint';
const binary = path.join(root, 'tools', 'fakematch-lint', 'bin', exe);
const args = process.argv.slice(2);
// The repository config is lint.toml; callers may still override with --config.
if (!args.includes('--config')) {
  args.unshift('--config', 'lint.toml');
}
const formatIndex = args.indexOf('--format');
const problemsMode = formatIndex !== -1 && args[formatIndex + 1] === 'problems';

if (!fs.existsSync(binary)) {
  const message = `Scan incomplete: ${path.relative(root, binary)} is missing; run ` +
    'pnpm run lint:install';
  if (problemsMode) {
    console.log(`${rootPosix}/lint.toml:1:1: error FM000: [scanner] ${message}`);
  }
  console.error(`UNRESOLVED: ${message}`);
  process.exit(2);
}

if (!problemsMode) {
  const result = spawnSync(binary, args, { stdio: 'inherit', cwd: root });
  if (result.error) {
    console.error(`UNRESOLVED: ${result.error.message}`);
    process.exit(2);
  }
  process.exit(result.status === null ? 2 : result.status);
}

// Problems mode: rewrite the leading root of each diagnostic line so the path
// casing matches the workspace folder VS Code launched us from.
const rootLower = rootPosix.toLowerCase();
const recase = (line) => {
  const head = line.slice(0, rootPosix.length);
  if (head.toLowerCase() === rootLower && line.charAt(rootPosix.length) === '/') {
    return rootPosix + line.slice(rootPosix.length);
  }
  return line;
};

const child = spawn(binary, args, { stdio: ['inherit', 'pipe', 'inherit'], cwd: root });
child.on('error', (err) => {
  console.error(`UNRESOLVED: ${err.message}`);
  process.exit(2);
});
const lines = readline.createInterface({ input: child.stdout, crlfDelay: Infinity });
lines.on('line', (line) => process.stdout.write(recase(line) + '\n'));
child.on('close', (code) => {
  lines.close();
  process.exit(code === null ? 2 : code);
});
