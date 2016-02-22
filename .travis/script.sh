#!/bin/sh

set -e

DIR=$(readlink -enq $(dirname $0))

phpize
./configure --silent
make --silent CFLAGS+="-Wall -Wextra -Wno-unused-parameter -DDEBUG_PHP_AA"

phpenv config-add "$DIR/apparmor.ini"
make install

php -i > /dev/null
php -m > /dev/null

(cd $DIR && php index.php > out.001 && cmp out.001 expected.001)
