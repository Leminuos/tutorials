Trong dự án ESP-IDF thì ta sẽ sử dụng cmake để thực hiện build system. Đây là cây thư mục đơn giản của một dự án esp-idf cần có:

```
my_app/
├── CMakeLists.txt                  <--- project level
├── sdkconfig
├── main/
│   ├── CMakeLists.txt              <--- component level
│   └── main.c
└── components/
    └── my_driver/
        ├── CMakeLists.txt          <--- component level
        ├── include/
        │   └── my_driver.h
        └── my_driver.c
```

Trong đó, các file `CMakeLists.txt` trong dự án ESP-IDF sẽ được chia thành hai phần là project level và component level. Bây giờ, ta sẽ đi chi tiết vào từng phần.

## Project level

File `CMakeLists.txt` sẽ nằm ở thư mục gốc project. Mục tiêu của file này là khai báo project ESP-IDF và thêm các external component.

Ví dụ tối thiểu của file này như sau:

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

`IDF_PATH` là biến môi trường trỏ tới thư mục cài đặt ESP-IDF.

Ví dụ:
- Windows: `IDF_PATH=C:/Espressif/frameworks/esp-idf-v5.4`
- Linux/macOS: `IDF_PATH=~/esp/esp-idf`

File `project.cmake` phải được include vì nó sẽ thực hiện:
- Thiết lập toolchain & target:
  - Xác định target: esp32, esp32s3, esp32c3, …
  - Chọn compiler và set các compiler option.
- Định nghĩa macro `idf_component_register`
- Thiết lập cơ chế quét `main/`, `components/`, `EXTRA_COMPONENT_DIRS`
- Load và parse `sdkconfig` và sinh file `sdkconfig.h`.
- ...

:::warning Chú ý
Nếu bỏ dòng include này thì CMake không biết compile cho project ESP-IDF như thế nào.
:::

Các trường hợp sử dụng `EXTRA_COMPONENT_DIRS` để thêm các external component như sau:

**Case 1: Component nằm ngoài project**

Giả sử bạn có một repo/thư mục dùng chung là `shared_components` chứa nhiều component.

```
workspace/
├── shared_components/
│   └── my_driver/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── my_driver.h
│       └── my_driver.c
└── my_app/
    ├── CMakeLists.txt
    ├── sdkconfig
    └── main/
        ├── CMakeLists.txt
        └── main.c
```

File `workspace/my_app/CMakeLists.txt` sẽ có nội dung:

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../shared_components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

**Case 2: Component nằm ở folder khác `components/` trong project**

Giả sử một cây thư mục như sau:

```
my_app/
├── CMakeLists.txt
├── sdkconfig
├── main/
│   ├── CMakeLists.txt
│   └── main.c
├── external-components/
│    └── my_crypto/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── my_crypto.h
│       └── my_crypto.c
└── components/
    └── my_driver/
        ├── CMakeLists.txt
        ├── include/
        │   └── my_driver.h
        └── my_driver.c
```

File `my_app/CMakeLists.txt` sẽ có nội dung:

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/external-components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

**Case 3: Component nằm trong folder con của folder `components/`**

Giả sử một cây thư mục như sau:

```
my_app/
├── CMakeLists.txt
├── sdkconfig
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    └── my_driver/
        ├── my_display
        │   ├── CMakeLists.txt
        │   ├── include/
        │   │   └── my_driver.h
        │   └── my_driver.c
        └── my_sensor
            ├── CMakeLists.txt
            ├── include/
            │   └── my_driver.h
            └── my_driver.c
```

File `my_app/CMakeLists.txt` sẽ có nội dung:

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/my_driver"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

:::warning Lưu ý
Folder `main` và `components` là các component mặc định nên không cần đưa vào `EXTRA_COMPONENT_DIRS`.
:::

## Component level

Component level là các file `CMakeLists.txt` nằm trong mỗi component. Trong file `CMakeLists.txt`, ta sẽ sử dụng api cmake đặc thù của ESP-IDF là `idf_component_register`, API này dùng để:
- Đăng ký một component
- Khai báo:
  - Source code
  - Header public/private
  - Dependency
  - File nhúng (embed)
- Tạo ra một CMake target cho component đó (${COMPONENT_LIB})

:::warning Chú ý
Mỗi một `CMakeLists.txt` của component phải có API `idf_component_register` thì ESP-IDF mới hiểu đây là một component và nó mới thực hiện build component đó.
:::

Cú pháp tổng quát như sau:

```cmake
idf_component_register(
    SRCS <list>
    SRC_DIRS <list>
    INCLUDE_DIRS <list>
    PRIV_INCLUDE_DIRS <list>
    REQUIRES <list>
    PRIV_REQUIRES <list>
    EMBED_FILES <list>
    EMBED_TXTFILES <list>
)
```

:::warning Chú ý
Tất cả tham số đều là tùy chọn, nhưng bắt buộc phải có source (SRCS hoặc SRC_DIRS), nếu không component rỗng.
:::

Giải thích chi tiết từng tham số:

- Tham số `SRCS`: Khai báo chính xác từng file `.c`/`.cpp` thuộc component.
  
  Ví dụ:

  ```
  components/
  └── my_driver/
      ├── my_driver.c
      ├── spi_port.c
      └── include/
          └── my_driver.h
  ```

  ```cmake
  idf_component_register(
    SRCS "my_driver.c" "spi_port.c"
    INCLUDE_DIRS "include"
  )
  ```

  Dùng tham số này khi ta không muốn build tất cả file source trong thư mục mà chỉ định file nào cần được build.

