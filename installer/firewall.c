#include "firewall.h"
#include "installer.h"
#include <objbase.h>
#include <oleauto.h>
#include <stdio.h>

/*
 * Windows Firewall COM API.
 * Используем INetFwPolicy2 / INetFwRules / INetFwRule.
 * Заголовки: netfw.h — в MinGW-w64 присутствует.
 */
#include <netfw.h>

/* MinGW uuid.a не содержит netfw GUID-ы — определяем явно. */
const CLSID CLSID_NetFwPolicy2 =
    {0xE2B3C97F,0x6AE1,0x41AC,{0x81,0x7A,0xF6,0xF9,0x21,0x66,0xD7,0xDD}};
const CLSID CLSID_NetFwRule =
    {0x2C5BC43E,0x3369,0x4C33,{0xAB,0x0C,0xBE,0x94,0x69,0x67,0x7A,0xF4}};
const IID IID_INetFwPolicy2 =
    {0x98325047,0xC671,0x4174,{0x8D,0x81,0xDE,0xFC,0xD3,0xF0,0x31,0x86}};
const IID IID_INetFwRule =
    {0xAF230D27,0xBABA,0x4E42,{0xAC,0xED,0xF5,0x24,0xF2,0x2C,0xFC,0xE2}};

#define RULE_NAME  L"Naleystogramm P2P"
#define RULE_DESC  L"Naleystogramm P2P-мессенджер: входящие TCP-соединения на порту " \
                   L"47821"

static INetFwPolicy2 *get_fw_policy(void) {
    INetFwPolicy2 *pPolicy = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_NetFwPolicy2, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_INetFwPolicy2, (void **)&pPolicy);
    return SUCCEEDED(hr) ? pPolicy : NULL;
}

int firewall_add_rule(const wchar_t *exe_path) {
    INetFwPolicy2 *pPolicy = get_fw_policy();
    if (!pPolicy) return -1;

    INetFwRules *pRules = NULL;
    HRESULT hr = pPolicy->lpVtbl->get_Rules(pPolicy, &pRules);
    if (FAILED(hr)) { pPolicy->lpVtbl->Release(pPolicy); return -1; }

    INetFwRule *pRule = NULL;
    hr = CoCreateInstance(&CLSID_NetFwRule, NULL, CLSCTX_INPROC_SERVER,
                           &IID_INetFwRule, (void **)&pRule);
    if (FAILED(hr)) goto fail;

    /* Имя */
    BSTR bname = SysAllocString(RULE_NAME);
    pRule->lpVtbl->put_Name(pRule, bname);
    SysFreeString(bname);

    /* Описание */
    BSTR bdesc = SysAllocString(RULE_DESC);
    pRule->lpVtbl->put_Description(pRule, bdesc);
    SysFreeString(bdesc);

    /* Протокол TCP = 6 */
    pRule->lpVtbl->put_Protocol(pRule, NET_FW_IP_PROTOCOL_TCP);

    /* Локальный порт */
    wchar_t port_str[16];
    _snwprintf(port_str, 16, L"%d", APP_PORT);
    BSTR bport = SysAllocString(port_str);
    pRule->lpVtbl->put_LocalPorts(pRule, bport);
    SysFreeString(bport);

    /* Путь к приложению */
    if (exe_path && exe_path[0]) {
        BSTR bapp = SysAllocString(exe_path);
        pRule->lpVtbl->put_ApplicationName(pRule, bapp);
        SysFreeString(bapp);
    }

    /* Направление: входящее */
    pRule->lpVtbl->put_Direction(pRule, NET_FW_RULE_DIR_IN);
    pRule->lpVtbl->put_Action(pRule, NET_FW_ACTION_ALLOW);
    pRule->lpVtbl->put_Enabled(pRule, VARIANT_TRUE);

    /* Все профили (Domain + Private + Public) */
    pRule->lpVtbl->put_Profiles(pRule, NET_FW_PROFILE2_ALL);

    hr = pRules->lpVtbl->Add(pRules, pRule);
    pRule->lpVtbl->Release(pRule);
    pRules->lpVtbl->Release(pRules);
    pPolicy->lpVtbl->Release(pPolicy);
    return SUCCEEDED(hr) ? 0 : -1;

fail:
    if (pRule)   pRule->lpVtbl->Release(pRule);
    if (pRules)  pRules->lpVtbl->Release(pRules);
    pPolicy->lpVtbl->Release(pPolicy);
    return -1;
}

int firewall_remove_rule(void) {
    INetFwPolicy2 *pPolicy = get_fw_policy();
    if (!pPolicy) return -1;

    INetFwRules *pRules = NULL;
    HRESULT hr = pPolicy->lpVtbl->get_Rules(pPolicy, &pRules);
    if (FAILED(hr)) { pPolicy->lpVtbl->Release(pPolicy); return -1; }

    BSTR bname = SysAllocString(RULE_NAME);
    hr = pRules->lpVtbl->Remove(pRules, bname);
    SysFreeString(bname);

    pRules->lpVtbl->Release(pRules);
    pPolicy->lpVtbl->Release(pPolicy);
    return (SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) ? 0 : -1;
}
