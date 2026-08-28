# PROJECT GO / NO-GO AUDIT REPORT
### C++ + DSA PBL — Surplus Food Recipient Recommendation System
**Evaluator Role:** Independent, skeptical PBL auditor
**Date:** August 27, 2026
**Status:** FINAL

---

## A. EXECUTIVE VERDICT

> **MODIFY**

The core idea is **not weak** — but in its current form it is **insufficiently structured as a DSA project**. The real-world problem is genuine, the gap is meaningful, and the C++ integration is naturally strong. The critical failure is that the current algorithm (Filter → Score → Sort → Recommend) is essentially **an array sort dressed in a social cause**. That is not enough for a dedicated Data Structures course PBL. However, the domain is rich enough that a specific, targeted modification — adding a Priority Queue for donation urgency management and a Hash Map for O(1) recipient lookup — transforms this from a passable C++ project into a legitimate dual-subject PBL without artificially padding it. The modification is surgical, not a reinvention. Go forward with the modified version described in Section G.

---

## B. SCORECARD

| Criterion | Weight | Raw Score (/10) | Weighted Score |
|---|---|---|---|
| Real-world problem strength | 15% | 8 | 1.20 |
| Evidence that problem genuinely exists | 10% | 7 | 0.70 |
| Quality of identified gap | 15% | 6 | 0.90 |
| Originality / differentiation | 10% | 5 | 0.50 |
| C++ syllabus integration | 15% | 8 | 1.20 |
| DSA syllabus integration | 20% | 5 | 1.00 |
| Ability to demonstrate concepts technically | 10% | 7 | 0.70 |
| Feasibility for first-year students | 5% | 8 | 0.40 |
| **TOTAL** | **100%** | — | **6.60 / 10** |

**Separate Ratings:**

| Dimension | Rating | Comment |
|---|---|---|
| Viva defensibility | 7/10 | Strong if you know *why* each structure was chosen |
| Scope risk | Low | Console-based, bounded dataset, no external deps |
| Research risk | Low | Problem is well-documented; no novel claim required |
| Risk of appearing generic | **Medium-High** | Easily looks like "FoodDonation class + sort" without deliberate framing |
| Risk of forcing syllabus concepts | Medium | Graphs and Trees have no natural fit; do NOT include them |

**Threshold verdict at current score (6.60): MODIFY — not strong enough as-is for DSA PBL.**

---

## C. PROBLEM VALIDATION

### Is the problem genuinely real?

**Yes, with important qualifications.**

