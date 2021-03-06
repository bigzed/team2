#include "saul_reg.h"
#include "net/gcoap.h"
#include "senml.h"
#include "floatint.h"

#include "smart_environment.h"

#define PING_TIMEOUT	60
#define SENML_LEN       75


int8_t senml_json_strout(char* json_buf, uint8_t dev_type) {
    phydat_t res;
    saul_reg_t* dev = saul_reg_find_type(dev_type);

    size_t num = saul_reg_read(dev, &res);

    char dev_name[21];
    strncpy(dev_name, dev->name, 20);
    dev_name[20] = 0;

    char unit[5];
    strncpy(unit, phydat_unit_to_str(res.unit), 4);
    unit[4] = 0;
    // necessary because later a snprintf will be used
    if (strcmp(unit, "%") == 0) {
    	unit[1] = '%';
    	unit[2] = 0;
    } 

    char values[25];
    strncpy(values, "", 1);
    for (size_t i=0; i<num; i++) {
        char curr_value[10];
        int_to_floatstring(curr_value, res.val[i], res.scale);
        strncat(values, curr_value, 10);
        if (i<num-1) {
            strncat(values, ",", 1);
        }
    }

    senml_base_info_t base_info = {
        .version = SENML_SUPPORTED_VERSION,
        .base_time = 0
    };
    senml_record_t records = {
	    .name = dev_name,
        .time = 0,
        .update_time = 0,
        .unit = unit,
        .value_sum = 0,
        .value_type = SENML_TYPE_STRING,
        .value.s = values
    };
    senml_pack_t pack = {
        .base_info = &base_info,
        .num = 1,
        .records = &records
    };
    int8_t senml_res = senml_encode_json_s(&pack, json_buf, SENML_LEN);

    if (!senml_res) {
        return 0;
    } else {
        return -1;
    }
}

ssize_t senml_json_send(coap_pkt_t* pdu, uint8_t *buf, size_t len, uint8_t dev_type) {
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);

    char* json_buf = malloc(SENML_LEN);
    int8_t senml_res = senml_json_strout(json_buf, dev_type);

    if (!senml_res) {
        printf("Successfully created SenML JSON string: %s\n", json_buf);

        size_t payload_len = snprintf(
            (char*)pdu->payload,
            GCOAP_PDU_BUF_SIZE,
            json_buf
        );

        free(json_buf);

        int8_t gcoap_res = gcoap_finish(pdu, payload_len, COAP_FORMAT_JSON);
        if (gcoap_res < 0) {
            puts("Failure sending message.");
        } else {
            printf("Message successfully sent with PDU size %d.\n", gcoap_res);
        }
        return gcoap_res;
    } else {
        puts("Failed to create full SenML JSON string. Message will not be sent.");
        free(json_buf);
        return -1;
    }
}

ssize_t temp_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len) {
    puts("Data from thermometer requested.");
    return senml_json_send(pdu, buf, len, SAUL_SENSE_TEMP);
}

ssize_t humid_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len) {
    puts("Data from humidity sensor requested.");
    return senml_json_send(pdu, buf, len, SAUL_SENSE_HUM);
}

ssize_t mag_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len) {
    puts("Data from magnetometer requested.");
    return senml_json_send(pdu, buf, len, SAUL_SENSE_MAG);
}

ssize_t press_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len) {
    puts("Data from pressure sensor requested.");
    return senml_json_send(pdu, buf, len, SAUL_SENSE_PRESS);
}

ssize_t gyro_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len) {
    puts("Data from gyroscope requested.");
    return senml_json_send(pdu, buf, len, SAUL_SENSE_GYRO);
}

ssize_t color_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len) {
    puts("Data from color sensor requested.");
    return senml_json_send(pdu, buf, len, SAUL_SENSE_COLOR);
}


void* ping_handler(void* args) {
    (void)args;
    sock_udp_ep_t remote = SOCK_IPV6_EP_ANY;
    remote.port = SERVER_CONN_PORT;
    ipv6_addr_from_str((ipv6_addr_t *)&remote.addr.ipv6, "ff02::1");
    
    puts("Ping thread running");
    
    while(true) {
        ssize_t res = sock_udp_send(
            NULL,
            app_id,
            APP_ID_LEN,
            &remote
        );
        
        if(res == -EAFNOSUPPORT) {
            fputs("Ping: EAFNOSUPPORT", stderr);
        } else if(res == -EHOSTUNREACH) {
            fputs("Ping: EHOSTUNREACH", stderr);
        } else if(res == -EINVAL) {
            fputs("Ping: EINVAL", stderr);
        } else if(res == -ENOMEM) {
            fputs("Ping: ENOMEM", stderr);
        } else if(res == -ENOTCONN) {
            fputs("Ping: ENOTCONN", stderr);
        } else if(res < 0) {
            fprintf(stderr, "Ping error: %d\n", res);
        }
        
        if(res < 0) {
            break;
        } else {
            xtimer_sleep(PING_TIMEOUT);
        }
    }
    
    puts("ping thread terminating!");
    
    return NULL;
}

const coap_resource_t coap_resources[] = {
    /* ressource-path, ressource-type, response-handler */
    // {"/se-app/color", COAP_GET, &color_handler},
    // {"/se-app/gyro", COAP_GET, &gyro_handler},
    {"/se-app/humid", COAP_GET, &humid_handler},
    // {"/se-app/mag", COAP_GET, &mag_handler},
    // {"/se-app/press", COAP_GET, &press_handler},
    {"/se-app/temp", COAP_GET, &temp_handler}
};

gcoap_listener_t coap_listener = {
    (coap_resource_t *)&coap_resources,
    sizeof(coap_resources) / sizeof(coap_resources[0]),
    NULL
};
