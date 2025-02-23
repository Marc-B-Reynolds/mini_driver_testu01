// Marc B. Reynolds, 2022-2025
// Public Domain under http://unlicense.org, see link for details.

#pramga once

// minimalist old school

#define ANSI_EC_START "\033["

static const char* ansi_rest      = ANSI_EC_START "0m";
static const char* ansi_bold      = ANSI_EC_START "1m";
static const char* ansi_faint     = ANSI_EC_START "2m";
static const char* ansi_underline = ANSI_EC_START "4m";
static const char* ansi_invert    = ANSI_EC_START "7m";

static const char* ansi_black   = ANSI_EC_START "30m";
static const char* ansi_red     = ANSI_EC_START "31m";
static const char* ansi_green   = ANSI_EC_START "32m";
static const char* ansi_yellow  = ANSI_EC_START "33m";
static const char* ansi_blue    = ANSI_EC_START "34m";
static const char* ansi_magenta = ANSI_EC_START "35m";
static const char* ansi_cyan    = ANSI_EC_START "36m";
static const char* ansi_white   = ANSI_EC_START "37m";

static const char* ansi_bg_black   = ANSI_EC_START "40m";
static const char* ansi_bg_red     = ANSI_EC_START "41m";
static const char* ansi_bg_green   = ANSI_EC_START "42m";
static const char* ansi_bg_yellow  = ANSI_EC_START "43m";
static const char* ansi_bg_blue    = ANSI_EC_START "44m";
static const char* ansi_bg_magenta = ANSI_EC_START "45m";
static const char* ansi_bg_cyan    = ANSI_EC_START "46m";
static const char* ansi_bg_white   = ANSI_EC_START "47m";

static const char* ansi_clear_screen = ANSI_EC_START "2J";
static const char* ansi_clear_eol    = ANSI_EC_START "K";