#### CONFIRMED (direct evidence found)
- India wastes an estimated **68–80 million tonnes of food annually** (multiple sources including UNEP Food Waste Index data, cited by Drishti IAS, FSSAI).
- A wedding with ~1,000 guests can generate **200–500 kg of surplus food in a single evening** (corroborated by food recovery organizations and hospitality industry reports).
- NGO coordination of large food donations currently relies **heavily on manual processes**: phone calls, WhatsApp, local volunteer knowledge (confirmed by research in ijirt.org and scribd.com).
- The "last-mile" logistics problem — matching surplus food to recipients before spoilage — is documented in academic literature (Wageningen University, ResearchGate).
- The **capacity mismatch problem** (a recipient's capacity being insufficient for the donated quantity) is identified as a documented challenge in food bank and food rescue literature.
- FSSAI has formally acknowledged the coordination problem and launched the Indian Food Sharing Alliance (IFSA) explicitly to address it.

#### OBSERVED (pattern supported by multiple credible sources, not a direct data point)
- Large single-event donations (marriages, corporate events) frequently exceed the capacity of a single nearby NGO. This is observed in practitioner descriptions, though no published dataset quantifies exactly how often a coordinator must choose between multiple NGOs for one donation.
- Volunteer coordinators at organizations like Robin Hood Army operate by **community mapping and local knowledge**, not by a ranked, scored system. The recipient selection step is genuinely informal.

#### HYPOTHESIS (plausible but not directly confirmed)
- That a **coordinator regularly faces exactly 3–5 feasible NGO candidates** and must pick one within a 30–60 minute decision window. The specific decision-window pressure is inferred from food perishability constraints, not a published operational figure.
- That coordinators would actually use a software recommendation over their own judgment. **This is the weakest assumption in the entire idea.**

#### What remains uncertain
- Whether coordinators, who often have deeply personal knowledge of recipients, would trust or use an algorithmic recommendation. **This is a genuine validity risk you must be prepared to address in your viva.**
- Whether the scenario (one large donation, multiple NGOs, coordinate picks one) is common enough to justify a dedicated tool, or whether it is a niche edge case.

---

## D. EXISTING SOLUTION GAP

### Comparison of existing systems against your proposed gap

| System | Does it rank recipients? | Uses distance? | Uses capacity? | Uses time pressure? | Automated multi-criteria match? | Solves your scenario? |
|---|---|---|---|---|---|---|
| **FoodRescueMap.in** | ❌ No ranking — self-service discovery | ✅ GPS proximity for display | ❌ Not visible | ✅ Auto-expiry only | ❌ No algorithm | ❌ No |
| **Robin Hood Army** | ❌ Manual, relationship-based | ❌ Informal | ❌ Informal | ❌ Informal | ❌ None | ❌ No |
| **No Food Waste** | ❌ Nearest hunger spot routing | ✅ Proximity | ❌ No | ✅ Alerts for freshness | Partial — logistics layer, not scored ranking | ❌ No |
| **Feeding India / Zomato** | ❌ 24/7 helpline model | ❌ | ❌ | ❌ | ❌ Manual | ❌ No |
| **FDRM-CA (Academic)** | ✅ Yes — preference-based matching | ✅ "Vicinity" factor | Partial | ✅ Chronological priority | ✅ Game-theoretic | **Closest overlap** |
| **Copia / MealConnect (International)** | ✅ Yes | ✅ | ✅ | ✅ | ✅ Full matching | ✅ Yes — but not Indian, not open, not C++ |

### Assessment of the gap

**The gap is real in the Indian grassroots context, but it is not the gap you have articulated it as.**

The precise gap is this: **Indian grassroots NGO coordination platforms provide visibility (who has food, where) but do not provide decision support (which recipient, and why, given current logistics).** FoodRescueMap shows pins on a map. It does not tell a coordinator "NGO A is closest but NGO B is a better fit because it can absorb the full quantity." That specific explainable recommendation does not exist in any deployed Indian system I could verify.

**The overlap concern with FDRM-CA:** FDRM-CA uses game theory and real-time deferred acceptance, not a weighted scoring function. Your approach is a simplified, explainable, single-donation decision support tool. These are different in both mechanism and scale. You are NOT reimplementing FDRM-CA. A strict professor will ask about this — you need to be ready with a clean answer: "FDRM-CA solves a multi-agent, multi-round matching problem. Our system solves a single-coordinator, single-donation, multi-criteria ranking problem. These are algorithmically distinct."

**Gap quality rating: 6/10.** It exists. It is not trivial. But it is narrow, and the counter-argument — "a coordinator already knows their NGO partners personally and doesn't need software to pick one" — is the strongest challenge to your problem statement. You must have a prepared rebuttal.

---

## E. PBL FIT

### C++ Syllabus Integration

**Rating: 8/10 — Genuinely strong if executed properly.**

The domain naturally produces C++ OOP opportunities:

- `FoodDonation` class with constructors, destructors, and data encapsulation is **not artificial** — the domain requires modelling a donation as an object with quantity, time, source, and urgency.
- `Recipient` class with capacity, distance, travel time, pickup availability — all naturally encapsulated.
- `Coordinator` as a class that holds a collection of recipients and operates on a donation — opens the door to **friend functions, operator overloading** (e.g., comparing recipients by score).
- File handling is **completely natural**: loading recipient profiles from a file, saving decision logs. This is the most real integration because coordinators genuinely need persistent records.
- Exception handling fits: what happens when a donation's quantity exceeds all combined recipient capacity? What happens with invalid input (negative distances)?
- STL: `vector` for recipient list, `priority_queue` from STL for donation urgency queue, `map` for recipient lookup — all genuinely motivated.
- Templates: a `Scorer<T>` template or a `RankedList<T>` could be demonstrated.

**The risk:** If you create `FoodDonation`, `Recipient`, `Volunteer` classes with only basic getters/setters and no meaningful OOP reasoning (no polymorphism, no inheritance justified by the domain), the evaluator will correctly identify this as "trivial class wrapping."

**The mitigation:** Inheritance is *marginally* justified — a `Recipient` base class with derived classes like `ShelterhomeRecipient` (fixed meal times), `OrphanageRecipient` (age-appropriate food filter), `CommunityKitchenRecipient` (bulk capacity) — but only if the derived behaviour actually differs in the algorithm. Do NOT do this unless the behaviour genuinely differs. If all recipients score the same way, a base class with no meaningful overrides is padding.

### DSA Syllabus Integration

**Rating: 5/10 — THIS IS THE PROBLEM.**

Your current core algorithm is:

```
Filter (linear scan) → Score (arithmetic) → Sort (array sort) → Output
```

**This is Unit 4 of the DSA syllabus: "Sorting." That is essentially it.**

A strict DSA evaluator will say: *"You wrote a bubble sort on a struct array and built a story around it. How is this a Data Structures project?"*

The filter step is sequential search — Unit 3. The score step has no DS involvement. The sort step is Unit 4. You have touched exactly 2 of 5 DSA units. The following units have **zero natural integration** in your current design:

- **Unit 2 (Queues, Linked Lists):** Not used.
- **Unit 3 (Trees, Hashing, Searching):** Hashing is not used. Binary search is irrelevant to an unsorted recipient list.
- **Unit 5 (File Structures, Graphs):** Graphs have no natural fit. File structures are partially relevant.

**This is the critical modification that must happen.** See Section G for the fix.

### Subject Integration (both subjects in one project)

**Rating: 7/10 with modification, 5/10 as-is.**

C++ and DSA do intersect in the project — a `priority_queue<FoodDonation>` from STL is simultaneously a C++ STL topic AND a DSA data structure topic. File handling in C++ with structured record I/O touches both subjects. The key is ensuring the DSA structures are **implemented manually** (not just called from STL) so you can demonstrate understanding of the underlying mechanism.

### Working Demonstration

**Rating: 9/10 — Excellent.**

This is the project's greatest strength. The demo scenario is concrete and vivid:
1. Load recipient profiles from file.
2. Enter donation (150 meals, 45 minutes remaining).
3. System filters infeasible recipients.
4. System scores and ranks feasible recipients.
5. System prints ranked list with explicit explanation.
6. Coordinator selects one.
7. Decision is logged to file.

This runs cleanly in a terminal with zero external dependencies. No installation issues. No network calls. No UI failures. Every phase evaluator (TA1 through TA3) can see a working system.

### Technical Understanding / Viva

**Rating: 7/10 with modification.**

The concepts you use are explainable. You can justify every decision. The risk areas are:
- Justifying why you chose **this** sort algorithm (use QuickSort or a custom comparator — not just `std::sort` with no explanation).
- Justifying the scoring function weights (you need a reasoned argument, not arbitrary numbers).
- Defending against "isn't this just a weighted average?" — because it is. Your defense is that the **structure** (Priority Queue for intake, Hash Map for lookup, Sort for ranking, File for persistence) is the learning vehicle, not the arithmetic.

---

## F. CONCEPT-BY-CONCEPT RECOMMENDATION

### C++ Concepts

| Concept | Natural? | Importance | Where It Fits | Keep/Remove |
|---|---|---|---|---|
| Classes & Objects | ✅ Strongly | High | `FoodDonation`, `Recipient`, `DonationCoordinator` | **KEEP** |
| Constructors / Destructors | ✅ Strongly | High | Donation object initialization; cleanup of dynamic recipient arrays | **KEEP** |
| Encapsulation / Data Hiding | ✅ Strongly | High | Recipient capacity, score are private; only exposed via methods | **KEEP** |
| Static data members | ✅ Moderately | Medium | Static counter for total donations processed | **KEEP** |
| Friend functions | ⚠️ Marginal | Low-Medium | Could justify for a `compare()` between two recipients | **KEEP IF JUSTIFIED** |
| Operator overloading | ✅ Moderately | Medium | `operator<` for Recipient to enable sort | **KEEP** |
| Copy constructor / this pointer | ⚠️ Marginal | Medium | Demonstrated through recipient list copying | **KEEP as DEMONSTRATION** |
| Arrays of objects | ✅ Strongly | High | Array of Recipient objects is the core data store | **KEEP** |
| Pointers to objects | ✅ Moderately | Medium | Dynamic recipient array; pointer-based operations | **KEEP** |
| Inheritance | ⚠️ Conditional | Low-Medium | Only if recipient types genuinely differ in scoring behavior | **OPTIONAL — only if behavior differs** |
| Polymorphism / Virtual functions | ⚠️ Conditional | Low | Only if inheritance is justified | **OPTIONAL** |
| File handling | ✅ Strongly | High | Load recipients from file; log decisions | **KEEP** |
| Exception handling | ✅ Moderately | Medium | Invalid donation qty, no feasible recipients, file not found | **KEEP** |
| Templates | ⚠️ Marginal | Low-Medium | `RankedList<T>` generic ranked container | **OPTIONAL — good bonus** |
| STL: vector | ✅ Strongly | High | Dynamic recipient list | **KEEP** |
| STL: priority_queue | ✅ Strongly | High | Donation urgency queue | **KEEP (see Section G)** |
| STL: map / unordered_map | ✅ Moderately | Medium | Recipient lookup by name/ID | **KEEP** |

### DSA Concepts

| Concept | Natural? | Importance | Where It Fits | Keep/Remove |
|---|---|---|---|---|
| Arrays | ✅ Strongly | High | Recipient array; donation record | **KEEP** |
| Time-Space Complexity analysis | ✅ Strongly | High | Filter O(n), Sort O(n log n), Hash lookup O(1) — compare explicitly | **KEEP** |
| Sequential Search | ✅ Strongly | High | Filter step — scan all recipients for feasibility | **KEEP** |
| Binary Search | ⚠️ Conditional | Low | Only if recipients are pre-sorted by distance | **OPTIONAL — demonstrate as comparison** |
| Sorting (Quick/Merge) | ✅ Strongly | High | Rank feasible recipients by score | **KEEP — implement manually** |
| **Priority Queue** | ✅ Strongly | **High** | **Multiple pending donations → process most urgent first** | **KEEP — THIS IS THE MISSING PIECE** |
| Hashing / Hash Table | ✅ Moderately | Medium | O(1) lookup of recipient by NGO ID | **KEEP — implement basic hash table** |
| Stack | ❌ Weak | Low | No natural fit | **REMOVE** |
| Linked List | ❌ Weak | Low | Vector of objects suffices; forced list adds no insight | **REMOVE** |
| Queue (regular FIFO) | ⚠️ Conditional | Low | Could model FCFS baseline to contrast with Priority Queue | **OPTIONAL — demonstration only** |
| Trees (BST, AVL) | ❌ Very weak | Very Low | No natural fit for this problem | **REMOVE** |
| Graphs (BFS/DFS) | ❌ Very weak | Very Low | Road network is real but requires maps/GPS; without it, graph is fictional | **REMOVE** |
| Heap | ✅ Moderately | Medium | Priority Queue implementation underneath | **KEEP as underlying structure** |
| File Structures | ✅ Strongly | High | Sequential file of recipient profiles; decision log | **KEEP** |
| Recursion | ⚠️ Marginal | Low | QuickSort recursive implementation | **KEEP as implementation detail** |

---

## G. BEST PROJECT VERSION

### Exact Problem Statement

> **"A food rescue coordinator receives a time-sensitive donation from a large event. Multiple NGO recipients are registered in the system. The coordinator needs to know: which recipient is the most logistically suitable, given distance, available capacity, and the remaining usable time of the food — and why?"**

**Secondary problem (DSA addition):**

> **"When multiple donations arrive simultaneously or in rapid succession from different events, the coordinator needs to process the most time-critical donation first — not the most recently arrived."**

This second problem naturally introduces the Priority Queue.

---

### Exact Solution

A **console-based C++ decision-support prototype** with two operational modes:

**Mode 1 — Single Donation Mode**
- Input: one donation (quantity, remaining usable time, source).
- Action: Filter recipients → Score → QuickSort → Print ranked list with explanation.
- Output: Ranked recommendation with per-recipient breakdown.

**Mode 2 — Queue Mode (the DSA upgrade)**
- Input: multiple donations entered in any order.
- Action: Insert all into a Priority Queue keyed on urgency (lower remaining time = higher priority).
- Process each donation in urgency order through Mode 1.
- Output: Processed in urgency-first order, not arrival order.

This single addition changes the project from "sorting problem in food clothing" to a genuine multi-structure DSA demonstration.

---

### Core Modules

| Module | What it does | Key concepts |
|---|---|---|
| `Recipient` class | Stores NGO data: name, ID, distance, capacity, travel time, pickup flag | Encapsulation, constructors, operator overloading |
| `FoodDonation` class | Stores donation: quantity, remaining time, source, urgency score | Class, static counter |
| `RecipientRegistry` class | Loads recipients from file; provides O(1) lookup via hash table | File handling, hashing |
| `FeasibilityFilter` | Scans recipient array, removes infeasible candidates | Sequential search, O(n) |
| `Scorer` | Computes weighted score per recipient | Arithmetic, configurable weights |
| `QuickSorter` | Sorts feasible recipients by score (implemented manually) | QuickSort, recursion, O(n log n) |
| `DonationQueue` | Priority queue of pending donations ordered by urgency | Priority Queue / Heap |
| `DecisionLogger` | Appends decision records to a log file | File handling, exception handling |
| `main.cpp` | Menu-driven coordinator interface | STL, exception handling |

---

### Core Algorithm

```
DONATION INTAKE:
  Insert FoodDonation into PriorityQueue (key: remaining_usable_time)

PROCESS LOOP (while queue not empty):
  donation ← PriorityQueue.extractMax()  // most urgent first
  
  FILTER:
    For each recipient r in RecipientRegistry:
      if r.capacity < donation.quantity: discard
      if r.travel_time > donation.remaining_time: discard
      if r.pickup_available == false: discard
    → feasible_list[]
  
  If feasible_list is empty:
    Output: "No feasible recipient. Manual override required."
    Log to file.
    Continue.
  
  SCORE:
    For each r in feasible_list:
      r.score = w1*(1/distance) + w2*(capacity/donation.quantity) + w3*(time_slack)
  
  SORT (QuickSort on score):
    feasible_list sorted descending by score
  
  OUTPUT:
    Print ranked list with explanation per recipient.
    Coordinator selects (manual final step).
    Log selection to file (RecipientRegistry.lookup(id) → O(1)).
```

**Complexity analysis you must present explicitly:**
- Filter: O(n) — sequential scan
- Score: O(k) where k ≤ n — feasible subset
- QuickSort: O(k log k) average — demonstrate analysis
- Hash lookup for logging: O(1) average
- Priority Queue insert/extract: O(log m) where m = pending donations

---

### Core C++ Concepts Used
- Classes, constructors, destructors, encapsulation
- Operator overloading (`<` for recipient comparison)
- Arrays of objects + dynamic allocation
- File I/O (loading recipient data, logging decisions)
- Exception handling (empty queue, no feasible recipient, file not found)
- STL: `priority_queue`, `vector`, `unordered_map`
- Templates: optional — `RankedList<T>` for generic use

### Core DSA Concepts Used
- Arrays: recipient storage
- Sequential search: filter step
- Priority Queue (Heap): donation urgency processing
- QuickSort (manual): recipient ranking
- Hashing: recipient lookup by ID
- Complexity analysis: O(n) filter vs O(1) lookup vs O(k log k) sort

### What Should NOT Be Included

| Element | Reason to exclude |
|---|---|
| Graphs / road network | Requires real map data; without it, the graph is fictional and undemonstrable |
| BST / AVL trees | No natural fit; forced insertion |
| Linked lists | Vector of objects is the right structure; linked list adds complexity without insight |
| Inheritance hierarchy deeper than 1 level | Unjustified by domain unless recipient types genuinely behave differently |
| ML / optimization for weights | Out of scope; arbitrary weights are fine if you justify the *rationale* |
| GUI or web interface | Distracts from DSA; console is sufficient and stable |
| Real GPS distances | Requires APIs; use Euclidean/manually entered distance |
| Multiple donation splits | Out of scope for MVP; adds algorithmic complexity without proportional learning gain |

---

## H. ALTERNATIVE DIRECTIONS

Only alternatives that genuinely outperform the current concept are listed. None do so cleanly enough to recommend abandoning the current domain.

### Alternative 1: Donation Triage Station (Priority Queue-First Design)

**Core shift:** Instead of recipient recommendation being the core, make **donation urgency triage** the core. A coordinator receives dozens of small donations per day from different sources. The system manages which to process first, maintains an active queue, and produces a pick-up schedule.

**Stronger DSA integration:** Priority Queue is the *primary* structure, not secondary. Also uses circular queue for volunteer assignment rotation.

**Weaker:** The recipient-selection problem (the interesting multi-criteria part) disappears or becomes trivial.

**Verdict:** This is a stronger DSA project but a weaker C++ OOP project. Net: roughly equal, not clearly better.

### Alternative 2: Recipient Registry with Multi-Criteria Search

**Core shift:** Focus entirely on efficiently maintaining and querying a registry of NGO recipients. Use BST for sorted access by distance. Use hash table for O(1) ID lookup. Use a min-heap for "find nearest with capacity."

**Stronger DSA integration:** BST + Hashing + Heap used simultaneously.

**Weaker:** The problem narrative becomes less compelling. "Build a better NGO database" is not as vivid as "recommend who gets this 150-meal donation right now."

**Verdict:** Stronger DSA integration, weaker problem motivation. Worth partially merging (hash table for registry lookup) but not worth switching to as the primary framing.

### Alternative 3: Full Pipeline — Triage + Recommend + Log

This is essentially the modified version described in Section G. It is the strongest version that keeps both subjects integrated without forcing anything.

**No other alternatives are genuinely stronger.** The food rescue domain is the right choice. The problem framing needs sharpening, not replacement.

---

## I. HARSH VIVA TEST

### The 10 Hardest Questions

---

**Q1: "Why does a coordinator need your software at all? Don't NGO workers already know their partner organizations personally?"**

*Why asked:* This attacks the core usefulness of the entire project.

*Strong answer:* "That's a valid challenge. Our system is designed not to replace human judgment but to augment it when scale and time pressure make informal knowledge unreliable. A coordinator managing 5 familiar NGOs can rely on memory. A coordinator who receives a 300-meal surplus from a new venue, with 3 unfamiliar NGOs in range and 40 minutes before the food spoils, benefits from an explicit, reproducible recommendation. Our system targets that specific high-pressure, low-familiarity scenario. We also note that our literature review shows capacity mismatch is a documented inefficiency in food rescue systems — the 'nearest' choice often fails because the nearest recipient cannot absorb the full volume."

*Weak answer:* "Because our algorithm is more accurate." — Undefendable without real comparison data.

---

**Q2: "Your scoring function is Score = w1×(1/distance) + w2×(capacity/quantity) + w3×time_slack. How did you choose w1, w2, w3? Are they arbitrary?"**

*Why asked:* The scoring function is the intellectual core of the recommendation. If the weights are arbitrary, the recommendation is meaningless.

*Strong answer:* "We chose equal weights (w1=w2=w3=1/3) for the MVP to avoid false precision. We acknowledge that in a real deployment, weights would be determined empirically by coordinators. Our system is designed to make the weights configurable — a coordinator can adjust them, run the model on historical decisions, and observe which weight setting best approximates past good decisions. The important point is that the structure of the multi-criteria comparison is explicit and auditable, unlike an informal mental calculation."

*Weak answer:* "We just used equal weights because it seemed fair." — Passable but invites follow-up.

---

**Q3: "You use a Priority Queue for donation urgency. Why not just sort all incoming donations by time and process them in order?"**

*Why asked:* Tests whether you understand when a Priority Queue is superior to a sorted array.

*Strong answer:* "Sorting all donations at the start requires knowing all donations upfront — a static batch. In the real scenario, donations arrive dynamically throughout the day. A Priority Queue supports dynamic insertion in O(log n) time, so a new urgent donation can be added and immediately positioned correctly without re-sorting the entire list. This is the classic case where a heap outperforms a sorted array for an online problem."

*Weak answer:* "Priority Queue seemed like the right data structure." — Fails immediately.

---

**Q4: "You claim to address the capacity mismatch problem. But what if the best-scoring recipient has only 80% of the required capacity? What does your system do?"**

*Why asked:* Tests whether you have considered edge cases in your own problem.

*Strong answer:* "Our current system handles this in two ways. First, the feasibility filter removes any recipient whose capacity is below a configurable threshold (default: must accept at least 70% of the donation). Second, the output explicitly states the coverage percentage, so the coordinator can make an informed decision — for example, choosing to split the donation between NGO A (80 meals) and NGO C (70 meals) as a manual decision. We have deliberately not automated the split-donation case because it introduces the Assignment Problem, which is outside our scope and would require LP or graph matching."

*Weak answer:* "We only consider recipients who can take all of it." — Acceptable but invites "what if nobody can?"

---

**Q5: "What is the time complexity of your filtering step, and can you prove it?"**

*Why asked:* DSA evaluators want to see complexity analysis, not just implementation.

*Strong answer:* "The filter step performs one pass through all n recipients, checking three conditions per recipient. Each condition check is O(1). Therefore the filter is O(n). In the worst case, all recipients are feasible, and sorting follows at O(k log k) ≈ O(n log n). In the best case for filtering, all recipients are eliminated immediately after the scan — still O(n). The total pipeline for one donation is O(n + k log k) = O(n log n) worst case."

*Weak answer:* "It goes through all the recipients, so it's linear." — Correct but demonstrates shallow understanding.

---

**Q6: "Why did you implement QuickSort manually instead of using std::sort()?"**

*Why asked:* Tests whether you understand the difference between using a library and understanding an algorithm.

*Strong answer:* "std::sort() is correct and we used it in our STL demonstration. However, the DSA course requires demonstrating algorithmic understanding, not just API usage. We implemented QuickSort manually to demonstrate the divide-and-conquer paradigm, the role of the pivot, and the average-case O(n log n) vs worst-case O(n²) analysis. The manual implementation also lets us step through the sort during the demonstration, which is not possible with std::sort()."

---

**Q7: "Is your project essentially the same as FDRM-CA? How is it different?"**

*Why asked:* Academic overlap is a legitimate evaluation concern.

*Strong answer:* "FDRM-CA (2021, Applied Sciences) solves a fundamentally different problem: multi-round, multi-agent matching between many donors and many receivers simultaneously, using game-theoretic deferred acceptance with chronological tie-breaking. It is designed for a platform that brokers between many parties in real time. Our system solves a single-coordinator, single-donation, multi-criteria ranking problem — essentially, a decision support tool for one human making one decision under time pressure. The algorithm is a weighted scoring function with QuickSort, not deferred acceptance. The scope, mechanism, and implementation are all different."

---

**Q8: "Your hash table gives O(1) lookup. But you have only ~10-20 recipients. The benefit of O(1) vs O(n) for n=15 is negligible. Why use hashing at all?"**

*Why asked:* Tests whether you understand when data structures are appropriate vs when they are forced.

*Strong answer:* "That is a fair challenge, and we want to be transparent about it. At n=15, hashing vs linear search provides no practical performance benefit. We use it for two reasons. First, it demonstrates the data structure — understanding that hashing decouples lookup time from collection size is a fundamental DSA concept regardless of n. Second, if the system were to scale to a city-level registry with hundreds of NGOs, the hash table becomes meaningful. We present it explicitly as a scalability demonstration, not a performance fix for our MVP."

*Weak answer:* "Hashing is faster." — The professor will immediately ask "faster by how much for n=15?"

---

**Q9: "What happens when the donation expires before any recipient can be contacted? Does your system handle failure cases?"**

*Why asked:* Tests robustness and exception handling.

*Strong answer:* "Yes. We define 'infeasible due to time' as travel_time > remaining_usable_time. If all candidates are filtered out — either because the quantity exceeds all capacities or the time window is too narrow — the system outputs a specific warning: 'No feasible recipient identified. Manual override required.' This is logged to file with the donation details and timestamp for post-event analysis. We handle this as an exception in C++ — a custom `NoFeasibleRecipientException` is thrown by the filter and caught at the coordinator level."

---

**Q10: "You said the system 'recommends' but the human decides. If the human can override the recommendation at any time, what value does the recommendation actually add?"**

*Why asked:* This is the hardest philosophical challenge. It attacks the value proposition directly.

*Strong answer:* "Decision support systems do not need to replace human judgment to add value — they need to make human judgment faster, more consistent, and more defensible. Consider: a coordinator who picks NGO B every time by instinct may not realize that NGO C consistently has faster pickup times. Our system makes the comparison explicit and reproducible. Second, in high-pressure situations — 40 minutes, three phone calls to make — having a ranked list reduces cognitive load. Third, the logged decisions allow retrospective analysis: 'Did we always pick the top-ranked recipient? If not, why not? Should the weights be adjusted?' That feedback loop is not possible without a structured system. The recommendation is not a replacement for judgment — it is a structured input to judgment."

---

## J. FINAL GO / NO-GO DECISION

---

> ### DECISION: MODIFY

---

> ### "Would you personally approve this as a first-year C++ + DSA PBL proposal under the evaluation system described above?"

> ## YES, WITH CONDITIONS

**Explanation:**

The problem is real, the domain is defensible, and the C++ integration is genuinely strong. The project idea fails the DSA bar in its current form — Filter → Score → Sort is not a Data Structures project, it is an algorithms exercise disguised as one. However, the modification is precise and achievable: adding a Priority Queue for donation urgency and a Hash Table for recipient lookup transforms the data structure coverage from 2/5 syllabus units to 4/5, without forcing anything unnatural. The implementation complexity remains appropriate for first-year students.

The conditions for approval are:
1. **Priority Queue is added** and demonstrated as an operational component, not just defined.
2. **QuickSort is implemented manually** (at minimum, the core partition function) — not just `std::sort()`.
3. **The hash table is implemented manually** (at least a basic version) and its O(1) lookup property is explicitly demonstrated and analyzed.
4. **Complexity analysis** (Big-O for filter, sort, lookup, and heap operations) is prepared and rehearseable for viva.
5. **The project is not called** a "food management system," a "donation management system," or a "food rescue app." It must be framed explicitly as a **decision support prototype** with a specific problem statement (Section G above).
6. **The viva preparation** must include rehearsed answers to at minimum Q1, Q2, Q3, Q7, and Q10 above — these are the most likely questions and the most dangerous if answered poorly.

If those six conditions are met, this is a **strong, defensible first-year PBL** with a real problem, clean technical integration, and a vivid working demonstration.

---

*End of Report*
