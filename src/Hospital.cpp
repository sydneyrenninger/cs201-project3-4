// Hospital.cpp
#include "Hospital.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "DataLoader.h"

namespace {

std::string describe(const Patient& p) {
    return p.name() + " (" + std::to_string(p.age()) + ", level " + std::to_string(p.condition()) + ", " +
           p.ailment() + ")";
}

std::string doctorName(const Doctor& d) { return "Dr. " + d.name(); }

}  // namespace

Hospital::Hospital(std::vector<std::unique_ptr<Doctor>> doctors, std::vector<std::unique_ptr<Patient>> patients,
                   int startTick)
    : doctors_(std::move(doctors)), patients_(std::move(patients)), tick_(startTick) {
    // Doctors are staff, so they go into their B-Tree right away.
    for (const auto& d : doctors_) {
        if (!doctorTree_.insert(d->key(), d.get()))
            throw std::runtime_error("duplicate doctor: " + d->key().toString());
    }

    // Patients enter the patient B-Tree when they arrive, but the keys must
    // still be unique across the whole dataset.
    std::set<PersonKey> seen;
    for (const auto& p : patients_) {
        if (!seen.insert(p->key()).second) throw std::runtime_error("duplicate patient: " + p->key().toString());
    }
    std::stable_sort(patients_.begin(), patients_.end(),
                     [](const std::unique_ptr<Patient>& a, const std::unique_ptr<Patient>& b) {
                         return a->arrivalTick() < b->arrivalTick();
                     });

    logEvent("Simulation started. " + std::to_string(doctors_.size()) + " doctors on staff, " +
             std::to_string(patients_.size()) + " patients expected.");
    admitArrivals();
    checkCriticalPatients();
    assignDoctors();
}

std::unique_ptr<Hospital> Hospital::fromFiles(const std::string& doctorsCsv, const std::string& patientsCsv) {
    return std::unique_ptr<Hospital>(
        new Hospital(dataloader::loadDoctors(doctorsCsv), dataloader::loadPatients(patientsCsv)));
}

// ---------------------------------------------------------------------------
// Time steps
// ---------------------------------------------------------------------------

void Hospital::step() {
    ++tick_;
    finishTreatments();
    logShiftChanges();
    admitArrivals();
    checkCriticalPatients();
    assignDoctors();
}

void Hospital::advance(int ticks) {
    for (int i = 0; i < ticks; ++i) step();
}

bool Hospital::finished() const {
    if (nextArrival_ < patients_.size()) return false;
    for (const auto& p : patients_)
        if (p->status() != PatientStatus::Discharged) return false;
    return true;
}

void Hospital::finishTreatments() {
    for (const auto& d : doctors_) {
        if (Patient* done = d->advanceTreatment()) {
            done->discharge(tick_);
            logEvent(doctorName(*d) + " discharged " + done->name() + " after " +
                     simtime::formatDuration(tick_ - done->treatmentStartTick()) + " of care.");
        }
    }
}

void Hospital::logShiftChanges() {
    const int now = simtime::minuteOfDay(tick_);
    const int before = simtime::minuteOfDay(tick_ - 1);
    for (const auto& d : doctors_) {
        bool wasOn = d->isOnShift(before);
        bool isOn = d->isOnShift(now);
        if (!wasOn && isOn) {
            logEvent(doctorName(*d) + " started their shift (" + d->shiftString() + ").");
        } else if (wasOn && !isOn) {
            if (d->isBusy())
                logEvent(doctorName(*d) + "'s shift ended; finishing care of " + d->currentPatient()->name() +
                         " before leaving.");
            else
                logEvent(doctorName(*d) + " ended their shift.");
        }
    }
}

void Hospital::admitArrivals() {
    int arrivedNow = 0;
    while (nextArrival_ < patients_.size() && patients_[nextArrival_]->arrivalTick() <= tick_) {
        Patient* p = patients_[nextArrival_++].get();
        p->admit(tick_);
        patientTree_.insert(p->key(), p);
        pushTriage(p);
        ++arrivedNow;
        logEvent("Arrived: " + describe(*p) + (p->isCritical() ? " - CRITICAL on arrival" : "") + ".");
    }
    if (arrivedNow > 1) logEvent(std::to_string(arrivedNow) + " patients arrived at the same time.");
}

void Hospital::checkCriticalPatients() {
    // Only admitted patients (indices before nextArrival_) can be waiting.
    for (std::size_t i = 0; i < nextArrival_; ++i) {
        Patient* p = patients_[i].get();
        if (p->status() != PatientStatus::Waiting || p->isCritical()) continue;
        if (p->ticksWaited(tick_) >= p->criticalLimitTicks()) {
            p->becomeCritical();
            pushTriage(p);   // the old entry is now stale and will be skipped
            logEvent("CRITICAL: " + p->name() + " was not seen within " +
                     simtime::formatDuration(p->criticalLimitTicks()) + " and is now level 10.");
        }
    }
}


void Hospital::assignDoctors() {
    const int now = simtime::minuteOfDay(tick_);

    // Priority queue: highest-skilled doctor first.
    std::priority_queue<
        Doctor*,
        std::vector<Doctor*>,
        AvailableDoctorOrder
    > available;

    // Find doctors who are currently free and on shift.
    for (const auto& doctor : doctors_) {
        if (!doctor->isBusy() && doctor->isOnShift(now)) {
            available.push(doctor.get());
        }
    }

    // Assign the highest-priority patient to the
    // highest-skilled available doctor.
    while (!available.empty()) {
        Patient* p = popNextPatient();

        // Stop if no patients are waiting.
        if (!p) break;

        Doctor* d = available.top();
        available.pop();

        int ticks = d->treatmentTicksFor(*p);

        d->beginTreatment(p, ticks);
        p->startTreatment(d, tick_);

        logEvent(
            doctorName(*d) +
            " (skill " + std::to_string(d->skill()) +
            ") is seeing " + describe(*p) +
            ", about " + simtime::formatDuration(ticks) +
            ". Waited " +
            simtime::formatDuration(p->ticksWaited(tick_)) +
            "."
        );
    }
}


