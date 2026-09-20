import { spawnSync } from 'node:child_process';

const [action, ...args] = process.argv.slice(2);
let command;
let commandArgs;
if (process.platform === 'win32') {
  const scripts = { build: 'build', start: 'run', test: 'test', smoke: 'test', soak: 'test', setup: 'setup' };
  command = 'powershell';
  commandArgs = ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', `tools/${scripts[action]}.ps1`];
  if (action === 'smoke') commandArgs.push('-Smoke');
  if (action === 'soak') commandArgs.push('-SkipBuild', '-Soak', args[0] ?? '1000');
  else commandArgs.push(...args);
} else if (process.platform === 'darwin') {
  if (action === 'start') {
    command = 'open';
    commandArgs = ['release/macos/Battlegrounds.app'];
  } else {
    command = 'bash';
    const scripts = { build: 'build', test: 'test', smoke: 'test', soak: 'test', setup: 'setup' };
    commandArgs = [`tools/${scripts[action]}-macos.sh`];
    if (action === 'smoke') commandArgs.push('--smoke');
    if (action === 'soak') commandArgs.push('--skip-build', '--soak', args[0] ?? '1000');
    else commandArgs.push(...args);
  }
} else {
  console.error('Native build scripts support Windows and macOS.');
  process.exit(1);
}
const result = spawnSync(command, commandArgs, { stdio: 'inherit' });
if (result.error) console.error(result.error.message);
process.exit(result.status ?? 1);
