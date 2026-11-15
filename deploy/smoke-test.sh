#!/usr/bin/env bash
# Test de fumée du déploiement (ADR 0044) : construit les deux images, les inspecte, puis lance docker-compose.yml dans
# quatre configurations (site ouvert, mot de passe protégé de deux façons, mot de passe mal protégé). Lancé par le CI
# (job deploy-smoke) ; à la main, il faut Docker et Docker Compose v2 :   ./deploy/smoke-test.sh
set -euo pipefail

cd "$(dirname "$0")/.."

TAG=smoke
SERVER_IMAGE="ghcr.io/1-doc-1/noc-server:${TAG}"
WEB_IMAGE="ghcr.io/1-doc-1/noc-web:${TAG}"
ORIGIN="https://noc.example.test"
PASSWORD="soiree-entre-amis"
PROJECT="nocsmoke"
WORK="$(mktemp -d)"
trap 'docker compose -p "$PROJECT" -f deploy/docker-compose.yml down -v --remove-orphans >/dev/null 2>&1 || true; rm -rf "$WORK"' EXIT

step() { printf '\n==> %s\n' "$*"; }
fail() { printf 'ÉCHEC : %s\n' "$*" >&2; exit 1; }

compose() {
    docker compose -p "$PROJECT" -f deploy/docker-compose.yml --env-file "$ENV_FILE" "$@"
}

# Code HTTP d'une adresse, avec des identifiants facultatifs ; « 000 » si personne ne répond.
http_status() {
    local url="$1"; shift
    curl -s -o /dev/null -w '%{http_code}' --max-time 5 "$@" "$url" || true
}

expect_status() {
    local want="$1" label="$2"; shift 2
    local got
    got="$(http_status "$@")"
    [ "$got" = "$want" ] || fail "$label : code $got au lieu de $want"
    echo "OK   $label ($got)"
}

web_address() {
    docker inspect -f "{{(index .NetworkSettings.Networks \"${PROJECT}_default\").IPAddress}}" \
        "$(compose ps -q web)"
}

# Quand un conteneur ne devient pas sain : son état (code de sortie, OOM, erreur Docker) et le même conteneur lancé à la main,
# avec puis sans chaque durcissement, pour désigner le coupable.
diagnose() {
    local service="$1" id image
    id="$(compose ps -aq "$service")"
    echo "--- État du conteneur $service" >&2
    docker inspect -f '{{json .State}} restarts={{.RestartCount}}' "$id" >&2 || true
    sudo dmesg 2>/dev/null | tail -n 40 | grep -iE "oom|killed process" >&2 || true
    if [ "$service" = web ]; then
        echo "--- Le conteneur web lancé à la main, variante par variante (124 = toujours en vie après 8 s : bon signe)" >&2
        local hardening="--read-only --tmpfs /tmp --cap-drop ALL --security-opt no-new-privileges:true -m 96m --memory-swap 96m --pids-limit 128 --cpus 1"
        local variants=("$hardening" "--tmpfs /tmp --cap-drop ALL -m 96m --memory-swap 96m" "--read-only --tmpfs /tmp -m 96m --memory-swap 96m" "--read-only --tmpfs /tmp --cap-drop ALL" "--read-only --tmpfs /tmp --cap-drop ALL -m 96m --memory-swap 96m --cpus 1" "")
        local flags code
        for flags in "${variants[@]}"; do
            # shellcheck disable=SC2086
            code=0
            timeout 8 docker run --rm -e GOMEMLIMIT=64MiB $flags "$WEB_IMAGE" >"$WORK/diag.log" 2>&1 || code=$?
            echo "[${flags:-sans aucune option}] -> code $code" >&2
            tail -n 3 "$WORK/diag.log" >&2
        done
    fi
}

wait_healthy() {
    local service="$1" deadline=$((SECONDS + 120)) id state
    id="$(compose ps -q "$service")"
    while [ "$SECONDS" -lt "$deadline" ]; do
        state="$(docker inspect -f '{{.State.Health.Status}}' "$id" 2>/dev/null || echo none)"
        [ "$state" = healthy ] && return 0
        sleep 2
    done
    compose logs "$service" 2>&1 | tail -n 12 >&2 || true
    diagnose "$service"
    fail "$service n'est pas devenu « healthy » en 120 s"
}

