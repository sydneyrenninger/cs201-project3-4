# Nota Hospital ER Simulation (CS 201 Project 3)

A C++17 / SFML simulation of the Nota Hospital emergency room. Patients arrive with a
condition level from 0 (healthy) to 10 (critical), wait in a triage priority queue, and are
treated by doctors who work set shifts and have different skill levels. Patients and doctors
are stored in a B-Tree that we wrote ourselves.

**Team:** Sameeksha Sharma, Sydney Renninger, Long Huynh, Anh Thu Nguyen

- UML class diagram: [UML.md](UML.md)
- Data: [data/doctors.csv](data/doctors.csv), [data/patients.csv](data/patients.csv)

---

## Building and running

You need a C++17 compiler and `make`. Only the GUI needs **SFML** (2.5+ or 3.x).

| Command | What it does |
| --- | --- |
| `make` | builds the SFML GUI (`./hospital`) |
| `make run` | builds and runs the GUI |
| `make cli` | builds the console version (`./hospital_cli`), no SFML needed |
| `make test` | builds and runs the test suite (`./hospital_tests`) |
| `make data` | generates the larger synthetic dataset (`data/doctors_large.csv`, `data/patients_large.csv`) |
| `make run-large` | generates that dataset if needed and runs the GUI on it |
| `make clean` | removes all build output |

Run the programs from the project folder so they can find `data/`.

Installing SFML:

- **Ubuntu/WSL:** `sudo apt install libsfml-dev`, then `make`
- **macOS:** `brew install sfml`, then `make` (the Makefile finds Homebrew automatically)
- **Windows (MSYS2):** `pacman -S mingw-w64-ucrt-x86_64-sfml`, then `make`
- **Somewhere else:** `make SFML_DIR=/path/to/SFML` (the folder that contains `include/` and `lib/`).
  On Windows the SFML `bin/*.dll` files must be on your `PATH` or next to `hospital.exe`.

The GUI uses a system font (Segoe UI/Arial on Windows, Arial/Helvetica on macOS,
DejaVu/Liberation/Noto on Linux). If none is found, copy any `.ttf` file to `assets/font.ttf`.

---

## 1. How does the simulation work?

### Time

Time only moves when the user presses a button. One press is one **tick**, which is 15
minutes of hospital time. The simulation starts on Day 1 at 06:00. Each tick,
`Hospital::step()` does these steps in order:

1. **Treatments progress.** Each busy doctor's remaining time goes down by one tick. When it
   reaches zero, the patient is **discharged** (condition 0) and the doctor is free.
2. **Shift changes** are logged as doctors come on or go off shift.
3. **Arrivals.** Every patient whose arrival time has come is admitted. Several patients can
   arrive in the same tick; the dataset has a car accident (4 patients at 08:30), a
   food-poisoning family (3 at 17:45) and a bar fight (2 at 23:15). An admitted patient is
   inserted into the **patient B-Tree** and pushed onto the **triage priority queue**.
4. **Critical check.** Every patient has a time limit in the data (e.g. 30 minutes for
   chest pain, 6 hours for a sore throat). A waiting patient who goes past it enters a
   **critical state**: their condition becomes 10 and they move to the front of the queue.
5. **Doctor assignment.** Every doctor who is free **and on shift** takes the next patient
   from the priority queue.

### Considerations from the assignment

| Requirement | How we handle it |
| --- | --- |
| Doctors have different skill levels and take different amounts of time | Skill 1-10. Treatment time = `1 + ceil(condition * (12 - skill) / 12)` ticks, so sicker patients take longer and better doctors work faster. A level 10 patient takes 45 min with a skill 10 doctor and 2 h 15 m with a skill 3 doctor. |
| Doctors have set shifts and are not always working | Each doctor has a shift start and end (overnight shifts such as 22:00-06:00 work too). Off-shift doctors do not take new patients. A doctor whose shift ends while treating someone finishes that patient first. |
| Patients may go critical if not cared for in time | Per-patient `time_limit_min` in the data; past it, the condition becomes 10 (see step 4). |
| Patients enter and leave with a condition level; some arrive together | Arrival day/time and condition come from `patients.csv`; patients leave when discharged. Group arrivals are logged ("4 patients arrived at the same time"). |
| We know a patient's condition before they are seen | The triage queue is ordered by condition as soon as they arrive. |
| B-Tree for patient and doctor data | `BTree<PersonKey, Patient*, 3>` and `BTree<PersonKey, Doctor*, 3>` (see section 2). |
| A data structure to decide who is seen next | `std::priority_queue` (a binary heap). See below. |

