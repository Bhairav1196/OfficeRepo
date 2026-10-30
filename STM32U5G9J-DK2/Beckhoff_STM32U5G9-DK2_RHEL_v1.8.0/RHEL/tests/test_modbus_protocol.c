/**
 * @file test_modbus_protocol.c
 * @brief Offline verification of Modbus TCP framing, single Unit-ID routing, and tables.
 * @author Prathamesh Chipkar
 */
#include "gateway/gateway_config.h"
#include "gateway/route_manager.h"
#include "ead/terminals.h"
#include "modbus_tcp/modbus_tcp_protocol.h"
#include "modbus_tcp/modbus_holding_registers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

static void fn_BuildRead(uint8_t *pui8_Request,
                         uint16_t ui16_Transaction,
                         uint8_t ui8_Unit,
                         uint8_t ui8_Function,
                         uint16_t ui16_Address,
                         uint16_t ui16_Quantity)
{
    (void)memset(pui8_Request, 0, 12U);
    pui8_Request[0] = (uint8_t)(ui16_Transaction >> 8U);
    pui8_Request[1] = (uint8_t)(ui16_Transaction & UINT16_C(0x00FF));
    pui8_Request[5] = 6U;
    pui8_Request[6] = ui8_Unit;
    pui8_Request[7] = ui8_Function;
    pui8_Request[8] = (uint8_t)(ui16_Address >> 8U);
    pui8_Request[9] = (uint8_t)(ui16_Address & UINT16_C(0x00FF));
    pui8_Request[10] = (uint8_t)(ui16_Quantity >> 8U);
    pui8_Request[11] = (uint8_t)(ui16_Quantity & UINT16_C(0x00FF));
}

