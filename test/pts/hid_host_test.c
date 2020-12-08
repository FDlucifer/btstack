/*
 * Copyright (C) 2017 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "hid_host_tast.c"

/*
 * hid_host_tast.c
 */

/* EXAMPLE_START(hid_host_tast): HID Host Demo
 *
 * @text This example implements an HID Host. For now, it connnects to a fixed device, queries the HID SDP
 * record and opens the HID Control + Interrupt channels
 */

#include <inttypes.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300

// SDP
static uint8_t            hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t           hid_descriptor_len;

static uint16_t           hid_control_psm;
static uint16_t           hid_interrupt_psm;

static uint8_t            attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;


// PTS
static const char * remote_addr_string = "00:1B:DC:08:E2:5C";

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;

// Needed for queries
typedef enum {
    HID_HOST_IDLE,
    HID_HOST_CONTROL_CONNECTION_ESTABLISHED,
    HID_HOST_W2_SEND_GET_REPORT,
    HID_HOST_W4_GET_REPORT_RESPONSE,
    HID_HOST_W2_SEND_SET_REPORT,
    HID_HOST_W4_SET_REPORT_RESPONSE,
    HID_HOST_W2_SEND_GET_PROTOCOL,
    HID_HOST_W4_GET_PROTOCOL_RESPONSE,
    HID_HOST_W2_SEND_SET_PROTOCOL,
    HID_HOST_W4_SET_PROTOCOL_RESPONSE,
    
} hid_host_state_t;

// Simplified US Keyboard with Shift modifier

#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      27
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   0x7f

/**
 * English (US)
 */
static const uint8_t keytable_us_none [] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /*   0-3 */
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',                   /*  4-13 */
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',                   /* 14-23 */
    'u', 'v', 'w', 'x', 'y', 'z',                                       /* 24-29 */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',                   /* 30-39 */
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            /* 40-44 */
    '-', '=', '[', ']', '\\', CHAR_ILLEGAL, ';', '\'', 0x60, ',',       /* 45-54 */
    '.', '/', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   /* 55-60 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 61-64 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 65-68 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 69-72 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 73-76 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 77-80 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 81-84 */
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       /* 85-97 */
    '6', '7', '8', '9', '0', '.', 0xa7,                                 /* 97-100 */
}; 

static const uint8_t keytable_us_shift[] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /*  0-3  */
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',                   /*  4-13 */
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',                   /* 14-23 */
    'U', 'V', 'W', 'X', 'Y', 'Z',                                       /* 24-29 */
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',                   /* 30-39 */
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            /* 40-44 */
    '_', '+', '{', '}', '|', CHAR_ILLEGAL, ':', '"', 0x7E, '<',         /* 45-54 */
    '>', '?', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   /* 55-60 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 61-64 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 65-68 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 69-72 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 73-76 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 77-80 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 81-84 */
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       /* 85-97 */
    '6', '7', '8', '9', '0', '.', 0xb1,                                 /* 97-100 */
}; 


// hid device state
typedef struct hid_host {
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint16_t  control_cid;
    uint16_t  interrupt_cid;

    bool unplugged;

    // get report
    hid_report_type_t report_type;
    uint8_t           report_id;

    // set report
    uint8_t * report;
    uint16_t  report_len;

    // set protocol
    hid_protocol_mode_t protocol_mode;

    hid_host_state_t state;
    uint8_t   user_request_can_send_now; 
} hid_host_t;

static hid_host_t _hid_host;
static uint16_t hid_host_cid = 0;

static uint16_t hid_host_get_next_cid(void){
    hid_host_cid++;
    if (!hid_host_cid){
        hid_host_cid = 1;
    }
    return hid_host_cid;
}

static hid_host_t * hid_host_get_instance_for_hid_cid(uint16_t hid_cid){
    if (_hid_host.cid == hid_cid){
        return &_hid_host;
    }
    return NULL;
}

