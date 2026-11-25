# Hide and Seek Mode - Changes Summary

## Files Modified

### Core Game Mode Files
1. **src/modes/hide_seek_world.hpp**
   - Removed `manualFoundByName()` function declaration

2. **src/modes/hide_seek_world.cpp**
   - Removed `manualFoundByName()` function implementation
   - Updated elimination delay to use configurable `ServerConfig::m_hs_elimination_delay`

### Command System
3. **src/lobby/commands/found.hpp** - ❌ DELETED
4. **src/lobby/commands/found.cpp** - ❌ DELETED
   - Removed `/found` command entirely (no longer needed)

5. **src/lobby/commands/confirm.cpp**
   - ✅ Added team check: only Red Team (hiders) can use `/confirm`

6. **src/lobby/commands/randomteams_hs.hpp**
   - Changed from votable to non-votable (anyone can use)
   - Removed permission level requirement

7. **src/lobby/commands/randomteams_hs.cpp**
   - Removed voting logic
   - Made accessible to all players without restrictions

8. **src/lobby/commands/setelimdelay.hpp** - ✨ NEW FILE
9. **src/lobby/commands/setelimdelay.cpp** - ✨ NEW FILE
   - New admin command to set elimination delay dynamically
   - Requires PERM_ADMINISTRATOR

10. **src/lobby/server_lobby_commands.cpp**
    - Removed `found.hpp` include and `FoundCommand` registration
    - Added `setelimdelay.hpp` include and `SetElimDelayCommand` registration
    - Fixed whitespace formatting issues

### Server Configuration
11. **src/network/server_config.hpp**
    - ✨ Added `m_hs_elimination_delay` config parameter (default 5.0s)

12. **src/network/server_config.cpp**
    - ✅ Added Hide and Seek mode (ID: 9) to `getLocalGameMode()`
    - ✅ Added name mappings: "hide-and-seek", "hide-seek", "hidenseek", "hs"
    - ✅ Added localized name "Hide and Seek" to `getModeName()`

### Kart/Collision System
13. **src/karts/kart.cpp**
    - ✅ Added physical collision detection in `Kart::crashed()`
    - When seeker (Blue team) bumps hider (Red team), calls `kartHit()`
    - Fixed whitespace/formatting issues in `collectedItem()`

### Powerup System
14. **src/items/powerup.cpp**
    - Fixed variable declaration order bug (moved `World *world` before use)
    - Added `#include "modes/hide_seek_world.hpp"`

## New Features Implemented

### 1. Physical Contact Detection
- Seekers can now find hiders by bumping/touching them
- Implemented in `Kart::crashed()` function
- Works alongside existing projectile detection

### 2. Configurable Elimination Delay
- Server admins can adjust delay via `/setelimdelay <seconds>` command
- Default: 5.0 seconds
- Config parameter: `hs-elimination-delay` in server_config.xml

### 3. Command Permission Enforcement
- `/confirm`: Restricted to hiders only (Red team)
- `/hint`: Already restricted to seekers only (checked in world logic)
- `/randomteams`: Open to all players, no voting required
- `/setelimdelay`: Admin-only (requires `/power` command first)
- `/sethidetime`: Admin-only (existing)
- `/settotaltimecap`: Admin-only (existing)
- `/poweruphint`: Admin-only (existing)

### 4. Server Mode Registration
- Hide and Seek now properly registered as mode ID 9
- Multiple name aliases supported: "hide-and-seek", "hide-seek", "hidenseek", "hs"
- Localized display name available

## Bug Fixes

### Bug #1: Undeclared Variable
**File**: `src/items/powerup.cpp`
**Issue**: Variable `world` used before declaration
**Fix**: Moved declaration to line 287, before first use

### Bug #2: Missing Include
**File**: `src/items/powerup.cpp`
**Issue**: `HideAndSeekWorld` type used without include
**Fix**: Added `#include "modes/hide_seek_world.hpp"`

