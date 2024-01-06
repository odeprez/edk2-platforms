/** @file
  SSDT Pcie Table Generator.

  Copyright (c) 2021, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <IndustryStandard/Acpi.h>

#include <Library/AcpiLib.h>
#include <Library/AmlLib/AmlLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>

#include <PcieIortGenerator.h>
#include <SgiPlatform.h>

extern CHAR8 pciessdttemplate_aml_code[];

STATIC
EFI_STATUS
EFIAPI
UpdatePcieDeviceInfo (
    IN         AML_ROOT_NODE_HANDLE             RootNodeHandle,
    IN   CONST SGI_PCIE_CONFIG_TABLE    * CONST Config
    )
{
  EFI_STATUS                Status;
  AML_OBJECT_NODE_HANDLE    NameOpNode;

  Status = AmlFindNode (
      RootNodeHandle,
      "\\_SB_.PCI0._SEG",
      &NameOpNode
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpUpdateInteger (NameOpNode, (UINT64) Config->Segment);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlFindNode (
      RootNodeHandle,
      "\\_SB_.PCI0._BBN",
      &NameOpNode
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpUpdateInteger (NameOpNode, (UINT64) Config->Device.Bus.Address);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlFindNode (
      RootNodeHandle,
      "\\_SB_.PCI0._UID",
      &NameOpNode
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpUpdateInteger (NameOpNode, (UINT64) Config->Index);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
UpdateCrsInfo (
    IN         AML_ROOT_NODE_HANDLE             RootNodeHandle,
    IN   CONST SGI_PCIE_CONFIG_TABLE    * CONST Config
    )
{
  EFI_STATUS                Status;
  AML_OBJECT_NODE_HANDLE    NameOpCrsNode;
  AML_DATA_NODE_HANDLE      WordBusNumber;
  AML_DATA_NODE_HANDLE      QWordMemory;

  Status = AmlFindNode (RootNodeHandle, "\\_SB_.PCI0._CRS", &NameOpCrsNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpGetFirstRdNode (NameOpCrsNode, &WordBusNumber);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = AmlUpdateRdWord (
          WordBusNumber,
          Config->Device.Bus.Address,
          Config->Device.Bus.Size
          );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpGetNextRdNode (WordBusNumber, &QWordMemory);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = AmlUpdateRdQWord (
          QWordMemory,
          Config->Device.MmioL.Address,
          Config->Device.MmioL.Size,
          Config->Translation
          );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpGetNextRdNode (QWordMemory, &QWordMemory);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = AmlUpdateRdQWord (
          QWordMemory,
          Config->Device.MmioH.Address,
          Config->Device.MmioH.Size,
          0
          );

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
UpdateEcamInfo (
    IN         AML_ROOT_NODE_HANDLE             RootNodeHandle,
    IN   CONST SGI_PCIE_CONFIG_TABLE    * CONST Config
    )
{
  EFI_STATUS                Status;
  AML_OBJECT_NODE_HANDLE    NameOpCrsNode;
  AML_DATA_NODE_HANDLE      QWordMemory;

  Status = AmlFindNode (RootNodeHandle, "\\_SB.PCI0.RES0._CRS", &NameOpCrsNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpGetFirstRdNode (NameOpCrsNode, &QWordMemory);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = AmlUpdateRdQWord (
          QWordMemory,
          Config->Device.Ecam.Address,
          Config->Device.Ecam.Size,
          0
          );

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
UpdatePcieDeviceName (
    IN         AML_ROOT_NODE_HANDLE             RootNodeHandle,
    IN   CONST SGI_PCIE_CONFIG_TABLE    * CONST Config
    )
{
  EFI_STATUS                Status;
  AML_OBJECT_NODE_HANDLE    DeviceNode;

  Status = AmlFindNode (RootNodeHandle, "\\_SB.PCI0", &DeviceNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return AmlDeviceOpUpdateName (DeviceNode, (CHAR8 *)Config->Name);
}

STATIC
EFI_STATUS
EFIAPI
GenerateAndInstallPcieSsdt (
    IN  CONST SGI_PCIE_CONFIG_TABLE  * CONST Config,
    IN  CONST UINTN                          Count
  )
{
  EFI_STATUS                    Status;
  EFI_STATUS                    FreeStatus;
  UINTN                         TableHandle;
  UINTN                         Idx;
  EFI_ACPI_DESCRIPTION_HEADER * PcieSsdtTable;
  EFI_ACPI_TABLE_PROTOCOL     * AcpiProtocol;
  AML_ROOT_NODE_HANDLE          RootNodeHandle;

  ASSERT (Config != NULL);

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID**)&AcpiProtocol
                  );
  if (EFI_ERROR(Status)) {
    DEBUG ((
        DEBUG_ERROR,
        "PCIE SSDT Table generation failed\n"
        "Failed to install AcpiProtocol, Status = %r\n",
        Status
        ));
  }

  for (Idx = 0; Idx < Count; Idx++) {
    Status = AmlParseDefinitionBlock(
        (EFI_ACPI_DESCRIPTION_HEADER *) pciessdttemplate_aml_code,
        &RootNodeHandle
        );
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "PCIE SSDT Table generation failed\n"
            "Failed to parse PcieSsdtTemplate, Status = %r\n",
            Status
            ));
      goto exit_handler;
    }

    Status = UpdatePcieDeviceInfo (RootNodeHandle, &Config[Idx]);
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "PCIE SSDT Table generation failed\n"
            "Failed to update PCI device info in template, Status = %r\n",
            Status
            ));
      goto exit_handler;
    }

    Status = UpdateCrsInfo (RootNodeHandle, &Config[Idx]);
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "PCIE SSDT Table generation failed\n"
            "Failed to update CSR info in template, Status = %r\n",
            Status
            ));
      goto exit_handler;
    }

    Status = UpdateEcamInfo (RootNodeHandle, &Config[Idx]);
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "PCIE SSDT Table generation failed\n"
            "Failed to update ECAM in RES0 info in template, Status = %r\n",
            Status
            ));
      goto exit_handler;
    }

    Status = UpdatePcieDeviceName (RootNodeHandle, &Config[Idx]);
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "PCIE SSDT Table generation failed\n"
            "Failed to update name to %s in template, Status = %r\n",
            Config[Idx].Name,
            Status
            ));
      goto exit_handler;
    }

    Status = AmlSerializeDefinitionBlock (
        RootNodeHandle,
        &PcieSsdtTable
        );
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "PCIE SSDT Table generation failed\n"
            "Failed to serialize the table: %s, Status = %r\n",
            Config[Idx].Name,
            Status
            ));
      goto exit_handler;
    }

    Status = AcpiProtocol->InstallAcpiTable (
        AcpiProtocol,
        PcieSsdtTable,
        PcieSsdtTable->Length,
        &TableHandle
        );
    if (EFI_ERROR (Status)) {
      DEBUG ((
            DEBUG_ERROR,
            "Failed to install PCIE SSDT table. error: %d\n",
            Status
            ));
    } else {
      DEBUG ((
            DEBUG_ERROR,
            "Installed PCIE SSDT table\n"
            ));
    }

exit_handler:
    if (RootNodeHandle != NULL) {
      FreeStatus = AmlDeleteTree (RootNodeHandle);
      if (EFI_ERROR (FreeStatus)) {
        DEBUG ((
              DEBUG_ERROR,
              "PCIE SSDT Table generation failed\n"
              "Failed to Cleanup AML tree, Status = %r\n",
              FreeStatus
              ));

        if (!EFI_ERROR (Status)) {
          Status = FreeStatus;
        }
      }

      if (EFI_ERROR (Status)) {
        break;
      }
    }
  }

  return Status;
}

EFI_STATUS
EFIAPI
PcieTableGeneratorEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS Status;
  SGI_PCIE_CONFIG_TABLE Config;
  VOID *PcieMmapTableHob;
  SGI_PCIE_IO_BLOCK_LIST *IoBlockList;
  SGI_PCIE_IO_BLOCK *IoBlock;
  UINT8 Idx = 0;
  UINTN LoopRootPort;
  UINTN LoopIoBlock;

  PcieMmapTableHob = GetFirstGuidHob (&gArmSgiPcieMmapTablesGuid);
  if (PcieMmapTableHob != NULL) {
    IoBlockList =
      (SGI_PCIE_IO_BLOCK_LIST *)GET_GUID_HOB_DATA (PcieMmapTableHob);
    IoBlock = IoBlockList->IoBlocks;
    for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount;
         LoopIoBlock++) {
      SGI_PCIE_DEVICE *RootPorts = IoBlock->RootPorts;
      for (LoopRootPort = 0; LoopRootPort < IoBlock->Count; LoopRootPort++) {
        if (RootPorts[LoopRootPort].Ecam.Size != 0) {
          AsciiSPrint ((CHAR8 *)Config.Name, sizeof (Config.Name), "PCI%d", Idx);
          Config.Index = Idx;
          Config.Device = RootPorts[LoopRootPort];
          Config.Segment = IoBlock->Segment;
          Config.Translation = IoBlock->Translation;
          Status = GenerateAndInstallPcieSsdt (&Config, 1);
          if (EFI_ERROR(Status)) {
            return Status;
          }
          Idx++;
        }
      }
      IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
                  sizeof (SGI_PCIE_IO_BLOCK) +
                  (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
    }

    Status = GenerateAndInstallIortTable(IoBlockList);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
