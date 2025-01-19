# uefi-trampoline

`uefi-trampoline` is an UEFI OptionROM which loads over HTTP an additional EFI payload and gives control to it.
It uses the `EfiLoadFileProtocol` installed on a network device to fetch the binary and executes it through `LoadImage`
Boot Services function.


## Build

Before building, the address of the HTTP backend from which the EFI image is fetched needs to be configured in `PAYLOAD_URI`
variable in `rom.c`. Option Rom EFI image can be built with [tianocore/edk2](https://github.com/tianocore/edk2), by deploying the src directory
under Tianocore root and referencing `OptionROM.inf` from the dsc file of your build (e.g. `OvmfPkg/OvmfPkgX64.dsc`). Once the Efi
image is available, it has to be wrapped in to the appropriate OptionROM headers through `EfiRom` tool, also availble as part of
tianocore/edk2.

```
BaseTools/Source/C/bin/EfiRom -e ./Build/OvmfX64/DEBUG_GCC5/X64/OptionROM.efi -o rom.bin -f 0x1af4  -i 0x1000
```

`-f` specifies the Vendor ID and `-i` the device ID. In these example, we are targeting `qemu` virtio device.


## Build a second stage payload

The Option Rom is designed to download over HTTP an external EFI image. For testing purposes, this EFI image
could just be a "Hello world" binary built on top of `gnu-efi`:

```
#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
   InitializeLib(ImageHandle, SystemTable);
   Print(L"Hello, world!\n");

   return EFI_SUCCESS;
}
```

You can find a complete tutorial on how to build this binary [here](https://www.rodsbooks.com/efi-programming/hello.html).

## Run

The fastest way to test the Option Rom is through `qemu`:

```
qemu-system-x86_64  \
    -netdev user,id=mynet0  
    -device virtio-net-pci,netdev=mynet0,bootindex=0,romfile=rom.bin 
    -bios /usr/share/edk2/ovmf/OVMF_CODE.fd 
    -debugcon file:debug.log -global isa-debugcon.iobase=0x402
```

where `rom.bin` is the output of `EfiRom` tool.