### Data structures

- **B-Tree (our own, `include/BTree.h`).** Key = `PersonKey` (name, age, date of birth). The
  assignment says date of birth is unique for each (name, age) pair, so this key is unique
  even though our data has two different "Sam Lee, 34" patients and two "Maria Garcia, 28"
  patients. Names compare without case. Besides `insert` and `search` we added:
  - `rangeSearch(lo, hi)`: an in-order walk that skips subtrees outside the range. The
    lookup box uses it to find everyone whose name starts with what you typed.
  - `forEachNodeByLevel`: used by the GUI to draw the tree.
  - `validate`: checks every B-Tree rule; used by the tests after every insert.
- **Priority queue (`std::priority_queue<TriageEntry>`).** The highest condition is popped
  first. Ties go to whoever arrived first, then to whoever was queued first. A binary heap
  gives O(log n) push and pop. The heap cannot change a priority in place, so when a patient
  becomes critical we push a new entry with level 10. The old entry stays in the heap but is
  "stale" (its saved level no longer matches the patient), and it is skipped when popped.
- **`std::vector` of `unique_ptr`** owns all patients (sorted by arrival time, so admitting
  arrivals just moves an index forward) and all doctors. The trees and the queue hold
  non-owning pointers.
- **`std::deque`** holds the event log, capped at 500 lines.

### Example (from the real dataset)

1. **08:30** - A car accident brings in 4 patients at once: Marcus Johnson (level 9,
   20 min limit), Derek Simmons (7, 45 min), Kayla Johnson (6, 60 min) and Nina Petrova
   (5, 90 min). Only Dr. Patel and Dr. Kim are on shift, and both are busy.
2. **08:45** - Dr. Patel (skill 9) frees up and takes the top of the queue: **Marcus**
   (level 9), about 1 hour of care.
3. **09:00** - Dr. Kim (skill 4) frees up and takes **Derek** (level 7). Because Dr. Kim is
   less skilled, this takes about 1 h 30 m.
4. **09:30** - Kayla has waited her full 60 minutes. She becomes **critical (level 10)**,
   turns red in the waiting room, and moves above Nina and everyone else.
5. **09:45** - Dr. Patel finishes with Marcus and takes Kayla next.

Over the whole run (Day 1 06:00 to Day 2 06:00), all 54 patients are discharged. Six go
critical while waiting, mostly during the morning rush, and the average wait is 27 minutes.

### Visual component (SFML)

![The simulation at Day 1 09:30, right after the car accident](docs/screenshot.png)

The window has five areas:

- **Header:** the clock, live statistics (waiting, in treatment, discharged, went critical,
  average wait, doctors on shift) and the buttons **Next tick [Space]**, **+1 hour [H]** and
  **Auto [A]** (one tick every half second by default; **Up/Down** make auto-play faster or
  slower, from 0.1 to 2.0 seconds per tick, and the button shows the current speed).
- **Waiting room:** waiting patients in the exact order they will be seen. Each has a
  condition badge colored from **green (0)** through amber to **red (10)**, plus how long
  they have waited and how long until they go critical. Critical rows are highlighted red.
- **Doctors:** every doctor's skill (as a bar), shift and status: *Treating X (time left)*,
  *Available* or *Off shift*.
- **Event log:** arrivals, treatments, discharges, shift changes and critical alerts, color
  coded and newest first.