static uint8_t hid_host_send_get_report(uint16_t hid_cid,  hid_report_type_t report_type, uint8_t report_id){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host || !hid_host->control_cid){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 
    if (hid_host->state != HID_HOST_CONTROL_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    hid_host->state = HID_HOST_W2_SEND_GET_REPORT;
    hid_host->report_type = report_type;
    hid_host->report_id = report_id;

    l2cap_request_can_send_now_event(hid_host->control_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hid_host_send_get_output_report(uint16_t hid_cid, uint8_t report_id){
    return hid_host_send_get_report(hid_cid, HID_REPORT_TYPE_OUTPUT, report_id);
}

static uint8_t hid_host_send_get_feature_report(uint16_t hid_cid, uint8_t report_id){
    return hid_host_send_get_report(hid_cid, HID_REPORT_TYPE_FEATURE, report_id);
}

static uint8_t hid_host_send_get_input_report(uint16_t hid_cid, uint8_t report_id){
    return hid_host_send_get_report(hid_cid, HID_REPORT_TYPE_INPUT, report_id);
}

static uint8_t hid_host_send_set_report(uint16_t hid_cid, hid_report_type_t report_type, uint8_t report_id, uint8_t * report, uint8_t report_len){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host || !hid_host->control_cid){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 
    if (hid_host->state != HID_HOST_CONTROL_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    hid_host->state = HID_HOST_W2_SEND_SET_REPORT;
    hid_host->report_type = report_type;
    hid_host->report_id = report_id;
    hid_host->report = report;
    hid_host->report_len = report_len;

    l2cap_request_can_send_now_event(hid_host->control_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hid_host_send_set_output_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_set_report(hid_cid, HID_REPORT_TYPE_OUTPUT, report_id, report, report_len);
}

static uint8_t hid_host_send_set_feature_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_set_report(hid_cid, HID_REPORT_TYPE_FEATURE, report_id, report, report_len);
}

static uint8_t hid_host_send_set_input_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_set_report(hid_cid, HID_REPORT_TYPE_INPUT, report_id, report, report_len);
}

static uint8_t hid_host_send_get_protocol(uint16_t hid_cid){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host || !hid_host->control_cid) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    if (hid_host->state != HID_HOST_CONTROL_CONNECTION_ESTABLISHED) return ERROR_CODE_COMMAND_DISALLOWED;

    hid_host->state = HID_HOST_W2_SEND_GET_PROTOCOL;

    l2cap_request_can_send_now_event(hid_host->control_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hid_host_send_set_protocol(uint16_t hid_cid, hid_protocol_mode_t protocol_mode){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host || !hid_host->control_cid) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    if (hid_host->state != HID_HOST_CONTROL_CONNECTION_ESTABLISHED) return ERROR_CODE_COMMAND_DISALLOWED;

    hid_host->state = HID_HOST_W2_SEND_SET_PROTOCOL;
    hid_host->protocol_mode = protocol_mode;

    l2cap_request_can_send_now_event(hid_host->control_cid);
    return ERROR_CODE_SUCCESS;
}


// static void hid_host_connect(bd_addr_t addr, uint16_t * hid_cid){

// }

static hid_host_t * hid_host_provide_instance_for_bd_addr(bd_addr_t bd_addr){
    if (!_hid_host.cid){
        (void)memcpy(_hid_host.bd_addr, bd_addr, 6);
        _hid_host.cid = hid_host_get_next_cid();
        // _hid_host.protocol_mode = HID_PROTOCOL_MODE_REPORT;
        _hid_host.con_handle = HCI_CON_HANDLE_INVALID;
    }
    return &_hid_host;
}

static hid_host_t * hid_host_get_instance_for_l2cap_cid(uint16_t cid){
    if ((_hid_host.control_cid == cid) || (_hid_host.interrupt_cid == cid)){
        return &_hid_host;
    }
    return NULL;
}

static void hid_host_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host || !hid_host->control_cid) return;
    l2cap_send(hid_host->control_cid, (uint8_t*) message, message_len);
}

static uint8_t hid_host_send_suspend(uint16_t hcid){
    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_SUSPEND };
    hid_host_send_control_message(hcid, &report[0], sizeof(report));
    return ERROR_CODE_SUCCESS;
}

static uint8_t hid_host_send_exit_suspend(uint16_t hcid){
    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_EXIT_SUSPEND };
    hid_host_send_control_message(hcid, &report[0], sizeof(report));
    return ERROR_CODE_SUCCESS;
}

static uint8_t hid_host_send_virtual_cable_unplug(uint16_t hcid){
    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG };
    hid_host_send_control_message(hcid, &report[0], sizeof(report));
    return ERROR_CODE_SUCCESS;
}

static void hid_host_disconnect_interrupt_channel(uint16_t hid_cid){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host){
        log_error("hid_host_disconnect_interrupt_channel: could not find hid device instace");
        return;
    }
    log_info("Disconnect from interrupt channel HID Host");
    if (hid_host->interrupt_cid){
        l2cap_disconnect(hid_host->interrupt_cid, 0);  // reason isn't used
    }
}

