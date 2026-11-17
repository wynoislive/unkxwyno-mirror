using System.Runtime.InteropServices;

namespace UnkxwynoMirror.Desktop.Services;

public sealed record CoreProbeResult(bool Available, uint AbiVersion, string Version, string Explanation);

public static class IndependentCoreProbe
{
    public static CoreProbeResult Probe(string? baseDirectory = null)
    {
        var directory = baseDirectory ?? AppContext.BaseDirectory;
        var libraryPath = Path.Combine(directory, "unkxwyno_core.dll");
        if (!File.Exists(libraryPath))
        {
            return new(false, 0, string.Empty,
                "Independent core is not in this build. Discovery can run, but streaming stays unavailable.");
        }

        nint handle = 0;
        try
        {
            if (!NativeLibrary.TryLoad(libraryPath, out handle))
            {
                return new(false, 0, string.Empty, "Independent core exists but Windows could not load it.");
            }

            var abiExport = NativeLibrary.GetExport(handle, "uxm_core_abi_version");
            var versionExport = NativeLibrary.GetExport(handle, "uxm_core_version_string");
            var abi = Marshal.GetDelegateForFunctionPointer<AbiVersionDelegate>(abiExport)();
            var versionPointer = Marshal.GetDelegateForFunctionPointer<VersionStringDelegate>(versionExport)();
            var version = Marshal.PtrToStringUTF8(versionPointer) ?? "unknown";
            if (abi != 1)
            {
                return new(false, abi, version, $"Independent core ABI {abi} is incompatible with UI ABI 1.");
            }

            return new(true, abi, version,
                $"Independent core {version} is loaded. Native media adapters are still capability-gated.");
        }
        catch (Exception exception)
        {
            return new(false, 0, string.Empty, $"Independent core probe failed: {exception.Message}");
        }
        finally
        {
            if (handle != 0)
            {
                NativeLibrary.Free(handle);
            }
        }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint AbiVersionDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate nint VersionStringDelegate();
}

