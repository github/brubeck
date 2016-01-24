#include <string.h>

#include "sput.h"
#include "brubeck.h"

static void try_parse(struct brubeck_statsd_msg *msg, const char *msg_text, double expected, uint8_t expected_modifiers)
{
	char buffer[64];
	size_t len = strlen(msg_text);
	memcpy(buffer, msg_text, len);

	sput_fail_unless(brubeck_statsd_msg_parse(msg, buffer, len) == 0, msg_text);
	sput_fail_unless(expected == msg->value, "msg.value == expected");
	sput_fail_unless(expected_modifiers == msg->modifiers, "msg.modifiers == expected_modifiers");
}

void test_statsd_msg__parse_strings(void)
{
	struct brubeck_statsd_msg msg;

	try_parse(&msg, "github.auth.fingerprint.sha1:1|c", 1, 0);
	try_parse(&msg, "github.auth.fingerprint.sha1:1|c|@args", 1, 0);
	try_parse(&msg, "github.auth.fingerprint.sha1:1|g", 1, 0);
	try_parse(&msg, "lol:1|ms", 1, 0);
	try_parse(&msg, "this.is.sparta:199812|C", 199812, 0);
	try_parse(&msg, "this.is.sparta:0012|h", 12, 0);
	try_parse(&msg, "this.is.sparta:23.23|g", 23.23, 0);
	try_parse(&msg, "this.is.sparta:0.232030|g", 0.23203, 0);
	try_parse(&msg, "this.are.some.floats:1234567.89|g", 1234567.89, 0);
	try_parse(&msg, "gauge.increment:+1|g", 1, BRUBECK_MOD_RELATIVE_VALUE);
	try_parse(&msg, "gauge.decrement:-1|g", -1, BRUBECK_MOD_RELATIVE_VALUE);
}
