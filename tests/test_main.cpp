// test_main.cpp
// Self-contained tests for the B-Tree and the simulation rules.
// Build and run with:  make test
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "BTree.h"
#include "DataLoader.h"
#include "Hospital.h"

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++checks;                                                                         \
        if (!(cond)) {                                                                    \
            ++failures;                                                                   \
            std::cerr << "FAILED: " #cond << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        }                                                                                 \
    } while (0)

// Inserts n shuffled keys into a tree of minimum degree T and checks that
// every B-Tree property holds after each insert.
template <int T>
void testBTreeRandom(int n, unsigned seed) {
    BTree<int, int, T> tree;
    std::vector<int> keys(n);
    for (int i = 0; i < n; ++i) keys[i] = i * 2;   // even numbers only, so odd ones are "missing"
    std::shuffle(keys.begin(), keys.end(), std::mt19937(seed));

    bool alwaysValid = true;
    std::string why;
    for (int k : keys) {
        CHECK(tree.insert(k, k * 10));
        if (!tree.validate(&why)) alwaysValid = false;
    }
    if (!alwaysValid) std::cerr << "  t=" << T << " invariant broken: " << why << '\n';
    CHECK(alwaysValid);
    CHECK(tree.size() == static_cast<std::size_t>(n));

    bool allFound = true, noneExtra = true;
    for (int i = 0; i < n; ++i) {
        const int* v = tree.search(i * 2);
        if (!v || *v != i * 20) allFound = false;
        if (tree.search(i * 2 + 1)) noneExtra = false;
    }
    CHECK(allFound);
    CHECK(noneExtra);

    // In-order traversal is sorted.
    std::vector<int> inorder;
    tree.forEach([&](int k, int) { inorder.push_back(k); });
    CHECK(std::is_sorted(inorder.begin(), inorder.end()));
    CHECK(inorder.size() == static_cast<std::size_t>(n));

    // Range search returns exactly the keys in [lo, hi].
    std::vector<int> range;
    tree.rangeSearch(101, 301, [&](int k, int) { range.push_back(k); });
    std::vector<int> expected;
    for (int k = 102; k <= 300 && k < 2 * n; k += 2) expected.push_back(k);
    CHECK(range == expected);

    // Height stays logarithmic: h <= log_t((n+1)/2) + 1.
    double bound = std::log((n + 1) / 2.0) / std::log(static_cast<double>(T)) + 1.0;
    CHECK(tree.height() <= static_cast<int>(bound) + 1);
}

void testBTreeBasics() {
    BTree<int, std::string, 2> tree;
    CHECK(tree.empty());
    CHECK(tree.search(5) == nullptr);
    CHECK(tree.height() == 1);
    CHECK(tree.insert(5, "five"));
    CHECK(!tree.insert(5, "FIVE"));   // duplicate key replaces the value
    CHECK(tree.size() == 1);
    CHECK(*tree.search(5) == "FIVE");

    // Sequential inserts are the classic worst case for unbalanced trees.
    for (int i = 0; i < 1000; ++i) tree.insert(i, std::to_string(i));
    CHECK(tree.validate());
    int visited = 0;
    CHECK(tree.search(999, &visited) != nullptr);
    CHECK(visited <= tree.height());
}

void testPersonKey() {
    PersonKey a("Sam Lee", 34, "1992-02-14");
    PersonKey b("Sam Lee", 34, "1991-12-30");
    PersonKey c("sam lee", 34, "1992-02-14");
    CHECK(a != b);               // same name and age, different dob
    CHECK(b < a);                // ordered by dob as the last field
    CHECK(a == c);               // names compare without case
    auto range = PersonKey::prefixRange("Sam");
    CHECK(range.first < a && a < range.second);
    CHECK(!(PersonKey("Samuel Price", 69, "1957-12-24") < range.first));
    CHECK(PersonKey("Sarah", 1, "2000-01-01") < range.first || range.second < PersonKey("Sarah", 1, "2000-01-01"));
}

