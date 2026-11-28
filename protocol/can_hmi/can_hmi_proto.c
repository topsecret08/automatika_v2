#include "can_hmi_proto.h"

#include <string.h>

bool can_hmi_pack_tlm_a(uint8_t d[8], const can_hmi_tlm_a_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq; d[1] = s->stage; d[2] = s->mode_type; d[3] = s->flags1;
    d[4] = s->flags2; d[5] = s->eq_phase;
    d[6] = (uint8_t)((s->estop_mode & 0x0Fu) | (s->flags3 & 0xF0u));
    d[7] = s->reason_code;
    return true;
}
bool can_hmi_unpack_tlm_a(const uint8_t d[8], can_hmi_tlm_a_t *s) {
    if (!d || !s) return false;
    s->seq = d[0]; s->stage = d[1]; s->mode_type = d[2]; s->flags1 = d[3];
    s->flags2 = d[4]; s->eq_phase = d[5];
    s->estop_mode = (uint8_t)(d[6] & 0x0Fu);
    s->flags3 = (uint8_t)(d[6] & 0xF0u);
    s->reason_code = d[7];
    return true;
}
bool can_hmi_pack_tlm_b(uint8_t d[8], const can_hmi_tlm_b_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq;
    can_hmi_put_i16(&d[1], s->cube_x100);
    can_hmi_put_i16(&d[3], s->column_x100);
    can_hmi_put_i16(&d[5], s->deph_x100);
    d[7] = (uint8_t)s->reserved;
    return true;
}
bool can_hmi_unpack_tlm_b(const uint8_t d[8], can_hmi_tlm_b_t *s) {
    if (!d || !s) return false;
    s->seq = d[0];
    s->cube_x100 = can_hmi_i16(&d[1]);
    s->column_x100 = can_hmi_i16(&d[3]);
    s->deph_x100 = can_hmi_i16(&d[5]);
    s->reserved = (int8_t)d[7];
    return true;
}
bool can_hmi_pack_tlm_c(uint8_t d[8], const can_hmi_tlm_c_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq;
    can_hmi_put_i16(&d[1], s->jacket_x100);
    can_hmi_put_i16(&d[3], s->tank_x100);
    can_hmi_put_i16(&d[5], s->ambient_x100);
    d[7] = s->level_pct;
    return true;
}
bool can_hmi_unpack_tlm_c(const uint8_t d[8], can_hmi_tlm_c_t *s) {
    if (!d || !s) return false;
    s->seq = d[0];
    s->jacket_x100 = can_hmi_i16(&d[1]);
    s->tank_x100 = can_hmi_i16(&d[3]);
    s->ambient_x100 = can_hmi_i16(&d[5]);
    s->level_pct = d[7];
    return true;
}
bool can_hmi_pack_tlm_d(uint8_t d[8], const can_hmi_tlm_d_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq;
    can_hmi_put_i16(&d[1], s->pressure_x1000);
    can_hmi_put_i16(&d[3], s->vacuum_x10);
    can_hmi_put_u16(&d[5], s->flow_x100);
    d[7] = s->power_x2;
    return true;
}
bool can_hmi_unpack_tlm_d(const uint8_t d[8], can_hmi_tlm_d_t *s) {
    if (!d || !s) return false;
    s->seq = d[0];
    s->pressure_x1000 = can_hmi_i16(&d[1]);
    s->vacuum_x10 = can_hmi_i16(&d[3]);
    s->flow_x100 = can_hmi_u16(&d[5]);
    s->power_x2 = d[7];
    return true;
}
bool can_hmi_pack_tlm_e(uint8_t d[8], const can_hmi_tlm_e_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq;
    can_hmi_put_u16(&d[1], s->heads_ml);
    can_hmi_put_u16(&d[3], s->body_ml);
    can_hmi_put_u16(&d[5], s->tails_ml);
    d[7] = s->lsa_duty_x2;
    return true;
}
bool can_hmi_unpack_tlm_e(const uint8_t d[8], can_hmi_tlm_e_t *s) {
    if (!d || !s) return false;
    s->seq = d[0];
    s->heads_ml = can_hmi_u16(&d[1]);
    s->body_ml = can_hmi_u16(&d[3]);
    s->tails_ml = can_hmi_u16(&d[5]);
    s->lsa_duty_x2 = d[7];
    return true;
}
bool can_hmi_pack_tlm_f(uint8_t d[8], const can_hmi_tlm_f_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq;
    can_hmi_put_i16(&d[1], s->pt506_x1000);
    can_hmi_put_i16(&d[3], s->xgzp_x10);
    d[5] = s->zmpt_v1; d[6] = s->zmpt_v2; d[7] = s->zmpt_v3;
    /* zmpt_fault_phase packed in ACK/config or we accept drop; better pack in byte spare?
     * No spare. Put fault in bit7 of v1/v2/v3? no. Use separate ACK data.
     * Keep simple: store fault in v3 high bit? no. We'll pack fault into SNAPSHOT ack data. */
    (void)s->zmpt_fault_phase;
    return true;
}
bool can_hmi_unpack_tlm_f(const uint8_t d[8], can_hmi_tlm_f_t *s) {
    if (!d || !s) return false;
    s->seq = d[0];
    s->pt506_x1000 = can_hmi_i16(&d[1]);
    s->xgzp_x10 = can_hmi_i16(&d[3]);
    s->zmpt_v1 = d[5]; s->zmpt_v2 = d[6]; s->zmpt_v3 = d[7];
    s->zmpt_fault_phase = 0;
    return true;
}
bool can_hmi_pack_ack(uint8_t d[8], const can_hmi_ack_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq; d[1] = s->kind; d[2] = s->ok; d[3] = s->err_code;
    memcpy(&d[4], s->data, 4);
    return true;
}
bool can_hmi_unpack_ack(const uint8_t d[8], can_hmi_ack_t *s) {
    if (!d || !s) return false;
    s->seq = d[0]; s->kind = d[1]; s->ok = d[2]; s->err_code = d[3];
    memcpy(s->data, &d[4], 4);
    return true;
}
bool can_hmi_pack_tlm_g(uint8_t d[8], const can_hmi_tlm_g_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq;
    can_hmi_put_i16(&d[1], s->target_x100);
    can_hmi_put_i16(&d[3], s->deph_x100);
    can_hmi_put_u16(&d[5], s->elapsed_lo);
    d[7] = s->elapsed_hi;
    return true;
}
bool can_hmi_unpack_tlm_g(const uint8_t d[8], can_hmi_tlm_g_t *s) {
    if (!d || !s) return false;
    s->seq = d[0];
    s->target_x100 = can_hmi_i16(&d[1]);
    s->deph_x100 = can_hmi_i16(&d[3]);
    s->elapsed_lo = can_hmi_u16(&d[5]);
    s->elapsed_hi = d[7];
    s->reserved = 0;
    return true;
}
bool can_hmi_pack_cmd(uint8_t d[8], const can_hmi_cmd_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq; d[1] = s->cmd;
    d[2] = s->p0; d[3] = s->p1; d[4] = s->p2; d[5] = s->p3; d[6] = s->p4; d[7] = s->p5;
    return true;
}
bool can_hmi_unpack_cmd(const uint8_t d[8], can_hmi_cmd_t *s) {
    if (!d || !s) return false;
    s->seq = d[0]; s->cmd = d[1];
    s->p0 = d[2]; s->p1 = d[3]; s->p2 = d[4]; s->p3 = d[5]; s->p4 = d[6]; s->p5 = d[7];
    return true;
}
bool can_hmi_pack_log(uint8_t d[8], const can_hmi_log_t *s) {
    if (!d || !s) return false;
    d[0] = s->seq; d[1] = s->rec; d[2] = s->type; d[3] = s->flags;
    memcpy(&d[4], s->data, 4);
    return true;
}
bool can_hmi_unpack_log(const uint8_t d[8], can_hmi_log_t *s) {
    if (!d || !s) return false;
    s->seq = d[0]; s->rec = d[1]; s->type = d[2]; s->flags = d[3];
    memcpy(s->data, &d[4], 4);
    return true;
}
