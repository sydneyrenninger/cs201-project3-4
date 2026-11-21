# UML Class Diagram

GitHub renders the diagram below automatically. (`+` public, `-` private, `#` protected.)
A picture of the same diagram, for viewers that do not render Mermaid, is in [docs/UML.png](docs/UML.png) and [docs/UML.pdf](docs/UML.pdf).

```mermaid
classDiagram
    direction LR

    class PersonKey {
        -string name_
        -string folded_
        -int age_
        -string dob_
        +PersonKey(name, age, dob)
        +name() string
        +age() int
        +dob() string
        +operator<(other) bool
        +operator==(other) bool
        +toString() string
        +prefixRange(prefix)$ pair~PersonKey,PersonKey~
    }

    class Person {
        -PersonKey key_
        +Person(name, age, dob)
        +key() PersonKey
        +name() string
        +age() int
        +dob() string
    }

    class Patient {
        +kHealthy$ int = 0
        +kCritical$ int = 10
        -int condition_
        -int initialCondition_
        -string ailment_
        -int arrivalTick_
        -int criticalLimitTicks_
        -PatientStatus status_
        -bool wentCritical_
        -Doctor* doctor_
        -int admitTick_
        -int treatmentStartTick_
        -int dischargeTick_
        +condition() int
        +isCritical() bool
        +ticksWaited(now) int
        +ticksUntilCritical(now) int
        +admit(tick)
        +becomeCritical()
        +startTreatment(doctor, tick)
        +discharge(tick)
    }

    class PatientStatus {
        <<enumeration>>
        NotArrived
        Waiting
        InTreatment
        Discharged
    }

    class Doctor {
        -int skill_
        -int shiftStart_
        -int shiftEnd_
        -Patient* patient_
        -int ticksRemaining_
        -int patientsSeen_
        +skill() int
        +shiftString() string
        +isOnShift(minuteOfDay) bool
        +isBusy() bool
        +treatmentTicksFor(patient) int
        +beginTreatment(patient, ticks)
        +advanceTreatment() Patient*
    }

    class BTree~Key, Value, T~ {
        -unique_ptr~Node~ root_
        -size_t size_
        +insert(key, value) bool
        +search(key, nodesVisited) Value*
        +rangeSearch(lo, hi, fn)
        +forEach(fn)
        +forEachNodeByLevel(fn)
        +size() size_t
        +height() int
        +nodeCount() size_t
        +validate(why) bool
        -splitChild(x, i)$
        -insertNonFull(node, entry)$
    }

    class Node {
        +bool leaf
        +vector~Entry~ entries
        +vector~unique_ptr~Node~~ children
        +full() bool
    }

    class TriageEntry {
        +int condition
        +int arrivalTick
        +long long order
        +Patient* patient
    }

    class TriageOrder {
        +operator()(a, b) bool
    }

    class Hospital {
        +kBTreeDegree$ int = 3
        -vector~unique_ptr~Doctor~~ doctors_
        -vector~unique_ptr~Patient~~ patients_
        -size_t nextArrival_
        -BTree~PersonKey, Patient*, 3~ patientTree_
        -BTree~PersonKey, Doctor*, 3~ doctorTree_
        -priority_queue~TriageEntry~ triage_
        -deque~string~ log_
        -int tick_
        +fromFiles(doctorsCsv, patientsCsv)$ unique_ptr~Hospital~
        +step()
        +advance(ticks)
        +finished() bool
        +waitingInPriorityOrder() vector~const Patient*~
        +availableDoctorsInPriorityOrder() vector~const Doctor*~
        +stats() HospitalStats
        +lookup(query) LookupResult
        -finishTreatments()
        -logShiftChanges()
        -admitArrivals()
        -checkCriticalPatients()
        -assignDoctors()
        -popNextPatient() Patient*
    }

    class AvailableDoctorOrder {
        +operator()(a, b) bool
    }

    class HospitalStats {
        +int notYetArrived
        +int waiting
        +int inTreatment
        +int discharged
        +int wentCritical
        +int doctorsOnShift
        +double averageWaitTicks
    }

    class LookupResult {
        +bool ok
        +bool exact
        +string message
        +vector~const Patient*~ patients
        +vector~const Doctor*~ doctors
        +int patientNodesVisited
        +int doctorNodesVisited
    }

    class HospitalGui {
        -Hospital& hospital_
        -sf::RenderWindow window_
        -sf::Font font_
        -Tab tab_
        -bool autoPlay_
        -string query_
        -LookupResult lookup_
        +HospitalGui(hospital)
        +run() int
        -handleKey(key)
        -handleText(unicode)
        -handleClick(x, y)
        -draw()
        -drawWaiting()
        -drawDoctors()
        -drawLog()
        -drawLookup(area)
        -drawTreeView(area)
    }

    class dataloader {
        <<namespace>>
        +loadDoctors(path)$ vector~unique_ptr~Doctor~~
        +loadPatients(path)$ vector~unique_ptr~Patient~~
    }

    class simtime {
        <<namespace>>
        +kMinutesPerTick$ int = 15
        +formatClock(tick)$ string
        +formatDuration(ticks)$ string
        +parseHHMM(text)$ int
    }

    Person *-- PersonKey
    Person <|-- Patient
    Person <|-- Doctor
    Patient --> PatientStatus
    Patient --> Doctor : treated by
    Doctor --> Patient : treating
    BTree *-- Node
    Hospital *-- "many" Patient : owns
    Hospital *-- "many" Doctor : owns
    Hospital *-- "2" BTree : patientTree_, doctorTree_
    Hospital *-- TriageEntry : priority_queue
    TriageOrder ..> TriageEntry : compares
    Hospital ..> AvailableDoctorOrder : doctor priority_queue
    AvailableDoctorOrder ..> Doctor : compares skill
    Hospital ..> HospitalStats : creates
    Hospital ..> LookupResult : creates
    Hospital ..> dataloader : uses
    Hospital ..> simtime : uses
    HospitalGui --> Hospital : displays / drives
```
