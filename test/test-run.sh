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
    printf "%-48s" $t
    name="${t%.*}"
    result="${name}.result"
    reject="${name}.reject"
    ./${t} > "${reject}"
    diff -U8 "${result}" "${reject}" > "${tmp}"
    if [ $? -eq 0 ]; then
        rm -f "${reject}"
        echo "[ pass ]"
    else
        echo "[ fail ]"
        cat ${tmp}
    fi
done
rm -f "${tmp}"