static void hid_host_disconnect_control_channel(uint16_t hid_cid){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host){
        log_error("hid_host_disconnect_control_channel: could not find hid device instace");
        return;
    }
    log_info("Disconnect from control channel HID Host");
    if (hid_host->control_cid){
        l2cap_disconnect(hid_host->control_cid, 0);  // reason isn't used
    }
}

static void hid_host_disconnect(uint16_t hid_cid){
    hid_host_t * hid_host = hid_host_get_instance_for_hid_cid(hid_cid);
    if (!hid_host){
        log_error("hid_host_disconnect: could not find hid device instace");
        return;
    }
    log_info("Disconnect from HID Host");
    if (hid_host->interrupt_cid){
        l2cap_disconnect(hid_host->interrupt_cid, 0);  // reason isn't used
    }
    if (hid_host->control_cid){
        l2cap_disconnect(hid_host->control_cid, 0); // reason isn't used
    }
}
/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP is initialized 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // Initialize L2CAP 
    l2cap_init();
    // register L2CAP Services for reconnections
    l2cap_register_service(packet_handler, PSM_HID_INTERRUPT, 0xffff, gap_get_security_level());
    l2cap_register_service(packet_handler, PSM_HID_CONTROL, 0xffff, gap_get_security_level());

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);


    // Disable stdout buffering
    setbuf(stdout, NULL);
}
/* LISTING_END */

/* @section SDP parser callback 
 * 
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in  
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */


static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    des_iterator_t prot_it;
    uint8_t       *des_element;
    uint8_t       *element;
    uint32_t       uuid;
    uint8_t        status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                des_iterator_init(&prot_it, des_element);
                                element = des_iterator_get_element(&prot_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        des_iterator_next(&prot_it);
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_control_psm);
                                        printf("HID Control PSM: 0x%04x\n", (int) hid_control_psm);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_DES) continue;
                                    des_element = des_iterator_get_element(&additional_des_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_interrupt_psm);
                                            printf("HID Interrupt PSM: 0x%04x\n", (int) hid_interrupt_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_STRING) continue;
                                    element = des_iterator_get_element(&additional_des_it);
                                    const uint8_t * descriptor = de_get_string(element);
                                    hid_descriptor_len = de_get_data_size(element);
                                    memcpy(hid_descriptor, descriptor, hid_descriptor_len);
                                    printf("HID Descriptor:\n");
                                    printf_hexdump(hid_descriptor, hid_descriptor_len);
                                }
                            }                        
                            break;
                        default:
                            break;
                    }
                }
            } else {
                fprintf(stderr, "SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            if (!hid_control_psm) {
                printf("HID Control PSM missing\n");
                break;
            }
            if (!hid_interrupt_psm) {
                printf("HID Interrupt PSM missing\n");
                break;
            }
            printf("Setup HID\n");
            (void)memcpy(_hid_host.bd_addr, remote_addr, 6);
            status = l2cap_create_channel(packet_handler, remote_addr, hid_control_psm, 48, &_hid_host.control_cid);
            if (status){
                printf("Connecting to HID Control failed: 0x%02x\n", status);
            }
            break;
    }
}


/*
 * @section HID Report Handler
 * 
 * @text Use BTstack's compact HID Parser to process incoming HID Report
 * Iterate over all fields and process fields with usage page = 0x07 / Keyboard
 * Check if SHIFT is down and process first character (don't handle multiple key presses)
 * 
 */
