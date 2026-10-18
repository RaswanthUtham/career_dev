---
name: learning-c-cpp
description: 'Teach and assess C and C++ for embedded development and interviews using adaptive Feynman practice. Use for pointer arithmetic, arrays, memory, lifetime, ownership, bit manipulation, RAII, templates, data structures, debugging or coding drills.'
argument-hint: 'Concept, code, answer review, or coding interview'
---

# C And C++ From Memory To Ownership

Read and apply the [shared Feynman contract](../learning-feynman-coach/SKILL.md) first for every topic and follow-up. Its general lesson sequence (what, why with a simple problem, conceptual solution, then implementation or use) and Feynman learning cycle govern this entire skill, alongside assessment, pacing, feedback and retention. This skill supplies domain-specific accuracy and practice. Do not assume daily C use implies a correct memory model.

## Current Entry And Teaching Mode

Start the first lesson from one of the prioritized gap areas (allocation lifetime and failure ownership; overflow-safe size/range calculations; shared-data correctness and synchronization) unless the learner requests a specific topic. Default to Teach: a realistic failure, concrete C code, defined terms, worked trace, repair and a short Feynman recap for an imagined 12-year-old, provided by the tutor without requesting a teach-back while questions are paused. Do not restart elementary syntax, `printf` or pointer-sum drills. Missing semicolons in chat do not establish the learner's overall proficiency.

Questions are currently paused at the learner's request. Do not restart diagnostics, prediction requests or compulsory teach-back until explicitly invited. All practice requests below are governed by the coach's mode control; in Teach mode provide worked examples instead of withholding explanations. Resume Assess mode only when the learner says so explicitly (for example, "quiz me" or "assess"). On resume, begin with one bounded scenario from the prioritized gap list and confirm the mode change in one sentence.

Prioritize demonstrated gaps: allocation lifetime and failure ownership; overflow-safe size/range calculations; shared-data correctness and synchronization. Correct follow-ups after feedback demonstrate progress, not delayed retention or independent mastery. Representation, alignment, aliasing, `restrict`, callback ownership and optimization-informed debugging remain later topics. Do not preemptively assess or assume proficiency on these topics; teach them on request or when they arise naturally. If the learner requests a later topic directly, honor the request; briefly note any prerequisite that will be inlined and proceed in Teach mode.

## Prerequisite Ladder

Use this as a dependency reference, not a mandatory beginner sequence: typed objects -> bounds and lifetime -> ownership -> asynchronous lifetime. Explain only the prerequisite missing from the current advanced example. Revisit integer promotions, alignment and aliasing when needed. Plain language must not lower the technical level or erase relevant preconditions.

For C++, add references/const -> constructors/destructors -> RAII -> rule of zero/five -> copy/move -> owning handles and non-owning views -> containers/iterators -> interfaces/templates -> threads/synchronization. Keep language versions explicit. Do not teach Linux kernel implementation as C++.

## Accuracy Checklist

Apply only the sublist relevant to the current lesson topic.

### Pointers And Arrays

- A valid object pointer designates an object or permitted position; it is not the object's value. Null, uninitialized and dangling pointers are different states.
- Pointer addition steps in elements. Explain `sizeof(*pointer)` without assuming every `int` is four bytes. Arithmetic must remain within an array or one past it; one-past is not dereferenceable. Pointer subtraction requires the same array and a representable result. Standard C does not permit arithmetic on `void *`.
- Distinguish `array`, `&array`, `&array[0]`, array-to-pointer conversion, pointer-to-array and pointer-to-pointer. Multidimensional arrays do not become pointer-to-pointer structures.
- Explain `sizeof` on arrays versus function parameters, storage duration versus scope, ownership versus access, and allocation size overflow before calling an allocator.
- Non-owning views can dangle. Do not claim C++20 `std::span::operator[]` guarantees bounds checking. Container reallocation may invalidate pointers and iterators.
- Manual overlapping-copy exercises must not order unrelated object pointers as though that were portable. Start with source/destination offsets inside one known buffer, or use a valid temporary-buffer strategy.

### Integer And Bit Operations

- Check shift count, promoted operand type, signed overflow, unsigned wraparound and conversion. An observed output is not a guarantee for undefined behaviour.

### Concurrency Primitives

- Distinguish `const`, `volatile`, atomic operations, locks and memory barriers. `volatile` is neither mutual exclusion nor cache coherence.

### C++ Object Model

- Check alignment, aliasing and object lifetime. Use byte-wise access only where the language permits it; do not imply that a cast repairs alignment or lifetime. Do not copy a non-trivially-copyable C++ object with raw byte operations.
- `std::move` enables move semantics; it does not itself transfer a resource. Explain moved-from state according to the actual type's contract. Smart-pointer ownership does not make the pointed-to data race-free.

## Practice Procedure

