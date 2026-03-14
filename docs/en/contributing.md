# Contributing to rt-claw

**English** | [中文](../zh/contributing.md)

## Quick Checklist

Before submitting a patch, ensure:

1. Code compiles cleanly for at least one platform
2. Code passes `scripts/check-patch.sh`
3. Every commit has a `Signed-off-by` tag
4. Commit message follows the format described below

## Commit Message Format

```
subsystem: short summary of change

Optional longer description explaining the motivation
and approach. Wrap at 76 characters.

Signed-off-by: Your Name <your@email.com>
```

### Rules

- **Subject line**: `subsystem: description` (lowercase subsystem, no trailing dot)
- **Subject length**: 76 characters max
- **Body**: separated from subject by a blank line, wrapped at 76 characters
- **Signed-off-by**: required on every commit (`git commit -s`)

### Subsystem Prefixes

| Prefix     | Scope                                     |
|------------|-------------------------------------------|
| `osal`     | OS abstraction layer (`osal/`)            |
| `gateway`  | Message routing (`claw/core/gateway.*`)   |
| `swarm`    | Swarm service (`claw/services/swarm/`)    |
| `net`      | Network service (`claw/services/net/`)    |
| `ai`       | AI engine (`claw/services/ai/`)           |
| `sched`    | Scheduler (`claw/core/scheduler.*`)       |
| `feishu`   | Feishu IM integration (`claw/services/im/`) |
| `drivers`  | Hardware drivers (`drivers/`)             |
| `platform` | Platform-specific changes (`platform/`)   |
| `build`    | Build system (Meson, CMake, SCons)        |
| `docs`     | Documentation changes                     |
| `tools`    | Tool Use framework (`claw/tools/`)        |
| `scripts`  | Build/CI scripts (`scripts/`)             |
| `main`     | Boot entry and init (`claw/claw_init.*`)  |
| `claw`     | Cross-cutting core changes                |
| `ci`       | CI workflow changes (`.github/`)          |
| `tests`    | Test scripts (`tests/`)                   |

### Examples

```
gateway: add priority-based message routing

Messages now carry a priority field. The gateway dispatches
high-priority messages before low-priority ones in each
routing cycle.

Signed-off-by: Chao Liu <chao.liu.zevorn@gmail.com>
```

```
osal: implement mutex timeout for RT-Thread

Signed-off-by: Chao Liu <chao.liu.zevorn@gmail.com>
```

### Additional Tags

- `Fixes: <sha> ("original commit subject")` — when fixing a previous commit
- `Tested-by:` / `Reviewed-by:` — attribution for testing and review

## Coding Style

See [Coding Style](coding-style.md) for the full coding style guide.

Key points:
- 4 spaces, no tabs
- 80-char line width target
- `claw_` prefix for public API
- Traditional C comments (`/* */`)
- C99 (gnu99)

## Code Check Tools

### Style Checker

```bash
# Check all source in scope (claw/ + osal/ + include/)
scripts/check-patch.sh

# Check staged changes (useful before committing)
scripts/check-patch.sh --staged

# Check specific files
scripts/check-patch.sh --file claw/core/gateway.c
```

### Sign-off Checker

```bash
# Check commits since main
scripts/check-dco.sh

# Check last 3 commits
scripts/check-dco.sh --last 3
```

### Git Hooks

Install git hooks to automatically check style and commit messages:

```bash
scripts/install-hooks.sh          # install
scripts/install-hooks.sh --remove # remove
```

Hooks installed:
- **pre-commit**: runs `check-patch.sh --staged` on code in `claw/`, `osal/`, and `include/`
- **commit-msg**: validates commit message format and Signed-off-by

## Patch Guidelines

### Split Patches

Each commit should be a self-contained, compilable change. Don't combine
unrelated changes in one commit.

### Don't Include Irrelevant Changes

Don't mix style fixes with functional changes. If code needs reformatting,
submit it as a separate commit.

### Test Your Changes

- Build for at least one platform before submitting
- If adding a feature, include a basic test or demo if applicable

## Developer's Certificate of Origin

By adding a `Signed-off-by` tag to your commit, you certify that:

1. The contribution was created in whole or in part by you and you
   have the right to submit it under the MIT license; or
2. The contribution is based upon previous work that, to the best
   of your knowledge, is covered under an appropriate open source
   license and you have the right to submit that work with
   modifications; or
3. The contribution was provided directly to you by some other
   person who certified (1) or (2) and you have not modified it.
