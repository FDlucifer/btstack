/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "mesh_generic_server.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh_health_server.h"

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_util.h"

#include "mesh/mesh_access.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_generic_model.h"
#include "mesh/mesh_generic_server.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_upper_transport.h"

static void health_server_send_message(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu){
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_unacknowledged_pdu(pdu);
}
// Health State
const mesh_access_message_t mesh_foundation_health_period_status = {
        MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_STATUS, "1"
};

const mesh_access_message_t mesh_foundation_health_attention_status = {
        MESH_FOUNDATION_OPERATION_ATTENTION_STATUS, "1"
};

static mesh_pdu_t * health_current_status(btstack_linked_list_t * faults, uint32_t opcode, uint16_t company_id){
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(opcode);
    if (!transport_pdu) return NULL;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, faults);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_fault_t * fault = (mesh_fault_t *) btstack_linked_list_iterator_next(&it);
        if (fault->company_id != company_id) continue;
        mesh_access_transport_add_uint8(transport_pdu, fault->test_id);
        mesh_access_transport_add_uint16(transport_pdu, fault->company_id);
        int i;
        for (i = 0; i < fault->num_faults; i++){
             mesh_access_transport_add_uint8(transport_pdu, fault->faults[i]);
        }
        return (mesh_pdu_t *) transport_pdu;    
    }
    // no company with company_id found
    mesh_access_transport_add_uint8(transport_pdu, 0);
    mesh_access_transport_add_uint16(transport_pdu, company_id);
    return (mesh_pdu_t *) transport_pdu;
}

static void health_fault_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t company_id = mesh_access_parser_get_u16(&parser);

    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) health_current_status(&state->registered_faults, MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS, company_id);
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void clear_faults(btstack_linked_list_t * faults, uint16_t company_id){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, faults);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_fault_t * fault = (mesh_fault_t *) btstack_linked_list_iterator_next(&it);
        if (fault->company_id != company_id) continue;
        memset(fault->faults, 0, sizeof(fault->faults));
        return;    
    }
}

static void health_fault_clear_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t company_id = mesh_access_parser_get_u16(&parser);
    
    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
   
    clear_faults(&state->registered_faults, company_id);
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) health_current_status(&state->registered_faults, MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS, company_id);
       if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void health_fault_clear_unacknowledged_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t company_id = mesh_access_parser_get_u16(&parser);

    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    if (state == NULL){
        log_error("mesh_health_state ==  NULL");
    }
    clear_faults(&state->registered_faults, company_id);
    mesh_access_message_processed(pdu);
}


static void health_fault_test_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t  test_id    = mesh_access_parser_get_u8(&parser);
    uint16_t company_id = mesh_access_parser_get_u16(&parser);
    
    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    if (state == NULL){
        log_error("mesh_health_state ==  NULL");
    }

    uint8_t event[17 ];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    // reserve for size
    pos++;
    event[pos++] = MESH_SUBEVENT_HEALTH_PERFORM_TEST;
    // element index
    event[pos++] = mesh_model->element->element_index; 
    // model_id
    little_endian_store_32(event, pos, mesh_model->model_identifier);
    pos += 4;
    
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    little_endian_store_16(event, pos, mesh_pdu_netkey_index(pdu));
    pos += 2;
    little_endian_store_16(event, pos, mesh_pdu_appkey_index(pdu));
    pos += 2;
    little_endian_store_16(event, pos, company_id);
    pos += 2;
    event[pos++] = test_id; 
    event[1] = pos - 2;

    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

static mesh_pdu_t * health_fault_status(btstack_linked_list_t * faults, uint32_t opcode, uint8_t test_id, uint16_t company_id){
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(opcode);
    if (!transport_pdu) return NULL;
    mesh_access_transport_add_uint8(transport_pdu, test_id);

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, faults);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_fault_t * fault = (mesh_fault_t *) btstack_linked_list_iterator_next(&it);
        if (fault->company_id != company_id) continue;
        mesh_access_transport_add_uint8(transport_pdu, fault->test_id);
        mesh_access_transport_add_uint16(transport_pdu, fault->company_id);
        int i;
        for (i = 0; i < fault->num_faults; i++){
             mesh_access_transport_add_uint8(transport_pdu, fault->faults[i]);
        }
        return (mesh_pdu_t *) transport_pdu;    
    }
    // no company with company_id found
    mesh_access_transport_add_uint8(transport_pdu, 0);
    mesh_access_transport_add_uint16(transport_pdu, company_id);
    return (mesh_pdu_t *) transport_pdu;
}

void mesh_health_server_report_test_done(uint16_t element_index, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t test_id, uint16_t company_id){
    mesh_element_t * element = mesh_element_for_index(element_index);
    if (element == NULL) return;
    
    mesh_model_t * mesh_model = mesh_model_get_by_identifier(element, mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_HEALTH_SERVER));
    if (mesh_model == NULL) return;

    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) health_fault_status(&state->registered_faults, MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS, test_id, company_id);
    if (!transport_pdu) return;
    health_server_send_message(element_index, dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu);
}

// Health Message
const static mesh_operation_t mesh_health_model_operations[] = {
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_GET,                                   2, health_fault_get_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR,                                 2, health_fault_clear_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR_UNACKNOWLEDGED,                  2, health_fault_clear_unacknowledged_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST,                                  3, health_fault_test_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST_UNACKNOWLEDGED,                   3, health_fault_test_unacknowledged_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS_UNACKNOWLEDGED,                 3, health_fault_test_status_unacknowledged_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_GET,                                  2, health_period_get_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET,                                  2, health_period_set_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET_UNACKNOWLEDGED,                   2, health_period_set_unacknowledged_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_GET,                               2, health_attention_get_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_SET,                               2, health_attention_set_handler },
    // { MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_SET_UNACKNOWLEDGED,                2, health_attention_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_health_server_get_operations(void){
    return mesh_health_model_operations;
}

void  mesh_health_server_register_packet_handler(mesh_model_t *mesh_model, btstack_packet_handler_t events_packet_handler){
    if (events_packet_handler == NULL){
        log_error(" mesh_health_server_register_packet_handler called with NULL callback");
        return;
    }
    if (mesh_model == NULL){
        log_error(" mesh_health_server_register_packet_handler called with NULL mesh_model");
        return;
    }
    mesh_model->model_packet_handler = &events_packet_handler;
}
