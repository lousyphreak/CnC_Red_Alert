#!/bin/sh
set -eu

AUTH_CONFIG_FILE=/etc/nginx/conf.d/redalert-basic-auth.conf
AUTH_FILE=/etc/nginx/auth/redalert.htpasswd

mkdir -p /etc/nginx/auth

if [ -n "${RA_BASIC_AUTH_HTPASSWD:-}" ]; then
    printf '%s\n' "${RA_BASIC_AUTH_HTPASSWD}" > "${AUTH_FILE}"
elif [ -n "${RA_BASIC_AUTH_USERNAME:-}" ] && [ -n "${RA_BASIC_AUTH_PASSWORD:-}" ]; then
    printf '%s:%s\n' "${RA_BASIC_AUTH_USERNAME}" "$(openssl passwd -apr1 "${RA_BASIC_AUTH_PASSWORD}")" > "${AUTH_FILE}"
else
    printf '# basic auth disabled\n' > "${AUTH_CONFIG_FILE}"
    rm -f "${AUTH_FILE}"
    exit 0
fi

cat > "${AUTH_CONFIG_FILE}" <<EOF
auth_basic "Authentication Required - Red Alert";
auth_basic_user_file ${AUTH_FILE};
EOF
