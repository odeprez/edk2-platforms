/** @file
  ACPI Table Generator Entrypoint. It invokes functions to
  generate SRAT, HMAT tables.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "AcpiTableGenerator.h"

VOID       *mCxlProtocolRegistration;

STATIC EFI_ACPI_TABLE_PROTOCOL *mAcpiTableProtocol = NULL;

VOID
EFIAPI
AcpiTableGenerator (
  IN EFI_EVENT    Event,
  IN VOID*        Context
  )
{
  EFI_STATUS      Status;

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
    return;
  }

  Status = SratTableGenerator (mAcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to create SRAT table: %r\n",
      __FUNCTION__,
      Status
      ));
    return;
  }

  Status = HmatTableGenerator (mAcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to create HMAT table: %r\n",
      __FUNCTION__,
      Status
      ));
    return;
  }

  gBS->CloseEvent (Event);
  return;
}

/**
  Initialize function for the driver.

  Locate ACPI Table protocol and installs SRAT, HMAT tables.

  @param[in]  ImageHandle  Handle to image.
  @param[in]  SystemTable  Pointer to System table.

  @retval  EFI_SUCCESS  On successful installation of SRAT, HMAT ACPI tables.
  @retval  Other        Failure in installing ACPI tables.

**/
EFI_STATUS
EFIAPI
AcpiTableGeneratorEntryPoint (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_EVENT  CxlProtocolEvent;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  AcpiTableGenerator,
                  NULL,
                  &CxlProtocolEvent
                  );

  //
  // Register for protocol notifications on this event
  //
  Status = gBS->RegisterProtocolNotify (
                  &gCxlPlatformProtocolGuid,
                  CxlProtocolEvent,
                  &mCxlProtocolRegistration
                  );

  return Status;
}
