die() {
  echo $1
  exit 1
}

INSTANCE_DIR="$(pwd)/${NAME}"

source google/env.inc

[[ -a build/sql/mysqld ]] || die "mysqld is missing"
[[ -a build/client/mysql ]] || die "mysql is missing"
BIN_DIR=$(readlink -f build/)

read -a cmd <<< "${BIN_DIR}/sql/mysqld \
  --no-defaults \
  --datadir=${INSTANCE_DIR}/datadir \
  --tmpdir=${INSTANCE_DIR}/tmp/ \
  --language=${BIN_DIR}/sql/share/english \
  --character-sets-dir=${BIN_DIR}/sql/share/charsets \
  --sock=${INSTANCE_DIR}/mysql.sock \
  --port=${PORT} \
  --server_id=${ID} \
  --pid-file=${INSTANCE_DIR}/mysql.pid \
  ${MYSQLD_DEFAULT_ARGS} ${MYSQLD_EXTRA_ARGS}"

if [[ ! -d ${INSTANCE_DIR} ]]
then
  mkdir -p ${INSTANCE_DIR}/datadir/mysql ${INSTANCE_DIR}/tmp

  echo ">>>>>> Bootstraping ${NAME}..."
  ( echo "use mysql;"; \
    cat ${SRC_DIR}/scripts/mysql_system_tables.sql \
    ${SRC_DIR}/scripts/mysql_system_tables_data.sql ) | \
  ${cmd[*]} --bootstrap \
    &> ${INSTANCE_DIR}/mysql.err
fi

if [[ -a ${INSTANCE_DIR}/mysql.port ]]
then
  PORT=$(cat ${INSTANCE_DIR}/mysql.port)
else
  LAST_PORT=$(echo 51000 $(find instance/ -name mysql.port | xargs cat) | fmt -1 | sort -n | tail -n1)
  PORT=$((${LAST_PORT} + 1))
  echo ${PORT} > ${INSTANCE_DIR}/mysql.port
fi

if [[ -a ${INSTANCE_DIR}/mysql.id ]]
then
  ID=$(cat ${INSTANCE_DIR}/mysql.id)
else
  LAST_ID=$(echo 0 $(find instance/ -name mysql.id | xargs cat) | fmt -1 | sort -n | tail -n1)
  ID=$((${LAST_ID} + 1))
  echo ${ID} > ${INSTANCE_DIR}/mysql.id
fi

echo ">>>>>> Starting ${NAME}..."
echo ">>>>>> Connect: ${BIN_DIR}/client/mysql -u root -S ${INSTANCE_DIR}/mysql.sock"
echo ">>>>>> Shutdown: ${BIN_DIR}/client/mysqladmin -u root -S ${INSTANCE_DIR}/mysql.sock shutdown"

if [[ -n "${GDB}" ]]
then
  ${BIN_DIR}/client/mysqladmin ping -S ${INSTANCE_DIR}/mysql.sock &> /dev/null
  if [[ "$?" == "0" ]]
  then
    gdb --eval-command="handle SIGUSR1 nostop" \
      ${BIN_DIR}/sql/mysqld $(cat ${INSTANCE_DIR}/mysql.pid)
  else
    gdb --eval-command="handle SIGUSR1 nostop" --args ${cmd[*]}
  fi
else
  ${BIN_DIR}/client/mysqladmin ping -S ${INSTANCE_DIR}/mysql.sock &> /dev/null
  if [[ "$?" == "0" ]]
  then
    echo "${NAME} is already started."
  else
    set -x
    ${cmd[*]} &>> ${INSTANCE_DIR}/mysql.err &
    set +x
  fi
fi
