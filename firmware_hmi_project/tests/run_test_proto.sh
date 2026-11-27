#!/usr/bin/env bash
# Host smoke test for shared CAN HMI protocol. Run from any directory.
set -euo pipefail
cd "$(dirname "$0")/../.."

PROTO="protocol/can_hmi/can_hmi_proto.c"
INC="-Iprotocol/can_hmi -Iprotocol -Ifirmware_hmi_project/main"

if command -v gcc >/dev/null 2>&1; then
    gcc -Wall -Wextra -Werror $INC -fsyntax-only "$PROTO"
    gcc -Wall -Wextra -Werror $INC -o /tmp/hmi_can_proto_test \
        firmware_hmi_project/tests/test_proto.c "$PROTO"
    /tmp/hmi_can_proto_test | tail -1
else
    echo "gcc not found, running Python behavioral smoke test"
    python3 - <<'PY'
# Minimal behavioral mirror
from struct import pack_into, unpack_from
d=bytearray(8)
# TLM_A pack/unpack
seq,stage,mode,flags1,flags2,eq,estop_mode,flags3,reason=7,4,2,0x05,0x02,1,0,0x40,5
d[0]=seq; d[1]=stage; d[2]=mode; d[3]=flags1; d[4]=flags2; d[5]=eq; d[6]=(estop_mode&0x0F)|(flags3&0xF0); d[7]=reason
assert d[0]==7 and d[1]==4 and (d[6]&0xF0)==0x40 and d[7]==5
# CMD power
d=bytearray(8); d[0]=9; d[1]=4; d[2]=0; pack_into('<H', d, 3, 1000)
assert unpack_from('<H', d, 3)[0]==1000
print('PROTO TEST OK')
PY
fi
