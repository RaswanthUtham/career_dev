---
name: learning-lesson-review
description: "Use when reviewing Markdown lessons for technical accuracy, plan alignment, missing essential concepts, redundant explanations or duplicated code, faulty worked examples, or previous review fixes. Preserve correct explanations and report only evidenced checklist violations; never silently rewrite lessons."
argument-hint: "Lesson file/folder or previous review comments to verify"
---

# Evidence-Based Lesson Reviewer

## Purpose And Boundary

Review whether a lesson teaches its intended embedded technical concept
accurately. Keep the learner's simple English, analogies, examples, inline
comments and Feynman recaps when the meaning is sound.
Also check whether the lesson fits the active plan and supplies the concepts
needed for its stated outcome. Completeness means enough to understand and use
this lesson correctly, not coverage of every related advanced topic.

Apply the [shared teaching contract](../learning-feynman-coach/SKILL.md) and
only the domain skills needed by the inspected content. The
[roadmap](../../../plans/five_month_development_roadmap.md),
[project contracts](../../../plans/project_portfolio.md) and
[tracker](../../../plans/training_tracker.md) provide scope and evidence, not
automatic proof that a claim or example is correct.

Current teaching is MCU/FreeRTOS-first with bounded Zephyr. Existing Linux
lessons can still be reviewed when explicitly requested; their domain checklist
below is reference accuracy, not a recommendation to resume that curriculum.
Use archived domain references only for that bounded purpose.

This is a **read-only review workflow**. Do not edit the lessons, their index,
learner implementations, tracker, log, archives or system configuration during
a review. Do not create a report file unless requested. A review request is not
permission to build/run examples, install tools, flash hardware or publish code.
Do not quiz the learner or infer their competence from a document.
An explicit request to improve this review skill permits only the scoped skill
change; it does not authorize edits to the lessons being reviewed.

If the user later approves specific fixes, handle that as a separate editing
task: reread current text, apply only the approved minimal repairs, preserve
style and history, and validate the changes. A request to check whether fixes
were made remains read-only.

## Review Procedure

1. **Scope:** enumerate the actual Markdown files under the requested path.
   For "all lessons", read every lesson, not merely previous versions or the
   index. Inspect the index for contradictory navigation/status claims.
   Do not audit adjacent learner C files unless needed to verify a lesson claim.
2. **Intent:** identify the one concept and declared example assumptions.
   Review code and prose together, including recaps, tables, diagrams and
   interview answers. Short excerpts need not be complete runnable files.
   Map the lesson to its project purpose and roadmap stage. Identify essential
   prerequisites taught here, accurately linked earlier, or explicitly deferred.
   Trace the stated outcome through initial state, normal operation, relevant
   boundary/failure cases and observable evidence. Report a missing concept only
   when its absence prevents that outcome or creates a misleading guarantee.
3. **Evidence:** for material claims, inspect current definitions, callers,
   configuration, callbacks and relevant success/error/return paths. Prefer
   pinned upstream source and official specifications; record revision and
   distinguish the reference checkout from the eventual target.
4. **Recompute:** independently derive numerical examples, units, conversions,
   ranges and expected results. Check actual expression types, not just variable
   names. Trace before/after state and ownership for code examples.
5. **Verify candidates:** identify the exact incorrect claim, applicable
   checklist item, consequence and authoritative correction. A search hit or
   imagined edge case without applicable conditions is not enough.
6. **Deduplicate:** group repeated instances of the same misconception.
   Do not mark a finding resolved merely because the Corrections section says
   it is fixed; check every cited/repeated occurrence.
   Compare repeated explanations and code within and across the requested
   lessons. Identify what each repetition adds: prerequisite, new state/value
   trace, changed behavior, recap, or a necessary standalone reference.
   Flag avoidable duplication only with exact locations and a concrete cost:
   obscured lesson changes, ambiguous copy instructions, divergent versions,
   unnecessary reimplementation, or substantial repeated text with no new
   teaching purpose. Recommend one canonical explanation plus a precise link
   or delta, without removing what makes the lesson understandable.
7. **Report:** output only violations that pass the materiality filter below,
   plus a brief scope/validation-limit statement when needed. Leave correct
   material alone. Preserve unknowns rather than manufacturing certainty.
   When plan alignment or completeness is requested, give a short verdict and
   distinguish incorrect claims from essential omissions. Do not prescribe a
   new curriculum or turn optional depth into mandatory corrective work.

## Checklist

