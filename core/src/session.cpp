#include "unkxwyno/core/session.h"

#include <algorithm>
#include <limits>

namespace unkxwyno::core {
namespace {

bool is_terminal(SessionState state) noexcept {
    return state == SessionState::Stopped || state == SessionState::Failed;
}

}  // namespace

SessionController::SessionController(RecoveryPolicy policy)
    : policy_(policy) {
    if (policy_.missed_heartbeat_limit == 0) {
        policy_.missed_heartbeat_limit = 1;
    }
    if (policy_.maximum_retry_delay_ms < policy_.initial_retry_delay_ms) {
        policy_.maximum_retry_delay_ms = policy_.initial_retry_delay_ms;
    }
}

bool SessionController::transition(SessionEvent event, std::uint64_t now_ms) noexcept {
    if (event == SessionEvent::Stop && !is_terminal(state_)) {
        state_ = SessionState::Stopped;
        failure_ = SessionFailure::None;
        recovery_deadline_ms_.reset();
        return true;
    }
    if (event == SessionEvent::Fail && !is_terminal(state_)) {
        state_ = SessionState::Failed;
        failure_ = SessionFailure::TransportFailure;
        recovery_deadline_ms_.reset();
        return true;
    }

    SessionState next = state_;
    bool valid = true;
    switch (state_) {
        case SessionState::Idle:
            valid = event == SessionEvent::StartDiscovery;
            next = SessionState::Discovering;
            break;
        case SessionState::Discovering:
            valid = event == SessionEvent::PeerFound;
            next = SessionState::Pairing;
            break;
        case SessionState::Pairing:
            valid = event == SessionEvent::PairingAccepted;
            next = SessionState::Connecting;
            break;
        case SessionState::Connecting:
            valid = event == SessionEvent::TransportConnected;
            next = SessionState::Negotiating;
            break;
        case SessionState::Negotiating:
            valid = event == SessionEvent::NegotiationAccepted;
            next = SessionState::Streaming;
            break;
        case SessionState::Streaming:
            valid = event == SessionEvent::TransportLost;
            if (valid) {
                enter_recovering(now_ms);
                return true;
            }
            break;
        case SessionState::Recovering:
            valid = event == SessionEvent::TransportRestored;
            next = SessionState::Negotiating;
            break;
        case SessionState::Stopped:
        case SessionState::Failed:
            valid = false;
            break;
    }

    if (!valid) {
        failure_ = SessionFailure::InvalidTransition;
        return false;
    }

    state_ = next;
    failure_ = SessionFailure::None;
    if (state_ == SessionState::Streaming) {
        last_heartbeat_ms_ = now_ms;
        recovery_attempts_ = 0;
        recovery_deadline_ms_.reset();
    } else if (state_ == SessionState::Negotiating) {
        recovery_deadline_ms_.reset();
    }
    return true;
}

void SessionController::observe_heartbeat(std::uint64_t now_ms) noexcept {
    if (state_ == SessionState::Streaming) {
        last_heartbeat_ms_ = now_ms;
    }
}

void SessionController::poll(std::uint64_t now_ms) noexcept {
    if (state_ == SessionState::Streaming) {
        const auto missed_window = policy_.heartbeat_interval_ms
            * static_cast<std::uint64_t>(policy_.missed_heartbeat_limit);
        if (now_ms >= last_heartbeat_ms_ && now_ms - last_heartbeat_ms_ >= missed_window) {
            enter_recovering(now_ms);
        }
        return;
    }

    if (state_ == SessionState::Recovering && recovery_deadline_ms_.has_value()) {
        if (now_ms >= *recovery_deadline_ms_) {
            state_ = SessionState::Failed;
            failure_ = SessionFailure::RecoveryTimedOut;
            recovery_deadline_ms_.reset();
        }
    }
}

SessionState SessionController::state() const noexcept {
    return state_;
}

SessionFailure SessionController::failure() const noexcept {
    return failure_;
}

std::uint32_t SessionController::recovery_attempts() const noexcept {
    return recovery_attempts_;
}

std::uint64_t SessionController::next_retry_delay_ms() const noexcept {
    if (state_ != SessionState::Recovering) {
        return 0;
    }
    const auto completed_retries = recovery_attempts_ == 0 ? 0 : recovery_attempts_ - 1;
    const auto shift = std::min<std::uint32_t>(completed_retries, 20);
    const auto multiplier = std::uint64_t{1} << shift;
    if (policy_.initial_retry_delay_ms > std::numeric_limits<std::uint64_t>::max() / multiplier) {
        return policy_.maximum_retry_delay_ms;
    }
    return std::min(policy_.initial_retry_delay_ms * multiplier, policy_.maximum_retry_delay_ms);
}

std::optional<std::uint64_t> SessionController::recovery_deadline_ms() const noexcept {
    return recovery_deadline_ms_;
}

void SessionController::enter_recovering(std::uint64_t now_ms) noexcept {
    state_ = SessionState::Recovering;
    failure_ = SessionFailure::None;
    ++recovery_attempts_;
    const auto maximum_start = std::numeric_limits<std::uint64_t>::max() - policy_.recovery_window_ms;
    recovery_deadline_ms_ = now_ms > maximum_start
        ? std::numeric_limits<std::uint64_t>::max()
        : now_ms + policy_.recovery_window_ms;
}

}  // namespace unkxwyno::core

