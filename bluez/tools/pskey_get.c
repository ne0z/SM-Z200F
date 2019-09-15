#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "pskey"

#include "hciattach_sprd.h"

#define _FILE_PARSE_DEBUG_
#define CONF_ITEM_TABLE(ITEM, ACTION, BUF, LEN)    { #ITEM, ACTION, &(BUF.ITEM), LEN, (sizeof(BUF.ITEM) / LEN) }
#define ALOGI(fmt, arg...)  fprintf(stderr, "%s:%d()" fmt "\n", __FILE__,__LINE__, ## arg)
#define ALOGE(fmt, arg...)  fprintf(stderr, "%s:%d()" fmt "\n", __FILE__,__LINE__, ## arg)

#define PSKEY_PATH "/usr/etc/bluetooth/scx35_pikeavivaltove_3M_MARLIN_connectivity_configure.ini"

#define CONF_COMMENT '#'
#define CONF_DELIMITERS " =\n\r\t"
#define CONF_VALUES_DELIMITERS "=\n\r\t#"
#define CONF_VALUES_PARTITION " ,=\n\r\t#"
#define CONF_MAX_LINE_LEN 255

typedef int(conf_action_t)(char *p_conf_name, char *p_conf_value, void *buf,
                                   int len, int size);

typedef struct {
    const char *conf_entry;
    conf_action_t *p_action;
    void *buf;
    int len;
    int size;
} conf_entry_t;

pskey_config_t marlin_pskey = {0};

