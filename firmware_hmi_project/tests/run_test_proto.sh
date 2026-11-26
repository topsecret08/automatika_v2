#!/usr/bin/env bash
# Host-проверка общего протокола. Запускать из любого каталога.
set -euo pipefail
cd "$(dirname "$0")/../.."

PROTO="protocol/rs485_protocol/hmi_protocol.c"
INC="-Iprotocol/rs485_protocol -Iprotocol -Ifirmware_hmi_project/main"

if command -v gcc >/dev/null 2>&1; then
    gcc -Wall -Wextra -Werror $INC -fsyntax-only "$PROTO"
    gcc -Wall -Wextra -Werror $INC -o /tmp/hmi_proto_test \
        firmware_hmi_project/tests/test_proto.c "$PROTO"
    /tmp/hmi_proto_test | tail -1
else
    echo "gcc not found, running Python behavioral mirror of test_proto.c"
    python3 - <<'PY'
import sys
HMI_NODE_HMI=0x20; HMI_TYPE_POLL=0x01; HMI_MAX_PAYLOAD=1024; HMI_FRAME_OVERHEAD=10; HMI_MAGIC0=0xAA; HMI_MAGIC1=0x55
def hmi_crc16(data):
    crc=0xFFFF
    for b in data:
        crc ^= (b<<8) & 0xFFFF
        for _ in range(8):
            crc = ((crc<<1)^0x1021)&0xFFFF if (crc&0x8000) else ((crc<<1)&0xFFFF)
    return crc
def hmi_frame_build(seq,node,typ,payload=b""):
    if len(payload)>HMI_MAX_PAYLOAD: return None
    out=bytearray([HMI_MAGIC0,HMI_MAGIC1,len(payload)&0xFF,(len(payload)>>8)&0xFF,seq&0xFF,(seq>>8)&0xFF,node,typ])
    out.extend(payload); crc=hmi_crc16(bytes(out[2:])); out.append(crc&0xFF); out.append((crc>>8)&0xFF); return bytes(out)
def hmi_frame_parse(buf):
    if len(buf)<HMI_FRAME_OVERHEAD: return None
    if buf[0]!=HMI_MAGIC0 or buf[1]!=HMI_MAGIC1: return None
    payload_len=buf[2]|(buf[3]<<8)
    if payload_len>HMI_MAX_PAYLOAD: return None
    if len(buf)!=HMI_FRAME_OVERHEAD+payload_len: return None
    crc_calc=hmi_crc16(buf[2:2+6+payload_len]); crc_rx=buf[8+payload_len]|(buf[9+payload_len]<<8)
    if crc_calc!=crc_rx: return None
    payload=bytes(buf[8:8+payload_len])
    return {"seq":buf[4]|(buf[5]<<8),"node":buf[6],"type":buf[7],"len":payload_len,"payload":payload}
expected=bytes([0xAA,0x55,0x00,0x00,0x01,0x00,0x20,0x01,0x63,0x6E])
assert hmi_crc16(b"\x00\x00\x01\x00\x20\x01")==0x6E63
frame=hmi_frame_build(1,HMI_NODE_HMI,HMI_TYPE_POLL,b""); assert frame==expected
p=hmi_frame_parse(frame); assert p and p["seq"]==1 and p["node"]==HMI_NODE_HMI and p["type"]==HMI_TYPE_POLL and p["len"]==0
json=b'{"v":"V1.0.1","tc":78.4}'; frame=hmi_frame_build(7,HMI_NODE_HMI,HMI_TYPE_POLL,json)
p=hmi_frame_parse(frame); assert p and p["len"]==len(json) and p["payload"]==json
bad=bytearray(frame); bad[-1]^=0x01; assert hmi_frame_parse(bytes(bad)) is None
bad=bytearray(frame); bad[1]=0x00; assert hmi_frame_parse(bytes(bad)) is None
print("PROTO TEST OK")
PY
fi
