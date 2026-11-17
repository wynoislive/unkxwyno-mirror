# Privacy

## Runtime data flow

Mirrored video, audio, discovery, pairing, and statistics remain between the selected devices over USB or the local network. The product has no account service, cloud relay, analytics, advertisement, or telemetry backend.

## Stored data

- Settings are stored locally and exclude private keys.
- Paired-peer identities are stored in native secure storage.
- iPhone USB pairing creates a new identity for this app. Its certificate/key
  material is stored under this user's LocalAppData, encrypted with Windows
  DPAPI. Existing Apple/iTunes pairing records and their private keys are not
  read. Only public certificates are sent in the phone's normal Pair request.
- The trusted USB session pins the phone's public key, supplies this app's pairing
  root identity, and honors lockdownd's TLS requirement. It does not install a
  Windows root certificate or disable certificate verification globally.
- Screenshots and recordings are created only after an explicit user action at a user-visible location.
- Local logs contain state and error codes, not frame pixels, audio samples, pairing secrets, or private keys.

## Diagnostics

Diagnostic export is opt-in and displays the files to be exported. It redacts local usernames/paths, addresses where practical, pairing material, capture content, and secure-store identifiers. Exported settings omit identities and trusted-peer credentials.

## Permissions

Capture, microphone, local-network, notification/foreground-service, camera-for-QR, and USB permissions are requested only at the feature boundary that needs them. Denial leaves other features usable and produces a technical explanation instead of repeated coercive prompts.

## User control

Users can stop a stream immediately, revoke a paired peer, delete recordings/screenshots, clear settings, and reset local identity. Resetting identity invalidates previous pairings and requires explicit confirmation.
