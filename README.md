# ESP32 MAX30102 Porting Notes 
> Platform: ESP-IDF v5.5.4 | Sensor: MAX30102 | MCU: ESP32

---

## Project Structure

```
project/
├── CMakeLists.txt                        # Top-level, includes IDF
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   └── task_max30102.c
└── components/
    └── max30102/
        ├── CMakeLists.txt
        ├── max30102.c / max30102.h       # Sensor driver 
        ├── i2c_wrapper.c / i2c_wrapper.h # HAL layer 
        └── algorithm.c / algorithm.h     # Heart rate / SpO2 algorithm
```

---

## CMakeLists.txt Rules

ESP-IDF wraps standard CMake with its own macro `idf_component_register()`.

```cmake
idf_component_register(
    SRCS         "file1.c" "file2.c"   # Must list every .c file explicitly, with extension
    INCLUDE_DIRS "."                    # Header search path, auto-exposed to dependants
    REQUIRES     esp_driver_i2c        # Component dependencies (affects linking + headers)
)
```

**Key rules:**
- `SRCS` — every `.c` file must be listed **with** the `.c` extension. Missing extension = CMake cannot find the file.
- `INCLUDE_DIRS` — paths declared here are automatically inherited by any component that `REQUIRES` this one. This is why `#include "max30102.h"` works in `main/` without a full path.
- `REQUIRES` — if you call a function from another component, you must declare it here. Missing dependency = `undefined reference` at link time.

### Bugs Fixed

| File | Bug | Fix |
|---|---|---|
| `components/max30102/CMakeLists.txt` | `"i2c_wrapper"` missing `.c` extension | `"i2c_wrapper.c"` |
| `components/max30102/CMakeLists.txt` | `algorithm.c` not listed in `SRCS` | Add `"algorithm.c"` |
| `main/CMakeLists.txt` | `max30102` component not in `REQUIRES` | Add `max30102` to `REQUIRES` |

---

## I2C Wrapper  

### Mbed vs ESP-IDF API Mapping



These are replaced 1-to-1 with:
```c
esp_err_t i2c_write_max(uint8_t addr, uint8_t *data, uint8_t len, bool repeated);
esp_err_t i2c_read_max (uint8_t addr, uint8_t *data, uint8_t len, bool repeated);
```

### Device Address Design

MAX30102 uses a **split address scheme** defined in `max30102.h`:

```c
#define I2C_WRITE_ADDR  0xAE   // 7-bit (0x57) << 1 | 0 (Write)
#define I2C_READ_ADDR   0xAF   // 7-bit (0x57) << 1 | 1 (Read)
```

The device address is handled at the **`max30102.c` layer**, not inside `i2c_wrapper`.  
`i2c_wrapper` receives the full 8-bit address and sends it as-is onto the bus.


### I2C Bus Sequence

| Call in max30102.c | Bus sequence |
|---|---|
| `write_reg` (len=2) | `START → 0xAE → reg_addr → reg_data → STOP` |
| `read_reg` write phase (len=1, repeated=true) | `START → 0xAE → reg_addr` (no STOP) |
| `read_reg` read phase (len=1) | `START → 0xAF → data → STOP` |
| `read_fifo` write phase (len=1, repeated=true) | `START → 0xAE → REG_FIFO_DATA` (no STOP) |
| `read_fifo` read phase (len=6) | `START → 0xAF → 6 bytes → STOP` |

### Why the Last Read Byte Must NACK

```c
i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);      // bytes 1..N-1: ACK = "keep sending"
i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK); // byte N: NACK = "I'm done, stop"
```

ACK tells the slave to continue sending. NACK on the last byte signals end of transfer. Without it, the slave does not know when to stop.

---



## algorithm.h — Declaration vs Definition

### The Problem

Defining arrays in a header file causes `multiple definition` link errors when the header is included by more than one `.c` file.

```c
// ❌ algorithm.h — causes multiple definition at link time
const uint16_t auw_hamm[31] = { 41, 276, ... };
const uint8_t  uch_spo2_table[184] = { 95, 95, ... };
```

### The Fix

```c
// ✅ algorithm.h — declaration only
extern const uint16_t auw_hamm[31];
extern const uint8_t  uch_spo2_table[184];

// ✅ algorithm.c — one definition
const uint16_t auw_hamm[31] = { 41, 276, ... };
const uint8_t  uch_spo2_table[184] = { 95, 95, ... };
```

**Rule:** Header files describe *what exists*. Source files define *what it is*.

`extern` tells the compiler: "this symbol exists but is defined elsewhere — the linker will find it."


---



## Array vs Pointer

In C, an array name is a pointer to its first element:

```c
uint8_t ach_i2c_data[2];

// These are equivalent
ach_i2c_data        // pointer to ach_i2c_data[0]
&ach_i2c_data[0]    // same

// Passing to a function expecting uint8_t* — automatic conversion
i2c_write_max(addr, ach_i2c_data, 2, false);  // no cast needed (same type)
i2c_write_max(addr, (uint8_t*)ach_i2c_data, 2, false);  // cast needed if char[]
```

Because the function cannot know the array length from the pointer alone, `len` must always be passed explicitly.

---

## Reading Linker Error Messages

| Error message pattern | Cause |
|---|---|
| `multiple definition` + points to `.h` file | Array/variable defined in a header |
| `undefined reference to X` | Function declared but not defined, or `.c` missing from `SRCS` |
| `cannot find source file` | Filename typo or wrong extension in `CMakeLists.txt` |
| `fatal error: X.h: No such file or directory` | Component missing from `REQUIRES`, or wrong header path |

---
