#include "php_apparmor.h"
#include <main/php_ini.h>
#include <ext/standard/info.h>
#include <sys/apparmor.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

ZEND_DECLARE_MODULE_GLOBALS(apparmor);

static int g_fd = -1;

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("aa.default_hat_name", "",   PHP_INI_SYSTEM,                  OnUpdateString, default_hat_name, zend_apparmor_globals, apparmor_globals)
	STD_PHP_INI_ENTRY("aa.hat_name",         "",   PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdateString, hat_name,         zend_apparmor_globals, apparmor_globals)
	STD_PHP_INI_ENTRY("aa.allow_server_aa",  "0",  PHP_INI_SYSTEM,                  OnUpdateLong,   allow_server_aa,  zend_apparmor_globals, apparmor_globals)
PHP_INI_END()

static PHP_MINIT_FUNCTION(apparmor)
{
	REGISTER_INI_ENTRIES();

	g_fd = open("/dev/urandom", O_RDONLY);
	if (g_fd < 0) {
		zend_error(E_CORE_ERROR, "Failed to open /dev/urandom: %s\n", strerror(errno));
		return FAILURE;
	}

	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(apparmor)
{
	UNREGISTER_INI_ENTRIES();

	if (g_fd != -1) {
		close(g_fd);
	}

	return SUCCESS;
}

unsigned long int generate_token(void)
{
	unsigned long int token;
	const size_t size = sizeof(token);
	size_t read_bytes = 0;
	char* buf = (char*)&token;

	while (read_bytes < size) {
		ssize_t n = read(g_fd, buf + read_bytes, size - read_bytes);
		if (n < 0) {
			break;
		}

		read_bytes += n;
	}

	if (read_bytes != size) {
		zend_error(E_CORE_ERROR, "Could not gather sufficient random data");
		return 0;
	}

	if (!token) {
		token = random();
		if (!token) {
			token = 1;
		}
	}

	return token;
}

/*
* hat_name, METHOD-SERVER-SCRIPT, SERVER-SCRIPT, SERVER, METHOD-SCRIPT, SCRIPT, default_hat_name
*/
static void do_change_hat(
#if PHP_MAJOR_VERSION < 7
	zval** server, unsigned long int* token TSRMLS_DC
#else
	zval* server, unsigned long int* token TSRMLS_DC
#endif
)
{
	char* default_hat_name = AAG(default_hat_name);
	char* hat_name         = AAG(hat_name);
	char* mss              = NULL;
	char* ss               = NULL;
	char* ms               = NULL;
	size_t idx             = 0;
	const char* subprofiles[8];

	if (hat_name && *hat_name) {
		subprofiles[idx++] = hat_name;
	}

	if (server) {
		char* req_m = NULL;
		char* scr_n = NULL;
		char* srv_n = NULL;
		size_t req_m_len = 0;
		size_t scr_n_len = 0;
		size_t srv_n_len = 0;

#if PHP_MAJOR_VERSION < 7
		zval** request_method  = NULL;
		zval** script_name     = NULL;
		zval** server_name     = NULL;

		zend_hash_quick_find(Z_ARRVAL_PP(server), ZEND_STRS("REQUEST_METHOD"), zend_inline_hash_func(ZEND_STRS("REQUEST_METHOD")), (void**)&request_method);
		zend_hash_quick_find(Z_ARRVAL_PP(server), ZEND_STRS("SCRIPT_NAME"),    zend_inline_hash_func(ZEND_STRS("SCRIPT_NAME")),    (void**)&script_name);
		zend_hash_quick_find(Z_ARRVAL_PP(server), ZEND_STRS("SERVER_NAME"),    zend_inline_hash_func(ZEND_STRS("SERVER_NAME")),    (void**)&server_name);

		if (request_method && Z_TYPE_PP(request_method) == IS_STRING) {
			req_m     = Z_STRVAL_PP(request_method);
			req_m_len = Z_STRLEN_PP(request_method);
		}

		if (script_name && Z_TYPE_PP(script_name) == IS_STRING) {
			scr_n     = Z_STRVAL_PP(script_name);
			scr_n_len = Z_STRLEN_PP(script_name);
		}

		if (server_name && Z_TYPE_PP(server_name) == IS_STRING) {
			srv_n     = Z_STRVAL_PP(server_name);
			srv_n_len = Z_STRLEN_PP(server_name);
		}
#else
		zval* request_method = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("REQUEST_METHOD"));
		zval* script_name    = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("SCRIPT_NAME"));
		zval* server_name    = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("SERVER_NAME"));

		if (request_method && Z_TYPE_P(request_method) == IS_STRING) {
			req_m     = Z_STRVAL_P(request_method);
			req_m_len = Z_STRLEN_P(request_method);
		}

		if (script_name && Z_TYPE_P(script_name) == IS_STRING) {
			scr_n     = Z_STRVAL_P(script_name);
			scr_n_len = Z_STRLEN_P(script_name);
		}

		if (server_name && Z_TYPE_P(server_name) == IS_STRING) {
			srv_n     = Z_STRVAL_P(server_name);
			srv_n_len = Z_STRLEN_P(server_name);
		}
