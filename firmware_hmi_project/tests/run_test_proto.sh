#!/usr/bin/env bash
# Host-проверка общего протокола. Запускать из любого каталога.
set -euo pipefail
cd "$(dirname "$0")/../.."

PROTO="protocol/component/hmi_protocol.c"
INC="-Iprotocol/component -Iprotocol -Ifirmware_hmi_project/main"

gcc -Wall -Wextra -Werror $INC -fsyntax-only "$PROTO"
gcc -Wall -Wextra -Werror $INC -o /tmp/hmi_proto_test \
    firmware_hmi_project/tests/test_proto.c "$PROTO"
/tmp/hmi_proto_test | tail -1
