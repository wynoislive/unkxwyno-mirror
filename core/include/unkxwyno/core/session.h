#pragma once

#include "unkxwyno/core/c_api.h"

#include <cstdint>
#include <optional>

namespace unkxwyno::core {

enum class SessionState : std::uint8_t {
    Idle,
    Discovering,
    Pairing,
    Connecting,
    Negotiating,
    Streaming,
    Recovering,
    Stopped,
    Failed,
};

enum class SessionEvent : std::uint8_t {
    StartDiscovery,
    PeerFound,
    PairingAccepted,
    TransportConnected,
    NegotiationAccepted,
    TransportLost,
    TransportRestored,
    Stop,
    Fail,
};

enum class SessionFailure : std::uint8_t {
    None,
    InvalidTransition,
    RecoveryTimedOut,
    TransportFailure,
    NegotiationFailure,
};

struct RecoveryPolicy {
    std::uint64_t heartbeat_interval_ms{1'000};
    std::uint32_t missed_heartbeat_limit{3};
    std::uint64_t recovery_window_ms{60'000};
    std::uint64_t initial_retry_delay_ms{250};
    std::uint64_t maximum_retry_delay_ms{5'000};
};

class UXM_API SessionController final {
public:
    explicit SessionController(RecoveryPolicy policy = {});

    [[nodiscard]] bool transition(SessionEvent event, std::uint64_t now_ms = 0) noexcept;
    void observe_heartbeat(std::uint64_t now_ms) noexcept;
    void poll(std::uint64_t now_ms) noexcept;

    [[nodiscard]] SessionState state() const noexcept;
    [[nodiscard]] SessionFailure failure() const noexcept;
    [[nodiscard]] std::uint32_t recovery_attempts() const noexcept;
    [[nodiscard]] std::uint64_t next_retry_delay_ms() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t> recovery_deadline_ms() const noexcept;

private:
    void enter_recovering(std::uint64_t now_ms) noexcept;

    RecoveryPolicy policy_;
    SessionState state_{SessionState::Idle};
    SessionFailure failure_{SessionFailure::None};
    std::uint64_t last_heartbeat_ms_{};
    std::optional<std::uint64_t> recovery_deadline_ms_;
    std::uint32_t recovery_attempts_{};
};

}  // namespace unkxwyno::core

