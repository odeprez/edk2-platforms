/** @file
  Discovers CXL capable device and reads out device capabilities.

  This driver locates PciIo Protocol and discovers PCIe devices with CXL.Mem
  capability. If a device with CXL.Mem capability is found then DOE capability
  is looked for. Once DOE capability is found, CDAT structures are fetched from
  the respective device.
  It also installs CXL Platform portocol, which can be used by other
  Platform drivers for capturing remote memory configurations and attributes.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - CXL Specificiation Revision 3.0, Version 0.7, Chapter 8.1.11
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Cxl.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Pci22.h>

#include "CxlDxe.h"

STATIC  EFI_EVENT     CxlEvent;
VOID                  *mPciRegistration;
STATIC  UINT32        RemoteMemCount;
//TODO: For now considered maximum 5 remote memory ranges.
//      In future it will be made dynamic.
STATIC  REMOTE_MEMORY_CONFIG    RemoteMemConfig[5];

/**
  Check whether device is ready to receive new data through DOE request.

  @param[in] Pci      PCI IO Protocol handle.
  @param[in] DoeBase  Base offset of DOE status registers in PCIe device
                      config space.

  @retval  EFI_SUCCESS   Successful read operation.
  @retval  Other         Device not ready or failed to check device status.
**/
STATIC
EFI_STATUS
IsDoeBusy (
  IN EFI_PCI_IO_PROTOCOL    *Pci,
  IN UINT32                 DoeBase
  )
{
  EFI_STATUS  Status;
  UINT32      DoeStatVal;

  Status = Pci->Pci.Read (Pci, EfiPciIoWidthUint32, DoeBase, 1, &DoeStatVal);
  if (EFI_ERROR (Status))
    return Status;

  if (DoeStatVal & DOE_STAT_DOE_BUSY)
    return EFI_ALREADY_STARTED;

  return Status;
}

/**
  Read out CDAT structure data for host memory configuration.

  From the DOE response data, various CDAT structure data are filtered out
  for host platform configuration.

  @param[in]  DoeRespCdatDat    Response data from DOE operation.
  @param[in]  Length            DOE response data length in bytes.
**/
STATIC
VOID
UpdateCdatData (
  IN UINT32 *DoeRespCdatData,
  IN UINT16 Length
  )
{
    UINT32      Index;
    CDAT_DSMAS  *Dsmas;

    // Skipping the CDAT header.
    Index = CDAT_TABLE_HEADER_SIZE;

    while (Index < Length) {
      switch (DoeRespCdatData[Index] & 0xff) {
      case CDAT_STRUCTURE_DSMAS:
        Dsmas = (CDAT_DSMAS *)(& (DoeRespCdatData [Index]));
        RemoteMemConfig[RemoteMemCount].DpaAddress = Dsmas->DpaBase;
        RemoteMemConfig[RemoteMemCount].DpaLength = Dsmas->DpaLength;
        RemoteMemCount ++;
        Index += CDAT_STRUCTURE_DSMAS_SIZE;
        break;
      default:
        break;
      }
    }

  return;
}