- Tham số `SRC_DIRS`: ESP-IDF tự quét tất cả source trong thư mục được chỉ định.

  ```
  components/
  └── my_driver/
      ├── core/
      │   ├── driver.c
      │   └── utils.c
      └── include/
          └── my_driver.h
  ```

  ```cmake
  idf_component_register(
    SRC_DIRS "core"
    INCLUDE_DIRS "include"
  )
  ```

  Dùng tham số này khi ta muốn build tất cả file source trong thư mục được chỉ định.

  :::warning Lưu ý
  ESP_IDF không quét đệ quy `SRC_DIRS`
  :::

- Tham số `EMBED_TXTFILES`: Nhúng file text như html, json, txt,...

  Ví dụ:

  ```
  main/
  └── assets/
      └── wifi_configuration.html
  ```

  ```cmake
  idf_component_register(
    SRCS "main.c"
    EMBED_TXTFILES "wifi_configuration.html"
  )
  ```

  Tên symbol trong `C`/`C++` được tạo theo quy tắc:
  - `_binary_<path_with_underscore>_start`
  - `_binary_<path_with_underscore>_end`
  
  ```c
  extern const unsigned char wifi_configuration_html_start[] asm("_binary_wifi_configuration_html_start");
  extern const unsigned char wifi_configuration_html_end[]   asm("_binary_wifi_configuration_html_end");
  ```
  
  :::tip
  `EMBED_TXTFILES` thường thêm `\0` ở cuối file.
  :::

- Tham số `EMBED_FILES`: Nhúng file binary như ảnh, cert DER, firmware phụ…

  Ví dụ:

  ```
  idf_component_register(
    SRCS "main.c"
    EMBED_FILES "cert.der"
  )
  ```

  Tên symbol trong `C`/`C++` được tạo theo quy tắc:
  - `<path_with_underscore>_start`
  - `<path_with_underscore>_end`

  ```c
  extern const uint8_t cert_der_start[];
  extern const uint8_t cert_der_end[];
  ```

- Tham số `INCLUDE_DIRS` và `PRIV_INCLUDE_DIRS`:
  
  Mỗi component trong ESP-IDF là một CMake library target. `INCLUDE_DIRS` và `PRIV_INCLUDE_DIRS` là cách ta gắn header file vào target theo 2 cách:
  - `INCLUDE_DIRS` ≈ `target_include_directories(component PUBLIC ...)`: Các header file mà component khác được phép include.
  - `PRIV_INCLUDE_DIRS` ≈ `target_include_directories(component PRIVATE ...)`: Header file chỉ được dùng nội bộ trong component, các component khác không thấy được.

  Giả sử, ta có một component như sau:

  ```
  components/
  └── my_driver/
      ├── include/
      │   └── my_driver.h
      └── my_driver.c
  ```

  Ví dụ cho `INCLUDE_DIRS`:

  ```cmake
  idf_component_register(
    SRCS "my_driver.c"
    INCLUDE_DIRS "include"
  )
  ```

  Component khác có thể: `#include "my_driver.h"`

  Ví dụ cho `PRIV_INCLUDE_DIRS`

  ```
  idf_component_register(
    SRCS "my_driver.c"
    PRIV_INCLUDE_DIRS "private"
  )
  ```

  Nếu component khác thực hiện `#include "my_driver.h"` thì sẽ báo fail.

  :::tip Sử dụng
  - Header file ta muốn component khác có thể include được thì đặt trong thư mục thuộc `INCLUDE_DIRS`.
  - Header file chỉ có thể dùng nội bộ trong component thì đặt trong `PRIV_INCLUDE_DIRS`.
  :::

- Tham số `REQUIRES` và `PRIV_REQUIRES`:

  `REQUIRES` và `PRIV_REQUIRES` là cách ta gắn thực hiện link dependency vào target theo 2 cách:
  - REQUIRES ≈ target_link_libraries(component PUBLIC dep)
  - PRIV_REQUIRES ≈ target_link_libraries(component PRIVATE dep)

  Khi một component export dependency dưới dạng `REQUIRES`, thì component khác link vào nó sẽ tự động có: link dependency và public header file của dependency.

  Khi một component export dependency dưới dạng `PRIV_REQUIRES` thì chỉ nội bộ component sử dụng dependency đó và những component khác link vào component này sẽ không thể sử dụng dependency đó.

  ```tip Sử dụng
  - Nếu header public (tức các file trong `INCLUDE_DIRS`) có `#include` header thuộc dependency thì dependency đó phải là `REQUIRES`.
  - Ngược lại, nếu dependency chỉ xuất hiện trong `.c`/`.cpp` nội bộ, và header public không lộ ra, thì dùng `PRIV_REQUIRES`.
  ```

Trong file `CMakeLists.txt`, ngoài `idf_component_register` thì còn có thể:

- Thêm macro compile

  ```cmake
  target_compile_definitions(${COMPONENT_LIB}
    PRIVATE USE_FAST_MODE=1
  )
  ```

- Thêm source có theo điều kiện:

  Giả sử ta có option `CONFIG_USE_LED` trong menuconfig:

  ```cmake
  if(CONFIG_USE_LED)
    target_sources(${COMPONENT_LIB} PRIVATE "led.c")
  endif()
  ```

  Hoặc theo target chip:

  ```cmake
  idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")

  if(IDF_TARGET STREQUAL "esp32s3")
      target_sources(${COMPONENT_LIB} PRIVATE "s3_camera.c")
  endif()
  ```

:::warning Chú ý
Sau `idf_component_register`, ESP-IDF tạo target cho component là ${COMPONENT_LIB} nên `idf_component_register` phải luôn được thực hiện đầu tiên.
:::