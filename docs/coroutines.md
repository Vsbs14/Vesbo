# Coroutines

Vesbo features **first-class stackful coroutines** (`ObjCoroutine`), enabling state machines, iterators, generator patterns, and cooperative multitasking where tasks pause at `yield` and resume on subsequent calls.

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

## Cooperative Multitasking Pattern

Because Vesbo coroutines are **stackful**, a coroutine can `yield` from inside deeply nested helper functions, loops, or struct methods without flattening your program logic into manual state machines:

```vsb
struct Worker
	var name is "Worker"
	var x is 0

	fnc step(delta)
		self.x = self.x + delta
		return self.x

fnc task(worker)
	loop while worker.x < 50
		-- Step forward
		loop from 1 to 3 as i
			yield worker.step(10)
		-- Step backward slightly
		yield worker.step(-5)
	return "done"

fnc main()
	var worker is Worker("Worker 1", 0)
	var worker_co is coroutine(task, worker)

	loop while is_alive(worker_co)
		var pos is resume(worker_co)
		output $"{worker.name} position: {pos}"
```

---

## Error Propagation in Coroutines

If an uncaught runtime error occurs inside a coroutine:
1. The coroutine transitions immediately to `"dead"`.
2. The error unwinds cleanly to the caller that invoked `resume(co)`, where it can be caught with a standard `try` / `catch` block.
