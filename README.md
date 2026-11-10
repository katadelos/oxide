# Oxide

Oxide is a scriptlet runner for Kindle devices. It functions as a modern, daemon based replacement for [sh_integration](https://github.com/KindleModding/sh_integration).

## Installation

You need a jailbroken Kindle and root shell access.
Remove any existing scriptlet handler before installing.

Non-touch Kindle 4 on firmware **4.1.4** uses the separate `kindle4` package.

Oxide runs from `/var/local/oxide/oxide` so it remains available when user storage
switches to USB mode.

Run `start` after each reboot, manually or through your existing boot runner.
Oxide does not install a boot hook. Before replacing the binary, run
`uninstall`, then activate it again.

## Commands
| Command | Purpose |
| --- | --- |
| `./oxide start` | Activate and request a script refresh. Does nothing if already active. |
| `./oxide start --no-rescan` | Activate without requesting a content refresh, for boot setups where USB storage may be unavailable. |
| `./oxide status` | Show daemon, registration, and pending scanner restart state. |
| `./oxide rescan` | Refresh scripts and icons, including files copied over SSH. Scanning finishes asynchronously. |
| `./oxide uninstall` | Stop scripts and the daemon, remove Oxide’s registrations, and restart the scanner. |

After uninstalling, you can delete `/var/local/oxide/oxide`. Your scripts, catalogue
entries, logs, and shared `/var/local/su` helper are retained.

## Scriptlet format
Place optional headers before the first command:

| Header | Meaning |
| --- | --- |
| `# Name: ...` | Library title; defaults to the filename. |
| `# Author: ...` | Author; defaults to `Unknown`. |
| `# Icon: ...` | Image file path, relative to the script or absolute. Ignored on K4. |
| `# DontUseFBInk` | Log output without touching the screen. |

Scripts run as root with `/bin/sh -l`. Returning Home cancels the
running script. Applications must stay in the foreground and restore the Kindle
framework if they suspend it. Hooks are not supported.

## Troubleshooting

- **Script missing or icon unchanged:** on Kindle Touch and later, eject USB storage
  and run `rescan`. Local icons must be at most 4 MiB.
- **Conflicting registration:** remove the previous scriptlet handler using its
  own uninstaller before activating Oxide.
- **Scanner restart pending:** retry `start` or `uninstall` to finish the operation.
- **Active, but refresh failed:** resolve the reported error, then run `rescan`.
- **Script fails:** read `/var/local/oxide/last-run.log` (up to 1 MiB). Daemon errors
  are in `/var/run/oxide/daemon.log` on Kindle Touch and later.
- **No screen output:** check for `# DontUseFBInk` in the script header.

## License

Oxide is developed by [katadelos](https://github.com/katadelos) and licensed under
[GNU GPL version 3](LICENSE) (`GPL-3.0-only`).
Dependencies retain their own licenses.
