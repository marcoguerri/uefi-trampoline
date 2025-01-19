# uefi-trampoline

`uefi-trampoline` is an UEFI OptionROM which loads over HTTP an additional EFI payload and gives control to it.
It uses the `EfiLoadFileProtocol` installed on a network device to fetch the binary and executes it through `LoadImage`
Boot Services function.


## Build it

You build the Option Rom EFI image with [tianocore/edk2](https://github.com/tianocore/edk2), by simply deploying the src directory
under Tianocore root and referencing `OptionROM.inf` from the dsc file of your build (e.g. `OvmfPkg/OvmfPkgX64.dsc`). Once the Efi
image is available, it has to be wrapped in to the appropriate OptionROM headers through `EfiRom` tool, also availble as part of
tianocore/edk2.

```
BaseTools/Source/C/bin/EfiRom -e ./Build/OvmfX64/DEBUG_GCC5/X64/OptionROM.efi -o rom.bin -f 0x1af4  -i 0x1000
```

`-f` specifies the Vendor ID and `-i` the device ID. In these example, we are targeting `qemu` virtio device.


## Build a second stage payload


## Run it
