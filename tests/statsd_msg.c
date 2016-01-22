#include <string.h>

#include "sput.h"
#include "brubeck.h"

static void must_parse(const char *msg_text, double value, double sample)
{
	struct brubeck_statsd_msg msg;
	char buffer[128];
	size_t len = strlen(msg_text);
	memcpy(buffer, msg_text, len);

	sput_fail_unless(brubeck_statsd_msg_parse(&msg, buffer, buffer + len) == 0, msg_text);
	sput_fail_unless(value == msg.value, "msg.value == expected");
	sput_fail_unless(sample == msg.sample_rate, "msg.sample_rate == expected");
}

static void must_not_parse(const char *msg_text)
{
	struct brubeck_statsd_msg msg;
	char buffer[128];
	size_t len = strlen(msg_text);
	memcpy(buffer, msg_text, len);

	sput_fail_unless(brubeck_statsd_msg_parse(&msg, buffer, buffer + len) < 0, msg_text);
}

void test_statsd_msg__parse_strings(void)
{
	must_parse("github.auth.fingerprint.sha1:1|c", 1, 1.0);
	must_parse("github.auth.fingerprint.sha1:1|c|@0.1", 1, 0.1);
	must_parse("github.auth.fingerprint.sha1:1|g", 1, 1.0);
	must_parse("lol:1|ms", 1, 1.0);
	must_parse("this.is.sparta:199812|C", 199812, 1.0);
	must_parse("this.is.sparta:0012|h", 12, 1.0);
	must_parse("this.is.sparta:23.23|g", 23.23, 1.0);
	must_parse("this.is.sparta:0.232030|g", 0.23203, 1.0);
	must_parse("this.are.some.floats:1234567.89|g", 1234567.89, 1.0);
	must_parse("this.are.some.floats:1234567.89|g|@0.023", 1234567.89, 0.023);
	must_parse("this.are.some.floats:1234567.89|g|@0.3", 1234567.89, 0.3);
	must_parse("this.are.some.floats:1234567.89|g|@0.01", 1234567.89, 0.01);
	must_parse("this.are.some.floats:1234567.89|g|@000.0100", 1234567.89, 0.01);
	must_parse("this.are.some.floats:1234567.89|g|@1.0", 1234567.89, 1.0);
	must_parse("this.are.some.floats:1234567.89|g|@1", 1234567.89, 1.0);
	must_parse("this.are.some.floats:1234567.89|g|@1.", 1234567.89, 1.0);
	must_parse("this.are.some.floats:|g", 0.0, 1.0);

	must_not_parse("this.are.some.floats:12.89.23|g");
	must_not_parse("this.are.some.floats:12.89|a");
	must_not_parse("this.are.some.floats:12.89|msdos");
	must_not_parse("this.are.some.floats:12.89g|g");
	must_not_parse("this.are.some.floats:12.89|");
	must_not_parse("this.are.some.floats:12.89");
	must_not_parse("this.are.some.floats:12.89 |g");
	must_not_parse("this.are.some.floats|g");
	must_not_parse("this.are some.floats:1.0|g|1.0");
	must_not_parse("this.are some.floats:1.0|g|0.1");
	must_not_parse("this.are some.floats:1.0|g|@0.1.1");
	must_not_parse("this.are some.floats:1.0|g|@0.1@");
	must_not_parse("this.are some.floats:1.0|g|@0.1125.2");
	must_not_parse("this.are some.floats:1.0|g|@0.1125.2");
	must_not_parse("this.are some.floats:1.0|g|@1.23");
	must_not_parse("this.are some.floats:1.0|g|@3.0");
	must_not_parse("this.are some.floats:1.0|g|@-3.0");
	must_not_parse("this.are some.floats:1.0|g|@-1.0");
	must_not_parse("this.are some.floats:1.0|g|@-0.23");
	must_not_parse("this.are some.floats:1.0|g|@0.0");
	must_not_parse("this.are some.floats:1.0|g|@0");
}