# ---- 1. Les images --------------------------------------------------------------------------------------------------
step "Construction des images"
docker build -f deploy/Dockerfile.server -t "$SERVER_IMAGE" .
docker build -f deploy/Dockerfile.web -t "$WEB_IMAGE" .
docker images --format '{{.Repository}}:{{.Tag}} {{.Size}}' | grep "ghcr.io/1-doc-1/noc-" || true

step "Inspection des images"
[ "$(docker run --rm --entrypoint id "$SERVER_IMAGE" -u)" = 10001 ] || fail "l'image serveur ne tourne pas en utilisateur 10001"
[ "$(docker run --rm --entrypoint id "$WEB_IMAGE" -u)" = 10001 ] || fail "l'image web ne tourne pas en utilisateur 10001"
echo "OK   utilisateur non root (10001) dans les deux images"

# Binaire de production : aucun crochet de test (SPEC §9.5). grep -c rend 0 (et un code 1) quand rien n'est trouvé.
hooks="$(docker run --rm --entrypoint grep "$SERVER_IMAGE" -c -e UNO_TEST_ -e UNO_ENABLE_TEST_HOOKS /app/uno_server || true)"
[ "$hooks" = 0 ] || fail "le binaire du serveur contient des crochets de test ($hooks occurrences)"
echo "OK   aucun crochet de test dans le binaire du serveur"

docker run --rm --entrypoint ldd "$SERVER_IMAGE" /app/uno_server
missing="$(docker run --rm --entrypoint sh "$SERVER_IMAGE" -c 'ldd /app/uno_server | grep -c "not found" || true' 2>/dev/null || echo 0)"
[ "${missing:-0}" = 0 ] || fail "des bibliothèques manquent dans l'image du serveur"
echo "OK   le binaire n'a besoin d'aucune bibliothèque absente de l'image"

# Aucune page /dev dans le client servi
if docker run --rm --entrypoint sh "$WEB_IMAGE" -c "grep -rl 'dev/table' /srv" | grep -q .; then
    fail "le client servi contient des pages de développement"
fi
echo "OK   aucune page de développement dans le client servi"

# ---- 2. Les quatre configurations ---------------------------------------------------------------------------------
HASH="$(docker run --rm --entrypoint caddy-noc "$WEB_IMAGE" hash-password --plaintext "$PASSWORD")"
[ "${#HASH}" = 60 ] || fail "hachage bcrypt inattendu (longueur ${#HASH})"
ESCAPED="${HASH//\$/\$\$}"

base_env() {
    cat <<EOF
NOC_IMAGE_TAG=${TAG}
NOC_PUBLIC_ORIGIN=${ORIGIN}
EOF
}

up() {
    compose up -d
}

check_isolation() {
    local networks ports
    networks="$(docker inspect -f '{{range $name, $_ := .NetworkSettings.Networks}}{{$name}} {{end}}' "$(compose ps -q server)")"
    [ "$networks" = "${PROJECT}_noc-internal " ] || fail "le serveur doit être SEUL sur le réseau interne, il est sur : $networks"
    [ "$(docker network inspect -f '{{.Internal}}' "${PROJECT}_noc-internal")" = true ] || fail "noc-internal n'est pas internal: true"
    ports="$(docker ps --filter "label=com.docker.compose.project=${PROJECT}" --format '{{.Ports}}' | tr -d '[:space:]')"
    case "$ports" in *'->'*) fail "un port est publié : $ports" ;; esac
    echo "OK   serveur seul sur un réseau interne, aucun port publié"
}

check_memory_limits() {
    for service in server web; do
        local limit
        limit="$(docker inspect -f '{{.HostConfig.Memory}}' "$(compose ps -q "$service")")"
        [ "$limit" -gt 0 ] || fail "$service n'a pas de limite de mémoire"
    done
    echo "OK   limite de mémoire sur chaque conteneur"
}

