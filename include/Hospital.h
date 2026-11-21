// Hospital.h
// The simulation engine. It owns every patient and doctor, keeps them in
// B-Trees for lookups, keeps waiting patients in a priority queue, and moves
// the clock forward one tick at a time.
//
// One tick (step) does, in order:
//   1. advance every treatment; finished patients are discharged
//   2. log doctors whose shift starts or ends
//   3. admit patients whose arrival time has come (several can arrive at once)
//   4. any waiting patient past their time limit becomes critical (level 10)
//   5. every free, on-shift doctor takes the highest-priority waiting patient
#pragma once

#include <deque>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "BTree.h"
#include "Doctor.h"
#include "Patient.h"
#include "PersonKey.h"
#include "SimTime.h"

// One entry in the triage priority queue. The condition is copied in when the
// entry is pushed; if the patient's condition changes later, a new entry is
// pushed and this one becomes stale (it is skipped when popped).
struct TriageEntry {
    int condition;
    int arrivalTick;
    long long order;   // insertion counter, final tie-breaker
    Patient* patient;
};

// std::priority_queue puts the "largest" element on top. An entry is
// "smaller" (lower priority) if its condition is lower, or if it is equal and
// the patient arrived later.
struct TriageOrder {
    bool operator()(const TriageEntry& a, const TriageEntry& b) const {
        if (a.condition != b.condition) return a.condition < b.condition;
        if (a.arrivalTick != b.arrivalTick) return a.arrivalTick > b.arrivalTick;
        return a.order > b.order;
    }
};


// Higher-skilled doctors have higher priority.
// If skills are equal, order by doctor key.
struct AvailableDoctorOrder {
    bool operator()(const Doctor* a, const Doctor* b) const {
        if (a->skill() != b->skill())
            return a->skill() < b->skill();
        return b->key() < a->key();
    }
};


struct HospitalStats {
    int notYetArrived = 0;
    int waiting = 0;
    int inTreatment = 0;
    int discharged = 0;
    int wentCritical = 0;
    int doctorsOnShift = 0;
    double averageWaitTicks = 0.0;   // over patients whose treatment has started
};

// Result of a patient/doctor lookup from the GUI or console.
struct LookupResult {
    bool ok = false;
    bool exact = false;           // true: "name, age, dob" search; false: name prefix search
    std::string message;
    std::vector<const Patient*> patients;
    std::vector<const Doctor*> doctors;
    int patientNodesVisited = 0;  // only filled for exact searches
    int doctorNodesVisited = 0;
};

class Hospital {
public:
    // t = 3: every node holds 2-5 keys. See README.md for why.
    static constexpr int kBTreeDegree = 3;
    using PatientTree = BTree<PersonKey, Patient*, kBTreeDegree>;
    using DoctorTree = BTree<PersonKey, Doctor*, kBTreeDegree>;

    static constexpr int kDefaultStartTick = 6 * simtime::kTicksPerHour;   // Day 1, 06:00
    static constexpr std::size_t kMaxLogLines = 500;

    // Throws std::runtime_error if two doctors or two patients share a key.
    Hospital(std::vector<std::unique_ptr<Doctor>> doctors, std::vector<std::unique_ptr<Patient>> patients,
             int startTick = kDefaultStartTick);

    // Loads data/doctors.csv-style and data/patients.csv-style files.
    static std::unique_ptr<Hospital> fromFiles(const std::string& doctorsCsv, const std::string& patientsCsv);

    void step();                 // advance the clock by one tick
    void advance(int ticks);     // step() repeated

    int tick() const { return tick_; }
    std::string clock() const { return simtime::formatClock(tick_); }
    bool finished() const;       // everyone has arrived and been discharged

    // Waiting patients, highest priority first (the order they will be seen).
    std::vector<const Patient*> waitingInPriorityOrder() const;
    const std::vector<std::unique_ptr<Doctor>>& doctors() const { return doctors_; }
    // Return free, on-shift doctors ordered by skill.
    std::vector<const Doctor*> availableDoctorsInPriorityOrder() const;
    const std::vector<std::unique_ptr<Patient>>& patients() const { return patients_; }
    const std::deque<std::string>& log() const { return log_; }   // oldest first
    HospitalStats stats() const;

    // Query forms: "Sam"            -> everyone whose name starts with "Sam"
    //              "Sam Lee, 34, 1992-02-14" -> exact B-Tree search
    // A leading "Dr." is ignored. Searches both the patient and doctor trees.
    LookupResult lookup(const std::string& query) const;

    const PatientTree& patientTree() const { return patientTree_; }
    const DoctorTree& doctorTree() const { return doctorTree_; }

private:
    void finishTreatments();
    void logShiftChanges();
    void admitArrivals();
    void checkCriticalPatients();
    void assignDoctors();

    void pushTriage(Patient* patient);
    Patient* popNextPatient();
    static bool isCurrent(const TriageEntry& entry);
    void logEvent(const std::string& text);

    std::vector<std::unique_ptr<Doctor>> doctors_;
    std::vector<std::unique_ptr<Patient>> patients_;   // sorted by arrival time
    std::size_t nextArrival_ = 0;                      // first patient not yet admitted

    // Patients are inserted when they arrive, doctors when the data is loaded.
    PatientTree patientTree_;
    DoctorTree doctorTree_;

    std::priority_queue<TriageEntry, std::vector<TriageEntry>, TriageOrder> triage_;
    long long triageCounter_ = 0;

    std::deque<std::string> log_;
    int tick_;
};
