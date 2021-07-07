/** @file
  Driver to handle and support all platform errors.

  Installs the SDEI and HEST ACPI tables for firmware first error handling.

  Copyright (c) 2020 - 2021, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - SDEI Platform Design Document, revision c, 10 Appendix D, ACPI table
      definitions for SDEI
**/

#include <IndustryStandard/Acpi.h>
#include <Library/AcpiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/HestTableProtocol.h>

/**
  Build and install the SDEI ACPI table.

  For platforms that allow firmware-first platform error handling, SDEI is used
  as the notification mechanism for those errors. Installing the SDEI ACPI
  table informs OS about the presence of SDEI.

  @retval EFI_SUCCESS  SDEI table installed successfully.
  @retval Other        On failure during installation.
**/
STATIC
EFI_STATUS
InstallSdeiTable (
  VOID
  )
{
  EFI_ACPI_TABLE_PROTOCOL     *mAcpiTableProtocol = NULL;
  EFI_ACPI_DESCRIPTION_HEADER Header;
  EFI_STATUS                  Status;
  UINTN                       AcpiTableHandle;
  UINT8                       OemId[6] = {'A', 'R', 'M', 'L', 'T', 'D'};

  Header.Signature =
    EFI_ACPI_6_4_SOFTWARE_DELEGATED_EXCEPTIONS_INTERFACE_TABLE_SIGNATURE;
  Header.Length = sizeof (EFI_ACPI_DESCRIPTION_HEADER);
  Header.Revision = 0x01;
  Header.Checksum = 0x00;
  CopyMem (
    (UINT8 *)Header.OemId,
    (UINT8 *)OemId,
    sizeof (Header.OemId)
    );
  Header.OemTableId =  0x4152464e49464552;       // OemTableId:"REFINFRA"
  Header.OemRevision = 0x20201027;
  Header.CreatorId = 0x204d5241;                 // CreatorId:"ARM "
  Header.CreatorRevision = 0x00000001;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&mAcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate ACPI table protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = mAcpiTableProtocol->InstallAcpiTable (
                                 mAcpiTableProtocol,
                                 &Header,
                                 Header.Length,
                                 &AcpiTableHandle
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install SDEI ACPI table, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

/**
  Install the HEST ACPI table.

  HEST ACPI table is used to list the platform error sources for which the
  error handling has been supported. Once the HEST table is build with all
  supported error source descriptors, use the HEST table generation protocol
  to install the HEST table.

  @retval EFI_SUCCESS  HEST table installed successfully.
  @retval Other        On failure during installation.
**/
STATIC
EFI_STATUS
InstallHestTable (
  VOID
  )
{
  EDKII_HEST_TABLE_PROTOCOL *HestProtocol;
  EFI_STATUS                Status;

  Status = gBS->LocateProtocol (
                  &gHestTableProtocolGuid,
                  NULL,
                  (VOID **)&HestProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate HEST DXE Protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = HestProtocol->InstallHestTable ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install HEST table, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

/**
  Entry point for the DXE driver.

  This function installs the HEST ACPI table, using the HEST table generation
  protocol. Also creates and installs the SDEI ACPI table to enable SDEI as
  notification event on the platform.

  @param[in] ImageHandle  Handle to the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS  On successful installation of ACPI tables.
  @retval Other        On Failure.
**/
EFI_STATUS
EFIAPI
PlatformErrorHandlerEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  // Build and install SDEI table.
  Status = InstallSdeiTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Install the created HEST table.
  Status = InstallHestTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
