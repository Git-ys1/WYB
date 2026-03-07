# Git Workflow (T-1.1.5D-R1-GIT)

## Repository Info
- Local root: `F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3\WYB`
- Remote: `https://github.com/Git-ys1/WYB.git`
- Default branch: `main`
- GitHub account: `Git-ys1`
- Git email: `3091964993@qq.com`
- Visibility: `private` (preferred)

## Baseline Anchor
- Baseline commit (stable OLED+ADC smoke): `<to be filled after first commit>`

## Mandatory Routine For Every Round

### Before Any Code Change (required)
```powershell
git status
git add .
git commit -m "checkpoint: before <TASK_ID>"
git push
```

### After Round Completed (required)
```powershell
git status
git add .
git commit -m "<TASK_ID>: <summary>"
git push
```

## Rollback Strategies (fixed)

### 1) Broken but not committed
```powershell
git restore .
```
or single file:
```powershell
git restore App\app.c
```

### 2) Already committed, safe reverse
```powershell
git revert HEAD
```

### 3) Local hard rollback (only for unshared local experiments)
```powershell
git reset --hard HEAD~1
```

## Commit Message Templates
- Baseline: `baseline: <stable state summary>`
- Round start checkpoint: `checkpoint: before <TASK_ID>`
- Round result: `<TASK_ID>: <what changed>`

## Refactor Discipline (next stage)
- Freeze current stable OLED/ADC baseline.
- Refactor by one module per round only:
  - `display`
  - `adc`
  - `menu`
  - `measure_res`
- No coding starts before checkpoint commit+push.
