# Network-Emulation-SDK Ruby SDK

`gem install network-emulation-sdk`

Windows x64 Ruby3.1+.

```ruby
require "network_emulation_sdk"
NetworkEmulationSDKSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
