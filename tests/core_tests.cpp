#include "capture/capture_coordinator.h"
#include "group/group_model.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

using quickky::AttachmentRequest;
using quickky::CaptureCoordinator;
using quickky::CaptureState;
using quickky::GroupModel;
using quickky::NativeWindow;
using quickky::RestoreRequest;
using quickky::TabId;
using quickky::WindowCandidate;
using quickky::WindowIdentity;

class TestContext {
public:
    explicit TestContext(const char* name) : name_(name) {}

    bool Check(bool condition, const char* expression, int line)
    {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL [" << name_ << "] line " << line
                      << ": " << expression << '\n';
        }
        return condition;
    }

    int failures() const noexcept { return failures_; }

private:
    const char* name_;
    int failures_ = 0;
};

#define CHECK(context, expression) \
    do { \
        if (!(context).Check(static_cast<bool>(expression), #expression, __LINE__)) { \
            return; \
        } \
    } while (false)

WindowCandidate Candidate(NativeWindow handle, std::uint32_t process_id = 100,
                          std::uint32_t thread_id = 200,
                          std::uint64_t process_started_at = 300)
{
    return {handle, process_id, thread_id, process_started_at};
}

WindowIdentity Identity(NativeWindow handle, std::uint64_t generation = 1,
                        std::uint32_t process_id = 100,
                        std::uint32_t thread_id = 200,
                        std::uint64_t process_started_at = 300)
{
    return {Candidate(handle, process_id, thread_id, process_started_at), generation};
}

bool CheckOrder(TestContext& context, const GroupModel& group,
                std::initializer_list<WindowIdentity> expected)
{
    if (!context.Check(group.tabs().size() == expected.size(),
                       "group tab count matches expected order", __LINE__)) {
        return false;
    }
    std::size_t index = 0;
    for (const auto& window : expected) {
        if (!context.Check(group.tabs()[index].window == window,
                           "tab window matches expected order", __LINE__)) {
            return false;
        }
        ++index;
    }
    return true;
}

void TestWindowIdentityValidation(TestContext& context)
{
    const auto valid = Candidate(1);
    CHECK(context, valid.valid());
    CHECK(context, !Candidate(0).valid());
    CHECK(context, !Candidate(1, 0, 200, 300).valid());
    CHECK(context, !Candidate(1, 100, 0, 300).valid());
    CHECK(context, !Candidate(1, 100, 200, 0).valid());

    CHECK(context, Identity(1) == Identity(1));
    CHECK(context, Identity(1) != Identity(1, 2));
    CHECK(context, Identity(1) != Identity(1, 1, 101, 200, 300));
}

void TestGroupDuplicateAndConflictingLifetimeAdmission(TestContext& context)
{
    GroupModel group;
    const auto first = Identity(10);
    const auto second = Identity(11);
    const auto first_id = group.Add(first, false);
    const auto second_id = group.Add(second, true);
    CHECK(context, first_id.has_value() && second_id.has_value());
    CHECK(context, group.active().has_value() && group.active() == second_id);

    const auto duplicate_id = group.Add(first, true);
    CHECK(context, duplicate_id.has_value() && duplicate_id == first_id);
    CHECK(context, group.tabs().size() == 2);
    CHECK(context, group.active() == second_id);

    const auto replacement = Identity(10, 2, 101, 201, 301);
    CHECK(context, !group.Add(replacement, true).has_value());
    CHECK(context, group.tabs().size() == 2);
    CHECK(context, group.active() == second_id);

    CHECK(context, group.Remove(first));
    const auto replacement_id = group.Add(replacement, false);
    CHECK(context, replacement_id.has_value());
    CHECK(context, *replacement_id > *second_id);
    CHECK(context, group.Find(replacement) != nullptr);
}

void TestGroupStableIdsOrderingAndSelection(TestContext& context)
{
    GroupModel group;
    const auto a = Identity(20);
    const auto b = Identity(21);
    const auto c = Identity(22);
    const auto d = Identity(23);
    const auto id_a = group.Add(a, false);
    const auto id_b = group.Add(b, false);
    const auto id_c = group.Add(c, false);
    const auto id_d = group.Add(d, false);
    CHECK(context, id_a && id_b && id_c && id_d);
    CHECK(context, group.active() == id_a);
    CHECK(context, group.Activate(*id_b));

    CHECK(context, group.Move(*id_d, 1));
    CHECK(context, CheckOrder(context, group, {a, d, b, c}));
    CHECK(context, group.active() == id_b);
    CHECK(context, group.Move(*id_a, 3));
    CHECK(context, CheckOrder(context, group, {d, b, c, a}));
    CHECK(context, group.active() == id_b);

    CHECK(context, group.Find(b) != nullptr && group.Find(b)->id == *id_b);
    CHECK(context, group.Find(a) != nullptr && group.Find(a)->id == *id_a);
    CHECK(context, group.Find(c) != nullptr && group.Find(c)->id == *id_c);
    CHECK(context, group.Find(d) != nullptr && group.Find(d)->id == *id_d);
}

void TestGroupRemovalFallback(TestContext& context)
{
    GroupModel group;
    const auto a = Identity(30);
    const auto b = Identity(31);
    const auto c = Identity(32);
    const auto d = Identity(33);
    const auto id_a = group.Add(a, false);
    const auto id_b = group.Add(b, false);
    const auto id_c = group.Add(c, false);
    const auto id_d = group.Add(d, false);
    CHECK(context, id_a && id_b && id_c && id_d);

    CHECK(context, group.Activate(*id_b));
    CHECK(context, group.Remove(b));
    CHECK(context, group.active() == id_c);
    CHECK(context, CheckOrder(context, group, {a, c, d}));

    CHECK(context, group.Remove(a));
    CHECK(context, group.active() == id_c);
    CHECK(context, group.Remove(d));
    CHECK(context, group.active() == id_c);
    CHECK(context, CheckOrder(context, group, {c}));

    CHECK(context, group.Remove(c));
    CHECK(context, group.tabs().empty());
    CHECK(context, !group.active().has_value());
}

void TestGroupInvalidOperationsAreNoOps(TestContext& context)
{
    GroupModel group;
    CHECK(context, !group.Activate(1));
    CHECK(context, !group.Move(1, 0));
    CHECK(context, !group.Remove(Identity(0)));
    CHECK(context, !group.Add(Identity(40, 0), true).has_value());
    CHECK(context, group.tabs().empty());
    CHECK(context, !group.active().has_value());

    const auto window = Identity(40);
    const auto id = group.Add(window, false);
    CHECK(context, id.has_value());
    CHECK(context, !group.Activate(*id + 100));
    CHECK(context, !group.Move(*id + 100, 0));
    CHECK(context, !group.Move(*id, group.tabs().size()));
    CHECK(context, !group.Remove(Identity(40, 2)));
    CHECK(context, group.tabs().size() == 1);
    CHECK(context, group.tabs().front().id == *id);
    CHECK(context, group.tabs().front().window == window);
    CHECK(context, group.active() == id);
}

void TestObserveDeduplicationAndLifetimeChanges(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    auto candidate = Candidate(50);
    const auto first = capture.Observe(candidate);
    const auto duplicate = capture.Observe(candidate);
    CHECK(context, first.has_value() && duplicate.has_value());
    CHECK(context, *first == *duplicate);
    CHECK(context, first->generation == 1);
    CHECK(context, capture.size() == 1);

    ++candidate.process_id;
    const auto process_change = capture.Observe(candidate);
    CHECK(context, process_change.has_value());
    CHECK(context, process_change->generation == 2);
    CHECK(context, process_change->window.process_id == candidate.process_id);
    CHECK(context, !capture.BeginAttach(*first).has_value());

    ++candidate.thread_id;
    const auto thread_change = capture.Observe(candidate);
    CHECK(context, thread_change.has_value());
    CHECK(context, thread_change->generation == 3);
    CHECK(context, thread_change->window.thread_id == candidate.thread_id);
    CHECK(context, !capture.BeginAttach(*process_change).has_value());

    ++candidate.process_started_at;
    const auto reused_handle = capture.Observe(candidate);
    CHECK(context, reused_handle.has_value());
    CHECK(context, reused_handle->generation == 4);
    CHECK(context, reused_handle->window.handle == first->window.handle);
    CHECK(context, reused_handle->window.process_started_at == candidate.process_started_at);
    CHECK(context, !capture.BeginAttach(*thread_change).has_value());
    CHECK(context, capture.Find(candidate.handle) != nullptr);
    CHECK(context, capture.Find(candidate.handle)->window == *reused_handle);

    const auto invalid = Candidate(0);
    CHECK(context, !capture.Observe(invalid).has_value());
    CHECK(context, capture.size() == 1);
    CHECK(context, capture.Find(candidate.handle)->window == *reused_handle);
}

void TestAttachRequiresCurrentMatchingAcknowledgment(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto observed = capture.Observe(Candidate(60));
    CHECK(context, observed.has_value());
    const auto request = capture.BeginAttach(*observed);
    CHECK(context, request.has_value());
    CHECK(context, capture.Find(60)->state == CaptureState::Attaching);
    CHECK(context, group.tabs().empty());
    CHECK(context, !capture.BeginAttach(*observed).has_value());

    auto mismatched = *request;
    ++mismatched.token;
    CHECK(context, !capture.CompleteAttach(mismatched, true, true));
    CHECK(context, group.tabs().empty());
    CHECK(context, capture.Find(60)->state == CaptureState::Attaching);

    CHECK(context, capture.CompleteAttach(*request, true, false));
    CHECK(context, capture.Find(60)->state == CaptureState::Managed);
    CHECK(context, group.tabs().size() == 1);
    CHECK(context, group.Find(*observed) != nullptr);
    CHECK(context, !capture.CompleteAttach(*request, true, true));
}

void TestFailedAttachRetryUsesFreshToken(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto observed = capture.Observe(Candidate(61));
    CHECK(context, observed.has_value());
    const auto first = capture.BeginAttach(*observed);
    CHECK(context, first.has_value());
    CHECK(context, !capture.CompleteAttach(*first, false, true));
    CHECK(context, capture.Find(61)->state == CaptureState::Discovered);
    CHECK(context, group.tabs().empty());

    const auto retry = capture.BeginAttach(*observed);
    CHECK(context, retry.has_value());
    CHECK(context, retry->token != first->token);
    CHECK(context, !capture.CompleteAttach(*first, true, true));
    CHECK(context, capture.Find(61)->state == CaptureState::Attaching);
    CHECK(context, group.tabs().empty());
    CHECK(context, capture.CompleteAttach(*retry, true, true));
    CHECK(context, group.tabs().size() == 1);
}

void TestDestroyDuringAttachInvalidatesAcknowledgment(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto observed = capture.Observe(Candidate(62));
    CHECK(context, observed.has_value());
    const auto request = capture.BeginAttach(*observed);
    CHECK(context, request.has_value());
    CHECK(context, capture.Forget(*observed));
    CHECK(context, capture.Find(62) == nullptr);
    CHECK(context, !capture.CompleteAttach(*request, true, true));
    CHECK(context, group.tabs().empty());

    const auto observed_again = capture.Observe(Candidate(62));
    CHECK(context, observed_again.has_value());
    CHECK(context, observed_again->generation != observed->generation);
    CHECK(context, !capture.CompleteAttach(*request, true, true));
}

void TestDestroyDuringManagedRemovesMembership(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto observed = capture.Observe(Candidate(63));
    CHECK(context, observed.has_value());
    const auto attach = capture.BeginAttach(*observed);
    CHECK(context, attach.has_value());
    CHECK(context, capture.CompleteAttach(*attach, true, true));
    CHECK(context, capture.Find(63)->state == CaptureState::Managed);
    CHECK(context, group.Find(*observed) != nullptr);

    CHECK(context, capture.Forget(*observed));
    CHECK(context, capture.Find(63) == nullptr);
    CHECK(context, group.Find(*observed) == nullptr);
    CHECK(context, !capture.Forget(*observed));
    CHECK(context, !capture.OnForeground(*observed));
}

void TestDestroyDuringRestoreInvalidatesCompletion(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto observed = capture.Observe(Candidate(64));
    CHECK(context, observed.has_value());
    const auto attach = capture.BeginAttach(*observed);
    CHECK(context, attach.has_value());
    CHECK(context, capture.CompleteAttach(*attach, true, true));
    const auto restore = capture.BeginRestore(*observed);
    CHECK(context, restore.has_value());
    CHECK(context, capture.Find(64)->state == CaptureState::Restoring);

    CHECK(context, capture.Forget(*observed));
    CHECK(context, group.Find(*observed) == nullptr);
    CHECK(context, !capture.CompleteRestore(*restore, true));
    CHECK(context, capture.Find(64) == nullptr);
}

void TestForegroundSelectsOnlyManagedRecords(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto first = capture.Observe(Candidate(65));
    const auto second = capture.Observe(Candidate(66));
    CHECK(context, first && second);
    const auto attach_first = capture.BeginAttach(*first);
    CHECK(context, attach_first.has_value());
    CHECK(context, capture.CompleteAttach(*attach_first, true, true));
    const auto first_id = group.Find(*first)->id;
    CHECK(context, group.Activate(first_id));

    CHECK(context, !capture.OnForeground(*second));
    CHECK(context, group.active() == first_id);
    const auto attach_second = capture.BeginAttach(*second);
    CHECK(context, attach_second.has_value());
    CHECK(context, !capture.OnForeground(*second));
    CHECK(context, group.active() == first_id);
    CHECK(context, capture.CompleteAttach(*attach_second, true, false));
    const auto second_id = group.Find(*second)->id;
    CHECK(context, capture.OnForeground(*second));
    CHECK(context, group.active() == second_id);

    CHECK(context, group.Activate(first_id));
    const auto restore = capture.BeginRestore(*second);
    CHECK(context, restore.has_value());
    CHECK(context, !capture.OnForeground(*second));
    CHECK(context, group.active() == first_id);
    CHECK(context, !capture.CompleteRestore(*restore, false));
    CHECK(context, capture.OnForeground(*second));
    CHECK(context, group.active() == second_id);
}

void TestRestoreRetryRejectsStaleToken(TestContext& context)
{
    GroupModel group;
    CaptureCoordinator capture(group);
    const auto observed = capture.Observe(Candidate(67));
    CHECK(context, observed.has_value());
    const auto attach = capture.BeginAttach(*observed);
    CHECK(context, attach.has_value());
    CHECK(context, capture.CompleteAttach(*attach, true, true));

    const auto first = capture.BeginRestore(*observed);
    CHECK(context, first.has_value());
    CHECK(context, !capture.CompleteRestore(*first, false));
    CHECK(context, capture.Find(67)->state == CaptureState::Managed);
    CHECK(context, group.Find(*observed) != nullptr);

    const auto retry = capture.BeginRestore(*observed);
    CHECK(context, retry.has_value());
    CHECK(context, retry->token != first->token);
    CHECK(context, !capture.CompleteRestore(*first, true));
    CHECK(context, capture.Find(67)->state == CaptureState::Restoring);
    CHECK(context, group.Find(*observed) != nullptr);
    CHECK(context, capture.CompleteRestore(*retry, true));
    CHECK(context, capture.Find(67) == nullptr);
    CHECK(context, group.Find(*observed) == nullptr);
}

using TestFunction = void (*)(TestContext&);

struct TestCase {
    const char* name;
    TestFunction run;
};

} // namespace

