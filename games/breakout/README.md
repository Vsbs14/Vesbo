# Vesbo Breakout

A complete, playable 2D Breakout / Brick Breaker game written in **pure Vesbo (`.vsb`)** running on a 60 FPS game loop powered by **Raylib 6.0** built-in bindings.

---

## Quick Start

### 1. Run with Tree-Walking Interpreter
```bat
games\breakout\run.bat
```
*(or from root: `vesbo.exe games\breakout\breakout.vsb`)*

### 2. Run with Bytecode Virtual Machine (VM)
```bat
games\breakout\run_bytecode.bat
```
*(or from root: `vesbo.exe -c games\breakout\breakout.vsb -o games\breakout\breakout.vbo && vesbo.exe games\breakout\breakout.vbo`)*

---

## Controls

| Key | Action |
| :--- | :--- |
| **`Left Arrow` / `A`** | Move paddle left |
| **`Right Arrow` / `D`** | Move paddle right |
| **`Space`** | Launch ball from paddle |
| **`R`** | Restart game |
| **`Esc`** | Exit game |

---

## Gameplay & Features

- **50 Dynamic Bricks**: 5 rows $\times$ 10 columns colored across arcade tiers:
  - Red: 50 points
  - Orange: 40 points
  - Gold: 30 points
  - Lime: 20 points
  - Sky Blue: 10 points
- **Deflection Paddle Physics**: The ball bounces at sharper angles when struck near the paddle edges, allowing tactical aiming.
- **Spark Particle System**: Shattering a brick spawns a burst of spark particles matching the brick's color.
- **Full Game States**: Pre-launch ready state, life lost penalty, game over, and victory screens with restart options.
- **Live HUD**: Real-time Score, Remaining Lives, Remaining Bricks, and dynamic FPS counter.

---

## How It Stress-Tests Vesbo

Running Breakout tests several core subsystems of the Vesbo language and runtime:

1. **60 FPS Game Loop Performance**:
   Executes thousands of bytecode instructions and AST evaluations per second while maintaining 60 FPS.
2. **Collection Traversal in Hot Paths**:
   Iterates over 50 brick maps every frame checking circle-rectangle collisions (`rl_check_collision_circle_rec`).
3. **In-Place Map Mutation**:
   Tests dictionary mutation (`set b["alive"] to false`) inside active iteration loops across arrays of dictionaries.
4. **Dynamic Entity Lifecycle**:
   Simulates active spark particles pushed to dynamic arrays and pruned when dead.
5. **Real-time String Interpolation**:
   Formats HUD labels (`$"SCORE: {score}"`, `$"FPS: {rl_get_fps()}"`) dynamically each frame.
6. **Dual Execution Engine Parity**:
   Compiles and runs identically on both the **Tree-Walking Interpreter** and the **Bytecode Compiler + Stack VM**.
