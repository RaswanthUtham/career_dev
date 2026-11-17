---
name: learning-lesson-review
description: "Use when reviewing Markdown lessons, auditing teaching accuracy, finding misinformation or faulty worked examples, or checking whether lesson review comments were fixed. Preserve correct explanations and report only evidenced checklist violations; never silently rewrite lessons."
argument-hint: "Lesson file/folder or previous review comments to verify"
---

# Evidence-Based Lesson Reviewer

## Purpose And Boundary

Review whether a lesson teaches its intended embedded technical concept
accurately. Keep the learner's simple English, analogies, examples, inline
comments and Feynman recaps when the meaning is sound.

Apply the [shared teaching contract](../learning-feynman-coach/SKILL.md) and
only the domain skills needed by the inspected content. The
[roadmap](../../../plans/five_month_development_roadmap.md),
[project contracts](../../../plans/project_portfolio.md) and
[tracker](../../../plans/training_tracker.md) provide scope and evidence, not
automatic proof that a claim or example is correct.

This is a **read-only review workflow**. Do not edit the lessons, their index,
learner implementations, tracker, log, archives or system configuration during
a review. Do not create a report file unless requested. A review request is not
permission to build/run examples, install tools, flash hardware or publish code.
Do not quiz the learner or infer their competence from a document.

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
7. **Report:** output only violations that pass the materiality filter below,
   plus a brief scope/validation-limit statement when needed. Leave correct
   material alone. Preserve unknowns rather than manufacturing certainty.

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

### Domain Accuracy Prompts

Use only the relevant prompts; they are not a requirement to add every advanced
topic to an introductory lesson.

- **C/C++:** integer promotions and conversion before comparison; signed UB
  versus unsigned arithmetic; output pointer validity versus NULL checking;
  object lifetime, alignment, array bounds; zero-size and overlap policy.
- **Linux/IIO:** file-descriptor lookup versus VFS dispatch; syscall versus
  task switch; sample counts versus bytes; watermark versus notification;
  partial reads, restartable signals and user-copy errors; managed cleanup
  versus in-flight users; selected buffer/controller behavior.
- **Architecture:** BBB Armv7-A, Arm64 ELs, RISC-V privilege modes and Cortex-M
  Thread/Handler are different models. Distinguish live from saved registers,
  original arguments from result slots, general registers from FP/vector state,
  memory protection from translation, and context-dependent address layouts.
- **RTOS/hardware:** mutexes versus ISR-safe operations; task progress versus
  watchdog feeding; measured latency versus a worst-case bound; hardware
  capability versus enabled/tested driver support; photos versus functional
  verification; safe voltage/power/connection assumptions.
- **Build/setup:** match host tools, release, configuration and target ABI;
  a container is not an independent host kernel; firmware flashes, disk writes
  and remote authentication need scoped, recoverable procedures.

## Materiality Filter: Preserve Useful Simplicity

Report a comment only when all are true:

1. You can point to a specific sentence, expression, input, table or command.
2. An applicable checklist rule is violated under the stated conditions.
3. It can cause a wrong technical understanding, wrong result, unsafe action
   or an unjustified evidence claim.
4. You have high-confidence reasoning/source evidence, not just a preference.

Do **not** report:

- Cosmetic grammar, capitalization, formatting, verbosity or preferred wording.
- A simple analogy that explains the scoped concept correctly.
- Missing unrelated advanced detail or an omitted implementation-only template.
- Clearly labelled illustrative numbers or intentionally incomplete excerpts.
- A hypothetical alternate architecture/configuration when the lesson already
  scopes its claim correctly.
- Missing optional tests as a correctness bug when the stated test claim is
  bounded and accurate. Report only a demonstrated contract/coverage mismatch.
- An old finding that the current text has actually fixed.

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
scope."** Do not pad the response with suggestions. If files or essential
sources were inaccessible, name the limitation and do not claim full coverage.

For a recheck, focus on the previous findings and repeated occurrences; report
unresolved/new regressions only and state when the checked findings are fixed.
Never claim tests passed unless actually run or explicitly attribute a
historical report. Do not treat a clean review as certification of production
safety, exhaustive correctness or learner mastery.