- **Right panel, two tabs (Tab key):**
  - **Lookup:** type a name prefix (e.g. `Sam`) to range-search both B-Trees, or a full key
    (`Sam Lee, 34, 1991-12-30`) for an exact B-Tree search. The search shows how many tree
    nodes it visited. Each result card shows the person's live status.
  - **B-Trees:** a live drawing of the doctor and patient B-Trees, level by level, with node
    and height counts. You can watch nodes split as patients arrive, and keys that match the
    last lookup are highlighted.

A console version (`make cli`) runs the same engine with text commands, for machines
without SFML.

---

## 2. What value of t did we choose for the B-Tree, and why?

We chose **t = 3** (the minimum degree), so each node holds **2 to 5 keys** and has
**3 to 6 children**. (`Hospital::kBTreeDegree`; the tree itself is a template, so `t` can be
changed in one place.)

Why:

- **The tree lives in memory, not on disk.** Large `t` values (hundreds) are chosen when
  each node is a disk block, so that one disk read brings in many keys. We have no disk
  reads, so that benefit doesn't apply. Each node visit costs a small binary search, and
  inserting into a node shifts up to `2t-1` keys, so a small `t` keeps node work cheap.
- **The height is still small.** With `n` keys the height is at most `log_t((n+1)/2) + 1`.
  Our 54 patients fit in a tree of height 3 (15 nodes), and even 10,000 patients would need
  at most 9 levels.
- **It shows the B-Tree working.** With t = 3, nodes fill up and split during a normal run,
  and the tree grows from 1 to 3 levels over the day. The B-Trees tab shows that happening.
  With a large `t`, the whole dataset would sit in a single root node and look like a sorted
  array.
- **It is more than the minimum.** t = 2 (a 2-3-4 tree) also works, but it splits more
  often and makes the tree taller. t = 3 is a good middle ground.

Our tests (`make test`) insert 2,000 shuffled keys for t = 2, 3 and 5. After every insert
they check every B-Tree rule, and then check search, range search and the height bound.

---

## 3. What was each team member's contribution?


| Member | Contribution |
| --- | --- |
| Sameeksha Sharma |"B-Tree template (BTree.h): insert, search, range search, validation|
| Sydney Renninger |Writing report, compiling and merging features|
| Long Huynh |SFML GUI (Gui.cpp), dataset and data generator, auto-play speed control|
| Anh Thu Nguyen |Testing application and creating UML diagram|

---

## Project layout

```text
include/   headers: BTree.h (template), PersonKey, Person, Patient, Doctor,
           Hospital, DataLoader, SimTime, Gui
src/       implementation, main.cpp (GUI) and cli_main.cpp (console)
tests/     test_main.cpp
tools/     generate_data.cpp (synthetic data generator)
data/      doctors.csv, patients.csv (our own dataset, made up for this project),
           doctors_large.csv, patients_large.csv (generated, see below)
Makefile
UML.md     class diagram
```

### Data format

`doctors.csv`: `name,age,dob,skill,shift_start,shift_end`
`patients.csv`: `name,age,dob,condition,ailment,arrival_day,arrival_time,time_limit_min`

Dates are `YYYY-MM-DD`; times are `HH:MM`. Lines that start with `#` are comments. All
names and records are fictional and were created by the team for this project.

### Generating more data

`make data` builds `tools/generate_data.cpp` and writes a larger made-up dataset. You can
also run it yourself:

```text
./generate_data [seed] [doctors] [patients] [days] [out_dir]
```

The defaults are seed 201, 16 doctors, 400 patients, 3 days, and `data/`. The same seed always
gives the same files. Shifts are spread around the clock, most patients are mild and a few
are severe, sicker patients get shorter time limits, some patients arrive in groups, and some
share a name and age with a different date of birth. Run the GUI on it with
`make run-large`, or `./hospital data/doctors_large.csv data/patients_large.csv`. With the
default settings all 400 patients are discharged by Day 4 06:00 and 12 go critical (average
wait 35 minutes), so the waiting room fills up and the priority queue has real work to do.
