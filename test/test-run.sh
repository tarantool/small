#!/bin/bash

TESTS=$@
if [ "x${TESTS}" = "x" ]; then
    TESTS=*.test
fi

tmp=$(mktemp)

cat <<EOF
------------------------------------------------------------
TEST                                            RESULT
------------------------------------------------------------
EOF

for t in ${TESTS}; do
    echo -e -n "$t"
    name="${t%.*}"
    result="${name}.result"
    reject="${name}.reject"
    ./${t} > "${reject}"
    diff -U8 "${result}" "${reject}" > "${tmp}"
    if [ $? -eq 0 ]; then
        rm -f "${reject}"
        echo -e "\r\t\t\t\t\t\t[ pass ]"
    else
        echo -e "\r\t\t\t\t\t\t[ fail ]"
        cat ${tmp}
    fi
done
rm -f "${tmp}"
