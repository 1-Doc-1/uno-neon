#!/bin/sh
# Démarrage du conteneur web NOC (ADR 0044). Il ne calcule rien : il VÉRIFIE la configuration et refuse de démarrer si
# elle est abîmée, puis lance Caddy. Un site qu'on croyait protégé ne reste jamais ouvert par erreur.
set -eu

fail() {
    echo "NOC (web) : $*" >&2
    exit 1
}

# 0. Docker Compose passe SITE_PASSWORD_HASH VIDE quand personne ne l'a définie (${SITE_PASSWORD_HASH:-}). Pour Caddy, une
#    variable vide n'est pas une variable absente : elle ferait échouer la configuration (mot de passe vide). On la retire.
if [ -z "${SITE_PASSWORD_HASH:-}" ]; then
    unset SITE_PASSWORD_HASH
fi

# 1. Le hachage du mot de passe, s'il y en a un : un bcrypt de 60 caractères ($2a$, $2b$ ou $2y$, coût à deux chiffres).
#    Les « $ » d'un bcrypt sont mangés par l'interpolation de Docker Compose s'ils ne sont pas protégés ($$ ou
#    « Literal » dans Coolify, voir docs/DEPLOIEMENT.md) : on le détecte ici, sans jamais afficher la valeur.
if [ -n "${SITE_PASSWORD_HASH:-}" ]; then
    if ! printf '%s' "$SITE_PASSWORD_HASH" | grep -Eq '^\$2[aby]\$[0-9]{2}\$[./A-Za-z0-9]{53}$'; then
        fail "SITE_PASSWORD_HASH n'est pas un hachage bcrypt valide (longueur reçue : ${#SITE_PASSWORD_HASH}, 60 attendus).
Les « \$ » du hachage ont sans doute été interprétés par Docker Compose : cochez « Literal » sur la variable dans
Coolify, ou doublez chaque « \$ » (\$\$). Voir docs/DEPLOIEMENT.md, « Activer le mot de passe »."
    fi
fi

# 2. Les réseaux de Traefik (les seuls dont Caddy croit X-Forwarded-For) : des plages CIDR d'au moins /8, jamais « tout ».
subnets="${NOC_TRUSTED_PROXIES:-172.16.0.0/12}"
trusted=""
for cidr in $(printf '%s' "$subnets" | tr ',' ' '); do
    printf '%s' "$cidr" | grep -Eq '^([0-9]{1,3}\.){3}[0-9]{1,3}/[0-9]{1,2}$' \
        || fail "NOC_TRUSTED_PROXIES : '$cidr' n'est pas une plage IPv4 comme 172.16.0.0/12."
    prefix="${cidr#*/}"
    [ "$prefix" -ge 8 ] && [ "$prefix" -le 32 ] \
        || fail "NOC_TRUSTED_PROXIES : '$cidr' est trop large (au moins /8) : faire confiance à tout le monde est refusé."
    trusted="$trusted $cidr"
done
[ -n "$trusted" ] || fail "NOC_TRUSTED_PROXIES est vide."
NOC_TRUSTED_PROXIES="${trusted# }"
export NOC_TRUSTED_PROXIES

exec /usr/local/bin/caddy-noc run --config /etc/caddy/Caddyfile --adapter caddyfile