#define NUM_KEYS 6
static uint8_t last_keys[NUM_KEYS];
static void hid_host_handle_interrupt_report(const uint8_t * report, uint16_t report_len){
    // check if HID Input Report
    if (report_len < 1) return;
    if (*report != 0xa1) return; 
    report++;
    report_len--;
    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, hid_descriptor, hid_descriptor_len, HID_REPORT_TYPE_INPUT, report, report_len);
    int shift = 0;
    uint8_t new_keys[NUM_KEYS];
    memset(new_keys, 0, sizeof(new_keys));
    int     new_keys_count = 0;
    while (btstack_hid_parser_has_more(&parser)){
        uint16_t usage_page;
        uint16_t usage;
        int32_t  value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);
        if (usage_page != 0x07) continue;   
        switch (usage){
            case 0xe1:
            case 0xe6:
                if (value){
                    shift = 1;
                }
                continue;
            case 0x00:
                continue;
            default:
                break;
        }
        if (usage >= sizeof(keytable_us_none)) continue;

        // store new keys
        new_keys[new_keys_count++] = usage;

        // check if usage was used last time (and ignore in that case)
        int i;
        for (i=0;i<NUM_KEYS;i++){
            if (usage == last_keys[i]){
                usage = 0;
            }
        }
        if (usage == 0) continue;

        uint8_t key;
        if (shift){
            key = keytable_us_shift[usage];
        } else {
            key = keytable_us_none[usage];
        }
        if (key == CHAR_ILLEGAL) continue;
        if (key == CHAR_BACKSPACE){ 
            printf("\b \b");    // go back one char, print space, go back one char again
            continue;
        }
        printf("%c", key);
    }
    memcpy(last_keys, new_keys, NUM_KEYS);
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HCI Events.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    /* LISTING_PAUSE */
    uint8_t   event;
    bd_addr_t address;
    uint8_t   status;
    uint16_t  l2cap_cid;
    hid_host_t * hid_host;
    uint8_t param;
    hid_message_type_t         message_type;
    hid_handshake_param_type_t message_status;

    /* LISTING_RESUME */
    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BTstack up and running. \n");
                    }
                    break;

                /* LISTING_PAUSE */
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, address);
                    gap_pin_code_response(address, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                /* LISTING_RESUME */
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    printf("L2CAP_EVENT_INCOMING_CONNECTION \n");
                    l2cap_event_incoming_connection_get_address(packet, address); 
                            
                    switch (l2cap_event_incoming_connection_get_psm(packet)){
                        case PSM_HID_CONTROL:
                        case PSM_HID_INTERRUPT:
                            hid_host = hid_host_provide_instance_for_bd_addr(address);

                            if (!hid_host) {
                                log_error("L2CAP_EVENT_INCOMING_CONNECTION, cannot create instance for %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }
                            if (hid_host->unplugged) {
                                log_info("L2CAP_EVENT_INCOMING_CONNECTION, decline connection for %s, host is unplugged", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }

                            if ((hid_host->con_handle == HCI_CON_HANDLE_INVALID) || (l2cap_event_incoming_connection_get_handle(packet) == hid_host->con_handle)){
                                hid_host->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                                // hid_host->incoming = 1;
                                l2cap_event_incoming_connection_get_address(packet, hid_host->bd_addr);
                                switch (l2cap_event_incoming_connection_get_psm(packet)){
                                    case PSM_HID_CONTROL:
                                        hid_host->control_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                                        break;
                                    case PSM_HID_INTERRUPT:
                                        hid_host->interrupt_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                                    break;
                                    default:
                                        break;
                                }
                                printf("L2CAP_EVENT_INCOMING_CONNECTION l2cap_accept_connection\n");
                                l2cap_accept_connection(channel);
                            } else {
                                l2cap_decline_connection(channel);
                                log_info("L2CAP_EVENT_INCOMING_CONNECTION, decline connection for %s", bd_addr_to_str(address));
                            }
                            break;
                        default:
                            log_info("L2CAP_EVENT_INCOMING_CONNECTION, decline connection for %s", bd_addr_to_str(address));
                            l2cap_decline_connection(channel);
                            break;
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED: 
                    status = l2cap_event_channel_opened_get_status(packet); 
                    if (status){
                        printf("L2CAP Connection failed: 0x%02x\n", status);
                        break;
                    }
                    l2cap_cid  = l2cap_event_channel_opened_get_local_cid(packet);

                    if (!l2cap_cid) break;
                    
                    switch (l2cap_event_channel_opened_get_psm(packet)){
                        case PSM_HID_CONTROL:
                            if (l2cap_cid == _hid_host.control_cid){
                                status = l2cap_create_channel(packet_handler, remote_addr, hid_interrupt_psm, 48, &_hid_host.interrupt_cid);
                                if (status){
                                    printf("Connecting to HID Control failed: 0x%02x\n", status);
                                    break;
                                }
                                _hid_host.state = HID_HOST_CONTROL_CONNECTION_ESTABLISHED;
                            }   
                            break;
                        case PSM_HID_INTERRUPT:
                            if (l2cap_cid == _hid_host.interrupt_cid){
                                printf("HID Connection established\n");
                            }
                            break;
                        default:
                            break;
                    }
                    // disconnect?                    
                    break;

                case L2CAP_EVENT_CHANNEL_CLOSED:
                    l2cap_cid  = l2cap_event_channel_closed_get_local_cid(packet);
                    hid_host = hid_host_get_instance_for_l2cap_cid(l2cap_cid);
                    if (!hid_host) return;

                    if (l2cap_cid == hid_host->interrupt_cid){
                        hid_host->interrupt_cid = 0;
                    }
                    if (l2cap_cid == hid_host->control_cid){
                        hid_host->control_cid = 0;
                    }
                    if (!hid_host->interrupt_cid && !hid_host->control_cid){
                        // hid_host->connected = 0;
                        hid_host->con_handle = HCI_CON_HANDLE_INVALID;
                        hid_host->cid = 0;
                        // hid_host_emit_event(device, HID_SUBEVENT_CONNECTION_CLOSED);
                    }
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    l2cap_cid  = l2cap_event_can_send_now_get_local_cid(packet);
                    hid_host = hid_host_get_instance_for_l2cap_cid(l2cap_cid);
                    if (!hid_host) return;

                    printf("L2CAP_EVENT_CAN_SEND_NOW, hid_host.state = %d\n", hid_host->state);
                    switch(hid_host->state){
                        case HID_HOST_W2_SEND_GET_REPORT:{
                            uint8_t header = (HID_MESSAGE_TYPE_GET_REPORT << 4) | hid_host->report_type;
                            uint8_t report[] = {header, hid_host->report_id};
                            // TODO: optional Report ID (1)
                            // TODO: optional Maximum number of bytes to transfer during data phase, little end. (2)
                            
                            hid_host->state = HID_HOST_W4_GET_REPORT_RESPONSE;
                            l2cap_send(hid_host->control_cid, (uint8_t*) report, sizeof(report));
                            break;
                        }
                        case HID_HOST_W2_SEND_SET_REPORT:{
                            uint8_t header = (HID_MESSAGE_TYPE_SET_REPORT << 4) | hid_host->report_type;
                            hid_host->state = HID_HOST_W4_SET_REPORT_RESPONSE;

                            l2cap_reserve_packet_buffer();
                            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                            out_buffer[0] = header;
                            out_buffer[1] = hid_host->report_id;
                            (void)memcpy(out_buffer + 2, hid_host->report, hid_host->report_len);
                            l2cap_send_prepared(hid_host->control_cid, hid_host->report_len + 2);
                            break;
                        }
                        case HID_HOST_W2_SEND_GET_PROTOCOL:{
                            uint8_t header = (HID_MESSAGE_TYPE_GET_PROTOCOL << 4);
                            uint8_t report[] = {header};
                            hid_host->state = HID_HOST_W4_GET_PROTOCOL_RESPONSE;
                            l2cap_send(hid_host->control_cid, (uint8_t*) report, sizeof(report));
                            break;   
                        }
                        case HID_HOST_W2_SEND_SET_PROTOCOL:{
                            uint8_t header = (HID_MESSAGE_TYPE_SET_PROTOCOL << 4) | hid_host->protocol_mode;
                            uint8_t report[] = {header};

                            hid_host->state = HID_HOST_W4_SET_PROTOCOL_RESPONSE;
                            l2cap_send(hid_host->control_cid, (uint8_t*) report, sizeof(report));
                            break;   
                        }
                        default:
                            break;
                    }
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            if (channel == _hid_host.interrupt_cid){
                printf("HID Interrupt data: \n");
                printf_hexdump(packet, size);
                hid_host_handle_interrupt_report(packet,  size);
                break;
            }
            if (channel != _hid_host.control_cid) break;
            hid_host = hid_host_get_instance_for_l2cap_cid(channel);
            if (!hid_host) {
                log_error("no host with cid 0x%02x", channel);
                return;
            }
            message_type   = (hid_message_type_t)(packet[0] >> 4);
            message_status = (hid_handshake_param_type_t)(packet[0] & 0x0F);
            printf("HID Control data, message_type 0x%02x, status 0x%02x: \n", message_type, message_status);
            printf_hexdump(packet, size);

            // TODO handle handshake message_status
            switch (message_type){
                case HID_MESSAGE_TYPE_DATA:
                    switch (hid_host->state){
                        case HID_HOST_W4_GET_REPORT_RESPONSE:
                            printf("HID_HOST_W4_GET_REPORT_RESPONSE \n");
                            break;
                        case HID_HOST_W4_SET_REPORT_RESPONSE:
                            printf("HID_HOST_W4_SET_REPORT_RESPONSE \n");
                            break;
                        case HID_HOST_W4_GET_PROTOCOL_RESPONSE:
                            printf("HID_HOST_W4_GET_PROTOCOL_RESPONSE \n");
                            break;
                        case HID_HOST_W4_SET_PROTOCOL_RESPONSE:
                            printf("HID_HOST_W4_GET_PROTOCOL_RESPONSE \n");
                            break;
                        default:
                            printf("HID_MESSAGE_TYPE_DATA ???\n");
                            break;
                    }
                    hid_host->state =  HID_HOST_CONTROL_CONNECTION_ESTABLISHED;
                    break;

                case HID_MESSAGE_TYPE_HID_CONTROL:
                    param = packet[0] & 0x0F;

                    switch ((hid_control_param_t)param){
                        case HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG:
                            // hid_host_emit_event(device, HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG);
                            hid_host->unplugged = true;
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}
/* LISTING_END */


static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HID Host Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - start SDP scan and connect");
    printf("o      - get output report\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'a':
            printf("Start SDP HID query for remote HID Device.\n");
            sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
            break;
        case 'c':
            if (_hid_host.unplugged){
                printf("Cannot reconnect, host is unplugged.\n");
                break;  
            } 
            printf("Reconnect, control PSM.\n");
            
            status = l2cap_create_channel(packet_handler, remote_addr, BLUETOOTH_PSM_HID_CONTROL, 48, &_hid_host.control_cid);
            if (status){
                printf("Connecting to HID Control failed: 0x%02x\n", status);
            }
            break;
        case 'i':
            if (_hid_host.unplugged){
                printf("Cannot reconnect, host is unplugged.\n");
                break;  
            } 
            printf("Reconnect, interrupt PSM.\n");
            status = l2cap_create_channel(packet_handler, remote_addr, BLUETOOTH_PSM_HID_INTERRUPT, 48, &_hid_host.interrupt_cid);
            if (status){
                printf("Connecting to HID Control failed: 0x%02x\n", status);
            }
            break;
        case 'I':
            printf("Disconnect from interrupt channel %s...\n", bd_addr_to_str(remote_addr));
            hid_host_disconnect_interrupt_channel(_hid_host.cid);
            break;
        case 'C':
            printf("Disconnect from control channel %s...\n", bd_addr_to_str(remote_addr));
            hid_host_disconnect_control_channel(_hid_host.cid);
            break;

        case 's':
            printf("Send \'Suspend\'\n");
            hid_host_send_suspend(_hid_host.cid);
            break;
        case 'S':
            printf("Send \'Exit suspend\'\n");
            hid_host_send_exit_suspend(_hid_host.cid);
            break;
        case 'u':
            printf("Send \'Unplug\'\n");
            _hid_host.unplugged = true;
            hid_host_send_virtual_cable_unplug(_hid_host.cid);
            break;
        
        case '1':
            printf("Get feature report with id 0x05 from %s\n", remote_addr_string);
            status = hid_host_send_get_feature_report(_hid_host.cid, 0x05);
            break;
        case '2':
            printf("Get output report with id 0x03 from %s\n", remote_addr_string);
            status = hid_host_send_get_output_report(_hid_host.cid, 0x03);
            break;
        case '3':
            printf("Get input report from with id 0x02 %s\n", remote_addr_string);
            status = hid_host_send_get_input_report(_hid_host.cid, 0x02);
            break;
        
        case '4':{
            uint8_t report[] = {0, 0};
            printf("Set output report with id 0x03\n");
            status = hid_host_send_set_output_report(_hid_host.cid, 0x03, report, sizeof(report));
            break;
        }
        case '5':{
            uint8_t report[] = {0, 0, 0};
            printf("Set feature report with id 0x05\n");
            status = hid_host_send_set_feature_report(_hid_host.cid, 0x05, report, sizeof(report));
            break;
        }

        case '6':{
            uint8_t report[] = {0, 0, 0, 0};
            printf("Set input report with id 0x02\n");
            status = hid_host_send_set_input_report(_hid_host.cid, 0x02, report, sizeof(report));
            break;
        }
        
        case 'p':
            printf("Get Protocol\n");
            status = hid_host_send_get_protocol(_hid_host.cid);
            break;
        
        case 'R':
            printf("Set Report mode\n");
            status = hid_host_send_set_protocol(_hid_host.cid, HID_PROTOCOL_MODE_REPORT);
            break;
        
        case 'B':
            printf("Set Boot mode\n");
            status = hid_host_send_set_protocol(_hid_host.cid, HID_PROTOCOL_MODE_BOOT);
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("HID host cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    hid_host_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);
    btstack_stdin_setup(stdin_process);
    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}

/* EXAMPLE_END */