#endif

		if (request_method && server_name && script_name) {
			mss = emalloc(req_m_len + 1 + srv_n_len + 1 + scr_n_len + 1);
			memcpy(mss, req_m, req_m_len);
			memcpy(mss + req_m_len + 1, srv_n, srv_n_len);
			memcpy(mss + req_m_len + 1 + srv_n_len + 1, scr_n, scr_n_len + 1);
			*(mss + req_m_len) = '-';
			*(mss + req_m_len + 1 + srv_n_len) = '-';

			subprofiles[idx++] = mss;
		}

		if (server_name && script_name) {
			ss = emalloc(srv_n_len + 1 + scr_n_len + 1);
			memcpy(ss, srv_n, srv_n_len);
			memcpy(ss + srv_n_len + 1, scr_n, scr_n_len + 1);
			*(ss + srv_n_len) = '-';

			subprofiles[idx++] = ss;
		}

		if (server_name) {
			subprofiles[idx++] = srv_n;
		}

		if (request_method && script_name) {
			ms = emalloc(req_m_len + 1 + scr_n_len + 1);
			memcpy(ms, req_m, req_m_len);
			memcpy(ms + req_m_len + 1, scr_n, scr_n_len + 1);
			*(ms + req_m_len) = '-';

			subprofiles[idx++] = ms;
		}

		if (script_name) {
			subprofiles[idx++] = scr_n;
		}
	}

	if (default_hat_name && *default_hat_name) {
		subprofiles[idx++] = default_hat_name;
	}

	if (idx) {
		subprofiles[idx] = NULL;
		if (-1 == aa_change_hatv(subprofiles, *token)) {
			aa_change_hat(NULL, *token);
			*token = 0;
		}
	}

	if (mss) { efree(mss); }
	if (ss)  { efree(ss);  }
	if (ms)  { efree(ms);  }
}

static PHP_RINIT_FUNCTION(apparmor)
{
	unsigned long int token;
#if PHP_MAJOR_VERSION < 7
	zval** server;
#else
	zval* server;
#endif

	if (PG(auto_globals_jit)) {
#if PHP_MAJOR_VERSION < 7
		zend_is_auto_global_quick(ZEND_STRL("_SERVER"), zend_inline_hash_func(ZEND_STRS("_SERVER")) TSRMLS_CC);
#else
		zend_is_auto_global_str(ZEND_STRL("_SERVER"));
#endif
	}

#if PHP_MAJOR_VERSION < 7
	if (SUCCESS != zend_hash_quick_find(&EG(symbol_table), ZEND_STRS("_SERVER"), zend_inline_hash_func(ZEND_STRS("_SERVER")), (void**)&server) || Z_TYPE_PP(server) != IS_ARRAY) {
		server = NULL;
	}

	if (AAG(allow_server_aa) && server) {
		zval** aa_hat_name;

		if (SUCCESS == zend_hash_quick_find(Z_ARRVAL_PP(server), ZEND_STRS("AA_HAT_NAME"), zend_inline_hash_func(ZEND_STRS("AA_HAT_NAME")), (void**)&aa_hat_name) && Z_TYPE_PP(aa_hat_name) == IS_STRING) {
			zend_alter_ini_entry_ex(ZEND_STRS("aa.hat_name"), Z_STRVAL_PP(aa_hat_name), Z_STRLEN_PP(aa_hat_name), ZEND_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE, 1 TSRMLS_CC);
		}
	}
#else
	server = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SERVER"));
	if (server && Z_TYPE_P(server) != IS_ARRAY) {
		server = NULL;
	}

	if (AAG(allow_server_aa) && server) {
		zval* aa_hat_name = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("AA_HAT_NAME"));
		if (aa_hat_name && Z_TYPE_P(aa_hat_name) == IS_STRING) {
			zend_string* key = zend_string_init(ZEND_STRL("aa.hat_name"), 0);
			zend_alter_ini_entry_ex(key, Z_STR_P(aa_hat_name), ZEND_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE, 1);
			zend_string_release(key);
		}
	}
#endif

	token = generate_token();
	if (!token) {
		return FAILURE;
	}

	do_change_hat(server, &token TSRMLS_CC);
	AAG(magic_token) = token;
	return SUCCESS;
}

static PHP_GINIT_FUNCTION(apparmor)
{
	apparmor_globals->default_hat_name = NULL;
	apparmor_globals->hat_name         = NULL;
	apparmor_globals->magic_token      = 0;
	apparmor_globals->allow_server_aa  = 0;
}

static PHP_MINFO_FUNCTION(apparmor)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "AppArmor Module", "enabled");
	php_info_print_table_row(2, "version", PHP_APPARMOR_EXTVER);
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}

static ZEND_MODULE_POST_ZEND_DEACTIVATE_D(apparmor)
{
#if PHP_MAJOR_VERSION < 7
	TSRMLS_FETCH();
#endif
	unsigned long int magic_token = AAG(magic_token);

	if (magic_token != 0) {
		aa_change_hat(NULL, magic_token);
		AAG(magic_token) = 0;
	}

	return SUCCESS;
}

zend_module_entry apparmor_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	NULL,
	PHP_APPARMOR_EXTNAME,
	NULL,
	PHP_MINIT(apparmor),
	PHP_MSHUTDOWN(apparmor),
	PHP_RINIT(apparmor),
	NULL,
	PHP_MINFO(apparmor),
	PHP_APPARMOR_EXTVER,
	ZEND_MODULE_GLOBALS(apparmor),
	PHP_GINIT(apparmor),
	NULL,
	ZEND_MODULE_POST_ZEND_DEACTIVATE_N(apparmor),
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_APPARMOR
ZEND_GET_MODULE(apparmor)
#endif