/**
  Receive DOE response.

  For CXL, DOE responses carry CDAT structures that hold information about
  remote memory ranges and associated attributes.
  System firmware polls the Data Object Ready bit and, provided it is Set, reads
  data from the DOE Read Data Mailbox and writes 1 to the DOE Read Data Mailbox
  to indicate a successful read.In the read process, a DWORD is read at a time.
  Data Object Header2 holds number of DW to be transferred for capturing the
  entire DOE response.

  @param[in]  Pci          PCI IO Protocol handle.
  @param[in]  DoeBase      Base offset of DOE registers in PCIe device config
                           space.
  @param[out] EntryHandle  Value of next entry in table. For CXL, table type
                           is CDAT.

  @retval  EFI_SUCCESS     Successful receiving of DOE response.
  @retval  Other           Failed receiving of DOE response.
**/
STATIC
EFI_STATUS
DoeReceiveResponse (
  IN EFI_PCI_IO_PROTOCOL   *Pci,
  IN UINT32                DoeBase,
  OUT UINT16               *EntryHandle
  )
{
  EFI_STATUS  Status;
  UINT32      DoeReadMbData = 1;
  UINT32      DoeRespCdatData[TOTAL_CDAT_ENTRY] = {};
  UINT32      DoeStat;
  UINT32      Index = 0;
  UINT16      Length;
  UINT64      Reg;
  UINT32      Val;

  Reg = DoeBase + DOE_STATUS_REG;
  Status = Pci->Pci.Read ( Pci, EfiPciIoWidthUint32, Reg, 1, &DoeStat);
  if (EFI_ERROR (Status))
    return Status;

  if ((DoeStat & DOE_STAT_DATA_OBJ_READY) != 0) {
    Index = 0;
    Reg = DoeBase + DOE_READ_DATA_MAILBOX_REG;

    // Read the DOE header.
    Status = Pci->Pci.Read (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
    if (EFI_ERROR (Status))
      return Status;

    // Write 1 to DOE Read Data Mailbox to indicate successful Read.
    Status = Pci->Pci.Write (Pci, EfiPciIoWidthUint32, Reg, 1, &DoeReadMbData);
    if (EFI_ERROR (Status))
      return Status;

    // Read the DOE Header 2 for data length.
    Status = Pci->Pci.Read (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
    if (EFI_ERROR (Status))
      return Status;

    Length = Val & DOE_DATA_OBJECT_LENGTH;
    if (Length < 2) {
      DEBUG ((DEBUG_ERROR, " DOE data read error\n"));
      return EFI_PROTOCOL_ERROR;
    }

    // Write 1 to DOE Read Data Mailbox to indicate successful Read.
    Status = Pci->Pci.Write (Pci, EfiPciIoWidthUint32, Reg, 1, &DoeReadMbData);
    if (EFI_ERROR (Status))
      return Status;

    // Read DOE read entry response header.
    Status = Pci->Pci.Read (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
    if (EFI_ERROR (Status))
      return Status;

    *EntryHandle = ((Val & CXL_DOE_TABLE_ENTRY_HANDLE) >> 16);
    // Write 1 to DOE Read Data Mailbox to indicate successful Read.
    Status = Pci->Pci.Write (Pci, EfiPciIoWidthUint32, Reg, 1, &DoeReadMbData);
    if (EFI_ERROR (Status))
      return Status;

    // Discount the length of 2DW DOE Header and 1DW Read entry response
    Length -= 3;
    while (Index < Length) {
      Status = Pci->Pci.Read (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
      if (EFI_ERROR (Status))
        return Status;

      DoeRespCdatData[Index] = Val;
      Index++;
      // Write 1 to DOE Read Data Mailbox to indicate successful Read.
      Status = Pci->Pci.Write (
                          Pci,
                          EfiPciIoWidthUint32,
                          Reg,
                          1,
                          &DoeReadMbData
                          );
      if (EFI_ERROR (Status))
        return Status;
    }

    UpdateCdatData (DoeRespCdatData, Length);
  }

  return Status;
}

/**
  Make DOE request to fetch CDAT structures and receive DOE response.

  This function requests for DOE objects and receives response for the same.
  The steps include -
  1. System firmware checks that the DOE Busy bit is Clear.
  2. System firmware writes entire data object a DWORD(4 Bytes) at a time via
     DOE Write Data Mailbox register.
  3. System firmware writes 1b to the DOE Go bit.
  4. The DOE instance consumes the DOE request from the DOE mailbox.
  5. The DOE instance generates a DOE Response and Sets Data Object Ready bit.
  6. System firmware polls the Data Object Ready bit and, provided it is Set,
     reads data from the DOE Read Data Mailbox and writes 1 to the DOE Read
     Data Mailbox to indicate a successful read, a DWORD at a time until the
     entire DOE Response is read.
  7: DOE requests are made until response for last CDAT table entry is received.

  @param[in] Pci      PCI IO Protocol handle.
  @param[in] DoeBase  Base offset of DOE registers in PCIe device config space.

  @retval  EFI_SUCCESS   Successful DOE request and response receive.
  @retval  Other         Failed DOE request or response receive.
**/
STATIC
EFI_STATUS
SendDoeCommand (
  IN EFI_PCI_IO_PROTOCOL   *Pci,
  IN UINT32                DoeBase
  )
{
  EFI_STATUS               Status;
  UINT32                   Val;
  UINT64                   Reg;
  CXL_CDAT_READ_ENTRY_REQ  CxlDoeReq;
  UINT32                   Index = 0;

  // CDAT DOE Request header & Read entry request object.
  CxlDoeReq.Header.VendorId = DVSEC_CXL_VENDOR_ID;
  CxlDoeReq.Header.DataObjType = DOE_DATA_OBJ_TYPE_CDAT;
  CxlDoeReq.Header.Length = CDAT_READ_ENTRY_REQ_SIZE;

  // 0 indicates that it's a read request.
  CxlDoeReq.ReqCode = 0;

  // 0 indicates that table type is CDAT.
  CxlDoeReq.TableType = 0;

  // 0 represents very first entry in the table.
  CxlDoeReq.EntryHandle = 0;

  Reg = DoeBase + DOE_WRITE_DATA_MAILBOX_REG;

  do {
    Status = IsDoeBusy (Pci, DoeBase + DOE_STATUS_REG);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Device busy or DOE request can't be made\n"));
      return Status;
    }

    while (Index < CDAT_READ_ENTRY_REQ_SIZE) {
      Val = *((UINT32 *) (&CxlDoeReq) + Index);
      Status = Pci->Pci.Write (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "Error while making DOE request\n"));
        return Status;
      }
      Index++;
    }

    Reg = DoeBase + DOE_CONTROL_REG;
    Status = Pci->Pci.Read (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Error while reading DOE control reg\n"));
      return Status;
    }

    Val |= DOE_CTRL_DOE_GO;
    Status = Pci->Pci.Write (Pci, EfiPciIoWidthUint32, Reg, 1, &Val);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Error while writing into DOE control reg\n"));
      return Status;
    }

    Status = DoeReceiveResponse (Pci, DoeBase, &CxlDoeReq.EntryHandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "Error while receiving DOE response\n"));
      return Status;
    }
  } while (CxlDoeReq.EntryHandle < CXL_DOE_TABLE_ENTRY_HANDLE_LAST);

  return Status;
}

