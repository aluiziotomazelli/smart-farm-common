# smart-farm-common

Shared components for the Smart Farm project. Used as a Git submodule in application repositories.

## Components

| Component | Description |
| :--- | :--- |
| `common` | Shared protocol types (`farm_protocol_types.hpp`), HAL sleep abstraction |
| `nvs_core` | Non-Volatile Storage abstraction layer with HAL and dependency injection support |

## Usage

Add as a submodule in your application repository:

```bash
git submodule add https://github.com/aluiziotomazelli/smart-farm-common components/smart-farm-common
cd components/smart-farm-common && git checkout v1.0.0
```

Then in your app's `CMakeLists.txt`:

```cmake
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/smart-farm-common/common"
    "${CMAKE_CURRENT_LIST_DIR}/components/smart-farm-common/nvs_core"
)
```

## Host Tests

```bash
cd host_test/test_nvs_core
. $HOME/esp/esp-idf/export.sh
idf.py --preview set-target linux
idf.py build
./build/test_nvs_core.elf
```