| ID | Check | Report when |
| --- | --- | --- |
| C01 | Intended concept and explanation | The explanation, analogy or example teaches a materially different mechanism, or a key step contradicts the stated concept |
| C02 | Facts, sources and versions | Architecture/API/register/board/version claims conflict with authoritative evidence, or a source/target distinction changes the conclusion |
| C03 | Inputs, arithmetic and units | Values, integer promotions, signedness, overflow, shifts, division, byte/element units, addresses or expected outputs are wrong for the stated model |
| C04 | Code and API contract | Code contradicts its declared preconditions, return/error behavior, pointer/bounds/lifetime rules or valid edge-case policy |
| C05 | Control flow and execution context | Caller/callee, syscall/firmware route, register state, task/IRQ context, sleep/wakeup/return, privilege or asynchronous flow is wrongly attributed |
| C06 | Concurrency, lifetime and ownership | The lesson confuses notification with readiness, atomicity with ordering, or allocation with quiescence; it promises safety without the required coordination |
| C07 | Memory, DMA and hardware constraints | VA/PA/DMA addresses, cache/permission rules, voltage/pin/clock assumptions or peripheral support are incorrectly interchanged or asserted |
| C08 | Assumptions and limits | An example's essential condition is absent or contradicted, producing an incorrect guarantee such as no loss, immediate wakeup, exact timing or universal behavior |
| C09 | Internal consistency | Main text, tables, comments, diagrams, recap or interview answers disagree about the same mechanism; a correction leaves the old error elsewhere |
| C10 | Verification and tests | Claimed results contradict evidence; a runnable example's tests have wrong expectations, hide failure or cannot establish the claimed property |
| C11 | Actionable commands and references | A presented command is malformed, targets the wrong host/file, can hide the stated failure, or a broken/misdirected reference prevents the instructed step |
| C12 | Learning, project and operational honesty | Publication is called mastery, a toy model is called physical/production evidence, host work is misrepresented as target work, or instructions create an unjustified harmful/irreversible action |
| C13 | Plan and prerequisite alignment | Required work contradicts the active project scope/platform/sequence, relies on an untaught prerequisite without explanation or a usable reference, or replaces an outstanding milestone with assumed completion |
| C14 | Essential concept coverage | The stated outcome needs an omitted mechanism, precondition or distinction, so the learner cannot correctly explain or apply the presented example; a relevant earlier explanation may satisfy this need |
| C15 | Behavioral and boundary contracts | An undefined initial state, ownership rule, time/sampling bound, rearm/cleanup rule or fault policy leaves materially different outcomes under conditions the lesson claims to handle |
| C16 | Outcome and evidence coverage | A claimed practical result has no suitable observation or acceptance condition, or its described check cannot distinguish the intended behavior from a relevant failure |
| C17 | Redundant explanation and code duplication | Repeated material adds no distinct teaching/reference purpose and obscures the new concept or implementation delta, creates competing copies, or requires unnecessary reimplementation; cite each location and the minimal safe consolidation |

### Redundancy Review: Preserve Useful Reinforcement

- Compare meaning and behavior, not just identical wording. Similar-looking
  handlers can have different ownership, privilege or scheduling contracts;
  never recommend merging them without checking those differences.
- Distinguish an annotated excerpt from the complete copyable reference.
  Keep both when they serve different needs, clearly identifying which is
  canonical and whether snippets replace, extend, or merely illustrate it.
- Recaps, before/after diagrams, interview answers and essential safety gates
  are not redundant merely because they revisit a mechanism. Do not remove
  these required teaching surfaces or alter learner-approved simple wording
  solely to reduce line count.
- Cross-lesson boilerplate may be retained for a justified standalone build.
  Otherwise prefer a prerequisite link and exact list of changed units over
  re-explaining unchanged clock/startup/bit-mask logic. Do not add a shared
  library, new files or abstraction merely to eliminate pedagogical repetition.
- Report redundancy separately from technical defects when useful. Normally
  use Low impact for avoidable repetition; elevate only for demonstrated
  contradictory behavior, unsafe copying or a materially broken build path.

### Plan And Missing-Concept Checks

- Follow the roadmap's scope and prerequisite order, not file numbering alone.
  An early, bounded preview is acceptable when labelled and explained; a deep
  implementation dependency requires its prerequisite or an explicit deferral.
- Separate optional enrichment from missing essentials. Cite the lesson goal
  or exact code/claim that requires an omitted concept and explain the concrete
  misunderstanding/failure it permits.
- For state machines, consider initialization (including an already-active
  input), repeated/held events, release/rearm, period changes, late service,
  rollover and error recovery only where relevant to the stated behavior.
- For sampled inputs, distinguish physical transitions from sampled values.
  Check polling bounds, minimum detectable pulse/stability duration, tick
  resolution and the basis for any debounce threshold.
- For timer/ISR/RTOS comparisons, distinguish event expiry, Ready state and
  actual execution; verify tick units/configuration, progress assumptions,
  event coalescing, shared-state synchronization and rearming.
- For a practical layer, identify the minimum normal/boundary observations
  needed to support its claims. Do not demand a full test suite or unrelated
  hardware measurements for a small, explicitly scoped example.

### Domain Accuracy Prompts

Use only the relevant prompts; they are not a requirement to add every advanced
topic to an introductory lesson.