void testDoctorShifts() {
    Doctor day("A", 40, "1986-01-01", 5, 6 * 60, 14 * 60);
    Doctor night("B", 40, "1986-01-02", 5, 22 * 60, 6 * 60);
    CHECK(day.isOnShift(6 * 60));
    CHECK(!day.isOnShift(14 * 60));
    CHECK(!day.isOnShift(3 * 60));
    CHECK(night.isOnShift(23 * 60));
    CHECK(night.isOnShift(2 * 60));
    CHECK(!night.isOnShift(12 * 60));

    Doctor expert("C", 40, "1986-01-03", 10, 0, 0);
    Doctor junior("D", 40, "1986-01-04", 2, 0, 0);
    Patient sick("P", 30, "1996-01-01", 9, "x", 0, 4);
    CHECK(expert.treatmentTicksFor(sick) < junior.treatmentTicksFor(sick));
}

std::unique_ptr<Doctor> doc(const std::string& name, int skill, int startHour, int endHour) {
    return std::unique_ptr<Doctor>(new Doctor(name, 40, "1986-01-01", skill, startHour * 60, endHour * 60));
}

std::unique_ptr<Patient> pat(const std::string& name, int condition, int arrivalTick, int limitTicks) {
    return std::unique_ptr<Patient>(new Patient(name, 30, "1996-01-01", condition, "test", arrivalTick, limitTicks));
}

void testTriageAndCritical() {
    // One slow doctor who is busy with "First" for 9 ticks, so the others wait.
    std::vector<std::unique_ptr<Doctor>> doctors;
    doctors.push_back(doc("Solo", 2, 0, 0));   // always on shift
    std::vector<std::unique_ptr<Patient>> patients;
    patients.push_back(pat("First", 10, 0, 1));    // taken immediately
    patients.push_back(pat("Low", 3, 0, 2));       // will go critical after 2 ticks
    patients.push_back(pat("Mid", 6, 0, 100));
    Hospital h(std::move(doctors), std::move(patients), 0);

    auto waiting = h.waitingInPriorityOrder();
    CHECK(waiting.size() == 2);
    CHECK(waiting.size() == 2 && waiting[0]->name() == "Mid");   // higher level goes first

    h.advance(2);   // "Low" waits 2 ticks -> critical, jumps ahead of "Mid"
    waiting = h.waitingInPriorityOrder();
    CHECK(!waiting.empty() && waiting[0]->name() == "Low");
    CHECK(!waiting.empty() && waiting[0]->condition() == Patient::kCritical);
    CHECK(h.stats().wentCritical == 1);

    // Everyone is eventually treated.
    h.advance(50);
    CHECK(h.finished());
}

void testShiftsRespected() {
    // The only doctor works 08:00-09:00; a patient arriving at 06:00 must wait.
    std::vector<std::unique_ptr<Doctor>> doctors;
    doctors.push_back(doc("Late", 8, 8, 9));
    std::vector<std::unique_ptr<Patient>> patients;
    patients.push_back(pat("Early", 2, simtime::toTick(1, 6 * 60), 1000));
    Hospital h(std::move(doctors), std::move(patients), simtime::toTick(1, 6 * 60));
    h.advance(7);   // 07:45
    CHECK(h.stats().waiting == 1);
    h.advance(1);   // 08:00
    CHECK(h.stats().inTreatment == 1);
}


void testAvailableDoctorPriorityQueue() {
    // Insert the lower-skilled doctor first.
    std::vector<std::unique_ptr<Doctor>> doctors;
    doctors.push_back(doc("Junior", 2, 0, 0));
    doctors.push_back(doc("Expert", 10, 0, 0));
    doctors.push_back(doc("OffShift", 9, 8, 9));

    std::vector<std::unique_ptr<Patient>> patients;
    patients.push_back(pat("Critical", 10, 0, 100));
    patients.push_back(pat("Mild", 2, 0, 100));

    Hospital h(std::move(doctors), std::move(patients), 0);

    // Highest-skilled doctor gets the critical patient.
    CHECK(h.doctors()[1]->currentPatient() != nullptr);
    CHECK(h.doctors()[1]->currentPatient()->name() == "Critical");

    // Lower-skilled doctor gets the mild patient.
    CHECK(h.doctors()[0]->currentPatient() != nullptr);
    CHECK(h.doctors()[0]->currentPatient()->name() == "Mild");

    // Off-shift doctor must not receive a patient.
    CHECK(h.doctors()[2]->currentPatient() == nullptr);

    // Both working doctors are busy.
    CHECK(h.availableDoctorsInPriorityOrder().empty());
}

