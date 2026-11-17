# Android target

This is the independent Android application scaffold. It targets Android 12/API 31 or later and uses only public Android capture, codec, USB, and local-network APIs.

Implemented boundaries:

- explicit `MediaProjection` consent;
- a media-projection foreground service;
- delayed `VirtualDisplay` creation until an encoder surface is attached;
- runtime AVC, HEVC, and AV1 codec capability probing;
- DNS-SD discovery for `_unkxwyno._tcp.`;
- Android 17 local-network permission gating and Android 12 multicast-lock handling;
- receiver and capture states that do not claim a stream before negotiation.

The encoded-media transport and decoder/render session are not connected yet. The UI says so explicitly. Building requires JDK 17, Android SDK 37, Gradle 9.6, and Android Gradle Plugin 9.4.1. A Gradle wrapper binary is intentionally not committed until generated and verified from the official distribution. Android 17 users must grant local-device access before unrestricted discovery or advertising starts.
