/* Host smoke test for CAN HMI protocol pack/unpack. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "can_hmi_proto.h"

int main(void)
{
    uint8_t d[8];
    can_hmi_tlm_a_t a = { .seq = 7, .stage = 4, .mode_type = 2, .flags1 = CAN_HMI_F1_RUN | CAN_HMI_F1_SAFETY,
                          .flags2 = CAN_HMI_F2_EQ_LOCK, .eq_phase = 1, .estop_mode = 0, .flags3 = CAN_HMI_F3_DB_READY,
                          .reason_code = CAN_HMI_REASON_EQ_LOCK };
    can_hmi_tlm_a_t a2;
    assert(can_hmi_pack_tlm_a(d, &a));
    assert(can_hmi_unpack_tlm_a(d, &a2));
    assert(a2.seq == 7 && a2.stage == 4 && a2.mode_type == 2);
    assert(a2.flags1 == a.flags1 && a2.flags2 == a.flags2);
    assert(a2.eq_phase == 1 && a2.reason_code == CAN_HMI_REASON_EQ_LOCK);

    can_hmi_cmd_t cmd = { .seq = 9, .cmd = CAN_HMI_CMD_POWER, .p0 = 0, .p1 = 232, .p2 = 3 }; /* 1000 x10 */
    can_hmi_cmd_t cmd2;
    assert(can_hmi_pack_cmd(d, &cmd));
    assert(can_hmi_unpack_cmd(d, &cmd2));
    assert(cmd2.seq == 9 && cmd2.cmd == CAN_HMI_CMD_POWER);
    assert(can_hmi_u16(&d[3]) == 1000);

    can_hmi_ack_t ack = { .seq = 9, .kind = CAN_HMI_ACK_ACK, .ok = 1, .err_code = CAN_HMI_ERR_OK };
    can_hmi_ack_t ack2;
    assert(can_hmi_pack_ack(d, &ack));
    assert(can_hmi_unpack_ack(d, &ack2));
    assert(ack2.ok == 1 && ack2.kind == CAN_HMI_ACK_ACK);

    puts("PROTO TEST OK");
    return 0;
}