int main(void)
{
    S_GW_CONFIG st_Config;
    S_GW_ROUTE_MANAGER *pst_Manager = NULL;
    S_MODB_CONTEXT st_Context;
    S_GW_ROUTE_INFO st_Info;
    char ac_Error[512];
    uint8_t aui8_Request[32];
    uint8_t aui8_Response[GW_MODBUS_TCP_ADU_MAX];
    size_t sz_Consumed = 0U;
    size_t sz_ResponseLength = 0U;
    E_MODB_RESULT e_Result;

    TEST_CHECK(fn_GW_ConfigLoad(TEST_GATEWAY_CONFIG, &st_Config,
                                ac_Error, sizeof(ac_Error)) == GW_OK);
    TEST_CHECK(fn_GW_RouteManagerCreate(&st_Config, &pst_Manager,
                                        ac_Error, sizeof(ac_Error)) == GW_OK);

    TEST_CHECK(fn_GW_RouteGetInfo(pst_Manager, 1U, &st_Info) == GW_OK);
    TEST_CHECK(st_Info.sz_DigitalInputCount == 16U);
    TEST_CHECK(st_Info.sz_DigitalOutputCount == 16U);
    TEST_CHECK(st_Info.sz_AnalogInputCount == 4U);
    TEST_CHECK(st_Info.sz_AnalogOutputCount == 4U);

    TEST_CHECK(fn_MODB_ProtocolInitialize(&st_Context, pst_Manager,
                                          &st_Config) == GW_OK);


    /* AO commands use signed int16 hundredths of mA; REAL32 uses SI amperes. */
    {
        double d_Engineering = 0.0;
        uint16_t ui16_RegisterWord = 0U;
        uint8_t aui8_Real32[16U] = {0U};
        ead_terminal_config_t st_AoConfig;
        float f_Real32 = 0.0F;

        TEST_CHECK(fn_MODB_AoRegisterToEngineering(UINT16_C(132),
                                                    &d_Engineering) == GW_OK);
        TEST_CHECK(fabs(d_Engineering - 0.00132) < 0.000000001);

        TEST_CHECK(fn_MODB_AoRegisterToEngineering(
                       (uint16_t)(int16_t)-500,
                       &d_Engineering) == GW_OK);
        TEST_CHECK(fabs(d_Engineering - (-0.00500)) < 0.000000001);

        TEST_CHECK(fn_MODB_AoRegisterToEngineering(UINT16_C(2001),
                                                    &d_Engineering) ==
                   GW_ERR_RANGE);

        TEST_CHECK(fn_MODB_AoEngineeringToRegister(0.00899,
                                                   &ui16_RegisterWord) == GW_OK);
        TEST_CHECK((int16_t)ui16_RegisterWord == 899);

        TEST_CHECK(fn_MODB_AoEngineeringToRegister(-0.02000,
                                                   &ui16_RegisterWord) == GW_OK);
        TEST_CHECK((int16_t)ui16_RegisterWord == -2000);

        (void)memset(&st_AoConfig, 0, sizeof(st_AoConfig));
        st_AoConfig.pdo_layout = EAD_PDO_REAL32;
        st_AoConfig.analog_mode = EAD_ANALOG_BIPOLAR_20MA;
        st_AoConfig.engineering_min = -0.020;
        st_AoConfig.engineering_max = 0.020;
        st_AoConfig.channel_stride = 4U;
        TEST_CHECK(fn_El4074Encode(aui8_Real32, sizeof(aui8_Real32), 1U,
                                  0.00132, &st_AoConfig) == EAD_OK);
        (void)memcpy(&f_Real32, &aui8_Real32[0], sizeof(f_Real32));
        TEST_CHECK(fabs((double)f_Real32 - 0.00132) < 0.000000001);
    }

    /* Fragmented FC02 request must not be treated as a complete TCP ADU. */
    fn_BuildRead(aui8_Request, 1U, 1U, 2U, 0U, 16U);
    e_Result = fn_MODB_ProcessTcpRequest(&st_Context, aui8_Request, 6U,
                                         aui8_Response, sizeof(aui8_Response),
                                         &sz_Consumed, &sz_ResponseLength, 0U);
    TEST_CHECK(e_Result == MODB_RESULT_NEED_MORE_DATA);

    /* Unit 1 exposes sixteen EL1409 inputs as Discrete Inputs. */
    e_Result = fn_MODB_ProcessTcpRequest(&st_Context, aui8_Request,
                                         sizeof(uint8_t) * 12U,
                                         aui8_Response, sizeof(aui8_Response),
                                         &sz_Consumed, &sz_ResponseLength, 0U);
    TEST_CHECK(e_Result == MODB_RESULT_RESPONSE_READY);
    TEST_CHECK(aui8_Response[6] == 1U);
    TEST_CHECK(aui8_Response[7] == 0x82U);
    TEST_CHECK(aui8_Response[8] == MODB_EXCEPTION_SERVER_BUSY);
    TEST_CHECK(sz_ResponseLength == 9U);

    /* Unit 1 exposes four EL4074 commands as Holding Registers. */
    fn_BuildRead(aui8_Request, 2U, 1U, 3U, 0U, 4U);
    e_Result = fn_MODB_ProcessTcpRequest(&st_Context, aui8_Request, 12U,
                                         aui8_Response, sizeof(aui8_Response),
                                         &sz_Consumed, &sz_ResponseLength, 0U);
    TEST_CHECK(e_Result == MODB_RESULT_RESPONSE_READY);
    TEST_CHECK(aui8_Response[7] == 0x83U);
    TEST_CHECK(aui8_Response[8] == MODB_EXCEPTION_SERVER_BUSY);

    /* Unit 1 has no fifth AO Holding Register. */
    fn_BuildRead(aui8_Request, 3U, 1U, 3U, 4U, 1U);
    e_Result = fn_MODB_ProcessTcpRequest(&st_Context, aui8_Request, 12U,
                                         aui8_Response, sizeof(aui8_Response),
                                         &sz_Consumed, &sz_ResponseLength, 0U);
    TEST_CHECK(e_Result == MODB_RESULT_RESPONSE_READY);
    TEST_CHECK(aui8_Response[7] == (uint8_t)(3U | 0x80U));
    TEST_CHECK(aui8_Response[8] == MODB_EXCEPTION_SERVER_BUSY);

    /* An unconfigured Unit ID is rejected as an unavailable gateway target. */
    fn_BuildRead(aui8_Request, 4U, 7U, 2U, 0U, 1U);
    e_Result = fn_MODB_ProcessTcpRequest(&st_Context, aui8_Request, 12U,
                                         aui8_Response, sizeof(aui8_Response),
                                         &sz_Consumed, &sz_ResponseLength, 0U);
    TEST_CHECK(e_Result == MODB_RESULT_RESPONSE_READY);
    TEST_CHECK(aui8_Response[8] == MODB_EXCEPTION_GATEWAY_TARGET_FAILED);

    fn_GW_RouteManagerDestroy(&pst_Manager);
    (void)puts("Modbus TCP protocol and single Unit-ID routing tests passed");
    return 0;
}
