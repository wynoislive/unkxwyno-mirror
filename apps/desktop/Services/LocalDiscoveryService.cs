using System.Net;
using System.Net.Sockets;

namespace UnkxwynoMirror.Desktop.Services;

public sealed record DiscoveredPeer(string Id, string DisplayName, string Host, ushort Port);

public sealed class LocalDiscoveryService : IDisposable
{
    private const string ServiceType = "_unkxwyno._tcp.local.";
    private static readonly IPAddress MulticastAddress = IPAddress.Parse("224.0.0.251");
    private static readonly IPEndPoint MulticastEndpoint = new(MulticastAddress, 5353);
    private readonly object _gate = new();
    private readonly Dictionary<string, CacheEntry<string>> _instances = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CacheEntry<MdnsServiceTarget>> _targets = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, DiscoveredPeer> _published = new(StringComparer.OrdinalIgnoreCase);
    private CancellationTokenSource? _browseCancellation;
    private Task? _browseTask;
    private bool _disposed;

    public event EventHandler<DiscoveredPeer>? PeerAdded;
    public event EventHandler<string>? PeerRemoved;
    public event EventHandler<string>? DiscoveryError;
    public event EventHandler? EnumerationCompleted;

    public bool IsRunning
    {
        get
        {
            lock (_gate)
            {
                return _browseTask is { IsCompleted: false };
            }
        }
    }

    public void Start()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        lock (_gate)
        {
            if (_browseTask is { IsCompleted: false })
            {
                return;
            }

            _browseCancellation?.Dispose();
            _browseCancellation = new CancellationTokenSource();
            _browseTask = Task.Run(() => BrowseAsync(_browseCancellation.Token));
        }
    }

    public void Stop()
    {
        CancellationTokenSource? cancellation;
        lock (_gate)
        {
            cancellation = _browseCancellation;
            _browseCancellation = null;
            _browseTask = null;
        }
        cancellation?.Cancel();
        cancellation?.Dispose();
    }

    private async Task BrowseAsync(CancellationToken cancellationToken)
    {
        try
        {
            using var client = CreateMulticastClient();
            var query = MdnsPacketParser.CreatePtrQuery(ServiceType, requestUnicastResponse: true);
            var firstWindow = true;

            while (!cancellationToken.IsCancellationRequested)
            {
                await client.SendAsync(query, MulticastEndpoint, cancellationToken).ConfigureAwait(false);
                var listenUntil = DateTimeOffset.UtcNow.AddSeconds(firstWindow ? 3 : 5);

                while (DateTimeOffset.UtcNow < listenUntil && !cancellationToken.IsCancellationRequested)
                {
                    using var receiveWindow = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
                    receiveWindow.CancelAfter(TimeSpan.FromMilliseconds(750));
                    try
                    {
                        var result = await client.ReceiveAsync(receiveWindow.Token).ConfigureAwait(false);
                        Apply(MdnsPacketParser.Parse(result.Buffer, ServiceType), DateTimeOffset.UtcNow);
                    }
                    catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
                    {
                        PurgeExpired(DateTimeOffset.UtcNow);
                    }
                }

                if (firstWindow)
                {
                    firstWindow = false;
                    EnumerationCompleted?.Invoke(this, EventArgs.Empty);
                }
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            // Normal stop.
        }
        catch (Exception exception)
        {
            DiscoveryError?.Invoke(this, $"Local DNS-SD discovery failed: {exception.Message}");
        }
    }

    private static UdpClient CreateMulticastClient()
    {
        var client = new UdpClient(AddressFamily.InterNetwork);
        client.Client.ExclusiveAddressUse = false;
        client.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
        client.Client.Bind(new IPEndPoint(IPAddress.Any, 5353));
        client.JoinMulticastGroup(MulticastAddress);
        return client;
    }

    private void Apply(MdnsPacket packet, DateTimeOffset now)
    {
        List<DiscoveredPeer> added = [];
        List<string> removed = [];
        lock (_gate)
        {
            foreach (var pointer in packet.Pointers)
            {
                if (pointer.TimeToLive == 0)
                {
                    _instances.Remove(pointer.InstanceName);
                    continue;
                }
                _instances[pointer.InstanceName] = new(pointer.InstanceName, Expires(now, pointer.TimeToLive));
            }

            foreach (var service in packet.Services)
            {
                if (service.TimeToLive == 0)
                {
                    _targets.Remove(service.InstanceName);
                    continue;
                }
                _targets[service.InstanceName] = new(
                    new(service.HostName, service.Port),
                    Expires(now, service.TimeToLive));
            }

            Reconcile(now, added, removed);
        }
        Publish(added, removed);
    }

    private void PurgeExpired(DateTimeOffset now)
    {
        List<DiscoveredPeer> added = [];
        List<string> removed = [];
        lock (_gate)
        {
            foreach (var key in _instances.Where(item => item.Value.ExpiresAt <= now).Select(item => item.Key).ToArray())
            {
                _instances.Remove(key);
            }
            foreach (var key in _targets.Where(item => item.Value.ExpiresAt <= now).Select(item => item.Key).ToArray())
            {
                _targets.Remove(key);
            }
            Reconcile(now, added, removed);
        }
        Publish(added, removed);
    }

    private void Reconcile(DateTimeOffset now, List<DiscoveredPeer> added, List<string> removed)
    {
        var current = new Dictionary<string, DiscoveredPeer>(StringComparer.OrdinalIgnoreCase);
        foreach (var instance in _instances)
        {
            if (instance.Value.ExpiresAt <= now ||
                !_targets.TryGetValue(instance.Key, out var target) ||
                target.ExpiresAt <= now)
            {
                continue;
            }

            var displayName = MdnsPacketParser.DisplayName(instance.Key, ServiceType);
            current[instance.Key] = new(instance.Key, displayName, target.Value.HostName, target.Value.Port);
        }

        foreach (var peer in current.Values)
        {
            if (!_published.TryGetValue(peer.Id, out var previous) || previous != peer)
            {
                _published[peer.Id] = peer;
                added.Add(peer);
            }
        }
        foreach (var id in _published.Keys.Where(id => !current.ContainsKey(id)).ToArray())
        {
            _published.Remove(id);
            removed.Add(id);
        }
    }

    private void Publish(IEnumerable<DiscoveredPeer> added, IEnumerable<string> removed)
    {
        foreach (var id in removed)
        {
            PeerRemoved?.Invoke(this, id);
        }
        foreach (var peer in added)
        {
            PeerAdded?.Invoke(this, peer);
        }
    }

    private static DateTimeOffset Expires(DateTimeOffset now, uint ttlSeconds)
    {
        var boundedSeconds = Math.Min(ttlSeconds, 24U * 60U * 60U);
        return now.AddSeconds(Math.Max(1U, boundedSeconds));
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }
        Stop();
        _disposed = true;
        GC.SuppressFinalize(this);
    }

    private sealed record CacheEntry<T>(T Value, DateTimeOffset ExpiresAt);
}