1. In Teach mode, explain the concrete use case, prerequisites and what/why/how before the full program. Cover each needed header, exact API prototype (or documented generic interface for macros), parameters, pointer levels, callback signature, returns, errors and resource lifetime. Draw the relevant bounds, lifetime or execution order. In requested Assess mode, ask one scenario, wait and grade before elaborating. Respect the question pause.
2. Choose explicit preconditions: valid range, capacity, zero-length behaviour, null policy, overlap policy, ownership and result/error convention.
3. Show the complete explained example in chat only after its prerequisites and APIs. Create only empty exercise files, not scaffolds or solutions; preserve existing learner code unless explicitly asked to change it. The learner closes the explanation and implements from memory when ready. In memory/string exercises, avoid opaque helper calls as specified by the shared contract. Use descriptive names and small functions.
4. Explain tests for empty, singleton, exact-fit, insufficient capacity, absent terminator, invalid length and overlap as applicable. Run them only when requested; review submitted code before editing it. Do not invoke undefined behaviour in a normal correctness test; isolate intentional demonstrations and label them.
5. Provide the exact build/run command after tutoring, using Podman for current host-C lessons. Explain language mode, warnings, debug information and threading flags where applicable. Verify sanitizer support before claiming its use, and explain diagnostic limitations. Do not automatically compile or execute the learner's work during teaching.
6. Show a fresh application and a concise interview explanation in Teach mode. Request a learner version only when assessment is enabled. Record actual evidence, not merely compilation success or a tutor-provided solution.

## Concurrency Teaching Order

Keep POSIX userspace threads, ISO C threads, MCU interrupts/device access, Zephyr APIs and the Linux kernel memory model distinct. Before synchronization, teach processes versus threads, shared address space/resources versus per-thread registers/stacks, scheduling, runnable/running/blocked states, concurrency versus parallelism, preemption and lack of guaranteed execution order. A separate stack is not inaccessible to other threads through a pointer; object lifetime and synchronization still apply.

If the learner asks about a later stage, briefly confirm prerequisite understanding in one question; teach the requested stage rather than restarting the sequence.

Next teach creation and joining: pthread.h, opaque pthread_t identifiers, pthread_create parameter roles and worker callback, argument lifetime, worker return versus process exit, pthread_join completion/resource reclamation and error returns. Explain that pthread functions return error numbers directly rather than assuming errno, and explain -pthread. Only then introduce shared-data synchronization with one producer writing an ordinary payload and one consumer reading it once. State initialization-before-thread-start and no-further-writes assumptions.

Define thread as an independently executing instruction sequence, atomicity as indivisible access, and synchronization as the required ordering between threads before introducing the API names. A C data race involves conflicting accesses in different threads, at least one non-atomic, with neither ordered before the other by the language's happens-before relation; simultaneous wall-clock execution is not required. Explain undefined behaviour rather than promising a random value or a crash.

Use all-atomic relaxed examples when illustrating permitted weak outcomes; do not describe stale reads as the guaranteed alternatives of a racy ordinary C payload. Distinguish indivisible access from ordering across objects, memory-order acquire/release from lock ownership, and the seq_cst defaults from explicitly weaker orders. Do not claim a weak outcome must occur or infer a particular instruction from source alone.

Use a worked write -> release store -> acquire load observing that store -> read trace. Explain that release/acquire safely publishes earlier writes; it does not lock the payload, flush all memory, guarantee scheduling or protect a later ordinary write. Show the later-write failure explicitly. Introduce a mutex (exclusive access while held) or an acknowledgement before teaching repeated buffer reuse; defer protocol details until the one-time example is understood. Do not substitute unexplained barrier jargon for a mechanism.

Distinguish busy-waiting on an atomic readiness flag from a spinlock: the readiness flag does not acquire exclusive ownership; a spinlock repeatedly attempts exclusive acquisition. For mutex/condition-variable completion, explain the same-mutex rule for shared state, the predicate loop, atomic mutex release-and-begin-wait, reacquisition before returning, spurious wake-ups and notifications not being stored. A signal does not hand over mutex ownership. Later cover monotonic deadlines, READY/TIMEOUT/ERROR outcomes and late-callback/shutdown lifetime under the week-2 plan. Keep files empty and explain full programs only in chat under the shared workflow.

## Allocation And Integer Safety

For `realloc`, separate nonzero-size failure (old allocation survives) from success (old lifetime ends even at the same numerical address). Distinguish an element index from a snapshot of its value. A slot generation is application metadata for logical replacement, not a C facility or a count of reallocations; define the removal/reuse policy before showing code.

For allocation and parser guards, track units (bytes versus element counts), actual operand types, promotions and representable limits. Derive subtraction/division checks before risky addition/multiplication. Distinguish arithmetic validity from allocation success and numerical bounds from object validity. Use explicit small-number traces before large hexadecimal values. Unsigned wrap rules apply to the expression's actual unsigned type; do not ignore promotions or claim signed overflow wraps.

## Assessment When Resumed

Use one bounded scenario at a time across lifetime/allocation, representation/aliasing/alignment, integer safety, concurrency/publication and API/callback ownership. Grade explanation, failure mechanism, repair and verification separately using the shared rubric. Known gaps guide lessons now; do not demand another assessment round before teaching. Broader placement needs several domains and practical evidence, not experience years or isolated chat errors.

## Challenge Rotation

Draw the bytes; find the first invalid access; repair one allocation failure path; reverse a list; prove ring-buffer full/empty rules; replace manual cleanup with RAII; diagnose a dangling callback; explain array versus linked-list cost. Complexity and tests are part of the answer.

For selected code, inspect the exact declaration, definition and callers before explaining it. Prefer [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) for design rationale and the applicable language standard for exact rules; use [SEI CERT C](https://wiki.sei.cmu.edu/confluence/display/c) for targeted safety guidance. Explain any intentional difference between an interview exercise and production practice.