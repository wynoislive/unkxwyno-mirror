using UnkxwynoMirror.Desktop.Models;
using UnkxwynoMirror.Desktop.Services;

var failures = new List<string>();
void Check(bool condition, string message)
{
    if (!condition)
    {
        failures.Add(message);
    }
}

Check(QualityCatalog.Resolutions.Select(item => item.Value).SequenceEqual(
    Enum.GetValues<ResolutionChoice>()), "Every declared resolution must be visible exactly once.");
Check(QualityCatalog.FrameRates.Select(item => item.Value).SequenceEqual(
    Enum.GetValues<FrameRateChoice>()), "Every declared frame rate must be visible exactly once.");
Check(QualityCatalog.Resolutions.Any(item => item.Label.Contains("3840 \u00d7 2160", StringComparison.Ordinal)),
    "The free UHD option must expose its real dimensions.");
Check(QualityCatalog.Resolutions.All(item =>
        !item.Label.Contains('\u00c2') && !item.Label.Contains('\u00c3') && !item.Label.Contains('\ufffd')),
    "Quality labels must not contain common UTF-8 mojibake markers.");
Check(QualityCatalog.FrameRates.Any(item => item.Value == FrameRateChoice.Fps120),
    "The free 120 FPS request must exist.");

var selection = new QualitySelection
{
    RequestedResolution = ResolutionChoice.Uhd,
    RequestedFrameRate = FrameRateChoice.Fps120,
};
Check(selection.ActiveLabel == "\u2014", "A request must not create a fake active profile.");
Check(selection.RequestedLabel.Contains("4K / UHD") && selection.RequestedLabel.Contains("120 FPS"),
    "Requested resolution and FPS must remain independently visible.");
selection.ApplyActive("1920 \u00d7 1080", 60);
Check(selection.RequestedLabel.Contains("4K / UHD"), "Actual fallback must not overwrite the request.");
Check(selection.ActiveLabel == "1920 \u00d7 1080 \u00b7 60 FPS", "Actual delivery must be reported separately.");
selection.ClearActive("Transport unavailable.");
Check(selection.ActiveLabel == "\u2014" && selection.Limitation == "Transport unavailable.",
    "A stopped stream must clear active claims and retain the technical reason.");

Check(LocalEndpointValidator.TryValidate("192.168.1.20:49791", out _),
    "A private IPv4 endpoint must be accepted.");
Check(LocalEndpointValidator.TryValidate("[fd12:3456::8]:49791", out _),
    "An IPv6 unique-local endpoint must be accepted.");
Check(LocalEndpointValidator.TryValidate("phone.local:49791", out _),
    "An mDNS local host name must be accepted.");
Check(!LocalEndpointValidator.TryValidate("8.8.8.8:49791", out _),
    "A public IPv4 endpoint must be rejected.");
Check(!LocalEndpointValidator.TryValidate("example.com:49791", out _),
    "A public DNS host name must be rejected.");
Check(!LocalEndpointValidator.TryValidate("192.168.1.20", out _),
    "A manual endpoint without a port must be rejected.");

var query = MdnsPacketParser.CreatePtrQuery("_unkxwyno._tcp.local.", requestUnicastResponse: true);
Check(query.Length > 24 && query[4] == 0 && query[5] == 1,
    "DNS-SD discovery must emit exactly one bounded PTR question.");
Check(query[^4] == 0 && query[^3] == 12 && query[^2] == 0x80 && query[^1] == 1,
    "DNS-SD discovery must request a unicast PTR response while remaining mDNS compatible.");
var truncatedPacket = MdnsPacketParser.Parse([0, 1, 2], "_unkxwyno._tcp.local.");
Check(truncatedPacket.Pointers.Count == 0 && truncatedPacket.Services.Count == 0,
    "Truncated DNS packets must fail closed without throwing.");
Check(MdnsPacketParser.DisplayName("Living Room._unkxwyno._tcp.local.", "_unkxwyno._tcp.local.") == "Living Room",
    "DNS-SD instance display names must exclude the service suffix.");

static void AddUInt16(List<byte> bytes, ushort value)
{
    bytes.Add((byte)(value >> 8));
    bytes.Add((byte)value);
}
static void AddUInt32(List<byte> bytes, uint value)
{
    AddUInt16(bytes, (ushort)(value >> 16));
    AddUInt16(bytes, (ushort)value);
}
static byte[] EncodeName(string name)
{
    var bytes = new List<byte>();
    foreach (var label in name.TrimEnd('.').Split('.'))
    {
        var encoded = System.Text.Encoding.UTF8.GetBytes(label);
        bytes.Add((byte)encoded.Length);
        bytes.AddRange(encoded);
    }
    bytes.Add(0);
    return bytes.ToArray();
}