/**
  Return number of remote memory nodes discovered from CXL Mem devices.

  @retval UINT32   Number of supported remote memory nodes
**/
STATIC UINT32 EFIAPI CxlGetRemoteMemCount (VOID)
{
  return RemoteMemCount;
}

/**
  Update Remote memory information

  Update the Remote memory details, Base address and Length, for number of
  Remote memory nodes discovered from the CXL devices. If the update request
  for number of memory nodes is more than discovered remote memory nodes number,
  then MemCount is modified to number of discovered remote memory nodes.

  @param[out] RemoteMem    Array for updating Remote memory config.
  @param[in,out] MemCount  Number of supported remote memory nodes.

  @retval EFI_SUCCES    Memory is updated successfully
**/
STATIC
EFI_STATUS
EFIAPI
CxlGetRemoteMem (
  OUT REMOTE_MEMORY_CONFIG  *RemoteMemInfo,
  IN OUT UINT32  *MemCount
  )
{

  if ((*MemCount) > RemoteMemCount) {
    DEBUG ((DEBUG_WARN, "Requested for more than max. Remote Memory node\n"));
    *MemCount = RemoteMemCount;
  }

  CopyMem (
    RemoteMemInfo,
    RemoteMemConfig,
    sizeof (REMOTE_MEMORY_CONFIG) * (*MemCount)
    );

  return EFI_SUCCESS;
}