scenario_open() {
    step "Site ouvert (sans mot de passe)"
    ENV_FILE="$WORK/open.env"; base_env > "$ENV_FILE"
    up; wait_healthy server; wait_healthy web
    local ip; ip="$(web_address)"
    expect_status 200 "/sante" "http://$ip:8080/sante"
    expect_status 200 "page d'accueil" "http://$ip:8080/"
    expect_status 404 "/health fermé au public" "http://$ip:8080/health"
    [ "$(curl -s --max-time 5 "http://$ip:8080/sante")" = ok ] || fail "/sante doit répondre exactement « ok »"
    check_isolation
    check_memory_limits
    node deploy/smoke-chain.mjs "$ip" 8080 "$ORIGIN" || fail "la chaîne de confiance (X-Forwarded-For) est mal réglée"
    compose down -v >/dev/null
}

scenario_protected() {
    local label="$1" value="$2"
    step "Site protégé : $label"
    ENV_FILE="$WORK/protected.env"
    { base_env; printf '%s\n' "$value"; } > "$ENV_FILE"
    up; wait_healthy server; wait_healthy web
    local ip; ip="$(web_address)"
    expect_status 401 "page sans mot de passe" "http://$ip:8080/"
    expect_status 200 "/sante reste ouverte" "http://$ip:8080/sante"
    expect_status 401 "mauvais mot de passe" "http://$ip:8080/" -u "noc:${PASSWORD}-faux"
    expect_status 200 "bon mot de passe" "http://$ip:8080/" -u "noc:${PASSWORD}"
    local upgrade
    upgrade=(-H 'Connection: Upgrade' -H 'Upgrade: websocket' -H 'Sec-WebSocket-Version: 13' -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' -H "Origin: $ORIGIN")
    expect_status 401 "/ws sans mot de passe" "http://$ip:8080/ws" "${upgrade[@]}"
    got="$(curl -s -o /dev/null -w '%{http_code}' --max-time 3 -u "noc:${PASSWORD}" "${upgrade[@]}" "http://$ip:8080/ws" || true)"
    [ "$got" = 101 ] || fail "/ws avec le bon mot de passe : code $got au lieu de 101"
    echo "OK   /ws avec le bon mot de passe (101)"
    compose down -v >/dev/null
}

scenario_mangled() {
    step "Hachage mal protégé : le site ne doit JAMAIS rester ouvert"
    ENV_FILE="$WORK/mangled.env"
    # Les « $ » ne sont ni doublés ni entre apostrophes : Compose les interprète, le hachage arrive abîmé (ou pas du tout)
    { base_env; printf 'SITE_PASSWORD_HASH=%s\n' "$HASH"; } > "$ENV_FILE"
    if compose up -d >"$WORK/mangled.log" 2>&1; then
        sleep 15
        local id state
        id="$(compose ps -q web 2>/dev/null || true)"
        state="$([ -n "$id" ] && docker inspect -f '{{.State.Status}}' "$id" || echo absent)"
        [ "$state" != running ] || {
            ip="$(web_address)"
            [ "$(http_status "http://$ip:8080/")" = 200 ] && fail "le site est OUVERT alors que le hachage est abîmé"
        }
        compose logs web 2>&1 | grep -qi "bcrypt" || fail "le conteneur web n'a pas expliqué le problème de hachage"
        echo "OK   le conteneur web refuse de démarrer et explique pourquoi"
    else
        echo "OK   Docker Compose refuse la configuration : $(tail -n 1 "$WORK/mangled.log")"
    fi
    compose down -v >/dev/null 2>&1 || true
}

scenario_open
scenario_protected "hachage entre apostrophes (équivalent de « Literal » dans Coolify)" "SITE_PASSWORD_HASH='${HASH}'"
scenario_protected "hachage avec les « \$ » doublés" "SITE_PASSWORD_HASH=${ESCAPED}"
scenario_mangled

printf '\nTout est OK.\n'