int main()
{
    const TestCase tests[] = {
        {"window identity validation", TestWindowIdentityValidation},
        {"group duplicate and conflicting lifetime admission", TestGroupDuplicateAndConflictingLifetimeAdmission},
        {"group stable IDs, ordering, and selection", TestGroupStableIdsOrderingAndSelection},
        {"group removal fallback", TestGroupRemovalFallback},
        {"group invalid operations are no-ops", TestGroupInvalidOperationsAreNoOps},
        {"observe deduplication and lifetime changes", TestObserveDeduplicationAndLifetimeChanges},
        {"attach requires current matching acknowledgment", TestAttachRequiresCurrentMatchingAcknowledgment},
        {"failed attach retry uses fresh token", TestFailedAttachRetryUsesFreshToken},
        {"destroy during attach invalidates acknowledgment", TestDestroyDuringAttachInvalidatesAcknowledgment},
        {"destroy during managed removes membership", TestDestroyDuringManagedRemovesMembership},
        {"destroy during restore invalidates completion", TestDestroyDuringRestoreInvalidatesCompletion},
        {"foreground selects only managed records", TestForegroundSelectsOnlyManagedRecords},
        {"restore retry rejects stale token", TestRestoreRetryRejectsStaleToken},
    };

    int failures = 0;
    for (const auto& test : tests) {
        TestContext context(test.name);
        test.run(context);
        failures += context.failures();
    }

    if (failures != 0) {
        std::cerr << failures << " core test assertion(s) failed.\n";
        return 1;
    }
    std::cout << "All " << (sizeof(tests) / sizeof(tests[0]))
              << " core tests passed.\n";
    return 0;
}
