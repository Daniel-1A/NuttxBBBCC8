#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include "lorawan/uart_lorawan_layer.h"

int lora_init(void) { return -ENOSYS; }

void lorawan_radioenge_init(config_lorawan_radioenge_t config) { (void)config; }

/* Firma igual a la del .h */
int lorawan_radioenge_send_msg(unsigned char *pt_payload_uplink_hexstring,
                               int payload_len,
                               unsigned char *pt_downlink_hexstring,
                               int max_downlink_len,
                               int timeout_ms)
{
  (void)pt_payload_uplink_hexstring;
  (void)payload_len;
  (void)timeout_ms;
  if (pt_downlink_hexstring && max_downlink_len > 0) {
    pt_downlink_hexstring[0] = '\0';
  }
  return -ENOSYS; /* no implementado en BBB para la preentrega */
}
