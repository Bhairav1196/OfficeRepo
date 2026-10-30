/**
 * @file    diagnostics_service.c
 * @brief   Diagnostic data-model initialization.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include "diagnostics_service.h"
#include <string.h>

void fn_Diagnostics_Init(S_AppDiagnosticSnapshot *pst_Snapshot)
{
    if (pst_Snapshot != NULL)
    {
        (void)memset(pst_Snapshot, 0, sizeof(*pst_Snapshot));
        pst_Snapshot->e_IoConfigStatus = E_IO_CONFIG_NULL;
        pst_Snapshot->e_LastDisconnectReason = E_TCP_DISCONNECT_NONE;
    }
}