void testAvailableDoctorQueueFreeAndShift() {
    std::vector<std::unique_ptr<Doctor>> doctors;
    doctors.push_back(doc("Junior", 2, 0, 0));
    doctors.push_back(doc("Expert", 10, 0, 0));
    doctors.push_back(doc("Late", 9, 8, 9));

    std::vector<std::unique_ptr<Patient>> patients;

    Hospital h(
        std::move(doctors),
        std::move(patients),
        simtime::toTick(1, 6 * 60)
    );

    auto available = h.availableDoctorsInPriorityOrder();

    CHECK(available.size() == 2);
    CHECK(available[0]->name() == "Expert");
    CHECK(available[1]->name() == "Junior");

    // At 08:00, Late starts working.
    h.advance(8);

    available = h.availableDoctorsInPriorityOrder();

    CHECK(available.size() == 3);
    CHECK(available[0]->name() == "Expert");
    CHECK(available[1]->name() == "Late");
    CHECK(available[2]->name() == "Junior");

    // At 09:00, Late's shift ends.
    h.advance(4);

    available = h.availableDoctorsInPriorityOrder();

    CHECK(available.size() == 2);
    CHECK(available[0]->name() == "Expert");
    CHECK(available[1]->name() == "Junior");
}


void testDatasetRun() {
    auto h = Hospital::fromFiles("data/doctors.csv", "data/patients.csv");
    std::size_t total = h->patients().size();
    CHECK(total >= 50);
    CHECK(h->doctorTree().size() == h->doctors().size());

    bool ordered = true, doctorsLegal = true;
    for (int i = 0; i < 4 * simtime::kTicksPerDay && !h->finished(); ++i) {
        h->step();
        // The waiting list must never put a lower level before a higher one.
        auto w = h->waitingInPriorityOrder();
        for (std::size_t j = 1; j < w.size(); ++j)
            if (w[j - 1]->condition() < w[j]->condition()) ordered = false;
        // A patient who just started treatment must have an on-shift doctor.
        for (const auto& d : h->doctors()) {
            Patient* p = d->currentPatient();
            if (p && p->treatmentStartTick() == h->tick() && !d->isOnShift(simtime::minuteOfDay(h->tick())))
                doctorsLegal = false;
        }
    }
    CHECK(ordered);
    CHECK(doctorsLegal);
    CHECK(h->finished());
    CHECK(h->patientTree().size() == total);
    CHECK(h->patientTree().validate());

    // Lookups: prefix and exact, including two "Sam Lee, 34" patients.
    LookupResult sams = h->lookup("sam lee");
    CHECK(sams.patients.size() == 3);
    LookupResult exact = h->lookup("Sam Lee, 34, 1991-12-30");
    CHECK(exact.exact && exact.patients.size() == 1);
    CHECK(exact.patients.size() == 1 && exact.patients[0]->ailment() == "Allergic rash");
    LookupResult kim = h->lookup("Daniel Kim");
    CHECK(kim.patients.size() == 1 && kim.doctors.size() == 1);
    LookupResult dr = h->lookup("Dr. Maya");
    CHECK(dr.doctors.size() == 1);
    LookupResult missing = h->lookup("Sam Lee, 34, 2000-01-01");
    CHECK(missing.patients.empty());
}

}  // namespace

int main() {
    testBTreeBasics();
    testBTreeRandom<2>(2000, 1);
    testBTreeRandom<3>(2000, 2);
    testBTreeRandom<5>(2000, 3);
    testBTreeRandom<3>(7, 4);
    testPersonKey();
    testDoctorShifts();
    testTriageAndCritical();
    testShiftsRespected();
    testAvailableDoctorPriorityQueue();
    testAvailableDoctorQueueFreeAndShift();
    try {
        testDatasetRun();
    } catch (const std::exception& e) {
        ++failures;
        std::cerr << "FAILED: dataset run threw: " << e.what() << '\n';
    }

    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