- **C/C++:** integer promotions (`uint8_t`/`uint16_t` promoting to `int` if representable, else `unsigned int`) and narrowing conversion before comparison; signed UB versus unsigned arithmetic; output pointer validity versus NULL checking (and initializing `*out = 0` before early failure returns); deadline/guard check order in state machines; object lifetime, alignment, array bounds; zero-size and overlap policy.
- **Linux/IIO:** file-descriptor lookup (`ksys_read`) versus VFS dispatch (`vfs_read`); helper status return (`0`) versus copied byte count; syscall versus task switch (both blocking and preemption can switch tasks); sample counts versus bytes; watermark readiness batching versus per-push wait-queue notification; `user_mode_thread` (`PID 1`, no `PF_KTHREAD`) versus `kernel_thread` (`PF_KTHREAD`); `kernel_clone(.fn)` versus user-space `fork()`; initcall level order and `-EPROBE_DEFER`; partial reads, restartable signals and user-copy errors.
- **Architecture:** BBB Armv7-A, Arm64 ELs, RISC-V privilege modes and Cortex-M Thread/Handler are different models. Distinguish live from saved registers (`pt_regs` general registers vs. FP/vector state), original arguments (`orig_a0`) from result slots (`a0`/`regs[0]`), vector-table slot addresses (`0x08000004`) from handler entry addresses (`0x08000888`), symbol start addresses (`0x080004f4` prologue) from first-line breakpoint addresses (`0x080004f8`), actual `.map` section offsets, memory protection (MMU/MPU) from address translation, and architecture-specific `.bss`/MMU startup order.
- **RTOS/hardware:** 32-bit register width (`uint32_t ODR`) versus active 16-bit pin fields; output latch bits (`ODR`/`BSRR`) versus input reporting (`IDR`) and alternate-function pin muxing; inclusive down-counter `LOAD = N - 1` math; global `HAL_MspInit()` versus peripheral `HAL_UART_MspInit()`; `.elf` versus raw `.bin` versus address-bearing `.hex` records; mutexes versus ISR-safe operations; task progress versus watchdog feeding; measured latency versus a worst-case bound.
- **Build/setup:** match host tools, release, configuration and target ABI;
  a container is not an independent host kernel; firmware flashes, disk writes
  and remote authentication need scoped, recoverable procedures.

## Materiality Filter: Preserve Useful Simplicity

Report a comment only when all are true:

1. You can point to a specific sentence, expression, input, table or command;
   for omissions, cite the goal or code that needs the missing concept.
2. An applicable checklist rule is violated under the stated conditions.
3. It can cause a wrong technical understanding, wrong result, unsafe action
   or an unjustified evidence claim; for C17, demonstrate redundant teaching
   with no new purpose or a concrete implementation/maintenance ambiguity.
4. You have high-confidence reasoning/source evidence, not just a preference.

Do **not** report:

- Cosmetic grammar, capitalization, formatting, verbosity or preferred wording.
- A simple analogy that explains the scoped concept correctly.
- Missing unrelated advanced detail or an omitted implementation-only template.
- A concept correctly explained by a usable prerequisite link, or explicitly
  deferred without being needed by the current example.
- An optional preview or a different lesson split that still preserves the
  plan's scope, prerequisites and evidence requirements.
- Clearly labelled illustrative numbers or intentionally incomplete excerpts.
- A hypothetical alternate architecture/configuration when the lesson already
  scopes its claim correctly.
- Missing optional tests as a correctness bug when the stated test claim is
  bounded and accurate. Report only a demonstrated contract/coverage mismatch.
- An old finding that the current text has actually fixed.
- Purposeful recap, a new worked trace, or an explicitly canonical standalone
  listing. Similarity or length alone is not evidence of harmful redundancy.

If evidence is unavailable, do not call a claim false. State the narrow
verification limitation if it affects the review conclusion. A recorded
tutor-run result is not independently reverified by rereading the lesson, but
absence of a rerun is not proof that the historical run was invented.

## Output Contract

Give a brief scope line, then a violations-only list/table:

`ID | Impact | Lesson and exact line | Checklist | What is incorrect and why | Minimal correction`

- **High:** teaches unsafe implementation or a central incorrect mechanism.
- **Medium:** materially wrong fact/trace/guarantee that can mislead the learner.
- **Low:** bounded factual/operational mismatch worth correcting, not style.

Use clickable workspace file links with 1-based lines, and cite the decisive
source when it aids verification. Explain in simple English, with a tiny
counterexample if necessary. Suggest the smallest repair; do not provide an
unsolicited replacement lesson or add a new curriculum.

If no violations remain, say: **"No checklist violations found in the reviewed
scope."** Answer requested plan/completeness questions briefly without padding
the response with optional suggestions. If files or essential
sources were inaccessible, name the limitation and do not claim full coverage.

For a recheck, focus on the previous findings and repeated occurrences; report
unresolved/new regressions only and state when the checked findings are fixed.
Never claim tests passed unless actually run or explicitly attribute a
historical report. Do not treat a clean review as certification of production
safety, exhaustive correctness or learner mastery.
