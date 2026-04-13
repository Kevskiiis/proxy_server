function FindProxyForURL(url, host) {
    // Replace YOUR_AZURE_VM_IP with your VM's public IP or DNS
    var proxy = "PROXY YOUR_AZURE_VM_IP:443";
    // Example: var proxy = "PROXY 20.30.40.50:443";

    // Route all traffic through the proxy
    return proxy;
}
