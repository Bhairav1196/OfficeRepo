#ifndef IO_SIGNAL_IDS_H
#define IO_SIGNAL_IDS_H

/**
 * @file    io_signal_ids.h
 * @brief   Stable application signal identifiers independent of array position.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#define IO_ID_U1_DI_BASE   (1000U)
#define IO_ID_U1_DO_BASE   (1100U)
#define IO_ID_U1_AI_BASE   (2000U)
#define IO_ID_U1_AO_BASE   (3000U)

#define IO_ID_U1_DI(ch1)   ((uint16_t)(IO_ID_U1_DI_BASE + ((ch1) - 1U)))
#define IO_ID_U1_DO(ch1)   ((uint16_t)(IO_ID_U1_DO_BASE + ((ch1) - 1U)))
#define IO_ID_U1_AI(ch1)   ((uint16_t)(IO_ID_U1_AI_BASE + ((ch1) - 1U)))
#define IO_ID_U1_AO(ch1)   ((uint16_t)(IO_ID_U1_AO_BASE + ((ch1) - 1U)))

#endif /* IO_SIGNAL_IDS_H */
