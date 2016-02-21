#ifndef PHP_APPARMOR_H
#define PHP_APPARMOR_H

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include <main/php.h>
#include <TSRM/TSRM.h>
#include <Zend/zend.h>

#ifndef HAVE_SYS_APPARMOR_H
#	error "Fatal: <sys/apparmor.h> header is missing"
#endif

/*
#ifdef ZTS
#	error "ZTS is currently not supported"
#endif
*/

#define PHP_APPARMOR_EXTNAME    "AppArmor"
#define PHP_APPARMOR_EXTVER     "0.1"

#if __GNUC__ + 0 >= 4
#	define AA_VISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#else
#	define AA_VISIBILITY_HIDDEN
#endif

ZEND_BEGIN_MODULE_GLOBALS(apparmor)
	char* default_hat_name;
	char* hat_name;
	unsigned long int magic_token;
	long int allow_server_aa;
ZEND_END_MODULE_GLOBALS(apparmor)

AA_VISIBILITY_HIDDEN extern ZEND_DECLARE_MODULE_GLOBALS(apparmor);

#ifdef ZTS
#	define AAG(v)  TSRMG(apparmor_globals_id, zend_apparmor_globals*, v)
#else
#	define AAG(v)  (apparmor_globals.v)
#endif

AA_VISIBILITY_HIDDEN extern zend_module_entry apparmor_module_entry;
#define phpext_apparmor_ptr &apparmor_module_entry

#endif /* PHP_APPARMOR_H */
