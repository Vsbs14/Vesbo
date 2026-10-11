# Coroutines & Game Scripting

Vesbo features **first-class stackful coroutines** (`ObjCoroutine`), making it ideal for game entity behaviors, cutscenes, AI state machines, and cooperative multitasking where tasks pause at `yield` and resume on the next frame or tick.

---

## Creating and Resuming Coroutines

Create a coroutine by passing a function name and optional initial arguments to `coroutine()` (or its alias `spawn()`). New coroutines start in the `"suspended"` state.

```vsb
fnc counter_task(start)
	var step1 is yield start * 10
	var step2 is yield step1 + 5
	return step2 + 100

fnc main()
	var co is coroutine(counter_task, 2)
	output coroutine_status(co)  -- "suspended"
	output resume(co)            -- 20
	output resume(co, 30)        -- 35 (sends 30 as result of first yield)
	output resume(co, 7)         -- 107 (sends 7 as result of second yield)
	output is_alive(co)          -- false
	output coroutine_status(co)  -- "dead"
```

---

## Coroutine Built-ins

| Function | Description |
| :--- | :--- |
| `coroutine(fn, [args...])` / `spawn(fn, [args...])` | Create a suspended coroutine for function `fn` with initial arguments. |
| `resume(co, [value])` | Resume `co`. On first resume, runs from entry; on subsequent resumes, `value` becomes the return value of the suspended `yield` expression. |
| `yield [value]` | Suspend the currently running coroutine and return `value` (or `none`) to the `resume()` caller. |
| `coroutine_status(co)` / `status(co)` | Return `"suspended"`, `"running"`, or `"dead"`. |
| `is_alive(co)` | Return `true` if `co` is not `"dead"`, or `false` once finished. |

---

## Game Entity Pattern: Per-Frame AI Behavior

Because Vesbo coroutines are **stackful**, a coroutine can `yield` from inside deeply nested helper functions, loops, or struct methods without flattening your game logic into manual state machines:

```vsb
struct Enemy
	var name is "Sentinel"
	var x is 0
	var hp is 100

	fnc patrol_step(delta)
		self.x = self.x + delta
		return self.x

fnc patrol_ai(enemy)
	loop while enemy.hp > 0
		-- Patrol right for 3 ticks
		loop from 1 to 3 as i
			yield enemy.patrol_step(10)
		-- Patrol left for 3 ticks
		loop from 1 to 3 as i
			yield enemy.patrol_step(-10)
	return "defeated"

fnc main()
	var guard is Enemy("Guard", 100, 50)
	var ai is coroutine(patrol_ai, guard)

	-- Simulate 4 game loop frames
	loop from 1 to 4 as frame
		if is_alive(ai)
			var pos is resume(ai)
			output $"Frame {frame}: {guard.name} at x={pos}"
```

---

## Error Propagation in Coroutines

If an uncaught runtime error occurs inside a coroutine:
1. The coroutine transitions immediately to `"dead"`.
2. The error unwinds cleanly to the caller that invoked `resume(co)`, where it can be caught with a standard `try` / `catch` block.