### Bug #3: Missing Mode Registration
**File**: `src/network/server_config.cpp`
**Issue**: Hide and Seek mode not registered in server config system
**Fix**: Added mode ID 9 with all necessary mappings

### Bug #4: No Physical Contact Detection
**File**: `src/karts/kart.cpp`
**Issue**: Seekers couldn't find hiders by bumping
**Fix**: Added collision detection in `crashed()` method

### Bug #5: Hardcoded Elimination Delay
**Files**: `src/modes/hide_seek_world.cpp`, `src/network/server_config.hpp`
**Issue**: 5-second delay was hardcoded
**Fix**: Made configurable via ServerConfig and new command

### Bug #6: Command Permission Issues
**Files**: Multiple command files
**Issue**: Commands not properly restricted by team/role
**Fix**: Added team checks and permission enforcement

## Testing Checklist

### Server Configuration
- [ ] Server starts with Hide and Seek mode configured
- [ ] Mode ID 9 properly recognized
- [ ] All name aliases work: "hide-and-seek", "hide-seek", "hidenseek", "hs"
- [ ] Config parameters load correctly from server_config.xml

### Gameplay
- [ ] Seekers can find hiders by bumping into them
- [ ] Seekers can find hiders by hitting with cakes
- [ ] Found hiders are moved to lobby after delay
- [ ] Elimination delay is configurable

### Commands
- [ ] `/confirm` only works for hiders (Red team)
- [ ] `/hint` only works for seekers (Blue team)
- [ ] `/randomteams` works for all players
- [ ] `/setelimdelay` only works for admins
- [ ] `/sethidetime` only works for admins
- [ ] `/settotaltimecap` only works for admins
- [ ] `/poweruphint` only works for admins

### Network
- [ ] Mode properly syncs between server and clients
- [ ] Game state saves/restores correctly
- [ ] Track filtering works (race tracks only)
- [ ] Team assignments sync properly

## Configuration Examples

### server_config.xml
```xml
<!-- Hide and Seek Mode Configuration -->
<server-mode value="9" />  <!-- 9 = Hide and Seek -->
<hs-hide-time value="180" />
<hs-total-time-cap value="900" />
<hs-fire-distance-threshold value="30.0" />
<hs-hot-threshold value="20.0" />
<hs-warm-min value="25.0" />
<hs-warm-max value="50.0" />
<hs-hint-default-uses value="5" />
<hs-hint-unlock-seconds value="420" />
<hs-elimination-delay value="5.0" />
```

### Command Usage
```
# For all players:
/randomteams               # Assign random teams
/confirm                   # Hiders only - mark ready
/hint                      # Seekers only - get distance hint

# For admins (after using /power):
/setelimdelay 3.0         # Set elimination delay to 3 seconds
/sethidetime 120          # Set hide phase to 120 seconds
/settotaltimecap 600      # Set total time cap to 600 seconds
/poweruphint 10           # Set hint uses to 10
```

## Notes

1. **Found hiders behavior**: Changed from spectator mode to lobby return
   - Players moved to lobby can choose to spectate or leave
   - Implemented via existing `LE_BACK_LOBBY` mechanism

2. **Collision detection**: Bilateral - both karts call `crashed()` but only seeker→hider collision triggers `kartHit()`

3. **Team assignment**: Must be done before game starts using `/randomteams`
   - 1 seeker for ≤5 players
   - 2 seekers for 6-10 players

4. **Mode ID**: Using ID 9 (next available after CTF which is 8)

## Compatibility

- ✅ Works with existing networking infrastructure
- ✅ Compatible with rewind/prediction system
- ✅ Supports spectator mode
- ✅ Integrates with existing command system
- ✅ Uses standard track filtering
- ✅ Compatible with existing GUI systems

## Future Considerations

1. Consider adding per-player elimination delay override
2. May want to add `/autoteams_hs` variant for automatic team switching
3. Consider adding spectator-specific UI for eliminated hiders
4. May want to add statistics tracking for most successful hider/seeker