std::vector<const Doctor*>
Hospital::availableDoctorsInPriorityOrder() const {

    const int now = simtime::minuteOfDay(tick_);

    std::priority_queue<
        Doctor*,
        std::vector<Doctor*>,
        AvailableDoctorOrder
    > available;

    // Collect free doctors who are on shift.
    for (const auto& doctor : doctors_) {
        if (!doctor->isBusy() && doctor->isOnShift(now)) {
            available.push(doctor.get());
        }
    }

    std::vector<const Doctor*> result;

    // Extract doctors in priority order.
    while (!available.empty()) {
        result.push_back(available.top());
        available.pop();
    }

    return result;
}



// ---------------------------------------------------------------------------
// Triage priority queue
// ---------------------------------------------------------------------------

void Hospital::pushTriage(Patient* patient) {
    triage_.push(TriageEntry{patient->condition(), patient->arrivalTick(), triageCounter_++, patient});
}

bool Hospital::isCurrent(const TriageEntry& entry) {
    return entry.patient->status() == PatientStatus::Waiting && entry.patient->condition() == entry.condition;
}

Patient* Hospital::popNextPatient() {
    while (!triage_.empty()) {
        TriageEntry top = triage_.top();
        triage_.pop();
        if (isCurrent(top)) return top.patient;   // stale entries are simply dropped
    }
    return nullptr;
}

std::vector<const Patient*> Hospital::waitingInPriorityOrder() const {
    // Pop from a copy so the real queue is untouched.
    auto copy = triage_;
    std::vector<const Patient*> order;
    std::unordered_set<const Patient*> seen;
    while (!copy.empty()) {
        TriageEntry top = copy.top();
        copy.pop();
        if (isCurrent(top) && seen.insert(top.patient).second) order.push_back(top.patient);
    }
    return order;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

HospitalStats Hospital::stats() const {
    HospitalStats s;
    long long totalWait = 0;
    int started = 0;
    for (const auto& p : patients_) {
        switch (p->status()) {
            case PatientStatus::NotArrived: ++s.notYetArrived; break;
            case PatientStatus::Waiting: ++s.waiting; break;
            case PatientStatus::InTreatment: ++s.inTreatment; break;
            case PatientStatus::Discharged: ++s.discharged; break;
        }
        if (p->wentCritical()) ++s.wentCritical;
        if (p->treatmentStartTick() >= 0) {
            totalWait += p->ticksWaited(tick_);
            ++started;
        }
    }
    s.averageWaitTicks = started ? static_cast<double>(totalWait) / started : 0.0;
    const int now = simtime::minuteOfDay(tick_);
    for (const auto& d : doctors_)
        if (d->isOnShift(now)) ++s.doctorsOnShift;
    return s;
}

LookupResult Hospital::lookup(const std::string& rawQuery) const {
    LookupResult r;
    std::string query = trim(rawQuery);
    std::string lower = toLower(query);
    if (lower.rfind("dr.", 0) == 0) query = trim(query.substr(3));
    else if (lower.rfind("dr ", 0) == 0) query = trim(query.substr(3));

    if (query.empty()) {
        r.message = "Type a name, or: name, age, YYYY-MM-DD";
        return r;
    }

    std::vector<std::string> parts;
    std::stringstream ss(query);
    std::string part;
    while (std::getline(ss, part, ',')) parts.push_back(trim(part));

    if (parts.size() == 1) {
        // Name prefix: an in-order range search over each B-Tree.
        auto range = PersonKey::prefixRange(parts[0]);
        patientTree_.rangeSearch(range.first, range.second,
                                 [&](const PersonKey&, Patient* p) { r.patients.push_back(p); });
        doctorTree_.rangeSearch(range.first, range.second,
                                [&](const PersonKey&, Doctor* d) { r.doctors.push_back(d); });
        r.ok = true;
        r.message = std::to_string(r.patients.size() + r.doctors.size()) + " match(es) for names starting with \"" +
                    parts[0] + "\"";
        return r;
    }

    if (parts.size() != 3) {
        r.message = "Use: name   or   name, age, YYYY-MM-DD";
        return r;
    }

    int age = 0;
    try {
        std::size_t used = 0;
        age = std::stoi(parts[1], &used);
        if (used != parts[1].size()) throw std::invalid_argument("age");
    } catch (const std::exception&) {
        r.message = "Age must be a number.";
        return r;
    }

    // Exact key: a normal B-Tree search in each tree.
    PersonKey key(parts[0], age, parts[2]);
    r.exact = true;
    r.ok = true;
    if (Patient* const* p = patientTree_.search(key, &r.patientNodesVisited)) r.patients.push_back(*p);
    if (Doctor* const* d = doctorTree_.search(key, &r.doctorNodesVisited)) r.doctors.push_back(*d);
    r.message = (r.patients.empty() && r.doctors.empty()) ? "Not found: " + key.toString()
                                                          : "Found " + key.toString();
    return r;
}

void Hospital::logEvent(const std::string& text) {
    log_.push_back("[" + simtime::formatClock(tick_) + "] " + text);
    while (log_.size() > kMaxLogLines) log_.pop_front();
}
