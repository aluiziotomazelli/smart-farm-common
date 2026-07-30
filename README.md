# smart-farm-common

Shared base component for the Smart Farm project. Used as a Git submodule in application repositories.
This repository acts as a single ESP-IDF component providing shared interfaces, types, and persistence logic across all apps.

## Features

- **Protocol Types**: Shared data structures and protocol definitions (`farm_protocol_types.hpp`, `core_types.hpp`)
- **Persistence Backend**: Non-Volatile Storage (NVS) and RTC abstraction layer with HAL dependency injection support (`IPersistenceBackend`)
- **OTA Triggers**: Interfaces and implementations for triggering Over-The-Air updates (e.g., via button press or ESP-NOW)

## Usage

Add as a submodule in your application repository:

```bash
git submodule add https://github.com/aluiziotomazelli/smart-farm-common components/smart-farm-common
cd components/smart-farm-common && git checkout v1.0.0
```

Then in your app's `CMakeLists.txt`, add the root of the submodule to your extra components directory:

```cmake
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/smart-farm-common"
)
```

And in your component's `CMakeLists.txt` (like `main/CMakeLists.txt`), add it to your `REQUIRES`:

```cmake
    REQUIRES
        smart-farm-common
```

## Host Tests

```bash
cd host_test/test_nvs_core
. $HOME/dev/esp/esp-idf/export.sh
idf.py --preview set-target linux
idf.py build
./build/test_nvs_core.elf
```
