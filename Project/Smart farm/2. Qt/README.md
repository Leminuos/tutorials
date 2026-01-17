## install qtcreator

```bash
sudo apt install -y qtcreator qtbase5-dev qt5-qmake cmake
```

## Tools | Options | Kits | Kits

```bash
CMAKE_CXX_COMPILER:STRING=%{Compiler:Executable:Cxx}
CMAKE_C_COMPILER:STRING=%{Compiler:Executable:C}
CMAKE_PREFIX_PATH:STRING=%{Env:OECORE_TARGET_SYSROOT}/usr
CMAKE_TOOLCHAIN_FILE:STRING=%{Env:OE_CMAKE_TOOLCHAIN_FILE}
QT_QMAKE_EXECUTABLE:STRING=%{Qt:qmakeExecutable}
```

## run QT5_SmartFarm manual

```bash
export QT_QPA_FONTDIR=/usr/share/fonts/truetype
/opt/QT5_SmartFarm -platform linuxfb -plugin=tslib
```

## add packages to yocto: `build/conf/local.conf/local`

```bash
IMAGE_INSTALL:append = " qtbase fontconfig ttf-dejavu-sans libgpiod"
PACKAGECONFIG_DISTRO:pn-qtbase = " linuxfb tslib fontconfig"
```