/**
  Installs the CXL platform protocol.

  CXL platform protocol has interfaces for providing CXL mem device
  configurations. A Platform driver can fetch such configurations
  using these protocl interfaces.

  @retval EFI_SUCCESS  On successful installation of protocol interfaces.
  @retval Other        On failure of protocol installation.
**/
STATIC
EFI_STATUS
CxlInstallProtocol (VOID)
{
  EFI_STATUS                 Status;
  EFI_HANDLE                 *CxlPlatformHandle;
  CXL_PLATFORM_PROTOCOL      *CxlPlatformProtocol;
  STATIC  BOOLEAN            CxlProtocolInstalled = FALSE;

  if (CxlProtocolInstalled == TRUE) {
    DEBUG ((DEBUG_INFO, "Protocol already installed. \n"));
    return EFI_SUCCESS;
  }

  CxlPlatformHandle = (EFI_HANDLE *) AllocateZeroPool (sizeof(EFI_HANDLE));

  CxlPlatformProtocol =
    (CXL_PLATFORM_PROTOCOL *) AllocateZeroPool (sizeof(CXL_PLATFORM_PROTOCOL));

  if (!CxlPlatformProtocol) {
    DEBUG ((
      DEBUG_ERROR,
      "CxlInstallProtocol: Failed to allocate memory for CxlPlatformProtocol\n"
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  CxlPlatformProtocol->CxlGetRemoteMem = CxlGetRemoteMem;
  CxlPlatformProtocol->CxlGetRemoteMemCount = CxlGetRemoteMemCount;

  Status = gBS->InstallProtocolInterface (
                  CxlPlatformHandle,
                  &gCxlPlatformProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  CxlPlatformProtocol
                  );

  if (EFI_ERROR(Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "CxlInstallProtocol: Failed to install CxlPlatformProtocol: 0x%08x\n",
      Status
      ));

    return Status;
  }

  CxlProtocolInstalled = TRUE;

  DEBUG ((DEBUG_INFO, "Installed protocol: %p\n", CxlPlatformProtocol));
  return EFI_SUCCESS;
}

VOID
EFIAPI
PciBusEvent (
  IN EFI_EVENT    Event,
  IN VOID*        Context
  )
{

  EFI_STATUS               Status;
  EFI_PCI_IO_PROTOCOL      *Pci;
  UINTN                    Seg, Bus, Dev, Func;
  EFI_HANDLE               *HandleBuffer;
  UINTN                    HandleCount, Index;
  UINT32                   ExtCapOffset, NextExtCapOffset;
  UINT32                   NextDoeExtCapOffset;
  UINT32                   PcieExtCapAndDvsecHeader[3];
  UINT32                   PcieExtCapHeader;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate any PciIo protocols\n"));
    return;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&Pci
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get Pci handle\n"));
      return;
    }

    Pci->GetLocation (Pci, &Seg, &Bus, &Dev, &Func);
    NextExtCapOffset = PCIE_EXTENDED_CAP_OFFSET;

    do {
      ExtCapOffset = NextExtCapOffset;
      Status = Pci->Pci.Read (
                          Pci,
                          EfiPciIoWidthUint32,
                          ExtCapOffset,
                          PCIE_DVSEC_HEADER_MAX,
                          PcieExtCapAndDvsecHeader
                          );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to read PCI IO for Ext. capability\n"));
        return;
      }

      /* Check whether this is a CXL device */
      if (IS_CXL_DVSEC (PcieExtCapAndDvsecHeader[PCIE_DVSEC_HEADER_1])) {
        DEBUG ((DEBUG_INFO, "Found CXL Device\n"));

        NextDoeExtCapOffset = PCIE_EXTENDED_CAP_OFFSET;
        do {
          ExtCapOffset = NextDoeExtCapOffset;
          Status = Pci->Pci.Read (
                              Pci,
                              EfiPciIoWidthUint32,
                              ExtCapOffset,
                              1,
                              &PcieExtCapHeader
                              );
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to read PCI Ext. capability\n"));
            return;
          }

          if (IS_DOE_SUPPORTED (PcieExtCapHeader)) {
            DEBUG ((DEBUG_INFO, "Found DOE Capability\n"));
            Status = SendDoeCommand (Pci, ExtCapOffset);

            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_WARN, "Not Found DOE Capability\n"));
            } else {
              Status = CxlInstallProtocol();
              if (EFI_ERROR (Status))
                return;
            }

            NextExtCapOffset = 0;
            break;
          }

          NextDoeExtCapOffset = PCIE_EXTENDED_CAP_NEXT (PcieExtCapHeader);
        } while(NextDoeExtCapOffset);
      }

      if (NextExtCapOffset == 0)
        break;

      NextExtCapOffset = PCIE_EXTENDED_CAP_NEXT (
                           PcieExtCapAndDvsecHeader[PCIE_EXT_CAP_HEADER]
                           );

    } while (NextExtCapOffset);
  }

  gBS->CloseEvent (CxlEvent);
  CxlEvent = NULL;

  return;
}

/**
  Entry point for CXL DXE.

  This Dxe depends on gEfiPciEnumerationCompleteProtocolGuid. It locates
  PciIo Protocol and discovers PCIe devices with CXL.Mem capability. If a
  device with CXL.Mem capability is found then DOE capability is looked for.
  After that, CXL.Mem device configurations are fetched, and thereafter CXL
  Platform portocol is installed.

  @param[in] ImageHandle  Handle to the Efi image.
  @param[in] SystemTable  A pointer to the Efi System Table.

  @retval EFI_SUCCESS  On successful execution of mentioned functionlities.
  @retval Other        On failure.
**/
EFI_STATUS
EFIAPI
CxlDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS               Status;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  PciBusEvent,
                  NULL,
                  &CxlEvent
                  );

  //
  // Register for protocol notifications on this event
  //
  Status = gBS->RegisterProtocolNotify (
                  &gEfiPciEnumerationCompleteProtocolGuid,
                  CxlEvent,
                  &mPciRegistration
                  );

  return Status;
}