conf_entry_t marlin_table[] = {
    {"pskey_cmd", 0, 0, 0, 0},

    CONF_ITEM_TABLE(g_dbg_source_sink_syn_test_data, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_sys_sleep_in_standby_supported, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_sys_sleep_master_supported, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_sys_sleep_slave_supported, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(default_ahb_clk, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(device_class, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(win_ext, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(g_aGainValue, 0, marlin_pskey, 6),
    CONF_ITEM_TABLE(g_aPowerValue, 0, marlin_pskey, 5),

    CONF_ITEM_TABLE(feature_set, 0, marlin_pskey, 16),
    CONF_ITEM_TABLE(device_addr, 0, marlin_pskey, 6),

    CONF_ITEM_TABLE(g_sys_sco_transmit_mode, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_sys_uart0_communication_supported, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(edr_tx_edr_delay, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(edr_rx_edr_delay, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(g_wbs_nv_117, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(is_wdg_supported, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(share_memo_rx_base_addr, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(g_wbs_nv_118, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_nbv_nv_117, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(share_memo_tx_packet_num_addr, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(share_memo_tx_data_base_addr, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(g_PrintLevel, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(share_memo_tx_block_length, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(share_memo_rx_block_length, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(share_memo_tx_water_mark, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(g_nbv_nv_118, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(uart_rx_watermark, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(uart_flow_control_thld, 0, marlin_pskey, 1),

    CONF_ITEM_TABLE(comp_id, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(pcm_clk_divd, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(br_edr_diff_reserved, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_aBRChannelpwrvalue, 0, marlin_pskey, 8),
    CONF_ITEM_TABLE(g_aEDRChannelpwrvalue, 0, marlin_pskey, 8),
    CONF_ITEM_TABLE(g_aLEPowerControlFlag, 0, marlin_pskey, 1),
    CONF_ITEM_TABLE(g_aLEChannelpwrvalue, 0, marlin_pskey, 8)
};

pskey_config_t* bt_getPskey ()
{
	return &marlin_pskey;
}

static void marlin_pskey_dump(void *arg)
{
    pskey_config_t *p = &marlin_pskey;
    ALOGI("pskey_cmd: 0x%08X", p->pskey_cmd);

    ALOGI("g_dbg_source_sink_syn_test_data: 0x%02X",
          p->g_dbg_source_sink_syn_test_data);
    ALOGI("g_sys_sleep_in_standby_supported: 0x%02X",
          p->g_sys_sleep_in_standby_supported);
    ALOGI("g_sys_sleep_master_supported: 0x%02X",
          p->g_sys_sleep_master_supported);
    ALOGI("g_sys_sleep_slave_supported: 0x%02X", p->g_sys_sleep_slave_supported);

    ALOGI("default_ahb_clk: %d", p->default_ahb_clk);
    ALOGI("device_class: 0x%08X", p->device_class);
    ALOGI("win_ext: 0x%08X", p->win_ext);

    ALOGI("g_aGainValue: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
          p->g_aGainValue[0], p->g_aGainValue[1], p->g_aGainValue[2],
          p->g_aGainValue[3], p->g_aGainValue[4], p->g_aGainValue[5]);
    ALOGI("g_aPowerValue: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
          p->g_aPowerValue[0], p->g_aPowerValue[1], p->g_aPowerValue[2],
          p->g_aPowerValue[3], p->g_aPowerValue[4]);

    ALOGI(
        "feature_set(0~7): 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
        "0x%02X, 0x%02X",
        p->feature_set[0], p->feature_set[1], p->feature_set[2],
        p->feature_set[3], p->feature_set[4], p->feature_set[5],
        p->feature_set[6], p->feature_set[7]);
    ALOGI(
        "feature_set(8~15): 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, "
        "0x%02X, 0x%02X",
        p->feature_set[8], p->feature_set[9], p->feature_set[10],
        p->feature_set[11], p->feature_set[12], p->feature_set[13],
        p->feature_set[14], p->feature_set[15]);
    ALOGI("device_addr: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
          p->device_addr[0], p->device_addr[1], p->device_addr[2],
          p->device_addr[3], p->device_addr[4], p->device_addr[5]);

    ALOGI("g_sys_sco_transmit_mode: 0x%02X", p->g_sys_sco_transmit_mode);
    ALOGI("g_sys_uart0_communication_supported: 0x%02X",
          p->g_sys_uart0_communication_supported);
    ALOGI("edr_tx_edr_delay: %d", p->edr_tx_edr_delay);
    ALOGI("edr_rx_edr_delay: %d", p->edr_rx_edr_delay);

    ALOGI("g_wbs_nv_117 : 0x%04X", p->g_wbs_nv_117);

    ALOGI("is_wdg_supported: 0x%08X", p->is_wdg_supported);

    ALOGI("share_memo_rx_base_addr: 0x%08X", p->share_memo_rx_base_addr);
    ALOGI("g_wbs_nv_118 : 0x%04X", p->g_wbs_nv_118);
    ALOGI("g_nbv_nv_117 : 0x%04X", p->g_nbv_nv_117);

    ALOGI("share_memo_tx_packet_num_addr: 0x%08X",
          p->share_memo_tx_packet_num_addr);
    ALOGI("share_memo_tx_data_base_addr: 0x%08X",
          p->share_memo_tx_data_base_addr);

    ALOGI("g_PrintLevel: 0x%08X", p->g_PrintLevel);

    ALOGI("share_memo_tx_block_length: 0x%04X", p->share_memo_tx_block_length);
    ALOGI("share_memo_rx_block_length: 0x%04X", p->share_memo_rx_block_length);
    ALOGI("share_memo_tx_water_mark: 0x%04X", p->share_memo_tx_water_mark);
    ALOGI("g_nbv_nv_118 : 0x%04X", p->g_nbv_nv_118);

    ALOGI("uart_rx_watermark: %d", p->uart_rx_watermark);
    ALOGI("uart_flow_control_thld: %d", p->uart_flow_control_thld);
    ALOGI("comp_id: 0x%08X", p->comp_id);
    ALOGI("pcm_clk_divd : 0x%04X", p->pcm_clk_divd);

    ALOGI("br_edr_diff_reserved : 0x%04X", p->br_edr_diff_reserved);
    ALOGI("g_aBRChannelpwrvalue:0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
        p->g_aBRChannelpwrvalue[0], p->g_aBRChannelpwrvalue[1], p->g_aBRChannelpwrvalue[2], p->g_aBRChannelpwrvalue[3],
        p->g_aBRChannelpwrvalue[4], p->g_aBRChannelpwrvalue[5], p->g_aBRChannelpwrvalue[6], p->g_aBRChannelpwrvalue[7]);
    ALOGI("g_aEDRChannelpwrvalue:0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
        p->g_aEDRChannelpwrvalue[0], p->g_aEDRChannelpwrvalue[1], p->g_aEDRChannelpwrvalue[2], p->g_aEDRChannelpwrvalue[3],
        p->g_aEDRChannelpwrvalue[4], p->g_aEDRChannelpwrvalue[5], p->g_aEDRChannelpwrvalue[6], p->g_aEDRChannelpwrvalue[7]);
    ALOGI("g_aLEPowerControlFlag : 0x%08X", p->g_aLEPowerControlFlag);
    ALOGI("g_aLEChannelpwrvalue:0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X",
        p->g_aLEChannelpwrvalue[0], p->g_aLEChannelpwrvalue[1], p->g_aLEChannelpwrvalue[2], p->g_aLEChannelpwrvalue[3],
        p->g_aLEChannelpwrvalue[4], p->g_aLEChannelpwrvalue[5], p->g_aLEChannelpwrvalue[6], p->g_aLEChannelpwrvalue[7]);
}

static void parse_number(char *p_conf_name, char *p_conf_value, void *buf,
                         int len, int size)
{
    uint8 *dest = (uint8 *)buf;
    char *sub_value, *p;
    uint32 value;
    sub_value = strtok_r(p_conf_value, CONF_VALUES_PARTITION, &p);
    do {
        if (sub_value == NULL) {
            break;
        }

        if (sub_value[0] == '0' && (sub_value[1] == 'x' || sub_value[1] == 'X')) {
            value = strtoul(sub_value, 0, 16) & 0xFFFFFFFF;
        } else {
            value = strtoul(sub_value, 0, 10) & 0xFFFFFFFF;
        }

        switch (size) {
        case sizeof(uint8):
            *dest = value & 0xFF;
            dest += size;
            break;

        case sizeof(uint16):
            *((uint16 *)dest) = value & 0xFFFF;
            dest += size;
            break;

        case sizeof(uint32):
            *((uint32 *)dest) = value & 0xFFFFFFFF;
            dest += size;
            break;

        default:
            break;
        }
        sub_value = strtok_r(NULL, CONF_VALUES_PARTITION, &p);
    } while (--len);
}

static void sprd_vnd_reload(void)
{
    //set_mac_address(pskey->device_addr);
    marlin_pskey_dump(NULL);
}

void vnd_load_conf(const char *p_path)
{
    FILE *p_file;
    char *p_name, *p_value, *p;
    conf_entry_t *p_entry;
    char line[CONF_MAX_LINE_LEN + 1]; /* add 1 for \0 char */

    ALOGI("Attempt to load conf from %s", p_path);

    if ((p_file = fopen(p_path, "r")) != NULL) {
        /* read line by line */
        while (fgets(line, CONF_MAX_LINE_LEN + 1, p_file) != NULL) {
            if (line[0] == CONF_COMMENT) continue;

            p_name = strtok_r(line, CONF_DELIMITERS, &p);

            if (NULL == p_name) {
                continue;
            }

            p_value = strtok_r(NULL, CONF_VALUES_DELIMITERS, &p);

            if (NULL == p_value) {
                ALOGE("vnd_load_conf: missing value for name: %s", p_name);
                continue;
            }

            p_entry = (conf_entry_t *)&marlin_table;
            int i = 0;
            int array_len = sizeof(marlin_table)/sizeof(conf_entry_t);
            while (p_entry->conf_entry != NULL) {
                if (strcmp(p_entry->conf_entry, (const char *)p_name) == 0) {
                    if (p_entry->p_action) {
                        p_entry->p_action(p_name, p_value, p_entry->buf, p_entry->len,
                                          p_entry->size);
                    } else {
                        parse_number(p_name, p_value, p_entry->buf, p_entry->len,
                                     p_entry->size);
                    }
                    break;
                }
                p_entry++;
                i++;
                if (i > array_len) break;
            }
        }

        fclose(p_file);
    } else {
        ALOGI("vnd_load_conf file >%s< not found", p_path);
    }
    sprd_vnd_reload();
}

int bt_getPskeyFromFile()
{
    vnd_load_conf(PSKEY_PATH);
    return 0;
}

/*
 * hci command preload stream,  special order
 */
void pskey_stream_compose(uint8 * buf, pskey_config_t *bt_par) {
    int i = 0;
	uint8 *p = buf;

	ALOGI("%s", __func__);

    bt_par->pskey_cmd = 0xFCA001;
	UINT24_TO_STREAM(p, bt_par->pskey_cmd);
	UINT8_TO_STREAM(p, (uint8)( PSKEY_PREAMBLE_SIZE & 0xFF));

	UINT8_TO_STREAM(p, bt_par->g_dbg_source_sink_syn_test_data);
	UINT8_TO_STREAM(p, bt_par->g_sys_sleep_in_standby_supported);
	UINT8_TO_STREAM(p, bt_par->g_sys_sleep_master_supported);
	UINT8_TO_STREAM(p, bt_par->g_sys_sleep_slave_supported);

	UINT32_TO_STREAM(p, bt_par->default_ahb_clk);
	UINT32_TO_STREAM(p, bt_par->device_class);
	UINT32_TO_STREAM(p, bt_par->win_ext);

	for (i = 0; i < 6; i++) {
		UINT32_TO_STREAM(p, bt_par->g_aGainValue[i]);
	}
	for (i = 0; i < 5; i++) {
		UINT32_TO_STREAM(p, bt_par->g_aPowerValue[i]);
	}

	for (i = 0; i < 16; i++) {
		UINT8_TO_STREAM(p, bt_par->feature_set[i]);
	}
	for (i = 0; i < 6; i++) {
		UINT8_TO_STREAM(p, bt_par->device_addr[i]);
	}

	UINT8_TO_STREAM(p, bt_par->g_sys_sco_transmit_mode);
	UINT8_TO_STREAM(p, bt_par->g_sys_uart0_communication_supported);
	UINT8_TO_STREAM(p, bt_par->edr_tx_edr_delay);
	UINT8_TO_STREAM(p, bt_par->edr_rx_edr_delay);

	UINT16_TO_STREAM(p, bt_par->g_wbs_nv_117);

	UINT32_TO_STREAM(p, bt_par->is_wdg_supported);

	UINT32_TO_STREAM(p, bt_par->share_memo_rx_base_addr);
	//UINT32_TO_STREAM(p, bt_par->share_memo_tx_base_addr);
	UINT16_TO_STREAM(p, bt_par->g_wbs_nv_118);
	UINT16_TO_STREAM(p, bt_par->g_nbv_nv_117);

	UINT32_TO_STREAM(p, bt_par->share_memo_tx_packet_num_addr);
	UINT32_TO_STREAM(p, bt_par->share_memo_tx_data_base_addr);

	UINT32_TO_STREAM(p, bt_par->g_PrintLevel);

	UINT16_TO_STREAM(p, bt_par->share_memo_tx_block_length);
	UINT16_TO_STREAM(p, bt_par->share_memo_rx_block_length);
	UINT16_TO_STREAM(p, bt_par->share_memo_tx_water_mark);
	//UINT16_TO_STREAM(p, bt_par->share_memo_tx_timeout_value);
	UINT16_TO_STREAM(p, bt_par->g_nbv_nv_118);

	UINT16_TO_STREAM(p, bt_par->uart_rx_watermark);
	UINT16_TO_STREAM(p, bt_par->uart_flow_control_thld);
	UINT32_TO_STREAM(p, bt_par->comp_id);
	UINT16_TO_STREAM(p, bt_par->pcm_clk_divd);

       UINT16_TO_STREAM(p, bt_par->br_edr_diff_reserved);
       for (i = 0; i < 8; i++) {
            UINT32_TO_STREAM(p, bt_par->g_aBRChannelpwrvalue[i]);
       }
       for (i = 0; i < 8; i++) {
            UINT32_TO_STREAM(p, bt_par->g_aEDRChannelpwrvalue[i]);
       }
       UINT32_TO_STREAM(p, bt_par->g_aLEPowerControlFlag);
       for (i = 0; i < 8; i++) {
            UINT16_TO_STREAM(p, bt_par->g_aLEChannelpwrvalue[i]);
       }
}