var serviceName = EncodeName("_unkxwyno._tcp.local.");
var instanceName = EncodeName("Living Room._unkxwyno._tcp.local.");
var hostName = EncodeName("phone.local.");
var response = new List<byte>();
AddUInt16(response, 0);
AddUInt16(response, 0x8400);
AddUInt16(response, 0);
AddUInt16(response, 2);
AddUInt16(response, 0);
AddUInt16(response, 0);
response.AddRange(serviceName);
AddUInt16(response, 12);
AddUInt16(response, 1);
AddUInt32(response, 120);
AddUInt16(response, (ushort)instanceName.Length);
response.AddRange(instanceName);
response.AddRange(instanceName);
AddUInt16(response, 33);
AddUInt16(response, 1);
AddUInt32(response, 120);
AddUInt16(response, (ushort)(6 + hostName.Length));
AddUInt16(response, 0);
AddUInt16(response, 0);
AddUInt16(response, 49791);
response.AddRange(hostName);
var parsedResponse = MdnsPacketParser.Parse(response.ToArray(), "_unkxwyno._tcp.local.");
Check(parsedResponse.Pointers is [{ InstanceName: "Living Room._unkxwyno._tcp.local.", TimeToLive: 120 }],
    "DNS-SD must parse a matching PTR record and preserve its TTL.");
Check(parsedResponse.Services is [{ HostName: "phone.local.", Port: 49791, TimeToLive: 120 }],
    "DNS-SD must parse the matching SRV target and port.");

var testRoot = Path.Combine(Path.GetTempPath(), "unkxwyno-desktop-tests", Guid.NewGuid().ToString("N"));
try
{
    var settingsPath = Path.Combine(testRoot, "settings.json");
    var store = new SettingsStore(settingsPath);
    var missing = store.Load();
    Check(!missing.Loaded && missing.Warning is null &&
          missing.Settings.RequestedResolution == ResolutionChoice.Auto,
        "A missing settings file must load safe defaults without reporting corruption.");

    var persisted = new DesktopSettings
    {
        RequestedResolution = ResolutionChoice.Uhd,
        RequestedFrameRate = FrameRateChoice.Fps120,
    };
    Check(store.TrySave(persisted, out var saveError) && saveError is null,
        "Requested quality settings must save atomically.");
    var settingsJson = File.ReadAllText(settingsPath);
    Check(!settingsJson.Contains("active", StringComparison.OrdinalIgnoreCase) &&
          !settingsJson.Contains("actual", StringComparison.OrdinalIgnoreCase),
        "Persisted settings must never include unmeasured active or actual stream claims.");
    Check(!Directory.EnumerateFiles(testRoot, "*.tmp").Any(),
        "A successful atomic settings save must not leave temporary files.");
    var loaded = store.Load();
    Check(loaded.Loaded && loaded.Warning is null && loaded.Settings == persisted,
        "Saved requested resolution and frame rate must round-trip exactly.");

    File.WriteAllText(settingsPath, "{ invalid json");
    var invalid = store.Load();
    Check(!invalid.Loaded && invalid.Warning is not null &&
          invalid.Settings.RequestedResolution == ResolutionChoice.Auto &&
          invalid.Settings.RequestedFrameRate == FrameRateChoice.Auto,
        "Malformed settings must fail closed to safe defaults with a visible warning.");

    var blockedDirectory = Path.Combine(testRoot, "not-a-directory");
    File.WriteAllText(blockedDirectory, "blocking file");
    var blockedStore = new SettingsStore(Path.Combine(blockedDirectory, "settings.json"));
    Check(!blockedStore.TrySave(persisted, out var blockedError) && blockedError is not null,
        "An unavailable settings directory must return a recoverable error instead of throwing.");

    var diagnosticsLog = new AppDiagnosticsLog(Path.Combine(testRoot, "logs", "desktop.jsonl"));
    Check(diagnosticsLog.TryWrite("Info", "settings.saved", "Requested quality saved.", out var logError) &&
          logError is null,
        "Local diagnostics events must be writable without affecting product state.");
    var logLines = File.ReadAllLines(diagnosticsLog.LogPath);
    Check(logLines.Length == 1, "Each diagnostics event must occupy exactly one JSON line.");
    using var logDocument = System.Text.Json.JsonDocument.Parse(logLines[0]);
    Check(logDocument.RootElement.GetProperty("eventName").GetString() == "settings.saved",
        "Diagnostics JSON must preserve the stable event name.");
}
finally
{
    if (Directory.Exists(testRoot))
    {
        Directory.Delete(testRoot, recursive: true);
    }
}

using (var discovery = new LocalDiscoveryService())
{
    var enumeration = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
    string? discoveryError = null;
    discovery.EnumerationCompleted += (_, _) => enumeration.TrySetResult();
    discovery.DiscoveryError += (_, message) =>
    {
        discoveryError = message;
        enumeration.TrySetResult();
    };
    discovery.Start();
    var completed = await Task.WhenAny(enumeration.Task, Task.Delay(TimeSpan.FromSeconds(6)));
    discovery.Stop();
    Check(completed == enumeration.Task,
        "Local DNS-SD discovery must complete its initial bounded observation window.");
    Check(discoveryError is null,
        $"Local DNS-SD discovery must bind and query this Windows host: {discoveryError}");
    Check(!discovery.IsRunning,
        "Stopping local DNS-SD discovery must synchronously clear its running state.");
}

if (args.Length > 0)
{
    var probe = IndependentCoreProbe.Probe(Path.GetFullPath(args[0]));
    Check(probe.Available && probe.AbiVersion == 1 && !string.IsNullOrWhiteSpace(probe.Version),
        $"The packaged independent core must load through its actual C ABI: {probe.Explanation}");
}

await UsbMirrorSessionTests.RunAsync();

if (failures.Count == 0)
{
    Console.WriteLine("All desktop contract tests passed.");
    return 0;
}

foreach (var failure in failures)
{
    Console.Error.WriteLine($"FAIL: {failure}");
}
return 1;

